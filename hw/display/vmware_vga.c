/*
 * QEMU VMware-SVGA "chipset".
 *
 * Copyright (c) 2007 Andrzej Zaborowski  <balrog@zabor.org>
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

#define SVGA_PCI_DEVICE_ID     PCI_DEVICE_ID_VMWARE_SVGA2

#define HW_RECT_ACCEL
#define HW_FILL_ACCEL
#define HW_MOUSE_ACCEL

#include "vga_int.h"

/* See http://vmware-svga.sf.net/ for some documentation on VMWare SVGA */

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
    uint32_t guest;
    uint32_t svgaid;
    int syncing;

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
    int use_pitchlock;
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

    if (!s->config || !s->enable) {
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

#ifdef HW_MOUSE_ACCEL
    dpy_mouse_set(s->vga.con, s->cursor.x, s->cursor.y, s->cursor.on);
#endif
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
    if (y > SVGA_MAX_HEIGHT) {
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
        /* go for a fullscreen update as fallback */
        x = 0;
        y = 0;
        w = surface_width(surface);
        h = surface_height(surface);
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

static inline void vmsvga_update_rect_flush(struct vmsvga_state_s *s)
{
    struct vmsvga_rect_s *rect;

    if (s->invalidated) {
        s->redraw_fifo_last = 0;
        return;
    }
    /* Overlapping region updates can be optimised out here - if someone
     * knows a smart algorithm to do that, please share.  */
    for (int i = 0; i < s->redraw_fifo_last; i++) {
        rect = &s->redraw_fifo[i];
        vmsvga_update_rect(s, rect->x, rect->y, rect->w, rect->h);
    }

    s->redraw_fifo_last = 0;
}

static inline void vmsvga_update_rect_delayed(struct vmsvga_state_s *s,
                int x, int y, int w, int h)
{

    if (s->redraw_fifo_last >= REDRAW_FIFO_LEN) {
        trace_vmware_update_rect_delayed_flush();
        vmsvga_update_rect_flush(s);
    }

    struct vmsvga_rect_s *rect = &s->redraw_fifo[s->redraw_fifo_last++];

    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

#ifdef HW_RECT_ACCEL
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
        return -1;
    }
    if (!vmsvga_verify_rect(surface, "vmsvga_copy_rect/dst", x1, y1, w, h)) {
        return -1;
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

    vmsvga_update_rect_delayed(s, x1, y1, w, h);
    return 0;
}
#endif

#ifdef HW_FILL_ACCEL
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
        return -1;
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

    vmsvga_update_rect_delayed(s, x, y, w, h);
    return 0;
}
#endif

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

#ifdef HW_MOUSE_ACCEL
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
#ifdef DEBUG
        cursor_print_ascii_art(qc, "vmware/mono");
#endif
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
#ifdef DEBUG
        cursor_print_ascii_art(qc, "vmware/32bit");
#endif
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
#ifdef DEBUG
    cursor_print_ascii_art(qc, "rgba");
#endif
    dpy_cursor_define(s->vga.con, qc);
    cursor_put(qc);
}
#endif

static inline int vmsvga_fifo_length(struct vmsvga_state_s *s)
{
    int num;

    if (!s->config || !s->enable) {
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
    if (s->fifo_max > SVGA_FIFO_SIZE ||
        s->fifo_min >= SVGA_FIFO_SIZE ||
        s->fifo_stop >= SVGA_FIFO_SIZE ||
        s->fifo_next >= SVGA_FIFO_SIZE) {
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
    bool cmd_ignored;
    bool irq_pending = false;
    bool fifo_progress = false;

    len = vmsvga_fifo_length(s);
    while (len > 0 && --maxloop > 0) {
        /* May need to go back to the start of the command if incomplete */
        cmd_start = s->fifo_stop;
        cmd_ignored = false;

        switch (cmd = vmsvga_fifo_read(s)) {

        /* Implemented commands */
        case SVGA_CMD_UPDATE_VERBOSE:
            /* One extra word: an opaque cookie which is used for debugging */
            len -= 1;
            /* fall through */
        case SVGA_CMD_UPDATE:
            len -= 5;
            if (len < 0) {
                goto rewind;
            }

            x = vmsvga_fifo_read(s);
            y = vmsvga_fifo_read(s);
            width = vmsvga_fifo_read(s);
            height = vmsvga_fifo_read(s);
            if (cmd == SVGA_CMD_UPDATE_VERBOSE)
                vmsvga_fifo_read(s);
            vmsvga_update_rect_delayed(s, x, y, width, height);
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
#ifdef HW_RECT_ACCEL
            if (vmsvga_copy_rect(s, x, y, dx, dy, width, height) == 0) {
                break;
            }
#endif
            args = 0;
	printf("badcmd:1: ");
            goto badcmd;

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
	printf("badcmd:2: ");
                    goto badcmd;
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
#ifdef HW_MOUSE_ACCEL
            vmsvga_cursor_define(s, &cursor);
            break;
#else
            args = 0;
	printf("badcmd:3: ");
            goto badcmd;
#endif

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
            if ((SVGA_PIXMAP_SIZE(x, y, 32) > ARRAY_SIZE(cursor.and_mask))
               || (SVGA_PIXMAP_SIZE(x, y, 32) > ARRAY_SIZE(cursor.xor_mask))) {
	printf("badcmd:4: ");
                    goto badcmd;
            }

            len -= args;
            if (len < 0) {
                goto rewind;
            }

            for (i = 0; i < args; i++) {
                uint32_t rgba = vmsvga_fifo_read_raw(s);
                cursor.xor_mask[i] = rgba & 0x00ffffff;
                cursor.and_mask[i] = rgba & 0xff000000;
            }

#ifdef HW_MOUSE_ACCEL
            vmsvga_rgba_cursor_define(s, &cursor);
            break;
#else
            args = 0;
	printf("badcmd:5: ");
            goto badcmd;
#endif

        case SVGA_CMD_FRONT_ROP_FILL:
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 6;
            goto ignoredcmd;

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

            break;

        case SVGA_CMD_DEFINE_GMR2:
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 2;
	printf("badcmd:6: ");
            goto badcmd;

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

	printf("badcmd:7: ");
            goto badcmd;

        case SVGA_CMD_RECT_ROP_COPY: 
            len -= 1;
            if (len < 0) {
                goto rewind;
            }
            args = 7;
	printf("badcmd:8: ");
            goto badcmd;

case SVGA_CMD_ESCAPE:
printf("cmdbad:a: ");
goto cmdbad;

case SVGA_CMD_DEFINE_SCREEN:
printf("cmdbad:b: ");
goto cmdbad;

case SVGA_CMD_DESTROY_SCREEN:
printf("cmdbad:c: ");
goto cmdbad;

case SVGA_CMD_DEFINE_GMRFB:
printf("cmdbad:d: ");
goto cmdbad;

case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
printf("cmdbad:e: ");
goto cmdbad;

case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
printf("cmdbad:f: ");
goto cmdbad;

case SVGA_CMD_ANNOTATION_FILL:
printf("cmdbad:g: ");
goto cmdbad;

case SVGA_CMD_ANNOTATION_COPY:
printf("cmdbad:h: ");
goto cmdbad;

case SVGA_CMD_DEAD_2:
printf("cmdbad:i: ");
goto cmdbad;

case SVGA_CMD_NOP_ERROR:
printf("cmdbad:j: ");
goto cmdbad;

case SVGA_CMD_NOP:
printf("cmdbad:k: ");
goto cmdbad;

case SVGA_CMD_DEAD:
printf("cmdbad:l: ");
goto cmdbad;

case SVGA_CMD_INVALID_CMD:
printf("cmdbad:m: ");
goto cmdbad;


        default:
            args = 0;
	printf("cmdbad:9: ");
            goto cmdbad;
        ignoredcmd:
            cmd_ignored = true;
        badcmd:
            len -= args;
            if (len < 0) {
                goto rewind;
            }
            while (args--) {
                vmsvga_fifo_read(s);
            }
            if (!cmd_ignored) {
                printf("%s: Unknown command %d in SVGA command FIFO\n", __func__, cmd);
            }
            break;

        cmdbad:
            printf("%s: Unknown command %d in SVGA command FIFO\n", __func__, cmd);
            break;

        rewind:
            s->fifo_stop = cmd_start;
            s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
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

static uint32_t vmsvga_value_read(void *opaque, uint32_t address)
{
    uint32_t caps;
    struct vmsvga_state_s *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    PixelFormat pf;
    uint32_t ret;

    switch (s->index) {
    case SVGA_REG_ID:
        ret = s->svgaid;
        break;

    case SVGA_REG_ENABLE:
        ret = s->enable;
        break;

    case SVGA_REG_WIDTH:
        ret = s->new_width ? s->new_width : surface_width(surface);
        break;

    case SVGA_REG_HEIGHT:
        ret = s->new_height ? s->new_height : surface_height(surface);
        break;

    case SVGA_REG_MAX_WIDTH:
        ret = SVGA_MAX_WIDTH;
        break;

    case SVGA_REG_MAX_HEIGHT:
        ret = SVGA_MAX_HEIGHT;
        break;

    case SVGA_REG_DEPTH:
        ret = (s->new_depth == 32) ? 24 : s->new_depth;
        break;

    case SVGA_REG_BITS_PER_PIXEL:
    case SVGA_REG_HOST_BITS_PER_PIXEL:
        ret = s->new_depth;
        break;

    case SVGA_REG_PSEUDOCOLOR:
        ret = 0x0;
        break;

    case SVGA_REG_RED_MASK:
        pf = qemu_default_pixelformat(s->new_depth);
        ret = pf.rmask;
        break;

    case SVGA_REG_GREEN_MASK:
        pf = qemu_default_pixelformat(s->new_depth);
        ret = pf.gmask;
        break;

    case SVGA_REG_BLUE_MASK:
        pf = qemu_default_pixelformat(s->new_depth);
        ret = pf.bmask;
        break;

    case SVGA_REG_BYTES_PER_LINE:
        ret = (s->use_pitchlock >= 0) ?
                s->pitchlock :
                ((s->new_depth * s->new_width) / 8);
        if (!ret) {
            ret = surface_stride(surface);
        }
        break;

    case SVGA_REG_FB_START: {
        struct pci_vmsvga_state_s *pci_vmsvga
            = container_of(s, struct pci_vmsvga_state_s, chip);
        ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
        break;
    }

    case SVGA_REG_FB_OFFSET:
        ret = 0x0;
        break;

    case SVGA_REG_VRAM_SIZE:
        ret = s->vga.vram_size; /* No physical VRAM besides the framebuffer */
        break;

    case SVGA_REG_FB_SIZE:
        ret = s->vga.vram_size;
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
//caps |= SVGA_CAP_SCREEN_OBJECT_2;
caps |= SVGA_CAP_COMMAND_BUFFERS;
caps |= SVGA_CAP_DEAD1;
caps |= SVGA_CAP_CMD_BUFFERS_2;
//caps |= SVGA_CAP_GBOBJECTS;
caps |= SVGA_CAP_CMD_BUFFERS_3;
caps |= SVGA_CAP_CAP2_REGISTER;
        ret = caps;
        break;

    case SVGA_REG_CAP2:
caps = SVGA_CAP2_NONE;
caps |= SVGA_CAP2_GROW_OTABLE;
caps |= SVGA_CAP2_INTRA_SURFACE_COPY;
caps |= SVGA_CAP2_RESERVED;
        ret = caps;
        break;

    case SVGA_REG_MEM_START: {
        struct pci_vmsvga_state_s *pci_vmsvga
            = container_of(s, struct pci_vmsvga_state_s, chip);
        ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
        break;
    }

    case SVGA_REG_MEM_SIZE:
        ret = s->fifo_size;
        break;

    case SVGA_REG_CONFIG_DONE:
        ret = s->config;
        break;

    case SVGA_REG_SYNC:
    case SVGA_REG_BUSY:
        ret = s->syncing;
        break;

    case SVGA_REG_GUEST_ID:
        ret = s->guest;
        break;

    case SVGA_REG_CURSOR_ID:
        ret = s->cursor.id;
        break;

    case SVGA_REG_CURSOR_X:
        ret = s->cursor.x;
        break;

    case SVGA_REG_CURSOR_Y:
        ret = s->cursor.y;
        break;

    case SVGA_REG_CURSOR_ON:
        ret = s->cursor.on;
        break;

    case SVGA_REG_SCRATCH_SIZE:
        ret = s->scratch_size;
        break;

    case SVGA_REG_MEM_REGS:
        ret = s->num_fifo_regs;
        break;

    case SVGA_REG_NUM_DISPLAYS:
    case SVGA_PALETTE_BASE ... (SVGA_PALETTE_BASE + 767):
        ret = 1;
        break;

    case SVGA_REG_PITCHLOCK:
       ret = s->pitchlock;
       break;

    case SVGA_REG_IRQMASK:
        ret = s->irq_mask;
        break;

    case SVGA_REG_NUM_GUEST_DISPLAYS:
        ret = 0;
        break;
    case SVGA_REG_DISPLAY_ID:
        ret = s->display_id;
        break;
    case SVGA_REG_DISPLAY_IS_PRIMARY:
        ret = s->display_id == 0 ? 1 : 0;
        break;
    case SVGA_REG_DISPLAY_POSITION_X:
    case SVGA_REG_DISPLAY_POSITION_Y:
        ret = 0;
        break;
    case SVGA_REG_DISPLAY_WIDTH:
        if ((s->display_id == 0) || (s->display_id == SVGA_ID_INVALID))
            ret = s->new_width ? s->new_width : surface_width(surface);
        else
            ret = 800;
        break;
    case SVGA_REG_DISPLAY_HEIGHT:
        if ((s->display_id == 0) || (s->display_id == SVGA_ID_INVALID))
            ret = s->new_height ? s->new_height : surface_height(surface);
        else
            ret = 600;
        break;

    /* Guest memory regions */
    case SVGA_REG_GMRS_MAX_PAGES:
        ret = 524288;
        break;
    case SVGA_REG_GMR_ID:
        ret = 524288;
        break;
    case SVGA_REG_GMR_MAX_IDS:
        ret = 524288;
        break;
    case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
        ret = 524288;
        break;

    default:
        if (s->index >= SVGA_SCRATCH_BASE &&
            s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
            ret = s->scratch[s->index - SVGA_SCRATCH_BASE];
            break;
        }
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad register %d\n", __func__, s->index);
        ret = 0;
        break;
    }

    if (s->index >= SVGA_SCRATCH_BASE) {
        trace_vmware_scratch_read(s->index, ret);
    } else if (s->index >= SVGA_PALETTE_BASE) {
        trace_vmware_palette_read(s->index, ret);
    } else {
        trace_vmware_value_read(s->index, ret);
    }
    return ret;
}

static void vmsvga_value_write(void *opaque, uint32_t address, uint32_t value)
{
    struct vmsvga_state_s *s = opaque;

    if (s->index >= SVGA_SCRATCH_BASE) {
        trace_vmware_scratch_write(s->index, value);
    } else if (s->index >= SVGA_PALETTE_BASE) {
        trace_vmware_palette_write(s->index, value);
    } else {
        trace_vmware_value_write(s->index, value);
    }
    switch (s->index) {
    case SVGA_REG_ID:
        if (value == SVGA_ID_2 || value == SVGA_ID_1 || value == SVGA_ID_0) {
            s->svgaid = value;
        }
        break;

    case SVGA_REG_ENABLE:
        s->enable = !!value;
        s->invalidated = 1;
        s->vga.hw_ops->invalidate(&s->vga);
        if (s->enable && s->config) {
            vga_dirty_log_stop(&s->vga);
        } else {
            vga_dirty_log_start(&s->vga);
        }
        if (s->enable) {
            //s->fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_CURRENT;
            s->fifo[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_CURRENT;
	}
        break;

    case SVGA_REG_WIDTH:
        if (value <= SVGA_MAX_WIDTH) {
            s->new_width = value;
            s->invalidated = 1;
            /* This is a hack used to drop effective pitchlock setting
             * when guest writes screen width without prior write to
             * the pitchlock register.
             */
            if (s->use_pitchlock >= 0) {
                s->use_pitchlock--;
            }
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad width: %i\n", __func__, value);
        }
        break;

    case SVGA_REG_HEIGHT:
        if (value <= SVGA_MAX_HEIGHT) {
            s->new_height = value;
            s->invalidated = 1;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad height: %i\n", __func__, value);
        }
        break;

    case SVGA_REG_BITS_PER_PIXEL:
        if (value != 32) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad bits per pixel: %i bits\n", __func__, value);
            s->config = 0;
            s->invalidated = 1;
        }
        break;

    case SVGA_REG_CONFIG_DONE:
        if (value) {
            vga_dirty_log_stop(&s->vga);
        }
        s->config = !!value;
        break;

    case SVGA_REG_SYNC:
        s->syncing = 1;
        vmsvga_fifo_run(s); /* Or should we just wait for update_display? */
        break;

    case SVGA_REG_GUEST_ID:
        s->guest = value;
/*
#ifdef VERBOSE
        if (value >= GUEST_OS_BASE && value < GUEST_OS_BASE +
            ARRAY_SIZE(vmsvga_guest_id)) {
            printf("%s: guest runs %s.\n", __func__, vmsvga_guest_id[value - GUEST_OS_BASE]);
        }
#endif
*/
        break;

    case SVGA_REG_CURSOR_ID:
        s->cursor.id = value;
        break;

    case SVGA_REG_CURSOR_X:
        s->cursor.x = value;
        break;

    case SVGA_REG_CURSOR_Y:
        s->cursor.y = value;
        break;

    case SVGA_REG_CURSOR_ON:
        s->cursor.on |= (value == SVGA_CURSOR_ON_SHOW);
        s->cursor.on &= (value != SVGA_CURSOR_ON_HIDE);
#ifdef HW_MOUSE_ACCEL
        if (value <= SVGA_CURSOR_ON_SHOW) {
            dpy_mouse_set(s->vga.con, s->cursor.x, s->cursor.y, s->cursor.on);
        }
#endif
        break;

    case SVGA_REG_DEPTH:
    case SVGA_REG_MEM_REGS:
    case SVGA_REG_NUM_DISPLAYS:
    case SVGA_PALETTE_BASE ... (SVGA_PALETTE_BASE + 767):
        break;

    case SVGA_REG_PITCHLOCK:
       s->pitchlock = value;
       s->use_pitchlock = (value > 0) ? 1 : -1;
       s->invalidated = 1;
       break;

    case SVGA_REG_IRQMASK:
        s->irq_mask = value;
        break;

    /* We support single legacy screen at fixed offset of (0,0) */
    case SVGA_REG_DISPLAY_ID:
        s->display_id = value;
        break;
    case SVGA_REG_NUM_GUEST_DISPLAYS:
    case SVGA_REG_DISPLAY_IS_PRIMARY:
    case SVGA_REG_DISPLAY_POSITION_X:
    case SVGA_REG_DISPLAY_POSITION_Y:
        break;
    case SVGA_REG_DISPLAY_WIDTH:
        if ((s->display_id == 0) && (value <= SVGA_MAX_WIDTH)) {
            s->new_width = value;
            s->invalidated = 1;
        }
        break;
    case SVGA_REG_DISPLAY_HEIGHT:
        if ((s->display_id == 0) && (value <= SVGA_MAX_HEIGHT)) {
            s->new_height = value;
            s->invalidated = 1;
        }
        break;

    default:
        if (s->index >= SVGA_SCRATCH_BASE &&
                s->index < SVGA_SCRATCH_BASE + s->scratch_size) {
            s->scratch[s->index - SVGA_SCRATCH_BASE] = value;
            break;
        }
        printf("%s: Bad register %d\n", __func__, s->index);
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
    struct pci_vmsvga_state_s *pci_vmsvga =
        container_of(s, struct pci_vmsvga_state_s, chip);
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
    return 0xcafe;
}

static void vmsvga_bios_write(void *opaque, uint32_t address, uint32_t data)
{
    printf("%s: what are we supposed to do with (%08x)?\n", __func__, data);
}

static inline void vmsvga_check_size(struct vmsvga_state_s *s)
{
    DisplaySurface *surface = qemu_console_surface(s->vga.con);
    uint32_t new_stride;

    /* Don't allow setting uninitialized 0-size screen */
    if ((s->new_width == 0) || (s->new_height == 0)) return;

    new_stride = (s->use_pitchlock >= 0) ?
        s->pitchlock :
        ((s->new_depth * s->new_width) / 8);
    if (s->new_width != surface_width(surface) ||
        s->new_height != surface_height(surface) ||
        (new_stride != surface_stride(surface)) ||
        s->new_depth != surface_bits_per_pixel(surface)) {
        pixman_format_code_t format =
            qemu_default_pixman_format(s->new_depth, true);
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

    if (!s->enable || !s->config) {
        /* in standard vga mode */
        s->vga.hw_ops->gfx_update(&s->vga);
        return;
    }

    vmsvga_check_size(s);

    vmsvga_fifo_run(s);
    vmsvga_update_rect_flush(s);
    cursor_update_from_fifo(s);

    if (s->invalidated) {
        s->invalidated = 0;
        dpy_gfx_update_full(s->vga.con);
    }
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
    s->display_id = SVGA_ID_INVALID;
    s->pitchlock = 0;
    s->use_pitchlock = -1;

    vga_dirty_log_start(&s->vga);
}

static void vmsvga_invalidate_display(void *opaque)
{
    struct vmsvga_state_s *s = opaque;
    if (!s->enable) {
        s->vga.hw_ops->invalidate(&s->vga);
        return;
    }

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
        s->display_id = SVGA_ID_INVALID;
        s->pitchlock = 0;
        s->use_pitchlock = -1;
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
        VMSTATE_INT32_V(use_pitchlock, struct vmsvga_state_s, 1),
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
//    s->scratch_size = SVGA_SCRATCH_SIZE;
//    s->scratch = g_malloc(s->scratch_size * 4);

    s->vga.con = graphic_console_init(dev, 0, &vmsvga_ops, s);

    s->fifo_size = SVGA_FIFO_SIZE;
    memory_region_init_ram(&s->fifo_ram, NULL, "vmsvga.fifo", s->fifo_size,
                           &error_fatal);
    s->fifo = (uint32_t *)memory_region_get_ram_ptr(&s->fifo_ram);
    s->num_fifo_regs = SVGA_FIFO_NUM_REGS;
    s->	fifo[SVGA_FIFO_CAPABILITIES] =
      SVGA_FIFO_CAP_NONE | 
      SVGA_FIFO_CAP_FENCE | 
      SVGA_FIFO_CAP_ACCELFRONT | 
      SVGA_FIFO_CAP_PITCHLOCK | 
      SVGA_FIFO_CAP_VIDEO | 
      SVGA_FIFO_CAP_CURSOR_BYPASS_3 | 
      SVGA_FIFO_CAP_ESCAPE | 
      SVGA_FIFO_CAP_RESERVE | 
//      SVGA_FIFO_CAP_SCREEN_OBJECT | 
      SVGA_FIFO_CAP_GMR2 | 
      SVGA_FIFO_CAP_3D_HWVERSION_REVISED | 
//      SVGA_FIFO_CAP_SCREEN_OBJECT_2 | 
      SVGA_FIFO_CAP_DEAD;
    s->fifo[SVGA_FIFO_FLAGS] = 0;

    vga_common_init(&s->vga, OBJECT(dev), &error_fatal);
    vga_init(&s->vga, OBJECT(dev), address_space, io, true);
    vmstate_register(NULL, 0, &vmstate_vga_common, &s->vga);
    s->new_depth = 32;
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

    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH,
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
    k->device_id = SVGA_PCI_DEVICE_ID;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    k->subsystem_vendor_id = PCI_VENDOR_ID_VMWARE;
    k->subsystem_id = SVGA_PCI_DEVICE_ID;
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
