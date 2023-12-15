/*
 * QEMU VMware-SVGA "chipset".
 *
 * Copyright (c) 2007 Andrzej Zaborowski  <balrog@zabor.org>
 *
 * Copyright (c) 2023 Christopher Lentocha <christopherericlentocha@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/loader.h"
#include "trace.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qom/object.h"

#define VERBOSE

#include "include/includeCheck.h"
#include "include/svga3d_caps.h"
#include "include/svga3d_cmd.h"
#include "include/svga3d_devcaps.h"
#include "include/svga3d_dx.h"
#include "include/svga3d_limits.h"
#include "include/svga3d_reg.h"
#include "include/svga3d_shaderdefs.h"
#include "include/svga3d_surfacedefs.h"
#include "include/svga3d_types.h"
#include "include/svga_escape.h"
#include "include/svga_overlay.h"
#include "include/svga_reg.h"
#include "include/svga_types.h"
#include "include/VGPU10ShaderTokens.h"
#include "include/vmware_pack_begin.h"
#include "include/vmware_pack_end.h"

#include "vga_int.h"

struct vmsvga_state_s {
    VGACommonState vga;

    int invalidated;
    int enable;
    int config;
    struct {
        int id;
        int x;
        int y;
        int on;
    } cursor;
    uint32_t last_fifo_cursor_count;

    int index;
    int scratch_size;
    uint32_t *scratch;
    int new_width;
    int new_height;
    int new_depth;
    uint32_t wred;
    uint32_t wgreen;
    uint32_t wblue;
    uint32_t num_gd;
    uint32_t disp_prim;
    uint32_t disp_x;
    uint32_t disp_y;
    uint32_t devcap_val;
    uint32_t gmrdesc;
    uint32_t gmrid;
    uint32_t tracez;
    uint32_t cmd_low;
    uint32_t cmd_high;
    uint32_t guest;
    uint32_t svgaid;
    uint32_t thread;
    uint32_t fg;
    int syncing;
    int syncbusy;

    MemoryRegion fifo_ram;
    unsigned int fifo_size;

    uint32_t *fifo;
    uint32_t fifo_min;
    uint32_t fifo_max;
    uint32_t fifo_next;
    uint32_t fifo_stop;

#define REDRAW_FIFO_LEN  512
    struct vmsvga_rect_s {
        int x, y, w, h;
    } redraw_fifo[REDRAW_FIFO_LEN];
    int redraw_fifo_last;

    uint32_t num_fifo_regs;
    uint32_t irq_mask;
    uint32_t irq_status;
    uint32_t display_id;
    uint32_t pitchlock;
    uint32_t svgabasea;
    uint32_t svgabaseb;
};

#define TYPE_VMWARE_SVGA "vmware-svga"

DECLARE_INSTANCE_CHECKER(struct pci_vmsvga_state_s, VMWARE_SVGA,
                         TYPE_VMWARE_SVGA)

struct pci_vmsvga_state_s {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    struct vmsvga_state_s chip;
    MemoryRegion io_bar;
};

/* Update cursor position from SVGA_FIFO_CURSOR registers */
static void cursor_update_from_fifo(struct vmsvga_state_s *s)
{
    uint32_t fifo_cursor_count;
    uint32_t on_off;

    if (s->config != 1 || s->enable != 1) {
        return;
    }

    if (s->fifo_min <= SVGA_FIFO_CURSOR_LAST_UPDATED) {
        return;
    }

    fifo_cursor_count = s->fifo[SVGA_FIFO_CURSOR_COUNT];
    if (fifo_cursor_count == s->last_fifo_cursor_count)
        return;

    s->last_fifo_cursor_count = fifo_cursor_count;
    on_off = s->fifo[SVGA_FIFO_CURSOR_ON] ? SVGA_CURSOR_ON_SHOW : SVGA_CURSOR_ON_HIDE;
    s->cursor.on = on_off;
    s->cursor.x = s->fifo[SVGA_FIFO_CURSOR_X];
    s->cursor.y = s->fifo[SVGA_FIFO_CURSOR_Y];

    dpy_mouse_set(s->vga.con, s->cursor.x, s->cursor.y, s->cursor.on);
}

static inline bool vmsvga_verify_rect(DisplaySurface *surface,
                                      const char *name,
                                      int x, int y, int w, int h)
{
    if (x < 0) {
        trace_vmware_verify_rect_less_than_zero(name, "x", x);
        return false;
    }
    if (x > SVGA_MAX_WIDTH) {
        trace_vmware_verify_rect_greater_than_bound(name, "x", SVGA_MAX_WIDTH,
                                                    x);
        return false;
    }
    if (w < 0) {
        trace_vmware_verify_rect_less_than_zero(name, "w", w);
        return false;
    }
    if (w > SVGA_MAX_WIDTH) {
        trace_vmware_verify_rect_greater_than_bound(name, "w", SVGA_MAX_WIDTH,
                                                    w);
        return false;
    }
    if (x + w > surface_width(surface)) {
        trace_vmware_verify_rect_surface_bound_exceeded(name, "width",
                                                        surface_width(surface),
                                                        "x", x, "w", w);
        return false;
    }

    if (y < 0) {
        trace_vmware_verify_rect_less_than_zero(name, "y", y);
        return false;
    }
    if (y > SVGA_MAX_WIDTH) {
        trace_vmware_verify_rect_greater_than_bound(name, "y", SVGA_MAX_HEIGHT,
                                                    y);
        return false;
    }
    if (h < 0) {
        trace_vmware_verify_rect_less_than_zero(name, "h", h);
        return false;
    }
    if (h > SVGA_MAX_HEIGHT) {
        trace_vmware_verify_rect_greater_than_bound(name, "y", SVGA_MAX_HEIGHT,
                                                    y);
        return false;
    }
    if (y + h > surface_height(surface)) {
        trace_vmware_verify_rect_surface_bound_exceeded(name, "height",
                                                        surface_height(surface),
                                                        "y", y, "h", h);
        return false;
    }

    return true;
}

static inline void vmsvga_update_rect(struct vmsvga_state_s *s,
                                      int x, int y, int w, int h)
{
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    int line;
    int bypl;
    int width;
    int start;
    uint8_t *src;
    uint8_t *dst;

    if (!vmsvga_verify_rect(surface, __func__, x, y, w, h)) {
        x = 0;
        y = 0;
        w = s->new_width;
        h = s->new_height;
    }

    bypl = surface_stride(surface);
    width = surface_bytes_per_pixel(surface) * w;
    start = surface_bytes_per_pixel(surface) * x + bypl * y;
    src = s->vga.vram_ptr + start;
    dst = surface_data(surface) + start;

    for (line = h; line > 0; line--, src += bypl, dst += bypl) {
        memcpy(dst, src, width);
    }
    dpy_gfx_update(s->vga.con, x, y, w, h);
}

static inline int vmsvga_copy_rect(struct vmsvga_state_s *s,
                int x0, int y0, int x1, int y1, int w, int h)
{
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    uint8_t *vram = s->vga.vram_ptr;
    int bypl = surface_stride(surface);
    int bypp = surface_bytes_per_pixel(surface);
    int width = bypp * w;
    int line = h;
    uint8_t *ptr[2];

    if (!vmsvga_verify_rect(surface, "vmsvga_copy_rect/src", x0, y0, w, h)) {
	return 1;
    }
    if (!vmsvga_verify_rect(surface, "vmsvga_copy_rect/dst", x1, y1, w, h)) {
	return 1;
    }

    if (y1 > y0) {
        ptr[0] = vram + bypp * x0 + bypl * (y0 + h - 1);
        ptr[1] = vram + bypp * x1 + bypl * (y1 + h - 1);
        for (; line > 0; line --, ptr[0] -= bypl, ptr[1] -= bypl) {
            memmove(ptr[1], ptr[0], width);
        }
    } else {
        ptr[0] = vram + bypp * x0 + bypl * y0;
        ptr[1] = vram + bypp * x1 + bypl * y1;
        for (; line > 0; line --, ptr[0] += bypl, ptr[1] += bypl) {
            memmove(ptr[1], ptr[0], width);
        }
    }

    vmsvga_update_rect(s, x1, y1, w, h);
    return 0;
}

static inline int vmsvga_fill_rect(struct vmsvga_state_s *s,
                uint32_t c, int x, int y, int w, int h)
{
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    int bypl = surface_stride(surface);
    int width = surface_bytes_per_pixel(surface) * w;
    int line = h;
    int column;
    uint8_t *fst;
    uint8_t *dst;
    uint8_t *src;
    uint8_t col[4];

    if (!vmsvga_verify_rect(surface, __func__, x, y, w, h)) {
	return 1;
    }

    col[0] = c;
    col[1] = c >> 8;
    col[2] = c >> 16;
    col[3] = c >> 24;

    fst = s->vga.vram_ptr + surface_bytes_per_pixel(surface) * x + bypl * y;

    if (line--) {
        dst = fst;
        src = col;
        for (column = width; column > 0; column--) {
            *(dst++) = *(src++);
            if (src - col == surface_bytes_per_pixel(surface)) {
                src = col;
            }
        }
        dst = fst;
        for (; line > 0; line--) {
            dst += bypl;
            memcpy(dst, fst, width);
        }
    }

    vmsvga_update_rect(s, x, y, w, h);
    return 0;
}

struct vmsvga_cursor_definition_s {
    uint32_t width;
    uint32_t height;
    int id;
    int hot_x;
    int hot_y;
    uint32_t and_mask_bpp; // Value must be 1 or equal to BITS_PER_PIXEL
    uint32_t xor_mask_bpp; // Value must be 1 or equal to BITS_PER_PIXEL
    uint32_t and_mask[4096];
    uint32_t xor_mask[4096];
};

#define SVGA_BITMAP_SIZE(w, h)          ((((w) + 31) >> 5) * (h))
#define SVGA_PIXMAP_SIZE(w, h, bpp)     (((((w) * (bpp)) + 31) >> 5) * (h))

static inline void vmsvga_cursor_define(struct vmsvga_state_s *s,
                struct vmsvga_cursor_definition_s *c)
{
    QEMUCursor *qc;
    int i, pixels;

    qc = cursor_alloc(c->width, c->height);
    assert(qc != NULL);

    qc->hot_x = c->hot_x;
    qc->hot_y = c->hot_y;
    switch (c->xor_mask_bpp) {
    case 1:
        cursor_set_mono(qc, 0xffffff, 0x000000, (void *)c->xor_mask,
                        1, (void *)c->and_mask);
        break;
    case 32:
        /* fill alpha channel from mask, set color to zero */
        cursor_set_mono(qc, 0x000000, 0x000000, (void *)c->and_mask,
                        1, (void *)c->and_mask);
        /* add in rgb values */
        pixels = c->width * c->height;
        for (i = 0; i < pixels; i++) {
            qc->data[i] |= c->xor_mask[i] & 0xffffff;
        }
        break;
    default:
        fprintf(stderr, "%s: unhandled bpp %d, using fallback cursor\n", __func__, c->xor_mask_bpp);
        cursor_put(qc);
        qc = cursor_builtin_left_ptr();
    }

    dpy_cursor_define(s->vga.con, qc);
    cursor_put(qc);
}

static inline void vmsvga_rgba_cursor_define(struct vmsvga_state_s *s,
                struct vmsvga_cursor_definition_s *c)
{
    QEMUCursor *qc;
    int i, pixels = c->width * c->height;

    qc = cursor_alloc(c->width, c->height);
    qc->hot_x = c->hot_x;
    qc->hot_y = c->hot_y;

    /* fill alpha channel and rgb values */
    for (i = 0; i < pixels; i++) {
        qc->data[i] = c->xor_mask[i];
        /*
         * Turn semi-transparent pixels to fully opaque
         * (opaque pixels stay opaque), due to lack of
         * alpha-blending support in QEMU framework.
         * This is a trade-off between cursor completely
         * missing and cursors with some minor artifacts
         * (such as Windows Aero style cursors).
         */
        if (c->and_mask[i]) {
            qc->data[i] |= 0xff000000;
        }
    }
    dpy_cursor_define(s->vga.con, qc);
    cursor_put(qc);
}

static inline int vmsvga_fifo_length(struct vmsvga_state_s *s)
{
    int num;

    if (s->config != 1 || s->enable != 1) {
        return 0;
    }

    s->fifo_min  = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
    s->fifo_max  = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
    s->fifo_next = le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]);
    s->fifo_stop = le32_to_cpu(s->fifo[SVGA_FIFO_STOP]);

    /* Check range and alignment.  */
    if ((s->fifo_min | s->fifo_max | s->fifo_next | s->fifo_stop) & 3) {
        return 0;
    }
    if (s->fifo_min < sizeof(uint32_t) * 4) {
        return 0;
    }
    if (s->fifo_max > (8 * 1024 * 1024) ||
        s->fifo_min >= (8 * 1024 * 1024) ||
        s->fifo_stop >= (8 * 1024 * 1024) ||
        s->fifo_next >= (8 * 1024 * 1024)) {
        return 0;
    }
    if (s->fifo_max < s->fifo_min + 10 * KiB) {
        return 0;
    }

    num = s->fifo_next - s->fifo_stop;
    if (num < 0) {
        num += s->fifo_max - s->fifo_min;
    }
    return num >> 2;
}

static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s *s)
{
    uint32_t cmd = s->fifo[s->fifo_stop >> 2];

    s->fifo_stop += 4;
    if (s->fifo_stop >= s->fifo_max) {
        s->fifo_stop = s->fifo_min;
    }
    s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
    return cmd;
}

static inline uint32_t vmsvga_fifo_read(struct vmsvga_state_s *s)
{
    return le32_to_cpu(vmsvga_fifo_read_raw(s));
}

static void vmsvga_fifo_run(struct vmsvga_state_s *s)
{
    uint32_t cmd;
    int args, len, maxloop = 1024;
    int i, x, y, dx, dy, width, height;
    struct vmsvga_cursor_definition_s cursor;
    uint32_t cmd_start;
    uint32_t fence_arg;
    uint32_t flags, num_pages;
//    bool cmd_ignored;
    bool irq_pending = false;
    bool fifo_progress = false;

    len = vmsvga_fifo_length(s);
    while (len > 0 && --maxloop > 0) {
        /* May need to go back to the start of the command if incomplete */
        cmd_start = s->fifo_stop;
//        cmd_ignored = false;

#ifdef VERBOSE
        printf("%s: Unknown command in SVGA command FIFO\n", __func__);
#endif

        switch (cmd = vmsvga_fifo_read(s)) {

        /* Implemented commands */
        case SVGA_CMD_UPDATE_VERBOSE:
            len -= 6;
            if (len < 0) {
                goto rewind;
            }

            x = vmsvga_fifo_read(s);
            y = vmsvga_fifo_read(s);
            width = vmsvga_fifo_read(s);
            height = vmsvga_fifo_read(s);
            vmsvga_fifo_read(s);
            vmsvga_update_rect(s, x, y, width, height);
            args = 1;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_UPDATE_VERBOSE command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_UPDATE:
            len -= 5;
            if (len < 0) {
                goto rewind;
            }

            x = vmsvga_fifo_read(s);
            y = vmsvga_fifo_read(s);
            width = vmsvga_fifo_read(s);
            height = vmsvga_fifo_read(s);
            vmsvga_update_rect(s, x, y, width, height);
            args = 1;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_UPDATE command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_RECT_COPY:
            len -= 7;
            if (len < 0) {
                goto rewind;
            }

            x = vmsvga_fifo_read(s);
            y = vmsvga_fifo_read(s);
            dx = vmsvga_fifo_read(s);
            dy = vmsvga_fifo_read(s);
            width = vmsvga_fifo_read(s);
            height = vmsvga_fifo_read(s);
            vmsvga_copy_rect(s, x, y, dx, dy, width, height);
	args = 1;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_RECT_COPY command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEFINE_CURSOR:
            len -= 8;
            if (len < 0) {
                goto rewind;
            }

            cursor.id = vmsvga_fifo_read(s);
            cursor.hot_x = vmsvga_fifo_read(s);
            cursor.hot_y = vmsvga_fifo_read(s);
            cursor.width = x = vmsvga_fifo_read(s);
            cursor.height = y = vmsvga_fifo_read(s);
            cursor.and_mask_bpp = vmsvga_fifo_read(s);
            cursor.xor_mask_bpp = vmsvga_fifo_read(s);

            args = SVGA_PIXMAP_SIZE(x, y, cursor.and_mask_bpp) +
                SVGA_PIXMAP_SIZE(x, y, cursor.xor_mask_bpp);
            if (cursor.width > 256
                || cursor.height > 256
                || cursor.and_mask_bpp > 32
                || cursor.xor_mask_bpp > 32
                || SVGA_PIXMAP_SIZE(x, y, cursor.and_mask_bpp)
                    > ARRAY_SIZE(cursor.and_mask)
                || SVGA_PIXMAP_SIZE(x, y, cursor.xor_mask_bpp)
                    > ARRAY_SIZE(cursor.xor_mask)) {
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_CURSOR command in SVGA command FIFO\n", __func__);
#endif
                   break;
            }

            len -= args;
            if (len < 0) {
                goto rewind;
            }

            for (args = 0; args < SVGA_PIXMAP_SIZE(x, y, cursor.and_mask_bpp); args++) {
                cursor.and_mask[args] = vmsvga_fifo_read_raw(s);
            }
            for (args = 0; args < SVGA_PIXMAP_SIZE(x, y, cursor.xor_mask_bpp); args++) {
                cursor.xor_mask[args] = vmsvga_fifo_read_raw(s);
            }
            vmsvga_cursor_define(s, &cursor);
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_CURSOR command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEFINE_ALPHA_CURSOR:
            len -= 6;
            if (len < 0) {
                goto rewind;
            }

            cursor.id = vmsvga_fifo_read(s);
            cursor.hot_x = vmsvga_fifo_read(s);
            cursor.hot_y = vmsvga_fifo_read(s);
            cursor.width  = x = vmsvga_fifo_read(s);
            cursor.height = y = vmsvga_fifo_read(s);
            cursor.and_mask_bpp = 32;
            cursor.xor_mask_bpp = 32;
            args = x * y;

            len -= args;
            if (len < 0) {
                goto rewind;
            }

            for (i = 0; i < args; i++) {
                uint32_t rgba = vmsvga_fifo_read_raw(s);
                cursor.xor_mask[i] = rgba & 0x00ffffff;
                cursor.and_mask[i] = rgba & 0xff000000;
            }

            vmsvga_rgba_cursor_define(s, &cursor);
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_ALPHA_CURSOR command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_FRONT_ROP_FILL:
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 6;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_FRONT_ROP_FILL command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_FENCE:
            len -= 2;
            if (len < 0) {
                goto rewind;
            }

            fence_arg = vmsvga_fifo_read(s);
            s->fifo[SVGA_FIFO_FENCE] = cpu_to_le32(fence_arg);

            if (s->irq_mask & SVGA_IRQFLAG_ANY_FENCE) {
                s->irq_status |= SVGA_IRQFLAG_ANY_FENCE;
                irq_pending = true;
            }

            if ((s->irq_mask & SVGA_IRQFLAG_FENCE_GOAL)
               && (s->fifo_min > SVGA_FIFO_FENCE_GOAL)
               && (s->fifo[SVGA_FIFO_FENCE_GOAL] == fence_arg)) {
                s->irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
                irq_pending = true;
            }

            if ((s->irq_mask & SVGA_IRQFLAG_FENCE_GOAL)
               && (s->fifo_min > s->fg)
               && (s->fg == fence_arg)) {
                s->irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
                irq_pending = true;
            }

#ifdef VERBOSE
        printf("%s: SVGA_CMD_FENCE command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEFINE_GMR2:
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 2;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_GMR2 command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_REMAP_GMR2:
            len -= 5;
            if (len < 0) {
                goto rewind;
            }

            vmsvga_fifo_read(s);            /* gmrId */
            flags = vmsvga_fifo_read(s);
            vmsvga_fifo_read(s);            /* offsetPages */
            num_pages = vmsvga_fifo_read(s);

            if (flags & SVGA_REMAP_GMR2_VIA_GMR) {
                /* Read single struct SVGAGuestPtr */
                args = 2;
            } else {
                args = (flags & SVGA_REMAP_GMR2_SINGLE_PPN) ? 1 : num_pages;
                if (flags & SVGA_REMAP_GMR2_PPN64)
                    args *= 2;
            }

#ifdef VERBOSE
        printf("%s: SVGA_CMD_REMAP_GMR2 command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_RECT_ROP_COPY: 
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 7;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_RECT_ROP_COPY command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_INVALID_CMD:
		int cx = 0;
		int cy = 0;
		vmsvga_update_rect(s, cx, cy, s->new_width, s->new_height);
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_INVALID_CMD command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_ESCAPE:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_ESCAPE command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEFINE_SCREEN:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_SCREEN command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DESTROY_SCREEN:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DESTROY_SCREEN command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEFINE_GMRFB:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_GMRFB command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_BLIT_GMRFB_TO_SCREEN command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_BLIT_SCREEN_TO_GMRFB command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_ANNOTATION_FILL:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_ANNOTATION_FILL command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_ANNOTATION_COPY:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_ANNOTATION_COPY command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEAD:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEAD command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_DEAD_2:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_DEAD_2 command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_NOP:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_NOP command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_NOP_ERROR:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_NOP_ERROR command in SVGA command FIFO\n", __func__);
#endif
            break;

        case SVGA_CMD_MAX:
	args = 0;
#ifdef VERBOSE
        printf("%s: SVGA_CMD_MAX command in SVGA command FIFO\n", __func__);
#endif
            break;

        default:
            args = 0;
            if (len < 0) {
                goto rewind;
            }

            while (args--) {
                vmsvga_fifo_read(s);
            }

            printf("%s: Bad command %d in SVGA command FIFO\n", __func__, cmd);
#ifdef VERBOSE
        printf("%s: default command in SVGA command FIFO\n", __func__);
#endif
            break;

        rewind:
            s->fifo_stop = cmd_start;
            s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
#ifdef VERBOSE
        printf("%s: rewind command in SVGA command FIFO\n", __func__);
#endif
            break;
        }

        if (s->fifo_stop != cmd_start)
            fifo_progress = true;

    }

    if ((s->irq_mask & SVGA_IRQFLAG_FIFO_PROGRESS) &&
        fifo_progress) {
        s->irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
        irq_pending = true;
    }

    s->syncing = 0;

    /* Need to raise irq ? */

    if (irq_pending && (s->irq_status & s->irq_mask)) {
        struct pci_vmsvga_state_s *pci_vmsvga
            = container_of(s, struct pci_vmsvga_state_s, chip);
        pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
    }

}

static uint32_t vmsvga_index_read(void *opaque, uint32_t address)
{
    struct vmsvga_state_s *s = opaque;

    return s->index;
}

static void vmsvga_index_write(void *opaque, uint32_t address, uint32_t index)
{
    struct vmsvga_state_s *s = opaque;

    s->index = index;
}

void *vmsvga_fifo_hack(void *arg);

void *vmsvga_fifo_hack(void *arg) {
	struct vmsvga_state_s *s = (struct vmsvga_state_s *)arg;
	while (true) {
		int cx = 0;
		int cy = 0;
		if (s->enable != 1 && s->config != 1) {
			return 0;
		};
		vmsvga_update_rect(s, cx, cy, s->new_width, s->new_height);
	};
};

static uint32_t vmsvga_value_read(void *opaque, uint32_t address)
{
    uint32_t caps;
    uint32_t cap2;
    struct vmsvga_state_s *s = opaque;
//    DisplaySurface *surface = qemu_console_surface(s->vga.con);
//    PixelFormat pf;
    uint32_t ret;

#ifdef VERBOSE
        printf("%s: Unknown register %d\n", __func__, s->index);
#endif

    switch (s->index) {
#define SVGA_CAP_DX 0x10000000
#define SVGA_CAP_HP_CMD_QUEUE 0x20000000
#define SVGA_CAP_NO_BB_RESTRICTION 0x40000000
#define SVGA_CAP2_DX2 0x00000004
#define SVGA_CAP2_GB_MEMSIZE_2 0x00000008
#define SVGA_CAP2_SCREENDMA_REG 0x00000010
#define SVGA_CAP2_OTABLE_PTDEPTH_2 0x00000020
#define SVGA_CAP2_NON_MS_TO_MS_STRETCHBLT 0x00000040
#define SVGA_CAP2_CURSOR_MOB 0x00000080
#define SVGA_CAP2_MSHINT 0x00000100
#define SVGA_CAP2_CB_MAX_SIZE_4MB 0x00000200
#define SVGA_CAP2_DX3 0x00000400
#define SVGA_CAP2_FRAME_TYPE 0x00000800
#define SVGA_CAP2_COTABLE_COPY 0x00001000
#define SVGA_CAP2_TRACE_FULL_FB 0x00002000
#define SVGA_CAP2_EXTRA_REGS 0x00004000
#define SVGA_CAP2_LO_STAGING 0x00008000
#define SVGA_CAP2_VIDEO_BLT 0x00010000
#define SVGA_REG_GBOBJECT_MEM_SIZE_KB 76
#define SVGA_REG_FENCE_GOAL 84

    case SVGA_REG_FENCE_GOAL:
        ret = s->fg;
#ifdef VERBOSE
        printf("%s: SVGA_REG_FENCE_GOAL register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_ID:
        ret = s->svgaid;
#ifdef VERBOSE
        printf("%s: SVGA_REG_ID register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_ENABLE:
        ret = s->enable;
#ifdef VERBOSE
        printf("%s: SVGA_REG_ENABLE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_WIDTH:
	ret = s->new_width;
#ifdef VERBOSE
        printf("%s: SVGA_REG_WIDTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_HEIGHT:
	ret = s->new_height;
#ifdef VERBOSE
        printf("%s: SVGA_REG_HEIGHT register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MAX_WIDTH:
        ret = SVGA_MAX_WIDTH;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MAX_WIDTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_SCREENTARGET_MAX_WIDTH:
        ret = SVGA_MAX_WIDTH;
#ifdef VERBOSE
        printf("%s: SVGA_REG_SCREENTARGET_MAX_WIDTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MAX_HEIGHT:
        ret = SVGA_MAX_HEIGHT;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MAX_HEIGHT register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_SCREENTARGET_MAX_HEIGHT:
        ret = SVGA_MAX_HEIGHT;
#ifdef VERBOSE
        printf("%s: SVGA_REG_SCREENTARGET_MAX_HEIGHT register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_BITS_PER_PIXEL:
	ret = s->new_depth;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BITS_PER_PIXEL register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_HOST_BITS_PER_PIXEL:
	ret = s->new_depth;
#ifdef VERBOSE
        printf("%s: SVGA_REG_HOST_BITS_PER_PIXEL register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_DEPTH:
	if (s->new_depth == 32) {
	ret = 24;
	} else {
	ret = s->new_depth;
	};
#ifdef VERBOSE
        printf("%s: SVGA_REG_DEPTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_PSEUDOCOLOR:
        ret = 0;
#ifdef VERBOSE
        printf("%s: SVGA_REG_PSEUDOCOLOR register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_RED_MASK:
        ret = s->wred;
#ifdef VERBOSE
        printf("%s: SVGA_REG_RED_MASK register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_GREEN_MASK:
        ret = s->wgreen;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GREEN_MASK register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_BLUE_MASK:
        ret = s->wblue;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BLUE_MASK register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_BYTES_PER_LINE:
        ret = (s->new_depth * s->new_width) / 8;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BYTES_PER_LINE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_FB_START: {
        struct pci_vmsvga_state_s *pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
        ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
#ifdef VERBOSE
        printf("%s: SVGA_REG_FB_START register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    }

    case SVGA_REG_FB_OFFSET:
        ret = 0;
#ifdef VERBOSE
        printf("%s: SVGA_REG_FB_OFFSET register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_BLANK_SCREEN_TARGETS:
        ret = 0;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BLANK_SCREEN_TARGETS register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_VRAM_SIZE:
        ret = 4194304;
#ifdef VERBOSE
        printf("%s: SVGA_REG_VRAM_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM:
        ret = 134217728;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_FB_SIZE:
        ret = 4194304;
#ifdef VERBOSE
        printf("%s: SVGA_REG_FB_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MOB_MAX_SIZE:
        ret = 1073741824;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MOB_MAX_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_GBOBJECT_MEM_SIZE_KB:
        ret = 8388608;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GBOBJECT_MEM_SIZE_KB register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
        ret = 8388608;
#ifdef VERBOSE
        printf("%s: SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CAPABILITIES:
caps = SVGA_CAP_NONE;
caps |= SVGA_CAP_RECT_COPY;
caps |= SVGA_CAP_CURSOR;
caps |= SVGA_CAP_CURSOR_BYPASS;
caps |= SVGA_CAP_CURSOR_BYPASS_2;
caps |= SVGA_CAP_8BIT_EMULATION;
caps |= SVGA_CAP_ALPHA_CURSOR;
caps |= SVGA_CAP_3D;
caps |= SVGA_CAP_EXTENDED_FIFO;
caps |= SVGA_CAP_MULTIMON;
caps |= SVGA_CAP_PITCHLOCK;
caps |= SVGA_CAP_IRQMASK;
caps |= SVGA_CAP_DISPLAY_TOPOLOGY;
caps |= SVGA_CAP_GMR;
caps |= SVGA_CAP_TRACES;
caps |= SVGA_CAP_GMR2;
#ifdef VERBOSE
caps |= SVGA_CAP_SCREEN_OBJECT_2;
#endif
caps |= SVGA_CAP_COMMAND_BUFFERS;
caps |= SVGA_CAP_DEAD1;
caps |= SVGA_CAP_CMD_BUFFERS_2;
#ifdef VERBOSE
caps |= SVGA_CAP_GBOBJECTS;
#endif
caps |= SVGA_CAP_CMD_BUFFERS_3;
caps |= SVGA_CAP_DX;
caps |= SVGA_CAP_HP_CMD_QUEUE;
caps |= SVGA_CAP_NO_BB_RESTRICTION;
caps |= SVGA_CAP_CAP2_REGISTER;
        ret = caps;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CAPABILITIES register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CAP2:
cap2 = SVGA_CAP2_NONE;
cap2 |= SVGA_CAP2_GROW_OTABLE;
cap2 |= SVGA_CAP2_INTRA_SURFACE_COPY;
cap2 |= SVGA_CAP2_DX2;
cap2 |= SVGA_CAP2_GB_MEMSIZE_2;
cap2 |= SVGA_CAP2_SCREENDMA_REG;
cap2 |= SVGA_CAP2_OTABLE_PTDEPTH_2;
cap2 |= SVGA_CAP2_NON_MS_TO_MS_STRETCHBLT;
cap2 |= SVGA_CAP2_CURSOR_MOB;
cap2 |= SVGA_CAP2_MSHINT;
cap2 |= SVGA_CAP2_CB_MAX_SIZE_4MB;
cap2 |= SVGA_CAP2_DX3;
cap2 |= SVGA_CAP2_FRAME_TYPE;
cap2 |= SVGA_CAP2_COTABLE_COPY;
cap2 |= SVGA_CAP2_TRACE_FULL_FB;
cap2 |= SVGA_CAP2_EXTRA_REGS;
cap2 |= SVGA_CAP2_LO_STAGING;
cap2 |= SVGA_CAP2_VIDEO_BLT;
cap2 |= SVGA_CAP2_RESERVED;
        ret = cap2;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CAP2 register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MEM_START: {
        struct pci_vmsvga_state_s *pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
        ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
#ifdef VERBOSE
        printf("%s: SVGA_REG_MEM_START register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    }

    case SVGA_REG_MEM_SIZE:
        ret = 4194304;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MEM_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CONFIG_DONE:
        ret = s->config;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CONFIG_DONE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_SYNC:
        ret = s->syncing;
#ifdef VERBOSE
        printf("%s: SVGA_REG_SYNC register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_BUSY:
        ret = s->syncbusy;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BUSY register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_GUEST_ID:
        ret = s->guest;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GUEST_ID register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CURSOR_ID:
        ret = s->cursor.id;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_ID register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CURSOR_X:
        ret = s->cursor.x;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_X register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CURSOR_Y:
        ret = s->cursor.y;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_Y register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_CURSOR_ON:
        ret = s->cursor.on;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_ON register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_SCRATCH_SIZE:
        ret = s->scratch_size;
#ifdef VERBOSE
        printf("%s: SVGA_REG_SCRATCH_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MEM_REGS:
        ret = s->num_fifo_regs;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MEM_REGS register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_NUM_DISPLAYS:
        ret = 1;
#ifdef VERBOSE
        printf("%s: SVGA_REG_NUM_DISPLAYS register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_PITCHLOCK:
       ret = s->pitchlock;
#ifdef VERBOSE
        printf("%s: SVGA_REG_PITCHLOCK register %d with the return of %u\n", __func__, s->index, ret);
#endif
       break;

    case SVGA_REG_IRQMASK:
        ret = s->irq_mask;
#ifdef VERBOSE
        printf("%s: SVGA_REG_IRQMASK register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_NUM_GUEST_DISPLAYS:
        ret = s->num_gd;
#ifdef VERBOSE
        printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_DISPLAY_ID:
        ret = s->display_id;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_ID register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_DISPLAY_IS_PRIMARY:
        ret = s->disp_prim;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_DISPLAY_POSITION_X:
        ret = s->disp_x;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_POSITION_X register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_DISPLAY_POSITION_Y:
        ret = s->disp_y;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_DISPLAY_WIDTH:
	ret = s->new_width;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_WIDTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_DISPLAY_HEIGHT:
	ret = s->new_height;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_HEIGHT register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_GMRS_MAX_PAGES:
        ret = 1048576;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMRS_MAX_PAGES register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_GMR_ID:
        ret = s->gmrid;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_ID register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_GMR_DESCRIPTOR:
        ret = s->gmrdesc;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_DESCRIPTOR register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_GMR_MAX_IDS:
        ret = 8192;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_MAX_IDS register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
        ret = 1048576;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_TRACES:
        ret = s->tracez;
#ifdef VERBOSE
        printf("%s: SVGA_REG_TRACES register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_COMMAND_LOW:
        ret = (((unsigned long long)s->cmd_high << 32) | (s->cmd_low & ~SVGA_CB_CONTEXT_MASK));
#ifdef VERBOSE
        printf("%s: SVGA_REG_COMMAND_LOW register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_COMMAND_HIGH:
        ret = s->cmd_high;
#ifdef VERBOSE
        printf("%s: SVGA_REG_COMMAND_HIGH register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_DEV_CAP:
        ret = s->devcap_val;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DEV_CAP register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_REG_MEMORY_SIZE:
        ret = 8388608;
#ifdef VERBOSE
        printf("%s: SVGA_REG_MEMORY_SIZE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_PALETTE_BASE ... (SVGA_PALETTE_BASE + 767):
        ret = s->svgabasea;
#ifdef VERBOSE
        printf("%s: SVGA_PALETTE_BASE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    case SVGA_SCRATCH_BASE:
        ret = s->svgabaseb;
#ifdef VERBOSE
        printf("%s: SVGA_SCRATCH_BASE register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;

    default:
        if (s->index >= SVGA_SCRATCH_BASE &&
            s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
            ret = s->scratch[s->index - SVGA_SCRATCH_BASE];
            break;
        }
        printf("%s: Bad register %d\n", __func__, s->index);
        ret = 1;
#ifdef VERBOSE
        printf("%s: default register %d with the return of %u\n", __func__, s->index, ret);
#endif
        break;
    }
/*
    if (s->index >= SVGA_SCRATCH_BASE) {
        trace_vmware_scratch_read(s->index, ret);
    } else if (s->index >= SVGA_PALETTE_BASE) {
        trace_vmware_palette_read(s->index, ret);
    } else {
        trace_vmware_value_read(s->index, ret);
    }
*/
        trace_vmware_value_read(s->index, ret);
    return ret;
}

static void vmsvga_value_write(void *opaque, uint32_t address, uint32_t value)
{
    struct vmsvga_state_s *s = opaque;

/*
    if (s->index >= SVGA_SCRATCH_BASE) {
        trace_vmware_scratch_write(s->index, value);
    } else if (s->index >= SVGA_PALETTE_BASE) {
        trace_vmware_palette_write(s->index, value);
    } else {
        trace_vmware_value_write(s->index, value);
    }
*/
        trace_vmware_value_write(s->index, value);

#ifdef VERBOSE
        printf("%s: Unknown register %d with the value of %u\n", __func__, s->index, value);
#endif

	if (s->thread != 1) {
		s->thread = 1;
		pthread_t threads[1];
		pthread_create(threads, NULL, vmsvga_fifo_hack, (void *)s);
	};

    switch (s->index) {
    case SVGA_REG_ID:
	s->svgaid = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_ID register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_FENCE_GOAL:
	s->fg = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_FENCE_GOAL register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_PALETTE_BASE ... (SVGA_PALETTE_BASE + 767):
        s->svgabasea = value;
#ifdef VERBOSE
        printf("%s: SVGA_PALETTE_BASE register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_SCRATCH_BASE:
        s->svgabaseb = value;
#ifdef VERBOSE
        printf("%s: SVGA_SCRATCH_BASE register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_ENABLE:
        s->enable = value;
/*
        s->vga.hw_ops->invalidate(&s->vga);
        if (s->enable && s->config) {
            vga_dirty_log_stop(&s->vga);
        } else {
            vga_dirty_log_start(&s->vga);
        }
*/
        if (s->enable) {
                s->fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_CURRENT;
                s->fifo[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_CURRENT;
        }
#ifdef VERBOSE
        printf("%s: SVGA_REG_ENABLE register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_WIDTH:
	s->new_width = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_WIDTH register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_HEIGHT:
	s->new_height = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_HEIGHT register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_BITS_PER_PIXEL:
	s->new_depth = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BITS_PER_PIXEL register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_CONFIG_DONE:
//        if (value) {
//            vga_dirty_log_stop(&s->vga);
//        }
        s->config = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CONFIG_DONE register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_SYNC:
        s->syncing = value;
        vmsvga_fifo_run(s); /* Or should we just wait for update_display? */
#ifdef VERBOSE
        printf("%s: SVGA_REG_SYNC register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_BUSY:
        s->syncbusy = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_BUSY register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_GUEST_ID:
        s->guest = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GUEST_ID register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_CURSOR_ID:
        s->cursor.id = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_ID register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_CURSOR_X:
        s->cursor.x = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_X register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_CURSOR_Y:
        s->cursor.y = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_Y register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_CURSOR_ON:
        s->cursor.on |= (value == SVGA_CURSOR_ON_SHOW);
        s->cursor.on &= (value != SVGA_CURSOR_ON_HIDE);
        if (value <= SVGA_CURSOR_ON_SHOW) {
            dpy_mouse_set(s->vga.con, s->cursor.x, s->cursor.y, s->cursor.on);
        }
#ifdef VERBOSE
        printf("%s: SVGA_REG_CURSOR_ON register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_PITCHLOCK:
       s->pitchlock = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_PITCHLOCK register %d with the value of %u\n", __func__, s->index, value);
#endif
       break;

    case SVGA_REG_IRQMASK:
        s->irq_mask = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_IRQMASK register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_NUM_GUEST_DISPLAYS:
        s->num_gd = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;


    case SVGA_REG_DISPLAY_IS_PRIMARY:
        s->disp_prim = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DISPLAY_POSITION_X:
        s->disp_x = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_POSITION_X register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DISPLAY_POSITION_Y:
        s->disp_y = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DISPLAY_ID:
        s->display_id = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_ID register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DISPLAY_WIDTH:
	s->new_width = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_WIDTH register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DISPLAY_HEIGHT:
	s->new_height = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_DISPLAY_HEIGHT register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_TRACES:
        s->tracez = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_TRACES register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_COMMAND_LOW:
        s->cmd_low = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_COMMAND_LOW register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_COMMAND_HIGH:
        s->cmd_high = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_COMMAND_HIGH register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_GMR_ID:
        s->gmrid = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_ID register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;
    case SVGA_REG_GMR_DESCRIPTOR:
        s->gmrdesc = value;
#ifdef VERBOSE
        printf("%s: SVGA_REG_GMR_DESCRIPTOR register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    case SVGA_REG_DEV_CAP:
if(value==0){s->devcap_val=0x00000001;};
if(value==1){s->devcap_val=0x00000008;};
if(value==2){s->devcap_val=0x00000008;};
if(value==3){s->devcap_val=0x00000008;};
if(value==4){s->devcap_val=0x00000007;};
if(value==5){s->devcap_val=0x00000001;};
if(value==6){s->devcap_val=0x0000000d;};
if(value==7){s->devcap_val=0x00000001;};
if(value==8){s->devcap_val=0x00000008;};
if(value==9){s->devcap_val=0x00000001;};
if(value==10){s->devcap_val=0x00000001;};
if(value==11){s->devcap_val=0x00000004;};
if(value==12){s->devcap_val=0x00000001;};
if(value==13){s->devcap_val=0x00000001;};
if(value==14){s->devcap_val=0x00000001;};
if(value==15){s->devcap_val=0x00000001;};
if(value==16){s->devcap_val=0x00000001;};
if(value==17){s->devcap_val=0x000000bd;};
if(value==18){s->devcap_val=0x00000014;};
if(value==19){s->devcap_val=0x00008000;};
if(value==20){s->devcap_val=0x00008000;};
if(value==21){s->devcap_val=0x00004000;};
if(value==22){s->devcap_val=0x00008000;};
if(value==23){s->devcap_val=0x00008000;};
if(value==24){s->devcap_val=0x00000010;};
if(value==25){s->devcap_val=0x001fffff;};
if(value==26){s->devcap_val=0x000fffff;};
if(value==27){s->devcap_val=0x0000ffff;};
if(value==28){s->devcap_val=0x0000ffff;};
if(value==29){s->devcap_val=0x00000020;};
if(value==30){s->devcap_val=0x00000020;};
if(value==31){s->devcap_val=0x03ffffff;};
if(value==32){s->devcap_val=0x0018ec1f;};
if(value==33){s->devcap_val=0x0018e11f;};
if(value==34){s->devcap_val=0x0008601f;};
if(value==35){s->devcap_val=0x0008601f;};
if(value==36){s->devcap_val=0x0008611f;};
if(value==37){s->devcap_val=0x0000611f;};
if(value==38){s->devcap_val=0x0018ec1f;};
if(value==39){s->devcap_val=0x0000601f;};
if(value==40){s->devcap_val=0x00006007;};
if(value==41){s->devcap_val=0x0000601f;};
if(value==42){s->devcap_val=0x0000601f;};
if(value==43){s->devcap_val=0x000040c5;};
if(value==44){s->devcap_val=0x000040c5;};
if(value==45){s->devcap_val=0x000040c5;};
if(value==46){s->devcap_val=0x0000e005;};
if(value==47){s->devcap_val=0x0000e005;};
if(value==48){s->devcap_val=0x0000e005;};
if(value==49){s->devcap_val=0x0000e005;};
if(value==50){s->devcap_val=0x0000e005;};
if(value==51){s->devcap_val=0x00014005;};
if(value==52){s->devcap_val=0x00014007;};
if(value==53){s->devcap_val=0x00014007;};
if(value==54){s->devcap_val=0x00014005;};
if(value==55){s->devcap_val=0x00014001;};
if(value==56){s->devcap_val=0x0080601f;};
if(value==57){s->devcap_val=0x0080601f;};
if(value==58){s->devcap_val=0x0080601f;};
if(value==59){s->devcap_val=0x0080601f;};
if(value==60){s->devcap_val=0x0080601f;};
if(value==61){s->devcap_val=0x0080601f;};
if(value==62){s->devcap_val=0x00000000;};
if(value==63){s->devcap_val=0x00000004;};
if(value==64){s->devcap_val=0x00000008;};
if(value==65){s->devcap_val=0x00014007;};
if(value==66){s->devcap_val=0x0000601f;};
if(value==67){s->devcap_val=0x0000601f;};
if(value==68){s->devcap_val=0x01246000;};
if(value==69){s->devcap_val=0x01246000;};
if(value==70){s->devcap_val=0x00000000;};
if(value==71){s->devcap_val=0x00000000;};
if(value==72){s->devcap_val=0x00000000;};
if(value==73){s->devcap_val=0x00000000;};
if(value==74){s->devcap_val=0x00000001;};
if(value==75){s->devcap_val=0x01246000;};
if(value==76){s->devcap_val=0x00000000;};
if(value==77){s->devcap_val=0x00000100;};
if(value==78){s->devcap_val=0x00008000;};
if(value==79){s->devcap_val=0x000040c5;};
if(value==80){s->devcap_val=0x000040c5;};
if(value==81){s->devcap_val=0x000040c5;};
if(value==82){s->devcap_val=0x00006005;};
if(value==83){s->devcap_val=0x00006005;};
if(value==84){s->devcap_val=0x00000000;};
if(value==85){s->devcap_val=0x00000000;};
if(value==86){s->devcap_val=0x00000000;};
if(value==87){s->devcap_val=0x00000001;};
if(value==88){s->devcap_val=0x00000001;};
if(value==89){s->devcap_val=0x0000000a;};
if(value==90){s->devcap_val=0x0000000a;};
if(value==91){s->devcap_val=0x01246000;};
if(value==92){s->devcap_val=0x00000000;};
if(value==93){s->devcap_val=0x00000001;};
if(value==94){s->devcap_val=0x00000000;};
if(value==95){s->devcap_val=0x00000001;};
if(value==96){s->devcap_val=0x00000000;};
if(value==97){s->devcap_val=0x00000010;};
if(value==98){s->devcap_val=0x0000000f;};
if(value==99){s->devcap_val=0x00000001;};
if(value==100){s->devcap_val=0x000002f7;};
if(value==101){s->devcap_val=0x000003f7;};
if(value==102){s->devcap_val=0x000002f7;};
if(value==103){s->devcap_val=0x000000f7;};
if(value==104){s->devcap_val=0x000000f7;};
if(value==105){s->devcap_val=0x000000f7;};
if(value==106){s->devcap_val=0x00000009;};
if(value==107){s->devcap_val=0x0000026b;};
if(value==108){s->devcap_val=0x0000026b;};
if(value==109){s->devcap_val=0x0000000b;};
if(value==110){s->devcap_val=0x000000f7;};
if(value==111){s->devcap_val=0x000000e3;};
if(value==112){s->devcap_val=0x000000f7;};
if(value==113){s->devcap_val=0x000000e3;};
if(value==114){s->devcap_val=0x00000063;};
if(value==115){s->devcap_val=0x00000063;};
if(value==116){s->devcap_val=0x00000063;};
if(value==117){s->devcap_val=0x00000063;};
if(value==118){s->devcap_val=0x00000063;};
if(value==119){s->devcap_val=0x000000e3;};
if(value==120){s->devcap_val=0x00000000;};
if(value==121){s->devcap_val=0x00000063;};
if(value==122){s->devcap_val=0x00000000;};
if(value==123){s->devcap_val=0x000003f7;};
if(value==124){s->devcap_val=0x000003f7;};
if(value==125){s->devcap_val=0x000003f7;};
if(value==126){s->devcap_val=0x000000e3;};
if(value==127){s->devcap_val=0x00000063;};
if(value==128){s->devcap_val=0x00000063;};
if(value==129){s->devcap_val=0x000000e3;};
if(value==130){s->devcap_val=0x000000e3;};
if(value==131){s->devcap_val=0x000000f7;};
if(value==132){s->devcap_val=0x000003f7;};
if(value==133){s->devcap_val=0x000003f7;};
if(value==134){s->devcap_val=0x000003f7;};
if(value==135){s->devcap_val=0x000003f7;};
if(value==136){s->devcap_val=0x00000001;};
if(value==137){s->devcap_val=0x0000026b;};
if(value==138){s->devcap_val=0x000001e3;};
if(value==139){s->devcap_val=0x000003f7;};
if(value==140){s->devcap_val=0x000001f7;};
if(value==141){s->devcap_val=0x00000001;};
if(value==142){s->devcap_val=0x00000041;};
if(value==143){s->devcap_val=0x00000041;};
if(value==144){s->devcap_val=0x00000000;};
if(value==145){s->devcap_val=0x000002e1;};
if(value==146){s->devcap_val=0x000003e7;};
if(value==147){s->devcap_val=0x000003e7;};
if(value==148){s->devcap_val=0x000000e1;};
if(value==149){s->devcap_val=0x000001e3;};
if(value==150){s->devcap_val=0x000001e3;};
if(value==151){s->devcap_val=0x000001e3;};
if(value==152){s->devcap_val=0x000002e1;};
if(value==153){s->devcap_val=0x000003e7;};
if(value==154){s->devcap_val=0x000003f7;};
if(value==155){s->devcap_val=0x000003e7;};
if(value==156){s->devcap_val=0x000002e1;};
if(value==157){s->devcap_val=0x000003e7;};
if(value==158){s->devcap_val=0x000003e7;};
if(value==159){s->devcap_val=0x00000261;};
if(value==160){s->devcap_val=0x00000269;};
if(value==161){s->devcap_val=0x00000063;};
if(value==162){s->devcap_val=0x00000063;};
if(value==163){s->devcap_val=0x000002e1;};
if(value==164){s->devcap_val=0x000003e7;};
if(value==165){s->devcap_val=0x000003f7;};
if(value==166){s->devcap_val=0x000002e1;};
if(value==167){s->devcap_val=0x000003f7;};
if(value==168){s->devcap_val=0x000002f7;};
if(value==169){s->devcap_val=0x000003e7;};
if(value==170){s->devcap_val=0x000003e7;};
if(value==171){s->devcap_val=0x000002e1;};
if(value==172){s->devcap_val=0x000003e7;};
if(value==173){s->devcap_val=0x000003e7;};
if(value==174){s->devcap_val=0x000002e1;};
if(value==175){s->devcap_val=0x00000269;};
if(value==176){s->devcap_val=0x000003e7;};
if(value==177){s->devcap_val=0x000003e7;};
if(value==178){s->devcap_val=0x00000261;};
if(value==179){s->devcap_val=0x00000269;};
if(value==180){s->devcap_val=0x00000063;};
if(value==181){s->devcap_val=0x00000063;};
if(value==182){s->devcap_val=0x000002e1;};
if(value==183){s->devcap_val=0x000003f7;};
if(value==184){s->devcap_val=0x000003e7;};
if(value==185){s->devcap_val=0x000003e7;};
if(value==186){s->devcap_val=0x000002e1;};
if(value==187){s->devcap_val=0x000003f7;};
if(value==188){s->devcap_val=0x000003e7;};
if(value==189){s->devcap_val=0x000003f7;};
if(value==190){s->devcap_val=0x000003e7;};
if(value==191){s->devcap_val=0x000002e1;};
if(value==192){s->devcap_val=0x000003f7;};
if(value==193){s->devcap_val=0x000003e7;};
if(value==194){s->devcap_val=0x000003f7;};
if(value==195){s->devcap_val=0x000003e7;};
if(value==196){s->devcap_val=0x00000001;};
if(value==197){s->devcap_val=0x000000e3;};
if(value==198){s->devcap_val=0x000000e3;};
if(value==199){s->devcap_val=0x000000e3;};
if(value==200){s->devcap_val=0x000000e1;};
if(value==201){s->devcap_val=0x000000e3;};
if(value==202){s->devcap_val=0x000000e1;};
if(value==203){s->devcap_val=0x000000e3;};
if(value==204){s->devcap_val=0x000000e1;};
if(value==205){s->devcap_val=0x000000e3;};
if(value==206){s->devcap_val=0x000000e1;};
if(value==207){s->devcap_val=0x00000063;};
if(value==208){s->devcap_val=0x000000e3;};
if(value==209){s->devcap_val=0x000000e1;};
if(value==210){s->devcap_val=0x00000063;};
if(value==211){s->devcap_val=0x000000e3;};
if(value==212){s->devcap_val=0x00000045;};
if(value==213){s->devcap_val=0x000002e1;};
if(value==214){s->devcap_val=0x000002f7;};
if(value==215){s->devcap_val=0x000002e1;};
if(value==216){s->devcap_val=0x000002f7;};
if(value==217){s->devcap_val=0x0000006b;};
if(value==218){s->devcap_val=0x0000006b;};
if(value==219){s->devcap_val=0x0000006b;};
if(value==220){s->devcap_val=0x00000001;};
if(value==221){s->devcap_val=0x000003f7;};
if(value==222){s->devcap_val=0x000003f7;};
if(value==223){s->devcap_val=0x000003f7;};
if(value==224){s->devcap_val=0x000003f7;};
if(value==225){s->devcap_val=0x000003f7;};
if(value==226){s->devcap_val=0x000003f7;};
if(value==227){s->devcap_val=0x000003f7;};
if(value==228){s->devcap_val=0x000003f7;};
if(value==229){s->devcap_val=0x000003f7;};
if(value==230){s->devcap_val=0x000003f7;};
if(value==231){s->devcap_val=0x000003f7;};
if(value==232){s->devcap_val=0x000003f7;};
if(value==233){s->devcap_val=0x00000269;};
if(value==234){s->devcap_val=0x000002f7;};
if(value==235){s->devcap_val=0x000000e3;};
if(value==236){s->devcap_val=0x000000e3;};
if(value==237){s->devcap_val=0x000000e3;};
if(value==238){s->devcap_val=0x000002f7;};
if(value==239){s->devcap_val=0x000002f7;};
if(value==240){s->devcap_val=0x000003f7;};
if(value==241){s->devcap_val=0x000003f7;};
if(value==242){s->devcap_val=0x000000e3;};
if(value==243){s->devcap_val=0x000000e3;};
if(value==244){s->devcap_val=0x00000001;};
if(value==245){s->devcap_val=0x00000001;};
if(value==246){s->devcap_val=0x00000001;};
if(value==247){s->devcap_val=0x00000001;};
if(value==248){s->devcap_val=0x00000001;};
if(value==249){s->devcap_val=0x00000001;};
if(value==250){s->devcap_val=0x00000000;};
if(value==251){s->devcap_val=0x000000e1;};
if(value==252){s->devcap_val=0x000000e3;};
if(value==253){s->devcap_val=0x000000e3;};
if(value==254){s->devcap_val=0x000000e1;};
if(value==255){s->devcap_val=0x000000e3;};
if(value==256){s->devcap_val=0x000000e3;};
if(value==257){s->devcap_val=0x00000000;};
if(value==258){s->devcap_val=0x00000001;};
if(value==259){s->devcap_val=0x00000001;};
if(value==260){s->devcap_val=0x00000010;};
if(value==261){s->devcap_val=0x00000001;};
if(value>=262){s->devcap_val=0x00000000;};
#ifdef VERBOSE
        printf("%s: SVGA_REG_DEV_CAP register %d with the value of %u\n", __func__, s->index, value);
#endif
        break;

    default:
        if (s->index >= SVGA_SCRATCH_BASE &&
                s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
            s->scratch[s->index - SVGA_SCRATCH_BASE] = value;
            break;
        }
        printf("%s: Bad register %d with the value of %u\n", __func__, s->index, value);
#ifdef VERBOSE
        printf("%s: default register %d with the value of %u\n", __func__, s->index, value);
#endif
    }
}

static uint32_t vmsvga_irqstatus_read(void *opaque, uint32_t address)
{
    struct vmsvga_state_s *s = opaque;
    return s->irq_status;
}

static void vmsvga_irqstatus_write(void *opaque, uint32_t address, uint32_t data)
{
    struct vmsvga_state_s *s = opaque;
    struct pci_vmsvga_state_s *pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    PCIDevice *pci_dev = PCI_DEVICE(pci_vmsvga);

    /*
     * Clear selected interrupt sources and lower
     * interrupt request when none are left active
     */

    s->irq_status &= ~data;
    if (!s->irq_status)
        pci_set_irq(pci_dev, 0);

}

static uint32_t vmsvga_bios_read(void *opaque, uint32_t address)
{
    printf("%s: what are we supposed to return?\n", __func__);
    return 0;
}

static void vmsvga_bios_write(void *opaque, uint32_t address, uint32_t data)
{
    printf("%s: what are we supposed to do with (%08x)?\n", __func__, data);
}

static inline void vmsvga_check_size(struct vmsvga_state_s *s)
{
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    uint32_t new_stride;

	if (s->new_width == 0) {
		return;
	};

	if (s->new_height == 0) {
		return;
	};

    new_stride = (s->new_depth * s->new_width) / 8;
    if (s->new_width != surface_width(surface) ||
        s->new_height != surface_height(surface) ||
        (new_stride != surface_stride(surface)) ||
        s->new_depth != surface_bits_per_pixel(surface)) {
        pixman_format_code_t format = qemu_default_pixman_format(s->new_depth, true);
        trace_vmware_setmode(s->new_width, s->new_height, s->new_depth);
        surface = qemu_create_displaysurface_from(s->new_width, s->new_height,
                                                  format, new_stride,
                                                  s->vga.vram_ptr);
        dpy_gfx_replace_surface(s->vga.con, surface);
        s->invalidated = 1;
    }
}

static void vmsvga_update_display(void *opaque)
{
    struct vmsvga_state_s *s = opaque;

    if (s->enable != 1 && s->config != 1) {
        /* in standard vga mode */
        s->vga.hw_ops->gfx_update(&s->vga);
        return;
    }

    vmsvga_check_size(s);

    vmsvga_fifo_run(s);
    cursor_update_from_fifo(s);

}

static void vmsvga_reset(DeviceState *dev)
{
    struct pci_vmsvga_state_s *pci = VMWARE_SVGA(dev);
    struct vmsvga_state_s *s = &pci->chip;

    s->index = 0;
    s->enable = 0;
    s->config = 0;
    s->svgaid = SVGA_ID_2;
    s->cursor.on = 0;
    s->redraw_fifo_last = 0;
    s->syncing = 0;
    s->irq_mask = 0;
    s->irq_status = 0;
    s->last_fifo_cursor_count = 0;
    s->display_id = SVGA_ID_2;
    s->pitchlock = 0;

    vga_dirty_log_start(&s->vga);
}

static void vmsvga_invalidate_display(void *opaque)
{
    struct vmsvga_state_s *s = opaque;
//    if (!s->enable) {
//        s->vga.hw_ops->invalidate(&s->vga);
//        return;
//    }

    s->invalidated = 1;
}

static void vmsvga_text_update(void *opaque, console_ch_t *chardata)
{
    struct vmsvga_state_s *s = opaque;

    if (s->vga.hw_ops->text_update) {
        s->vga.hw_ops->text_update(&s->vga, chardata);
    }
}

static int vmsvga_post_load(void *opaque, int version_id)
{
    struct vmsvga_state_s *s = opaque;

    s->invalidated = 1;

    if (version_id < 1) {
        s->irq_mask = 0;
        s->irq_status = 0;
        s->last_fifo_cursor_count = 0;
        s->display_id = SVGA_ID_2;
        s->pitchlock = 0;
    }

    return 0;
}

static const VMStateDescription vmstate_vmware_vga_internal = {
    .name = "vmware_vga_internal",
    .version_id = 1,
    .minimum_version_id = 0,
    .post_load = vmsvga_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_INT32_EQUAL(new_depth, struct vmsvga_state_s, NULL),
        VMSTATE_INT32(enable, struct vmsvga_state_s),
        VMSTATE_INT32(config, struct vmsvga_state_s),
        VMSTATE_INT32(cursor.id, struct vmsvga_state_s),
        VMSTATE_INT32(cursor.x, struct vmsvga_state_s),
        VMSTATE_INT32(cursor.y, struct vmsvga_state_s),
        VMSTATE_INT32(cursor.on, struct vmsvga_state_s),
        VMSTATE_INT32(index, struct vmsvga_state_s),
        VMSTATE_VARRAY_INT32(scratch, struct vmsvga_state_s,
                             scratch_size, 0, vmstate_info_uint32, uint32_t),
        VMSTATE_INT32(new_width, struct vmsvga_state_s),
        VMSTATE_INT32(new_height, struct vmsvga_state_s),
        VMSTATE_UINT32(guest, struct vmsvga_state_s),
        VMSTATE_UINT32(svgaid, struct vmsvga_state_s),
        VMSTATE_INT32(syncing, struct vmsvga_state_s),
        VMSTATE_UNUSED(4), /* was fb_size */
        VMSTATE_UINT32_V(irq_mask, struct vmsvga_state_s, 1),
        VMSTATE_UINT32_V(irq_status, struct vmsvga_state_s, 1),
        VMSTATE_UINT32_V(last_fifo_cursor_count, struct vmsvga_state_s, 1),
        VMSTATE_UINT32_V(display_id, struct vmsvga_state_s, 1),
        VMSTATE_UINT32_V(pitchlock, struct vmsvga_state_s, 1),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_vmware_vga = {
    .name = "vmware_vga",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, struct pci_vmsvga_state_s),
        VMSTATE_STRUCT(chip, struct pci_vmsvga_state_s, 0,
                       vmstate_vmware_vga_internal, struct vmsvga_state_s),
        VMSTATE_END_OF_LIST()
    }
};

static const GraphicHwOps vmsvga_ops = {
    .invalidate  = vmsvga_invalidate_display,
    .gfx_update  = vmsvga_update_display,
    .text_update = vmsvga_text_update,
};

static void vmsvga_init(DeviceState *dev, struct vmsvga_state_s *s,
                        MemoryRegion *address_space, MemoryRegion *io)
{

    s->scratch_size = 0x8000;
    s->scratch = g_malloc(s->scratch_size * 4);

    s->vga.con = graphic_console_init(dev, 0, &vmsvga_ops, s);

    s->fifo_size = (8 * 1024 * 1024);
    memory_region_init_ram(&s->fifo_ram, NULL, "vmsvga.fifo", s->fifo_size,
                           &error_fatal);
    s->fifo = (uint32_t *)memory_region_get_ram_ptr(&s->fifo_ram);
    s->num_fifo_regs = SVGA_FIFO_NUM_REGS;
    s->fifo[SVGA_FIFO_CAPABILITIES] =
      SVGA_FIFO_CAP_NONE | 
      SVGA_FIFO_CAP_FENCE | 
      SVGA_FIFO_CAP_ACCELFRONT | 
      SVGA_FIFO_CAP_PITCHLOCK | 
      SVGA_FIFO_CAP_VIDEO | 
      SVGA_FIFO_CAP_CURSOR_BYPASS_3 | 
      SVGA_FIFO_CAP_ESCAPE | 
      SVGA_FIFO_CAP_RESERVE | 
      SVGA_FIFO_CAP_SCREEN_OBJECT | 
      SVGA_FIFO_CAP_GMR2 | 
      SVGA_FIFO_CAP_3D_HWVERSION_REVISED | 
      SVGA_FIFO_CAP_SCREEN_OBJECT_2 | 
      SVGA_FIFO_CAP_DEAD;
    s->fifo[SVGA_FIFO_FLAGS] = 0;

    vga_common_init(&s->vga, OBJECT(dev), &error_fatal);
    vga_init(&s->vga, OBJECT(dev), address_space, io, true);
    vmstate_register(NULL, 0, &vmstate_vga_common, &s->vga);
    s->new_width = 1024;
    s->new_height = 768;
    s->new_depth = 32;
    switch (s->new_depth) {
    case 8:
        s->wred   = 0x00000007;
        s->wgreen = 0x00000038;
        s->wblue  = 0x000000c0;
        break;
    case 15:
        s->wred   = 0x0000001f;
        s->wgreen = 0x000003e0;
        s->wblue  = 0x00007c00;
        break;
    case 16:
        s->wred   = 0x0000001f;
        s->wgreen = 0x000007e0;
        s->wblue  = 0x0000f800;
        break;
    case 24:
        s->wred   = 0x00ff0000;
        s->wgreen = 0x0000ff00;
        s->wblue  = 0x000000ff;
        break;
    case 32:
        s->wred   = 0x00ff0000;
        s->wgreen = 0x0000ff00;
        s->wblue  = 0x000000ff;
        break;
    }
}

static uint64_t vmsvga_io_read(void *opaque, hwaddr addr, unsigned size)
{
    struct vmsvga_state_s *s = opaque;

    switch (addr) {
    case 1 * SVGA_INDEX_PORT: return vmsvga_index_read(s, addr);
    case 1 * SVGA_VALUE_PORT: return vmsvga_value_read(s, addr);
    case 1 * SVGA_BIOS_PORT: return vmsvga_bios_read(s, addr);
    case 1 * SVGA_IRQSTATUS_PORT: return vmsvga_irqstatus_read(s, addr);
    default: return -1u;
    }
}

static void vmsvga_io_write(void *opaque, hwaddr addr,
                            uint64_t data, unsigned size)
{
    struct vmsvga_state_s *s = opaque;

    switch (addr) {
    case 1 * SVGA_INDEX_PORT:
        vmsvga_index_write(s, addr, data);
        break;
    case 1 * SVGA_VALUE_PORT:
        vmsvga_value_write(s, addr, data);
        break;
    case 1 * SVGA_BIOS_PORT:
        vmsvga_bios_write(s, addr, data);
        break;
    case 1 * SVGA_IRQSTATUS_PORT:
        vmsvga_irqstatus_write(s, addr, data);
        break;
    }
}

static const MemoryRegionOps vmsvga_io_ops = {
    .read = vmsvga_io_read,
    .write = vmsvga_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = true,
    },
    .impl = {
        .unaligned = true,
    },
};

static void pci_vmsvga_realize(PCIDevice *dev, Error **errp)
{
    struct pci_vmsvga_state_s *s = VMWARE_SVGA(dev);

    dev->config[PCI_CACHE_LINE_SIZE] = 0x08;
    dev->config[PCI_LATENCY_TIMER] = 0x40;
    dev->config[PCI_INTERRUPT_LINE] = 0xff;          /* End */
    dev->config[PCI_INTERRUPT_PIN] = 1;  /* interrupt pin A */

    memory_region_init_io(&s->io_bar, OBJECT(dev), &vmsvga_io_ops, &s->chip,
                          "vmsvga-io", 0x10);
    memory_region_set_flush_coalesced(&s->io_bar);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->io_bar);

    vmsvga_init(DEVICE(dev), &s->chip,
                pci_address_space(dev), pci_address_space_io(dev));

    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_TYPE_32,
                     &s->chip.vga.vram);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &s->chip.fifo_ram);
}

static Property vga_vmware_properties[] = {
    DEFINE_PROP_UINT32("vgamem_mb", struct pci_vmsvga_state_s,
                       chip.vga.vram_size_mb, 512),
    DEFINE_PROP_BOOL("global-vmstate", struct pci_vmsvga_state_s,
                     chip.vga.global_vmstate, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void vmsvga_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = pci_vmsvga_realize;
    k->romfile = "vgabios-vmware.bin";
    k->vendor_id = PCI_VENDOR_ID_VMWARE;
    k->device_id = PCI_DEVICE_ID_VMWARE_SVGA2;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    k->subsystem_vendor_id = PCI_VENDOR_ID_VMWARE;
    k->subsystem_id = PCI_DEVICE_ID_VMWARE_SVGA2;
    k->revision = 0x00;
    dc->reset = vmsvga_reset;
    dc->vmsd = &vmstate_vmware_vga;
    device_class_set_props(dc, vga_vmware_properties);
    dc->hotpluggable = false;
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}

static const TypeInfo vmsvga_info = {
    .name          = TYPE_VMWARE_SVGA,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct pci_vmsvga_state_s),
    .class_init    = vmsvga_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void vmsvga_register_types(void)
{
    type_register_static(&vmsvga_info);
}

type_init(vmsvga_register_types)
