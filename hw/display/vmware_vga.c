/*

 QEMU VMware Super Video Graphics Array 2 [SVGA-II]

 Copyright (c) 2007 Andrzej Zaborowski <balrog@zabor.org>

 Copyright (c) 2023-2025 Christopher Eric Lentocha
 <christopherericlentocha@gmail.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

*/
// #define ANY_FENCE_OFF
// #define EXPCAPS
// #define QEMU_V9_2_0
// #define RAISE_IRQ_OFF
// #define VERBOSE
#include "qemu/osdep.h" // Required to be the first #include
#include "qapi/error.h"
#include <pthread.h>    // Required for Windows (MSYS2)
#ifdef QEMU_V9_2_0
#include "hw/pci/pci_device.h"
#else
#include "hw/pci/pci.h"
#endif
#include "hw/qdev-properties.h"
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
#include "include/vmware_pack_begin.h"
#include "include/vmware_pack_end.h"
#include "migration/vmstate.h"
#include "vga_int.h"
#include "include/VGPU10ShaderTokens.h" // Required to be the last #include
#define SVGA_CAP_UNKNOWN_A 0x00000001
#define SVGA_CAP_UNKNOWN_B 0x00000004
#define SVGA_CAP_UNKNOWN_C 0x00000008
#define SVGA_CAP_UNKNOWN_D 0x00000010
#define SVGA_CAP_UNKNOWN_E 0x00000400
#define SVGA_CAP_UNKNOWN_F 0x00000800
#define SVGA_CAP_UNKNOWN_G 0x00001000
#define SVGA_CAP_UNKNOWN_H 0x00002000
#define SVGA_CAP_HP_CMD_QUEUE 0x20000000
#define SVGA_CAP_NO_BB_RESTRICTION 0x40000000
#define VMSVGA_IS_VALID_FIFO_REG(a_iIndex, a_offFifoMin)                       \
  (((a_iIndex) + 1) * sizeof(uint32_t) <= (a_offFifoMin))
#define SVGA_PIXMAP_SIZE(w, h, bpp) (((((w) * (bpp))) >> 5) * (h))
#define SVGA_CMD_RECT_FILL 2
#define SVGA_CMD_DISPLAY_CURSOR 20
#define SVGA_CMD_MOVE_CURSOR 21
#define SVGA_REG_CURSOR_MOBID 65
#define SVGA_REG_CURSOR_MAX_BYTE_SIZE 66
#define SVGA_REG_CURSOR_MAX_DIMENSION 67
#define SVGA_REG_FIFO_CAPS 68
#define SVGA_REG_FENCE 69
#define SVGA_REG_SCREENDMA 75
#define SVGA_REG_GBOBJECT_MEM_SIZE_KB 76
#define SVGA_REG_FENCE_GOAL 84
#define SVGA_REG_MSHINT 81
#define SVGA_PALETTE_SIZE 769
#define SVGA_REG_PALETTE_MIN 1024
#define SVGA_REG_PALETTE_MAX (SVGA_REG_PALETTE_MIN + SVGA_PALETTE_SIZE)
#ifdef VERBOSE
#define VPRINT(fmt, ...)                                                       \
  printf("vmsvga (%s): %u - %s: " fmt, __FILE__, (unsigned)time(NULL),         \
         __func__, ##__VA_ARGS__)
#else
static void vprint_noop(const char *fmt, ...) { (void)fmt; }
#define VPRINT(fmt, ...) vprint_noop(fmt, ##__VA_ARGS__)
#endif
struct vmsvga_state_s {
  uint32_t svgapalettebase[SVGA_PALETTE_SIZE];
  uint32_t enable;
  uint32_t config;
  uint32_t index;
  uint32_t scratch_size;
  uint32_t new_width;
  uint32_t new_height;
  uint32_t new_depth;
  uint32_t num_gd;
  uint32_t disp_prim;
  uint32_t disp_x;
  uint32_t disp_y;
  uint32_t devcap_val;
  uint32_t gmrdesc;
  uint32_t gmrid;
  uint32_t gmrpage;
  uint32_t traces;
  uint32_t cmd_low;
  uint32_t cmd_high;
  uint32_t guest;
  uint32_t svgaid;
  uint32_t thread;
  uint32_t sync;
  uint32_t bios;
  uint32_t fifo_size;
  uint32_t fifo_min;
  uint32_t fifo_max;
  uint32_t fifo_next;
  uint32_t fifo_stop;
  uint32_t irq_mask;
  uint32_t irq_status;
  uint32_t display_id;
  uint32_t pitchlock;
  uint32_t cursor;
  uint32_t fc;
  uint32_t ff;
  uint32_t *fifo;
  uint32_t *scratch;
  VGACommonState vga;
  VGACommonState vcs;
  MemoryRegion fifo_ram;
};
DECLARE_INSTANCE_CHECKER(struct pci_vmsvga_state_s, VMWARE_SVGA, "vmware-svga")
struct pci_vmsvga_state_s {
  PCIDevice parent_obj;
  struct vmsvga_state_s chip;
  MemoryRegion io_bar;
};
static void cursor_update_from_fifo(struct vmsvga_state_s *s) {
  // VPRINT("cursor_update_from_fifo was just executed\n");
  if ((s->fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_SHOW) ||
      (s->fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_RESTORE_TO_FB)) {
    dpy_mouse_set(s->vga.con, s->fifo[SVGA_FIFO_CURSOR_X],
                  s->fifo[SVGA_FIFO_CURSOR_Y], SVGA_CURSOR_ON_SHOW);
  } else {
    dpy_mouse_set(s->vga.con, s->fifo[SVGA_FIFO_CURSOR_X],
                  s->fifo[SVGA_FIFO_CURSOR_Y], SVGA_CURSOR_ON_HIDE);
  };
};
struct vmsvga_cursor_definition_s {
  uint32_t width;
  uint32_t height;
  uint32_t id;
  uint32_t hot_x;
  uint32_t hot_y;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_mask[4096];
  uint32_t xor_mask[4096];
};
static inline void vmsvga_cursor_define(struct vmsvga_state_s *s,
                                        struct vmsvga_cursor_definition_s *c) {
  VPRINT("vmsvga_cursor_define was just executed\n");
  QEMUCursor *qc;
  qc = cursor_alloc(c->width, c->height);
  if (qc != NULL) {
    qc->hot_x = c->hot_x;
    qc->hot_y = c->hot_y;
    if (c->xor_mask_bpp != 1 && c->and_mask_bpp != 1) {
      uint32_t i;
      uint32_t pixels = ((c->width) * (c->height));
      for (i = 0; i < pixels; i++) {
        qc->data[i] = ((c->xor_mask[i]) + (c->and_mask[i]));
      };
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void *)c->xor_mask, 1,
                      (void *)c->and_mask);
    };
#ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_mono");
#endif
    VPRINT("vmsvga_cursor_define | xor_mask == %u : "
           "and_mask == %u\n",
           *c->xor_mask, *c->and_mask);
    dpy_cursor_define(s->vga.con, qc);
#ifdef QEMU_V9_2_0
    cursor_unref(qc);
#else
    cursor_put(qc);
#endif
  };
};
static inline void
vmsvga_rgba_cursor_define(struct vmsvga_state_s *s,
                          struct vmsvga_cursor_definition_s *c) {
  VPRINT("vmsvga_rgba_cursor_define was just executed\n");
  QEMUCursor *qc;
  qc = cursor_alloc(c->width, c->height);
  if (qc != NULL) {
    qc->hot_x = c->hot_x;
    qc->hot_y = c->hot_y;
    if (c->xor_mask_bpp != 1 && c->and_mask_bpp != 1) {
      uint32_t i;
      uint32_t pixels = ((c->width) * (c->height));
      for (i = 0; i < pixels; i++) {
        qc->data[i] = ((c->xor_mask[i]) + (c->and_mask[i]));
      };
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void *)c->xor_mask, 1,
                      (void *)c->and_mask);
    };
#ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_rgba");
#endif
    VPRINT("vmsvga_rgba_cursor_define | xor_mask == %u : "
           "and_mask == %u\n",
           *c->xor_mask, *c->and_mask);
    dpy_cursor_define(s->vga.con, qc);
#ifdef QEMU_V9_2_0
    cursor_unref(qc);
#else
    cursor_put(qc);
#endif
  };
};
static inline int vmsvga_fifo_length(struct vmsvga_state_s *s) {
  // VPRINT("vmsvga_fifo_length was just executed\n");
  uint32_t num;
  s->fifo_min = le32_to_cpu(s->fifo[SVGA_FIFO_MIN]);
  s->fifo_max = le32_to_cpu(s->fifo[SVGA_FIFO_MAX]);
  s->fifo_next = le32_to_cpu(s->fifo[SVGA_FIFO_NEXT_CMD]);
  s->fifo_stop = le32_to_cpu(s->fifo[SVGA_FIFO_STOP]);
  if ((s->fifo_next) >= (s->fifo_stop)) {
    num = ((s->fifo_next) - (s->fifo_stop));
  } else {
    num = (((s->fifo_max) - (s->fifo_min)) + ((s->fifo_next) - (s->fifo_stop)));
  };
  VPRINT("vmsvga_fifo_length: fifo_min: %u, fifo_max: "
         "%u, fifo_next: "
         "%u, fifo_stop: %u, num: %u, fifo_min: %u, fifo_max: %u, fifo_next: "
         "%u, fifo_stop: %u\n",
         s->fifo_min, s->fifo_max, s->fifo_next, s->fifo_stop, num,
         s->fifo[SVGA_FIFO_MIN], s->fifo[SVGA_FIFO_MAX],
         s->fifo[SVGA_FIFO_NEXT_CMD], s->fifo[SVGA_FIFO_STOP]);
  return (num >> 2);
};
static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_fifo_read_raw was just executed\n");
  uint32_t cmd = s->fifo[s->fifo_stop >> 2];
  s->fifo_stop += 4;
  if (s->fifo_stop >= s->fifo_max) {
    s->fifo_stop = s->fifo_min;
  };
  s->fifo[SVGA_FIFO_STOP] = cpu_to_le32(s->fifo_stop);
  return cmd;
};
static inline uint32_t vmsvga_fifo_read(struct vmsvga_state_s *s) {
  VPRINT("vmsvga_fifo_read was just executed\n");
  return le32_to_cpu(vmsvga_fifo_read_raw(s));
};
#define VMSVGA_FIFO_REWIND(_s, cmd)                                            \
  do {                                                                         \
    uint32_t _cmd_start = (_s)->fifo_stop;                                     \
    (_s)->fifo_stop = _cmd_start;                                              \
    (_s)->fifo[SVGA_FIFO_STOP] = cpu_to_le32((_s)->fifo_stop);                 \
    VPRINT("rewind command %u in SVGA command FIFO\n", cmd);                   \
  } while (0)

static void vmsvga_fifo_run(struct vmsvga_state_s *s) {
  // VPRINT("vmsvga_fifo_run was just executed\n");
  uint32_t UnknownCommandA;
  uint32_t UnknownCommandB;
  uint32_t UnknownCommandC;
  uint32_t UnknownCommandD;
  uint32_t UnknownCommandE;
  uint32_t UnknownCommandF;
  uint32_t UnknownCommandG;
  uint32_t UnknownCommandH;
  uint32_t UnknownCommandI;
  uint32_t UnknownCommandJ;
  uint32_t UnknownCommandK;
  uint32_t UnknownCommandL;
  uint32_t UnknownCommandM;
  uint32_t UnknownCommandN;
  uint32_t UnknownCommandO;
  uint32_t UnknownCommandP;
  uint32_t UnknownCommandQ;
  uint32_t UnknownCommandR;
  uint32_t UnknownCommandS;
  uint32_t UnknownCommandT;
  uint32_t UnknownCommandU;
  uint32_t UnknownCommandV;
  uint32_t UnknownCommandW;
  uint32_t UnknownCommandX;
  uint32_t UnknownCommandY;
  uint32_t UnknownCommandZ;
  uint32_t UnknownCommandAA;
  uint32_t UnknownCommandAB;
  uint32_t UnknownCommandAC;
  uint32_t UnknownCommandAD;
  uint32_t UnknownCommandAE;
  uint32_t UnknownCommandAF;
  uint32_t UnknownCommandAG;
  uint32_t UnknownCommandAH;
  uint32_t UnknownCommandAI;
  uint32_t UnknownCommandAJ;
  uint32_t UnknownCommandAK;
  uint32_t UnknownCommandAL;
  uint32_t UnknownCommandAM;
  uint32_t UnknownCommandAN;
  uint32_t UnknownCommandAO;
  uint32_t UnknownCommandAP;

  uint32_t UnknownCommandAV;
  uint32_t UnknownCommandAW;
  uint32_t UnknownCommandAX;
  uint32_t UnknownCommandAY;
  uint32_t UnknownCommandAZ;
  uint32_t UnknownCommandBA;
  uint32_t UnknownCommandBB;
  uint32_t UnknownCommandBC;
  uint32_t UnknownCommandBD;
  uint32_t gmrIdCMD;
  uint32_t offsetPages;
  uint32_t width;
  uint32_t height;
  uint32_t dx;
  uint32_t dy;
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t args;
  uint32_t len;
  uint32_t cmd;
  uint32_t i;
  uint32_t flags;
  uint32_t num_pages;
  uint32_t fence_arg;
  uint32_t irq_status;
  struct vmsvga_cursor_definition_s cursor;
  len = vmsvga_fifo_length(s);
  irq_status = s->irq_status;
  while (len >= 1) {
    cmd = vmsvga_fifo_read(s);
    VPRINT("Unknown command %u in SVGA command FIFO\n", cmd);
    switch (cmd) {
    case SVGA_CMD_INVALID_CMD:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_INVALID_CMD command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_CMD_UPDATE:
      if (len < 5) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 5;
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_UPDATE command %u in SVGA command FIFO %u "
             "%u %u %u\n",
             cmd, x, y, width, height);
      break;
    case SVGA_CMD_UPDATE_VERBOSE:
      if (len < 6) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 6;
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      z = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_UPDATE_VERBOSE command %u in SVGA command "
             "FIFO %u "
             "%u %u %u %u\n",
             cmd, x, y, width, height, z);
      break;
    case SVGA_CMD_RECT_FILL:
      if (len < 5) {  // color + x + y + width + height
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 5;
      uint32_t color = vmsvga_fifo_read(s);
      uint32_t x = vmsvga_fifo_read(s);
      uint32_t y = vmsvga_fifo_read(s);
      uint32_t width = vmsvga_fifo_read(s);
      uint32_t height = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_RECT_FILL command %u in SVGA command FIFO "
             "color=0x%x x=%u y=%u w=%u h=%u\n",
             cmd, color, x, y, width, height);
      break;
    case SVGA_CMD_RECT_COPY:
      if (len < 7) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 7;
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      dx = vmsvga_fifo_read(s);
      dy = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_RECT_COPY command %u in SVGA command FIFO "
             "%u %u %u "
             "%u %u %u\n",
             cmd, x, y, dx, dy, width, height);
      break;
    case SVGA_CMD_DEFINE_CURSOR:
      if (len < 8) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 8;
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = vmsvga_fifo_read(s);
      cursor.xor_mask_bpp = vmsvga_fifo_read(s);
      args =
          (SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.and_mask_bpp) +
           SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.xor_mask_bpp));
      if (cursor.width < 1 || cursor.height < 1 || cursor.and_mask_bpp < 1 ||
          cursor.xor_mask_bpp < 1 || cursor.width > s->new_width ||
          cursor.height > s->new_height || cursor.and_mask_bpp > s->new_depth ||
          cursor.xor_mask_bpp > s->new_depth) {
        VPRINT("SVGA_CMD_DEFINE_CURSOR command %u in SVGA "
               "command FIFO %u "
               "%u %u %u %u %u %u\n",
               cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width,
               cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        break;
      };
      if (len < args) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= args;
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height,
                                             cursor.and_mask_bpp);
           args++) {
        cursor.and_mask[args] = vmsvga_fifo_read_raw(s);
        VPRINT("cursor.and_mask[args] %u\n", cursor.and_mask[args]);
      };
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height,
                                             cursor.xor_mask_bpp);
           args++) {
        cursor.xor_mask[args] = vmsvga_fifo_read_raw(s);
        VPRINT("cursor.xor_mask[args] %u\n", cursor.xor_mask[args]);
      };
      vmsvga_cursor_define(s, &cursor);
      VPRINT("SVGA_CMD_DEFINE_CURSOR command %u in SVGA command "
             "FIFO %u %u "
             "%u %u %u %u %u\n",
             cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width,
             cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
      break;
    case SVGA_CMD_DEFINE_ALPHA_CURSOR:
      if (len < 6) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 6;
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = 32;
      cursor.xor_mask_bpp = 32;
      args = ((cursor.width) * (cursor.height));
      if (cursor.width < 1 || cursor.height < 1 || cursor.and_mask_bpp < 1 ||
          cursor.xor_mask_bpp < 1 || cursor.width > s->new_width ||
          cursor.height > s->new_height || cursor.and_mask_bpp > s->new_depth ||
          cursor.xor_mask_bpp > s->new_depth) {
        VPRINT("SVGA_CMD_DEFINE_ALPHA_CURSOR command %u in SVGA "
               "command "
               "FIFO %u %u %u %u %u %u %u\n",
               cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width,
               cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        break;
      };
      if (len < args) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= args;
      for (i = 0; i < args; i++) {
        uint32_t rgba = vmsvga_fifo_read_raw(s);
        cursor.xor_mask[i] = rgba & 0x00ffffff;
        cursor.and_mask[i] = rgba & 0xff000000;
        VPRINT("rgba %u\n", rgba);
      };
      vmsvga_rgba_cursor_define(s, &cursor);
      VPRINT("SVGA_CMD_DEFINE_ALPHA_CURSOR command %u in SVGA "
             "command FIFO "
             "%u %u %u %u %u %u %u\n",
             cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width,
             cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
      break;
    case SVGA_CMD_FENCE: {
      uint32_t offFifoMin = s->fifo[SVGA_FIFO_MIN];
      if (len < 2) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 2;
      fence_arg = vmsvga_fifo_read(s);
      s->fifo[SVGA_FIFO_FENCE] = fence_arg;
      if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_FENCE, offFifoMin)) {
        VPRINT("VMSVGA_IS_VALID_FIFO_REG SVGA_FIFO_FENCE "
               "1\n");
        if ((s->irq_mask) & (SVGA_IRQFLAG_ANY_FENCE)) {
          VPRINT("irq_status |= SVGA_IRQFLAG_ANY_FENCE\n");
#ifndef ANY_FENCE_OFF
          irq_status |= SVGA_IRQFLAG_ANY_FENCE;
#endif
        } else if (((VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_FENCE_GOAL,
                                              offFifoMin))) &&
                   ((s->irq_mask) & (SVGA_IRQFLAG_FENCE_GOAL)) &&
                   (fence_arg == s->fifo[SVGA_FIFO_FENCE_GOAL])) {
          VPRINT("irq_status |= SVGA_IRQFLAG_FENCE_GOAL\n");
          irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
        };
      } else {
        VPRINT("VMSVGA_IS_VALID_FIFO_REG SVGA_FIFO_FENCE "
               "0\n");
      };
      VPRINT("SVGA_CMD_FENCE command %u in SVGA command FIFO %u "
             "%u %u %u\n",
             cmd, s->irq_mask, irq_status, fence_arg, offFifoMin);
      break;
    }
    case SVGA_CMD_DEFINE_GMR2:
      if (len < 3) {  // gmrId + numPages
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t gmr_id = vmsvga_fifo_read(s);
        uint32_t num_pages = vmsvga_fifo_read(s);
        
        len -= 2;
        
        (void)gmr_id; (void)num_pages;
        
        VPRINT("SVGA_CMD_DEFINE_GMR2 command %u: gmr_id=%u num_pages=%u\n",
               cmd, gmr_id, num_pages);
      }
      break;
    case SVGA_CMD_REMAP_GMR2:
      if (len < 5) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 5;
      gmrIdCMD = vmsvga_fifo_read(s);
      flags = vmsvga_fifo_read(s);
      offsetPages = vmsvga_fifo_read(s);
      num_pages = vmsvga_fifo_read(s);
      if (flags & SVGA_REMAP_GMR2_VIA_GMR) {
        args = 2;
      } else {
        args = (flags & SVGA_REMAP_GMR2_SINGLE_PPN) ? 1 : num_pages;
        if (flags & SVGA_REMAP_GMR2_PPN64) {
          args *= 2;
        };
      };
      VPRINT("SVGA_CMD_REMAP_GMR2 command %u in SVGA command "
             "FIFO %u %u %u "
             "%u\n",
             cmd, gmrIdCMD, flags, offsetPages, num_pages);
      break;
    case SVGA_CMD_RECT_ROP_COPY:
      if (len < 8) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 8;
      UnknownCommandAY = vmsvga_fifo_read(s);
      UnknownCommandAZ = vmsvga_fifo_read(s);
      UnknownCommandBA = vmsvga_fifo_read(s);
      UnknownCommandBB = vmsvga_fifo_read(s);
      UnknownCommandBC = vmsvga_fifo_read(s);
      UnknownCommandBD = vmsvga_fifo_read(s);
      UnknownCommandM = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_RECT_ROP_COPY command %u in SVGA command "
             "FIFO %u %u "
             "%u %u %u %u %u\n",
             cmd, UnknownCommandAY, UnknownCommandAZ, UnknownCommandBA,
             UnknownCommandBB, UnknownCommandBC, UnknownCommandBD,
             UnknownCommandM);
      break;
    case SVGA_CMD_ESCAPE:
      if (len < 4) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 4;
      UnknownCommandA = vmsvga_fifo_read(s);
      UnknownCommandB = vmsvga_fifo_read(s);
      UnknownCommandAV = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_ESCAPE command %u in SVGA command FIFO %u "
             "%u %u\n",
             cmd, UnknownCommandA, UnknownCommandB, UnknownCommandAV);
      break;
    case SVGA_CMD_DEFINE_SCREEN:
      if (len < 10) {  // Minimum for SVGAScreenObject: structSize + id + flags + size(2) + root(2) + backingStore(3)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t structSize = vmsvga_fifo_read(s);
        uint32_t screen_id = vmsvga_fifo_read(s);
        uint32_t flags = vmsvga_fifo_read(s);
        uint32_t width = vmsvga_fifo_read(s);
        uint32_t height = vmsvga_fifo_read(s);
        int32_t root_x = (int32_t)vmsvga_fifo_read(s);
        int32_t root_y = (int32_t)vmsvga_fifo_read(s);
        uint32_t backing_gmr_id = vmsvga_fifo_read(s);
        uint32_t backing_offset = vmsvga_fifo_read(s);
        uint32_t backing_pitch = vmsvga_fifo_read(s);
        
        len -= 10;
        
        // Handle additional fields if present
        uint32_t clone_count = 0;
        if (len > 0 && structSize > 40) { // Has cloneCount field
          clone_count = vmsvga_fifo_read(s);
          len -= 1;
        }
        
        (void)structSize; (void)screen_id; (void)flags; (void)width; (void)height;
        (void)root_x; (void)root_y; (void)backing_gmr_id; (void)backing_offset;
        (void)backing_pitch; (void)clone_count;
        
        VPRINT("SVGA_CMD_DEFINE_SCREEN command %u: id=%u flags=0x%x size=%ux%u "
               "root=(%d,%d) backing_gmr=%u offset=%u pitch=%u clone=%u\n",
               cmd, screen_id, flags, width, height, root_x, root_y,
               backing_gmr_id, backing_offset, backing_pitch, clone_count);
      }
      break;
    case SVGA_CMD_DISPLAY_CURSOR:
      if (len < 3) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 3;
      UnknownCommandC = vmsvga_fifo_read(s);
      UnknownCommandN = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_DISPLAY_CURSOR command %u in SVGA command "
             "FIFO %u "
             "%u\n",
             cmd, UnknownCommandC, UnknownCommandN);
      break;
    case SVGA_CMD_DESTROY_SCREEN:
      if (len < 2) {  // screenId
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t screen_id = vmsvga_fifo_read(s);
        len -= 1;
        
        (void)screen_id;
        
        VPRINT("SVGA_CMD_DESTROY_SCREEN command %u: screen_id=%u\n",
               cmd, screen_id);
      }
      break;
    case SVGA_CMD_DEFINE_GMRFB:
      if (len < 6) {  // SVGAGuestPtr (2) + bytesPerLine + format (3 total for format)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t gmr_id = vmsvga_fifo_read(s);
        uint32_t offset = vmsvga_fifo_read(s);
        uint32_t bytes_per_line = vmsvga_fifo_read(s);
        uint32_t format_bpp = vmsvga_fifo_read(s);
        uint32_t format_colorDepth = vmsvga_fifo_read(s);
        uint32_t format_reserved = vmsvga_fifo_read(s);
        
        len -= 6;
        
        (void)gmr_id; (void)offset; (void)bytes_per_line;
        (void)format_bpp; (void)format_colorDepth; (void)format_reserved;
        
        VPRINT("SVGA_CMD_DEFINE_GMRFB command %u: gmr_id=%u offset=%u "
               "bytesPerLine=%u format(bpp=%u, depth=%u)\n",
               cmd, gmr_id, offset, bytes_per_line, format_bpp, format_colorDepth);
      }
      break;
    case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
      if (len < 8) {  // srcOrigin(2) + destRect(4) + destScreenId + pad
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        int32_t src_origin_x = (int32_t)vmsvga_fifo_read(s);
        int32_t src_origin_y = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_left = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_top = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_right = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_bottom = (int32_t)vmsvga_fifo_read(s);
        uint32_t dest_screen_id = vmsvga_fifo_read(s);
        
        len -= 7;
        
        (void)src_origin_x; (void)src_origin_y; (void)dest_left; (void)dest_top;
        (void)dest_right; (void)dest_bottom; (void)dest_screen_id;
        
        VPRINT("SVGA_CMD_BLIT_GMRFB_TO_SCREEN command %u: srcOrigin=(%d,%d) "
               "destRect=(%d,%d,%d,%d) destScreen=%u\n",
               cmd, src_origin_x, src_origin_y, dest_left, dest_top,
               dest_right, dest_bottom, dest_screen_id);
      }
      break;
    case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
      if (len < 8) {  // destOrigin(2) + srcRect(4) + srcScreenId + pad
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        int32_t dest_origin_x = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_origin_y = (int32_t)vmsvga_fifo_read(s);
        int32_t src_left = (int32_t)vmsvga_fifo_read(s);
        int32_t src_top = (int32_t)vmsvga_fifo_read(s);
        int32_t src_right = (int32_t)vmsvga_fifo_read(s);
        int32_t src_bottom = (int32_t)vmsvga_fifo_read(s);
        uint32_t src_screen_id = vmsvga_fifo_read(s);
        
        len -= 7;
        
        (void)dest_origin_x; (void)dest_origin_y; (void)src_left; (void)src_top;
        (void)src_right; (void)src_bottom; (void)src_screen_id;
        
        VPRINT("SVGA_CMD_BLIT_SCREEN_TO_GMRFB command %u: destOrigin=(%d,%d) "
               "srcRect=(%d,%d,%d,%d) srcScreen=%u\n",
               cmd, dest_origin_x, dest_origin_y, src_left, src_top,
               src_right, src_bottom, src_screen_id);
      }
      break;
    case SVGA_CMD_ANNOTATION_FILL:
      if (len < 1) {  // SVGAColorBGRX color (1 word)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t color = vmsvga_fifo_read(s);  // BGRX format
        
        len -= 1;
        
        (void)color;
        
        VPRINT("SVGA_CMD_ANNOTATION_FILL command %u: color=0x%08x\n",
               cmd, color);
      }
      break;
    case SVGA_CMD_ANNOTATION_COPY:
      if (len < 3) {  // srcOrigin(2) + srcScreenId
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        int32_t src_origin_x = (int32_t)vmsvga_fifo_read(s);
        int32_t src_origin_y = (int32_t)vmsvga_fifo_read(s);
        uint32_t src_screen_id = vmsvga_fifo_read(s);
        
        len -= 3;
        
        (void)src_origin_x; (void)src_origin_y; (void)src_screen_id;
        
        VPRINT("SVGA_CMD_ANNOTATION_COPY command %u: srcOrigin=(%d,%d) srcScreen=%u\n",
               cmd, src_origin_x, src_origin_y, src_screen_id);
      }
      break;
    case SVGA_CMD_MOVE_CURSOR:
      if (len < 3) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 3;
      UnknownCommandAO = vmsvga_fifo_read(s);
      UnknownCommandAP = vmsvga_fifo_read(s);
      VPRINT("SVGA_CMD_MOVE_CURSOR command %u in SVGA command "
             "FIFO %u %u\n",
             cmd, UnknownCommandAO, UnknownCommandAP);
      break;
    case SVGA_CMD_FRONT_ROP_FILL:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_FRONT_ROP_FILL command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_CMD_DEAD:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_DEAD command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_DEAD_2:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_DEAD_2 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_NOP:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_NOP command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_NOP_ERROR:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_NOP_ERROR command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_CMD_MAX:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_CMD_MAX command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_LEGACY_BASE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LEGACY_BASE command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SURFACE_DEFINE:
      if (len < 9) {  // sid + surfaceFlags + format + 6 faces (minimum)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t sid = vmsvga_fifo_read(s);
        uint32_t surfaceFlags = vmsvga_fifo_read(s);
        uint32_t format = vmsvga_fifo_read(s);
        uint32_t faces[6];
        for (int i = 0; i < 6; i++) {
          faces[i] = vmsvga_fifo_read(s);
        }
        (void)faces; // Suppress unused warning
        len -= 9;
        // TODO: Read additional mip level data based on face information
        VPRINT("SVGA_3D_CMD_SURFACE_DEFINE command %u in SVGA "
               "command FIFO sid=%u flags=%u format=%u\n",
               cmd, sid, surfaceFlags, format);
      }
      break;
    case SVGA_3D_CMD_SURFACE_DESTROY:
      if (len < 2) {  // sid
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t sid = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_SURFACE_DESTROY command %u in SVGA "
               "command FIFO sid=%u\n",
               cmd, sid);
      }
      break;
    case SVGA_3D_CMD_SURFACE_COPY:
      if (len < 7) {  // src surface image id (3) + dest surface image id (3) + at least one copy box
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        // Source surface image id
        uint32_t src_sid = vmsvga_fifo_read(s);
        uint32_t src_face = vmsvga_fifo_read(s);
        uint32_t src_mipmap = vmsvga_fifo_read(s);
        // Dest surface image id  
        uint32_t dest_sid = vmsvga_fifo_read(s);
        uint32_t dest_face = vmsvga_fifo_read(s);
        uint32_t dest_mipmap = vmsvga_fifo_read(s);
        (void)src_face; (void)src_mipmap; (void)dest_face; (void)dest_mipmap; // Suppress unused warnings
        len -= 6;
        // TODO: Read SVGA3dCopyBox structures (variable number)
        VPRINT("SVGA_3D_CMD_SURFACE_COPY command %u in SVGA "
               "command FIFO src_sid=%u dest_sid=%u\n",
               cmd, src_sid, dest_sid);
      }
      break;
    case SVGA_3D_CMD_SURFACE_STRETCHBLT:
      if (len < 15) {  // src + dest surface image ids (6) + two SVGA3dBox (6 each) + mode
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        // Source surface image id
        uint32_t src_sid = vmsvga_fifo_read(s);
        uint32_t src_face = vmsvga_fifo_read(s);
        uint32_t src_mipmap = vmsvga_fifo_read(s);
        // Dest surface image id
        uint32_t dest_sid = vmsvga_fifo_read(s);
        uint32_t dest_face = vmsvga_fifo_read(s);
        uint32_t dest_mipmap = vmsvga_fifo_read(s);
        // Source box
        uint32_t src_x = vmsvga_fifo_read(s);
        uint32_t src_y = vmsvga_fifo_read(s);
        uint32_t src_z = vmsvga_fifo_read(s);
        uint32_t src_w = vmsvga_fifo_read(s);
        uint32_t src_h = vmsvga_fifo_read(s);
        uint32_t src_d = vmsvga_fifo_read(s);
        // Dest box
        uint32_t dest_x = vmsvga_fifo_read(s);
        uint32_t dest_y = vmsvga_fifo_read(s);
        uint32_t dest_z = vmsvga_fifo_read(s);
        uint32_t dest_w = vmsvga_fifo_read(s);
        uint32_t dest_h = vmsvga_fifo_read(s);
        uint32_t dest_d = vmsvga_fifo_read(s);
        // Mode
        uint32_t mode = vmsvga_fifo_read(s);
        // Suppress unused warnings for detailed parameters
        (void)src_face; (void)src_mipmap; (void)dest_face; (void)dest_mipmap;
        (void)src_x; (void)src_y; (void)src_z; (void)src_w; (void)src_h; (void)src_d;
        (void)dest_x; (void)dest_y; (void)dest_z; (void)dest_w; (void)dest_h; (void)dest_d;
        len -= 19;
        VPRINT("SVGA_3D_CMD_SURFACE_STRETCHBLT command %u in SVGA "
               "command FIFO src_sid=%u dest_sid=%u mode=%u\n",
               cmd, src_sid, dest_sid, mode);
      }
      break;
    case SVGA_3D_CMD_SURFACE_DMA:
      if (len < 7) {  // guest ptr (2) + host surface id (3) + transfer type (1) + at least some data
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        // SVGAGuestImage guest - GMR ID + offset
        uint32_t guest_ptr_gmr = vmsvga_fifo_read(s);
        uint32_t guest_ptr_offset = vmsvga_fifo_read(s);
        // SVGA3dSurfaceImageId host
        uint32_t host_sid = vmsvga_fifo_read(s);
        uint32_t host_face = vmsvga_fifo_read(s);
        uint32_t host_mipmap = vmsvga_fifo_read(s);
        // Transfer type
        uint32_t transfer = vmsvga_fifo_read(s);
        // Suppress unused warnings
        (void)guest_ptr_gmr; (void)guest_ptr_offset; (void)host_face; (void)host_mipmap;
        len -= 6;
        // TODO: Read variable number of SVGA3dCopyBox structures
        VPRINT("SVGA_3D_CMD_SURFACE_DMA command %u in SVGA command "
               "FIFO host_sid=%u transfer=%u\n",
               cmd, host_sid, transfer);
      }
      break;
    case SVGA_3D_CMD_CONTEXT_DEFINE:
      if (len < 2) {  // cid
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_CONTEXT_DEFINE command %u in SVGA "
               "command FIFO cid=%u\n",
               cmd, cid);
      }
      break;
    case SVGA_3D_CMD_CONTEXT_DESTROY:
      if (len < 2) {  // cid
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_CONTEXT_DESTROY command %u in SVGA "
               "command FIFO cid=%u\n",
               cmd, cid);
      }
      break;
    case SVGA_3D_CMD_SETTRANSFORM:
      if (len < 19) {  // cid + type + matrix[16]
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        float matrix[16];
        for (int i = 0; i < 16; i++) {
          matrix[i] = *(float*)&s->fifo[s->fifo_stop >> 2];
          vmsvga_fifo_read_raw(s);
        }
        (void)matrix; // Suppress unused warning
        len -= 19;
        VPRINT("SVGA_3D_CMD_SETTRANSFORM command %u in SVGA "
               "command FIFO cid=%u type=%u\n",
               cmd, cid, type);
      }
      break;
    case SVGA_3D_CMD_SETZRANGE:
      if (len < 4) {  // cid + zRange (min + max)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        float min = *(float*)&s->fifo[s->fifo_stop >> 2];
        vmsvga_fifo_read_raw(s);
        float max = *(float*)&s->fifo[s->fifo_stop >> 2];
        vmsvga_fifo_read_raw(s);
        len -= 4;
        VPRINT("SVGA_3D_CMD_SETZRANGE command %u in SVGA command "
               "FIFO cid=%u min=%f max=%f\n",
               cmd, cid, min, max);
      }
      break;
    case SVGA_3D_CMD_SETRENDERSTATE:
      if (len < 4) {  // cid + at least one renderstate (state + value)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        len -= 1;
        // Read render states (each is 2 uint32s: state + value)
        while (len >= 2) {
          uint32_t state = vmsvga_fifo_read(s);
          uint32_t value = vmsvga_fifo_read(s);
          len -= 2;
          VPRINT("SVGA_3D_CMD_SETRENDERSTATE cid=%u state=%u value=%u\n",
                 cid, state, value);
        }
        VPRINT("SVGA_3D_CMD_SETRENDERSTATE command %u in SVGA "
               "command FIFO cid=%u\n",
               cmd, cid);
      }
      break;
    case SVGA_3D_CMD_SETRENDERTARGET:
      if (len < 5) {  // cid + type + target (SVGA3dSurfaceImageId has sid + face + mipmap)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        uint32_t sid = vmsvga_fifo_read(s);
        uint32_t face = vmsvga_fifo_read(s);
        uint32_t mipmap = vmsvga_fifo_read(s);
        (void)face; (void)mipmap; // Suppress unused warnings
        len -= 5;
        VPRINT("SVGA_3D_CMD_SETRENDERTARGET command %u in SVGA "
               "command FIFO cid=%u type=%u sid=%u\n",
               cmd, cid, type, sid);
      }
      break;
    case SVGA_3D_CMD_SETTEXTURESTATE:
      if (len < 4) {  // cid + at least one texture state (stage + name + value)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        len -= 1;
        // Read texture states (each is 3 uint32s: stage + name + value)
        while (len >= 3) {
          uint32_t stage = vmsvga_fifo_read(s);
          uint32_t name = vmsvga_fifo_read(s);
          uint32_t value = vmsvga_fifo_read(s);
          len -= 3;
          VPRINT("SVGA_3D_CMD_SETTEXTURESTATE stage=%u name=%u value=%u\n",
                 stage, name, value);
        }
        VPRINT("SVGA_3D_CMD_SETTEXTURESTATE command %u in SVGA "
               "command FIFO cid=%u\n",
               cmd, cid);
      }
      break;
    case SVGA_3D_CMD_SETMATERIAL:
      if (len < 19) {  // cid + face + material (diffuse[4] + ambient[4] + specular[4] + emissive[4] + shininess)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t face = vmsvga_fifo_read(s);
        // Read material properties (17 floats total)
        for (int i = 0; i < 17; i++) {
          vmsvga_fifo_read(s); // Skip for now, just consume the data
        }
        len -= 19;
        VPRINT("SVGA_3D_CMD_SETMATERIAL command %u in SVGA command "
               "FIFO cid=%u face=%u\n",
               cmd, cid, face);
      }
      break;
    case SVGA_3D_CMD_SETLIGHTDATA:
      if (len < 17) {  // cid + index + light data (15 values: type, inWorldSpace, 13 floats)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t index = vmsvga_fifo_read(s);
        // Read light data (15 more values)
        for (int i = 0; i < 15; i++) {
          vmsvga_fifo_read(s); // Skip for now, just consume the data
        }
        len -= 17;
        VPRINT("SVGA_3D_CMD_SETLIGHTDATA command %u in SVGA "
               "command FIFO cid=%u index=%u\n",
               cmd, cid, index);
      }
      break;
    case SVGA_3D_CMD_SETLIGHTENABLED:
      if (len < 4) {  // cid + index + enabled
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t index = vmsvga_fifo_read(s);
        uint32_t enabled = vmsvga_fifo_read(s);
        len -= 3;
        VPRINT("SVGA_3D_CMD_SETLIGHTENABLED command %u in SVGA "
               "command FIFO cid=%u index=%u enabled=%u\n",
               cmd, cid, index, enabled);
      }
      break;
    case SVGA_3D_CMD_SETVIEWPORT:
      if (len < 5) {  // cid + rect (x, y, w, h)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t x = vmsvga_fifo_read(s);
        uint32_t y = vmsvga_fifo_read(s);
        uint32_t w = vmsvga_fifo_read(s);
        uint32_t h = vmsvga_fifo_read(s);
        len -= 5;
        VPRINT("SVGA_3D_CMD_SETVIEWPORT command %u in SVGA command "
               "FIFO cid=%u x=%u y=%u w=%u h=%u\n",
               cmd, cid, x, y, w, h);
      }
      break;
    case SVGA_3D_CMD_SETCLIPPLANE:
      if (len < 7) {  // cid + index + plane[4]
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t index = vmsvga_fifo_read(s);
        float plane[4];
        for (int i = 0; i < 4; i++) {
          plane[i] = *(float*)&s->fifo[s->fifo_stop >> 2];
          vmsvga_fifo_read_raw(s);
        }
        (void)plane; // Suppress unused warning
        len -= 6;
        VPRINT("SVGA_3D_CMD_SETCLIPPLANE command %u in SVGA "
               "command FIFO cid=%u index=%u\n",
               cmd, cid, index);
      }
      break;
    case SVGA_3D_CMD_CLEAR:
      if (len < 5) {  // cid + clearFlag + color + depth + stencil (minimum, plus variable rects)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t clearFlag = vmsvga_fifo_read(s);
        uint32_t color = vmsvga_fifo_read(s);
        float depth = *(float*)&s->fifo[s->fifo_stop >> 2];
        vmsvga_fifo_read_raw(s);
        uint32_t stencil = vmsvga_fifo_read(s);
        len -= 5;
        // TODO: Read variable number of SVGA3dRect structures
        VPRINT("SVGA_3D_CMD_CLEAR command %u in SVGA command FIFO "
               "cid=%u flag=%u color=0x%x depth=%f stencil=%u\n",
               cmd, cid, clearFlag, color, depth, stencil);
      }
      break;
    case SVGA_3D_CMD_PRESENT:
      if (len < 2) {  // cid + at least some basic structure
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        len -= 1;
        // TODO: Read present rectangles and other parameters
        VPRINT("SVGA_3D_CMD_PRESENT command %u in SVGA command FIFO "
               "cid=%u\n", cmd, cid);
      }
      break;
    case SVGA_3D_CMD_SHADER_DEFINE:
      if (len < 3) {  // shid + type + at least some bytecode
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t shid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        len -= 2;
        // TODO: Read shader bytecode (variable length)
        VPRINT("SVGA_3D_CMD_SHADER_DEFINE command %u in SVGA "
               "command FIFO shid=%u type=%u\n",
               cmd, shid, type);
      }
      break;
    case SVGA_3D_CMD_SHADER_DESTROY:
      if (len < 4) {  // cid + shid + type
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t shid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        len -= 3;
        VPRINT("SVGA_3D_CMD_SHADER_DESTROY command %u in SVGA "
               "command FIFO cid=%u shid=%u type=%u\n",
               cmd, cid, shid, type);
      }
      break;
    case SVGA_3D_CMD_SET_SHADER:
      if (len < 4) {  // cid + type + shid
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        uint32_t shid = vmsvga_fifo_read(s);
        len -= 3;
        VPRINT("SVGA_3D_CMD_SET_SHADER command %u in SVGA command "
               "FIFO cid=%u type=%u shid=%u\n",
               cmd, cid, type, shid);
      }
      break;
    case SVGA_3D_CMD_SET_SHADER_CONST:
      if (len < 8) {  // cid + reg + type + ctype + values[4] (minimum)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t reg = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        uint32_t ctype = vmsvga_fifo_read(s);
        uint32_t values[4];
        for (int i = 0; i < 4; i++) {
          values[i] = vmsvga_fifo_read(s);
        }
        (void)values; (void)ctype; // Suppress unused warnings
        len -= 8;
        // TODO: Read additional values if present
        VPRINT("SVGA_3D_CMD_SET_SHADER_CONST command %u in SVGA "
               "command FIFO cid=%u reg=%u type=%u\n",
               cmd, cid, reg, type);
      }
      break;
    case SVGA_3D_CMD_DRAW_PRIMITIVES:
      if (len < 4) {  // cid + numVertexDecls + numRanges (minimum, plus variable data)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t numVertexDecls = vmsvga_fifo_read(s);
        uint32_t numRanges = vmsvga_fifo_read(s);
        len -= 3;
        // TODO: Read variable arrays of vertex declarations and primitive ranges
        VPRINT("SVGA_3D_CMD_DRAW_PRIMITIVES command %u in SVGA "
               "command FIFO cid=%u numVertexDecls=%u numRanges=%u\n",
               cmd, cid, numVertexDecls, numRanges);
      }
      break;
    case SVGA_3D_CMD_SETSCISSORRECT:
      if (len < 5) {  // cid + rect (x, y, w, h)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t x = vmsvga_fifo_read(s);
        uint32_t y = vmsvga_fifo_read(s);
        uint32_t w = vmsvga_fifo_read(s);
        uint32_t h = vmsvga_fifo_read(s);
        len -= 5;
        VPRINT("SVGA_3D_CMD_SETSCISSORRECT command %u in SVGA "
               "command FIFO cid=%u x=%u y=%u w=%u h=%u\n",
               cmd, cid, x, y, w, h);
      }
      break;
    case SVGA_3D_CMD_BEGIN_QUERY:
      if (len < 3) {  // cid + type
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_BEGIN_QUERY command %u in SVGA command "
               "FIFO cid=%u type=%u\n",
               cmd, cid, type);
      }
      break;
    case SVGA_3D_CMD_END_QUERY:
      if (len < 3) {  // cid + type
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_END_QUERY command %u in SVGA command "
               "FIFO cid=%u type=%u\n",
               cmd, cid, type);
      }
      break;
    case SVGA_3D_CMD_WAIT_FOR_QUERY:
      if (len < 5) {  // cid + type + guestResult (2 uint32s)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t cid = vmsvga_fifo_read(s);
        uint32_t type = vmsvga_fifo_read(s);
        uint32_t guest_ptr_low = vmsvga_fifo_read(s);
        uint32_t guest_ptr_high = vmsvga_fifo_read(s);
        (void)guest_ptr_low; (void)guest_ptr_high; // Suppress unused warnings
        len -= 4;
        VPRINT("SVGA_3D_CMD_WAIT_FOR_QUERY command %u in SVGA "
               "command FIFO cid=%u type=%u\n",
               cmd, cid, type);
      }
      break;
    case SVGA_3D_CMD_PRESENT_READBACK:
      if (len < 2) {  // Minimal structure, likely cid or similar
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t param = vmsvga_fifo_read(s);
        len -= 1;
        VPRINT("SVGA_3D_CMD_PRESENT_READBACK command %u in SVGA "
               "command FIFO param=%u\n",
               cmd, param);
      }
      break;
    case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
      if (len < 12) {  // srcImage (3) + srcRect (4) + destScreenId (1) + destRect (4)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        // Source surface image id
        uint32_t src_sid = vmsvga_fifo_read(s);
        uint32_t src_face = vmsvga_fifo_read(s);
        uint32_t src_mipmap = vmsvga_fifo_read(s);
        // Source rect
        int32_t src_left = (int32_t)vmsvga_fifo_read(s);
        int32_t src_top = (int32_t)vmsvga_fifo_read(s);
        int32_t src_right = (int32_t)vmsvga_fifo_read(s);
        int32_t src_bottom = (int32_t)vmsvga_fifo_read(s);
        // Dest screen id
        uint32_t dest_screen_id = vmsvga_fifo_read(s);
        // Dest rect
        int32_t dest_left = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_top = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_right = (int32_t)vmsvga_fifo_read(s);
        int32_t dest_bottom = (int32_t)vmsvga_fifo_read(s);
        // Suppress unused warnings for detailed parameters  
        (void)src_face; (void)src_mipmap;
        (void)src_left; (void)src_top; (void)src_right; (void)src_bottom;
        (void)dest_left; (void)dest_top; (void)dest_right; (void)dest_bottom;
        len -= 12;
        // TODO: Read optional clipping rectangles
        VPRINT("SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN command %u in SVGA "
               "command FIFO src_sid=%u screen_id=%u\n",
               cmd, src_sid, dest_screen_id);
      }
      break;
    case SVGA_3D_CMD_SURFACE_DEFINE_V2:
      if (len < 11) {  // sid + surfaceFlags + format + 6 faces + multisampleCount + autogenFilter (minimum)
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t sid = vmsvga_fifo_read(s);
        uint32_t surfaceFlags = vmsvga_fifo_read(s);
        uint32_t format = vmsvga_fifo_read(s);
        uint32_t faces[6];
        for (int i = 0; i < 6; i++) {
          faces[i] = vmsvga_fifo_read(s);
        }
        uint32_t multisampleCount = vmsvga_fifo_read(s);
        uint32_t autogenFilter = vmsvga_fifo_read(s);
        (void)faces; (void)multisampleCount; (void)autogenFilter; // Suppress unused warnings
        len -= 11;
        // TODO: Read additional mip level data based on face information
        VPRINT("SVGA_3D_CMD_SURFACE_DEFINE_V2 command %u in SVGA "
               "command FIFO sid=%u flags=%u format=%u\n",
               cmd, sid, surfaceFlags, format);
      }
      break;
    case SVGA_3D_CMD_GENERATE_MIPMAPS:
      if (len < 3) {  // sid + filter
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      {
        uint32_t sid = vmsvga_fifo_read(s);
        uint32_t filter = vmsvga_fifo_read(s);
        len -= 2;
        VPRINT("SVGA_3D_CMD_GENERATE_MIPMAPS command %u in SVGA "
               "command FIFO sid=%u filter=%u\n",
               cmd, sid, filter);
      }
      break;
    case SVGA_3D_CMD_DEAD4:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD4 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD5:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD5 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD6:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD6 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD7:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD7 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD8:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD8 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD9:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD9 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD10:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD10 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD11:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD11 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_ACTIVATE_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_ACTIVATE_SURFACE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEACTIVATE_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEACTIVATE_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SCREEN_DMA:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SCREEN_DMA command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEAD1:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD1 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD2:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD2 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD12:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD12 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD13:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD13 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD14:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD14 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD15:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD15 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD16:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD16 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DEAD17:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD17 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_SET_OTABLE_BASE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_OTABLE_BASE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_READBACK_OTABLE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_READBACK_OTABLE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_MOB:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_MOB command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DESTROY_GB_MOB:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DESTROY_GB_MOB command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEAD3:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEAD3 command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_UPDATE_GB_MOB_MAPPING:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_UPDATE_GB_MOB_MAPPING command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DESTROY_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DESTROY_GB_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BIND_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BIND_GB_SURFACE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_COND_BIND_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_COND_BIND_GB_SURFACE command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_UPDATE_GB_IMAGE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_UPDATE_GB_IMAGE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_UPDATE_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_UPDATE_GB_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_READBACK_GB_IMAGE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_READBACK_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_READBACK_GB_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_IMAGE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_INVALIDATE_GB_IMAGE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_INVALIDATE_GB_SURFACE command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DESTROY_GB_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DESTROY_GB_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BIND_GB_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BIND_GB_CONTEXT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_READBACK_GB_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_READBACK_GB_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_INVALIDATE_GB_CONTEXT command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SHADER command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DESTROY_GB_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DESTROY_GB_SHADER command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BIND_GB_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BIND_GB_SHADER command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SET_OTABLE_BASE64:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_OTABLE_BASE64 command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BEGIN_GB_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BEGIN_GB_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_END_GB_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_END_GB_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_WAIT_FOR_GB_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_WAIT_FOR_GB_QUERY command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_NOP:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_NOP command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_ENABLE_GART:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_ENABLE_GART command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DISABLE_GART:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DISABLE_GART command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_MAP_MOB_INTO_GART:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_MAP_MOB_INTO_GART command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_UNMAP_GART_RANGE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_UNMAP_GART_RANGE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SCREENTARGET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SCREENTARGET command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DESTROY_GB_SCREENTARGET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DESTROY_GB_SCREENTARGET command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BIND_GB_SCREENTARGET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BIND_GB_SCREENTARGET command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_UPDATE_GB_SCREENTARGET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_UPDATE_GB_SCREENTARGET command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_GB_SCREEN_DMA:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_GB_SCREEN_DMA command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_GB_MOB_FENCE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_GB_MOB_FENCE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V2:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SURFACE_V2 command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_MOB64:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_MOB64 command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_REDEFINE_GB_MOB64:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_REDEFINE_GB_MOB64 command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_NOP_ERROR:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_NOP_ERROR command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SET_VERTEX_STREAMS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_VERTEX_STREAMS command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SET_VERTEX_DECLS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_VERTEX_DECLS command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SET_VERTEX_DIVISORS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SET_VERTEX_DIVISORS command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DRAW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DRAW command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DRAW_INDEXED:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DRAW_INDEXED command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_CONTEXT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_READBACK_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_READBACK_CONTEXT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_INVALIDATE_CONTEXT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_INVALIDATE_CONTEXT command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SHADER_RESOURCES:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SHADER_RESOURCES command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SHADER command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SAMPLERS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SAMPLERS command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_INDEXED command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_INSTANCED:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_INSTANCED command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_AUTO:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_AUTO command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_INPUT_LAYOUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_INPUT_LAYOUT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_INDEX_BUFFER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_INDEX_BUFFER command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_TOPOLOGY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_TOPOLOGY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_RENDERTARGETS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_RENDERTARGETS command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_BLEND_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_BLEND_STATE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_RASTERIZER_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_RASTERIZER_STATE command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_QUERY_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_QUERY_OFFSET command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BEGIN_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BEGIN_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_END_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_END_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_READBACK_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_READBACK_QUERY command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_PREDICATION:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_PREDICATION command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SOTARGETS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SOTARGETS command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_VIEWPORTS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_VIEWPORTS command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SCISSORRECTS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SCISSORRECTS command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_COPY_REGION:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_COPY_REGION command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_COPY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRESENTBLT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRESENTBLT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_GENMIPS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_GENMIPS command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_READBACK_SUBRESOURCE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_READBACK_SUBRESOURCE command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW command "
             "%u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_BLEND_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_BLEND_STATE command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_BLEND_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_BLEND_STATE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_SHADER command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_SHADER command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_SHADER command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_STREAMOUTPUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_STREAMOUTPUT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_COTABLE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_COTABLE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_READBACK_COTABLE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_READBACK_COTABLE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BUFFER_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BUFFER_COPY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SURFACE_COPY_AND_READBACK:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SURFACE_COPY_AND_READBACK command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_MOVE_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_MOVE_QUERY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_ALL_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_ALL_QUERY command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_READBACK_ALL_QUERY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_READBACK_ALL_QUERY command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER command "
             "%u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_MOB_FENCE_64:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_MOB_FENCE_64 command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_ALL_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_ALL_SHADER command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_HINT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_HINT command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_DX_BUFFER_UPDATE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BUFFER_UPDATE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_HS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_HS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_DS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_DS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_CS_CONSTANT_BUFFER_OFFSET:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_CS_CONSTANT_BUFFER_OFFSET "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_COND_BIND_ALL_SHADER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_COND_BIND_ALL_SHADER command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SCREEN_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SCREEN_COPY command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_GROW_OTABLE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_GROW_OTABLE command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_GROW_COTABLE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_GROW_COTABLE command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_INTRA_SURFACE_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_INTRA_SURFACE_COPY command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V3:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SURFACE_V3 command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_RESOLVE_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_RESOLVE_COPY command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_RESOLVE_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_RESOLVE_COPY command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_CONVERT_REGION:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_CONVERT_REGION command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_PRED_CONVERT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_PRED_CONVERT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_WHOLE_SURFACE_COPY:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_WHOLE_SURFACE_COPY command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_UA_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_UA_VIEW command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DESTROY_UA_VIEW:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DESTROY_UA_VIEW command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_CLEAR_UA_VIEW_UINT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_CLEAR_UA_VIEW_UINT command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_CLEAR_UA_VIEW_FLOAT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_CLEAR_UA_VIEW_FLOAT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_COPY_STRUCTURE_COUNT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_COPY_STRUCTURE_COUNT command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_UA_VIEWS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_UA_VIEWS command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED_INDIRECT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED_INDIRECT "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DRAW_INSTANCED_INDIRECT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DRAW_INSTANCED_INDIRECT command %u "
             "in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DISPATCH:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DISPATCH command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DISPATCH_INDIRECT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DISPATCH_INDIRECT command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_WRITE_ZERO_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_WRITE_ZERO_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_HINT_ZERO_SURFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_HINT_ZERO_SURFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_BITBLT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_BITBLT command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_TRANSBLT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_TRANSBLT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_STRETCHBLT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_STRETCHBLT command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_COLORFILL:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_COLORFILL command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_ALPHABLEND:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_ALPHABLEND command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND command %u in "
             "SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V4:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DEFINE_GB_SURFACE_V4 command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_CS_UA_VIEWS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_CS_UA_VIEWS command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_MIN_LOD:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_MIN_LOD command %u in SVGA "
             "command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2 command "
             "%u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_SET_SHADER_IFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_SET_SHADER_IFACE command %u in SVGA "
             "command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_STREAMOUTPUT:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_STREAMOUTPUT command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_SURFACE_STRETCHBLT_NON_MS_TO_MS:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_SURFACE_STRETCHBLT_NON_MS_TO_MS "
             "command %u in "
             "SVGA command FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_DX_BIND_SHADER_IFACE:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_DX_BIND_SHADER_IFACE command %u in "
             "SVGA command "
             "FIFO\n",
             cmd);
      break;
    case SVGA_3D_CMD_MAX:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_MAX command %u in SVGA command FIFO\n", cmd);
      break;
    case SVGA_3D_CMD_FUTURE_MAX:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("SVGA_3D_CMD_FUTURE_MAX command %u in SVGA command "
             "FIFO\n",
             cmd);
      break;
    default:
      if (len < 1) {
        VMSVGA_FIFO_REWIND(s, cmd);
        break;
      }
      len -= 1;
      VPRINT("default command %u in SVGA command FIFO\n", cmd);
      break;
    };
  };
  if ((irq_status) || ((s->irq_mask) & (SVGA_IRQFLAG_FIFO_PROGRESS))) {
    VPRINT("s -> irq_mask || irq_status & "
           "SVGA_IRQFLAG_FIFO_PROGRESS\n");
    if ((s->irq_mask) & (SVGA_IRQFLAG_FIFO_PROGRESS)) {
      VPRINT("irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS\n");
      irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
    };
    if ((s->irq_mask) & (irq_status)) {
#ifndef RAISE_IRQ_OFF
      struct pci_vmsvga_state_s *pci_vmsvga =
          container_of(s, struct pci_vmsvga_state_s, chip);
#endif
      VPRINT("FIFO: Pci_set_irq=1\n");
      s->irq_status = irq_status;
#ifndef RAISE_IRQ_OFF
      pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
#endif
    };
  };
};
static uint32_t vmsvga_index_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_index_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_index_read %u %u\n", address, s->index);
  return s->index;
};
static void vmsvga_index_write(void *opaque, uint32_t address, uint32_t index) {
  VPRINT("vmsvga_index_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_index_write %u %u\n", address, index);
  s->index = index;
};
static inline void vmsvga_check_size(struct vmsvga_state_s *s) {
  // VPRINT("vmsvga_check_size was just executed\n");
  DisplaySurface *surface = qemu_console_surface(s->vga.con);
  uint32_t new_stride;
  if (s->pitchlock >= 1) {
    new_stride = s->pitchlock;
  } else {
    new_stride = (((s->new_depth) * (s->new_width)) / (8));
  };
  if (s->new_width != surface_width(surface) ||
      s->new_height != surface_height(surface) ||
      (new_stride != surface_stride(surface)) ||
      s->new_depth != surface_bits_per_pixel(surface)) {
    pixman_format_code_t format =
        qemu_default_pixman_format(s->new_depth, true);
    surface = qemu_create_displaysurface_from(
        s->new_width, s->new_height, format, new_stride, s->vga.vram_ptr);
    dpy_gfx_replace_surface(s->vga.con, surface);
  };
};
static void *vmsvga_loop(void *arg) {
  // VPRINT("vmsvga_loop was just executed\n");
  struct vmsvga_state_s *s = (struct vmsvga_state_s *)arg;
  while (true) {
    s->fifo[32] = 186;
    s->fifo[33] = 256;
    s->fifo[35] = 1;
    s->fifo[36] = 1;
    s->fifo[37] = 8;
    s->fifo[38] = 2;
    s->fifo[39] = 8;
    s->fifo[40] = 3;
    s->fifo[41] = 8;
    s->fifo[42] = 4;
    s->fifo[43] = 7;
    s->fifo[44] = 5;
    s->fifo[45] = 1;
    s->fifo[46] = 6;
    s->fifo[47] = 13;
    s->fifo[48] = 7;
    s->fifo[49] = 1;
    s->fifo[50] = 8;
    s->fifo[51] = 8;
    s->fifo[52] = 9;
    s->fifo[53] = 1;
    s->fifo[54] = 10;
    s->fifo[55] = 1;
    s->fifo[56] = 11;
    s->fifo[57] = 4;
    s->fifo[58] = 12;
    s->fifo[59] = 1;
    s->fifo[60] = 13;
    s->fifo[61] = 1;
    s->fifo[62] = 14;
    s->fifo[63] = 1;
    s->fifo[64] = 15;
    s->fifo[65] = 1;
    s->fifo[66] = 16;
    s->fifo[67] = 1;
    s->fifo[68] = 17;
    s->fifo[69] = 1128071168;
    s->fifo[70] = 18;
    s->fifo[71] = 20;
    s->fifo[72] = 19;
    s->fifo[73] = 32768;
    s->fifo[74] = 20;
    s->fifo[75] = 32768;
    s->fifo[76] = 21;
    s->fifo[77] = 16384;
    s->fifo[78] = 22;
    s->fifo[79] = 32768;
    s->fifo[80] = 23;
    s->fifo[81] = 32768;
    s->fifo[82] = 24;
    s->fifo[83] = 16;
    s->fifo[84] = 25;
    s->fifo[85] = 2097151;
    s->fifo[86] = 26;
    s->fifo[87] = 1048575;
    s->fifo[88] = 27;
    s->fifo[89] = 65535;
    s->fifo[90] = 28;
    s->fifo[91] = 65535;
    s->fifo[92] = 29;
    s->fifo[93] = 32;
    s->fifo[94] = 30;
    s->fifo[95] = 32;
    s->fifo[96] = 31;
    s->fifo[97] = 67108863;
    s->fifo[98] = 32;
    s->fifo[99] = 1633311;
    s->fifo[100] = 33;
    s->fifo[101] = 1630495;
    s->fifo[102] = 34;
    s->fifo[103] = 548895;
    s->fifo[104] = 35;
    s->fifo[105] = 548895;
    s->fifo[106] = 36;
    s->fifo[107] = 549151;
    s->fifo[108] = 37;
    s->fifo[109] = 24863;
    s->fifo[110] = 38;
    s->fifo[111] = 1633311;
    s->fifo[112] = 39;
    s->fifo[113] = 24607;
    s->fifo[114] = 40;
    s->fifo[115] = 24583;
    s->fifo[116] = 41;
    s->fifo[117] = 24607;
    s->fifo[118] = 42;
    s->fifo[119] = 24607;
    s->fifo[120] = 43;
    s->fifo[121] = 16581;
    s->fifo[122] = 44;
    s->fifo[123] = 16581;
    s->fifo[124] = 45;
    s->fifo[125] = 16581;
    s->fifo[126] = 46;
    s->fifo[127] = 57349;
    s->fifo[128] = 47;
    s->fifo[129] = 57349;
    s->fifo[130] = 48;
    s->fifo[131] = 57349;
    s->fifo[132] = 49;
    s->fifo[133] = 57349;
    s->fifo[134] = 50;
    s->fifo[135] = 57349;
    s->fifo[136] = 51;
    s->fifo[137] = 81925;
    s->fifo[138] = 52;
    s->fifo[139] = 81927;
    s->fifo[140] = 53;
    s->fifo[141] = 81927;
    s->fifo[142] = 54;
    s->fifo[143] = 81925;
    s->fifo[144] = 55;
    s->fifo[145] = 81921;
    s->fifo[146] = 56;
    s->fifo[147] = 8413215;
    s->fifo[148] = 57;
    s->fifo[149] = 8413215;
    s->fifo[150] = 58;
    s->fifo[151] = 8413215;
    s->fifo[152] = 59;
    s->fifo[153] = 8413215;
    s->fifo[154] = 60;
    s->fifo[155] = 8413215;
    s->fifo[156] = 61;
    s->fifo[157] = 8413215;
    s->fifo[158] = 62;
    s->fifo[160] = 63;
    s->fifo[161] = 4;
    s->fifo[162] = 64;
    s->fifo[163] = 8;
    s->fifo[164] = 65;
    s->fifo[165] = 81927;
    s->fifo[166] = 66;
    s->fifo[167] = 24607;
    s->fifo[168] = 67;
    s->fifo[169] = 24607;
    s->fifo[170] = 68;
    s->fifo[171] = 19161088;
    s->fifo[172] = 69;
    s->fifo[173] = 19161088;
    s->fifo[174] = 70;
    s->fifo[176] = 71;
    s->fifo[178] = 72;
    s->fifo[180] = 73;
    s->fifo[182] = 74;
    s->fifo[183] = 1;
    s->fifo[184] = 75;
    s->fifo[185] = 19161088;
    s->fifo[186] = 76;
    s->fifo[188] = 77;
    s->fifo[189] = 256;
    s->fifo[190] = 78;
    s->fifo[191] = 32768;
    s->fifo[192] = 79;
    s->fifo[193] = 16581;
    s->fifo[194] = 80;
    s->fifo[195] = 16581;
    s->fifo[196] = 81;
    s->fifo[197] = 16581;
    s->fifo[198] = 82;
    s->fifo[199] = 24581;
    s->fifo[200] = 83;
    s->fifo[201] = 24581;
    s->fifo[202] = 84;
    s->fifo[204] = 85;
    s->fifo[206] = 86;
    s->fifo[208] = 87;
    s->fifo[209] = 1;
    s->fifo[210] = 88;
    s->fifo[211] = 1;
    s->fifo[212] = 89;
    s->fifo[213] = 1092616192;
    s->fifo[214] = 90;
    s->fifo[215] = 1092616192;
    s->fifo[216] = 91;
    s->fifo[217] = 19161088;
    if (s->pitchlock >= 1) {
      s->fifo[SVGA_FIFO_PITCHLOCK] = s->pitchlock;
    } else {
      s->fifo[SVGA_FIFO_PITCHLOCK] = (((s->new_depth) * (s->new_width)) / (8));
    };
    s->fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_WS8_B1;
    s->fifo[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_WS8_B1;
    // s -> fifo[SVGA_FIFO_FLAGS] = 0;
    s->fifo[SVGA_FIFO_FLAGS] = s->ff;
    s->fifo[SVGA_FIFO_BUSY] = s->sync;
    // s -> fifo[SVGA_FIFO_CAPABILITIES] = 1919;
    s->fifo[SVGA_FIFO_CAPABILITIES] = s->fc;
    // s->fifo[SVGA_FIFO_DEAD] = 2;
    s->fifo[SVGA_FIFO_DEAD] = 0;
    s->fifo[SVGA_FIFO_CURSOR_SCREEN_ID] = -1;
    if ((s->enable >= 1 || s->config >= 1) &&
        (s->new_width >= 1 && s->new_height >= 1 && s->new_depth >= 1)) {
      if (s->pitchlock >= 1) {
        s->new_width = (((s->pitchlock) * (8)) / (s->new_depth));
      };
      dpy_gfx_update(s->vga.con, 0, 0, s->new_width, s->new_height);
    };
  };
  return 0;
};
static uint32_t vmsvga_value_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_value_read was just executed\n");
  uint32_t ret;
  uint32_t caps;
  uint32_t cap2;
  struct vmsvga_state_s *s = opaque;
  VPRINT("Unknown register %u\n", s->index);

  if (s->index >= SVGA_REG_PALETTE_MIN && s->index <= SVGA_REG_PALETTE_MAX) {
    uint32_t idx = s->index - SVGA_REG_PALETTE_MIN;
    ret = s->svgapalettebase[idx];
    VPRINT("SVGA_REG_PALETTE_%u read %u\n", idx, ret);
    return ret;
  }

  switch (s->index) {
  case SVGA_REG_FENCE_GOAL:
    // ret = 0;
    ret = s->fifo[SVGA_FIFO_FENCE_GOAL];
    VPRINT("SVGA_REG_FENCE_GOAL register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_ID:
    // ret = -1879048190;
    ret = s->svgaid;
    VPRINT("SVGA_REG_ID register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_ENABLE:
    // ret = 1;
    ret = s->enable;
    VPRINT("SVGA_REG_ENABLE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_WIDTH:
    // ret = 1024;
    if (s->new_width >= 1) {
      ret = s->new_width;
    } else {
      ret = 1024;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_WIDTH register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_HEIGHT:
    // ret = 768;
    if (s->new_height >= 1) {
      ret = s->new_height;
    } else {
      ret = 768;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_HEIGHT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_WIDTH:
    ret = 8192;
    VPRINT("SVGA_REG_MAX_WIDTH register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_HEIGHT:
    ret = 8192;
    VPRINT("SVGA_REG_MAX_HEIGHT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SCREENTARGET_MAX_WIDTH:
    ret = 8192;
    VPRINT("SVGA_REG_SCREENTARGET_MAX_WIDTH register %u with the "
           "return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_SCREENTARGET_MAX_HEIGHT:
    ret = 8192;
    VPRINT("SVGA_REG_SCREENTARGET_MAX_HEIGHT register %u with "
           "the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    // ret = 32;
    if (s->new_depth >= 1) {
      ret = s->new_depth;
    } else {
      ret = 32;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_BITS_PER_PIXEL register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_HOST_BITS_PER_PIXEL:
    // ret = 32;
    if (s->new_depth >= 1) {
      ret = s->new_depth;
    } else {
      ret = 32;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_HOST_BITS_PER_PIXEL register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DEPTH:
    // ret = 0;
    if ((s->new_depth) == (32)) {
      ret = 24;
    } else if (s->new_depth >= 1) {
      ret = s->new_depth;
    } else {
      ret = 24;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_DEPTH register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_PSEUDOCOLOR:
    // ret = 0;
    if (s->new_depth == 8) {
      ret = 1;
    } else {
      ret = 0;
    };
    VPRINT("SVGA_REG_PSEUDOCOLOR register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_RED_MASK:
    // ret = 16711680;
    if (s->new_depth == 8) {
      ret = 0x00000007;
    } else if (s->new_depth == 15) {
      ret = 0x0000001f;
    } else if (s->new_depth == 16) {
      ret = 0x0000001f;
    } else {
      ret = 0x00ff0000;
    };
    VPRINT("SVGA_REG_RED_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_GREEN_MASK:
    // ret = 65280;
    if (s->new_depth == 8) {
      ret = 0x00000038;
    } else if (s->new_depth == 15) {
      ret = 0x000003e0;
    } else if (s->new_depth == 16) {
      ret = 0x000007e0;
    } else {
      ret = 0x0000ff00;
    };
    VPRINT("SVGA_REG_GREEN_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BLUE_MASK:
    // ret = 255;
    if (s->new_depth == 8) {
      ret = 0x000000c0;
    } else if (s->new_depth == 15) {
      ret = 0x00007c00;
    } else if (s->new_depth == 16) {
      ret = 0x0000f800;
    } else {
      ret = 0x000000ff;
    };
    VPRINT("SVGA_REG_BLUE_MASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BYTES_PER_LINE:
    // ret = 4096;
    if (s->pitchlock >= 1) {
      ret = s->pitchlock;
    } else {
      ret = (((s->new_depth) * (s->new_width)) / (8));
    };
    VPRINT("SVGA_REG_BYTES_PER_LINE register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_FB_START: {
    // ret = -268435456;
    struct pci_vmsvga_state_s *pci_vmsvga =
        container_of(s, struct pci_vmsvga_state_s, chip);
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
    VPRINT("SVGA_REG_FB_START register %u with the return of %u\n", s->index,
           ret);
    break;
  };
  case SVGA_REG_FB_OFFSET:
    ret = 0;
    VPRINT("SVGA_REG_FB_OFFSET register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_BLANK_SCREEN_TARGETS:
    ret = 0;
    VPRINT("SVGA_REG_BLANK_SCREEN_TARGETS register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_VRAM_SIZE:
    ret = s->vga.vram_size;
    VPRINT("SVGA_REG_VRAM_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_FB_SIZE:
    // ret = 3145728;
    if (s->pitchlock >= 1) {
      ret = ((s->new_height) * (s->pitchlock));
    } else {
      ret = ((s->new_height) * ((((s->new_depth) * (s->new_width)) / (8))));
    };
    VPRINT("SVGA_REG_FB_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MOB_MAX_SIZE:
    ret = 1073741824;
    VPRINT("SVGA_REG_MOB_MAX_SIZE register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_GBOBJECT_MEM_SIZE_KB:
    ret = 8388608;
    VPRINT("SVGA_REG_GBOBJECT_MEM_SIZE_KB register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
    // ret = 3145728;
    if (s->pitchlock >= 1) {
      ret = ((s->new_height) * (s->pitchlock));
    } else {
      ret = ((s->new_height) * ((((s->new_depth) * (s->new_width)) / (8))));
    };
    VPRINT("SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB register %u "
           "with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_MSHINT:
    // ret = 0;
    ret = 1;
    VPRINT("SVGA_REG_MSHINT register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM:
    ret = 134217728;
    VPRINT("SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM register %u "
           "with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CAPABILITIES:
    // ret = 4261397474;
    caps = 0xffffffff;
#ifndef EXPCAPS
    caps -= SVGA_CAP_UNKNOWN_A;       // Windows 9x
    caps -= SVGA_CAP_UNKNOWN_C;       // Windows 9x
    caps -= SVGA_CAP_RECT_COPY;       // Windows 9x & Windows (XPDM)
    // Enable capabilities needed for Windows 7 Aero:
    // caps -= SVGA_CAP_SCREEN_OBJECT_2; // Enable for Windows 7 Aero
    // caps -= SVGA_CAP_CMD_BUFFERS_2;   // Enable for Windows 7 Aero
    // caps -= SVGA_CAP_GBOBJECTS;       // Enable for Windows 7 Aero
#endif
    ret = caps;
    VPRINT("SVGA_REG_CAPABILITIES register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_CAP2:
    // ret = 389119;
    cap2 = 0xffffffff;
    ret = cap2;
    VPRINT("SVGA_REG_CAP2 register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_MEM_START: {
    // ret = -75497472;
    struct pci_vmsvga_state_s *pci_vmsvga =
        container_of(s, struct pci_vmsvga_state_s, chip);
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
    VPRINT("SVGA_REG_MEM_START register %u with the return of %u\n", s->index,
           ret);
    break;
  };
  case SVGA_REG_MEM_SIZE:
    // ret = 262144;
    ret = s->fifo_size;
    VPRINT("SVGA_REG_MEM_SIZE register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CONFIG_DONE:
    // ret = 0;
    ret = s->config;
    VPRINT("SVGA_REG_CONFIG_DONE register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_SYNC:
    // ret = 0;
    ret = s->sync;
    VPRINT("SVGA_REG_SYNC register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_BUSY:
    // ret = 0;
    ret = s->sync;
    VPRINT("SVGA_REG_BUSY register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_GUEST_ID:
    // ret = 0;
    ret = s->guest;
    VPRINT("SVGA_REG_GUEST_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_ID:
    // ret = 0;
    ret = s->cursor;
    VPRINT("SVGA_REG_CURSOR_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_X:
    // ret = 0;
    ret = s->fifo[SVGA_FIFO_CURSOR_X];
    VPRINT("SVGA_REG_CURSOR_X register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_Y:
    // ret = 0;
    ret = s->fifo[SVGA_FIFO_CURSOR_Y];
    VPRINT("SVGA_REG_CURSOR_Y register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_CURSOR_ON:
    // ret = 0;
    if ((s->fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_SHOW) ||
        (s->fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_RESTORE_TO_FB)) {
      ret = SVGA_CURSOR_ON_SHOW;
    } else {
      ret = SVGA_CURSOR_ON_HIDE;
    };
    VPRINT("SVGA_REG_CURSOR_ON register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_SCRATCH_SIZE:
    // ret = 64;
    ret = s->scratch_size;
    VPRINT("SVGA_REG_SCRATCH_SIZE register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_MEM_REGS:
    ret = 291;
    VPRINT("SVGA_REG_MEM_REGS register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_NUM_DISPLAYS:
    // ret = 10;
    ret = 1;
    VPRINT("SVGA_REG_NUM_DISPLAYS register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_PITCHLOCK:
    // ret = 0;
    if (s->pitchlock >= 1) {
      ret = s->pitchlock;
    } else {
      ret = (((s->new_depth) * (s->new_width)) / (8));
    };
    VPRINT("SVGA_REG_PITCHLOCK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_IRQMASK:
    // ret = 0;
    ret = s->irq_mask;
    VPRINT("SVGA_REG_IRQMASK register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    // ret = 0;
    ret = 1;
    VPRINT("SVGA_REG_NUM_GUEST_DISPLAYS register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_ID:
    // ret = 0;
    ret = s->display_id;
    VPRINT("SVGA_REG_DISPLAY_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    // ret = 0;
    ret = s->disp_prim;
    VPRINT("SVGA_REG_DISPLAY_IS_PRIMARY register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    // ret = 0;
    ret = s->disp_x;
    VPRINT("SVGA_REG_DISPLAY_POSITION_X register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    // ret = 0;
    ret = s->disp_y;
    VPRINT("SVGA_REG_DISPLAY_POSITION_Y register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    // ret = 0;
    if (s->new_width >= 1) {
      ret = s->new_width;
    } else {
      ret = 1024;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_DISPLAY_WIDTH register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    // ret = 0;
    if (s->new_height >= 1) {
      ret = s->new_height;
    } else {
      ret = 768;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_DISPLAY_HEIGHT register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_GMRS_MAX_PAGES:
    ret = 65536;
    VPRINT("SVGA_REG_GMRS_MAX_PAGES register %u with the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_GMR_ID:
    // ret = 0;
    ret = s->gmrid;
    VPRINT("SVGA_REG_GMR_ID register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_GMR_MAX_IDS:
    ret = 64;
    VPRINT("SVGA_REG_GMR_MAX_IDS register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
    ret = 4096;
    VPRINT("SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH register %u with "
           "the return "
           "of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_TRACES:
    // ret = 0;
    ret = s->traces;
    VPRINT("SVGA_REG_TRACES register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_COMMAND_LOW:
    // ret = 0;
    ret = s->cmd_low;
    VPRINT("SVGA_REG_COMMAND_LOW register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_COMMAND_HIGH:
    // ret = 0;
    ret = s->cmd_high;
    VPRINT("SVGA_REG_COMMAND_HIGH register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_DEV_CAP:
    ret = s->devcap_val;
    VPRINT("SVGA_REG_DEV_CAP register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_MEMORY_SIZE:
    ret = 1073741824;
    VPRINT("SVGA_REG_MEMORY_SIZE register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  case SVGA_REG_SCREENDMA:
    ret = 1;
    VPRINT("SVGA_REG_SCREENDMA register %u with the return of %u\n", s->index,
           ret);
    break;
  case SVGA_REG_FENCE:
    // ret = -99;
    ret = s->fifo[SVGA_FIFO_FENCE];
    VPRINT("SVGA_REG_FENCE register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_FIFO_CAPS:
    // ret = 1919;
    ret = s->fc;
    VPRINT("68 register %u with the return of %u\n", s->index, ret);
    break;
  case SVGA_REG_CURSOR_MAX_DIMENSION:
    ret = 2048;
    VPRINT("SVGA_REG_CURSOR_MAX_DIMENSION register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CURSOR_MAX_BYTE_SIZE:
    ret = 8388608;
    VPRINT("SVGA_REG_CURSOR_MAX_BYTE_SIZE register %u with the "
           "return of %u\n",
           s->index, ret);
    break;
  case SVGA_REG_CURSOR_MOBID:
    ret = -1;
    VPRINT("SVGA_REG_CURSOR_MOBID register %u with the return of "
           "%u\n",
           s->index, ret);
    break;
  default:
    ret = 0;
    VPRINT("default register %u with the return of %u\n", s->index, ret);
    break;
  };
  return ret;
};
static void vmsvga_value_write(void *opaque, uint32_t address, uint32_t value) {
  struct vmsvga_state_s *s = opaque;
  VPRINT("Unknown register %u with the value of %u\n", s->index, value);

  if (s->index >= SVGA_REG_PALETTE_MIN && s->index <= SVGA_REG_PALETTE_MAX) {
    uint32_t idx = s->index - SVGA_REG_PALETTE_MIN;
    s->svgapalettebase[idx] = value;
    VPRINT("SVGA_REG_PALETTE_%u write %u\n", idx, value);
    return;
  }

  switch (s->index) {
  case SVGA_REG_ID:
    s->svgaid = value;
    VPRINT("SVGA_REG_ID register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_FENCE_GOAL:
    s->fifo[SVGA_FIFO_FENCE_GOAL] = value;
    VPRINT("SVGA_REG_FENCE_GOAL register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_ENABLE:
    if ((value < 1) || (value & SVGA_REG_ENABLE_DISABLE) ||
        (value & SVGA_REG_ENABLE_HIDE)) {
      s->enable = 0;
      s->config = 0;
    } else {
      s->enable = value;
    };
    VPRINT("SVGA_REG_ENABLE register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_WIDTH:
    if (value >= 1) {
      s->new_width = value;
    } else {
      s->new_width = 1024;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_WIDTH register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_HEIGHT:
    if (value >= 1) {
      s->new_height = value;
    } else {
      s->new_height = 768;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_HEIGHT register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    if (value >= 1) {
      s->new_depth = value;
    } else {
      s->new_depth = 32;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_BITS_PER_PIXEL register %u with the value "
           "of %u\n",
           s->index, value);
    break;
  case SVGA_REG_CONFIG_DONE:
    if (value < 1) {
      s->enable = 0;
      s->config = 0;
    } else {
      s->config = value;
    };
    VPRINT("SVGA_REG_CONFIG_DONE register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_SYNC:
    // s->sync = value;
    VPRINT("SVGA_REG_SYNC register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_BUSY:
    // s->sync = value;
    VPRINT("SVGA_REG_BUSY register %u with the value of %u\n", s->index, value);
    break;
  case SVGA_REG_GUEST_ID:
    s->guest = value;
    VPRINT("SVGA_REG_GUEST_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_ID:
    s->cursor = value;
    VPRINT("SVGA_REG_CURSOR_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_X:
    s->fifo[SVGA_FIFO_CURSOR_X] = value;
    VPRINT("SVGA_REG_CURSOR_X register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_Y:
    s->fifo[SVGA_FIFO_CURSOR_Y] = value;
    VPRINT("SVGA_REG_CURSOR_Y register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_CURSOR_ON:
    s->fifo[SVGA_FIFO_CURSOR_ON] = value;
    VPRINT("SVGA_REG_CURSOR_ON register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_BYTES_PER_LINE:
    if (value >= 1) {
      // s->pitchlock = value;
    } else {
      // s->pitchlock = (((s->new_depth) * (s->new_width)) / (8));
    };
    VPRINT("SVGA_REG_BYTES_PER_LINE register %u with the value "
           "of %u\n",
           s->index, value);
    break;
  case SVGA_REG_PITCHLOCK:
    if (value >= 1) {
      // s->pitchlock = value;
    } else {
      // s->pitchlock = (((s->new_depth) * (s->new_width)) / (8));
    };
    VPRINT("SVGA_REG_PITCHLOCK register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_IRQMASK: {
    uint32_t offFifoMin = s->fifo[SVGA_FIFO_MIN];
    uint32_t irq_status = s->irq_status;
    s->irq_mask = value;
#ifndef RAISE_IRQ_OFF
    struct pci_vmsvga_state_s *pci_vmsvga =
        container_of(s, struct pci_vmsvga_state_s, chip);
    PCIDevice *pci_dev = PCI_DEVICE(pci_vmsvga);
#endif
    if ((value) & (SVGA_IRQFLAG_ANY_FENCE)) {
      VPRINT("irq_status |= SVGA_IRQFLAG_ANY_FENCE\n");
#ifndef ANY_FENCE_OFF
      irq_status |= SVGA_IRQFLAG_ANY_FENCE;
#endif
    } else if (((VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_FENCE_GOAL, offFifoMin))) &&
               ((value) & (SVGA_IRQFLAG_FENCE_GOAL))) {
      VPRINT("irq_status |= SVGA_IRQFLAG_FENCE_GOAL\n");
      irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
    };
    if ((irq_status) || ((value) & (SVGA_IRQFLAG_FIFO_PROGRESS))) {
      VPRINT("value || irq_status & SVGA_IRQFLAG_FIFO_PROGRESS\n");
      if ((value) & (SVGA_IRQFLAG_FIFO_PROGRESS)) {
        VPRINT("irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS\n");
        irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
      };
      if ((value) & (irq_status)) {
        VPRINT("REG: Pci_set_irq=1\n");
        s->irq_status = irq_status;
#ifndef RAISE_IRQ_OFF
        pci_set_irq(pci_dev, 1);
#endif
      };
    } else if ((s->irq_status) & (value)) {
      VPRINT("REG: Pci_set_irq=1\n");
#ifndef RAISE_IRQ_OFF
      pci_set_irq(pci_dev, 1);
#endif
    } else {
      VPRINT("REG: Pci_set_irq=0\n");
#ifndef RAISE_IRQ_OFF
      pci_set_irq(pci_dev, 0);
#endif
    };
    VPRINT("SVGA_REG_IRQMASK register %u with the value of %u\n", s->index,
           value);
    break;
  }
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    s->num_gd = value;
    VPRINT("SVGA_REG_NUM_GUEST_DISPLAYS register %u with the "
           "value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    s->disp_prim = value;
    VPRINT("SVGA_REG_DISPLAY_IS_PRIMARY register %u with the "
           "value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    s->disp_x = value;
    VPRINT("SVGA_REG_DISPLAY_POSITION_X register %u with the "
           "value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    s->disp_y = value;
    VPRINT("SVGA_REG_DISPLAY_POSITION_Y register %u with the "
           "value of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_ID:
    s->display_id = value;
    VPRINT("SVGA_REG_DISPLAY_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    if (value >= 1) {
      s->new_width = value;
    } else {
      s->new_width = 1024;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_DISPLAY_WIDTH register %u with the value of "
           "%u\n",
           s->index, value);
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    if (value >= 1) {
      s->new_height = value;
    } else {
      s->new_height = 768;
      s->enable = 0;
      s->config = 0;
    };
    VPRINT("SVGA_REG_DISPLAY_HEIGHT register %u with the value "
           "of %u\n",
           s->index, value);
    break;
  case SVGA_REG_TRACES:
    s->traces = value;
    VPRINT("SVGA_REG_TRACES register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_COMMAND_LOW:
    s->cmd_low = value;
    VPRINT("SVGA_REG_COMMAND_LOW register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_COMMAND_HIGH:
    s->cmd_high = value;
    VPRINT("SVGA_REG_COMMAND_HIGH register %u with the value of "
           "%u\n",
           s->index, value);
    break;
  case SVGA_REG_GMR_ID:
    s->gmrid = value;
    VPRINT("SVGA_REG_GMR_ID register %u with the value of %u\n", s->index,
           value);
    break;
  case SVGA_REG_GMR_DESCRIPTOR:
    s->gmrdesc = value;
    VPRINT("SVGA_REG_GMR_DESCRIPTOR register %u with the value "
           "of %u\n",
           s->index, value);
    break;
  case SVGA_REG_DEV_CAP:
    static uint32_t devcap[SVGA3D_DEVCAP_MAX] = {
        [SVGA3D_DEVCAP_3D] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_LIGHTS] = 0x00000008,
        [SVGA3D_DEVCAP_MAX_TEXTURES] = 0x00000008,
        [SVGA3D_DEVCAP_MAX_CLIP_PLANES] = 0x00000008,
        [SVGA3D_DEVCAP_VERTEX_SHADER_VERSION] = 0x00000007,
        [SVGA3D_DEVCAP_VERTEX_SHADER] = 0x00000001,
        [SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION] = 0x0000000d,
        [SVGA3D_DEVCAP_FRAGMENT_SHADER] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_RENDER_TARGETS] = 0x00000008,
        [SVGA3D_DEVCAP_S23E8_TEXTURES] = 0x00000001,
        [SVGA3D_DEVCAP_S10E5_TEXTURES] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND] = 0x00000004,
        [SVGA3D_DEVCAP_D16_BUFFER_FORMAT] = 0x00000001,
        [SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT] = 0x00000001,
        [SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT] = 0x00000001,
        [SVGA3D_DEVCAP_QUERY_TYPES] = 0x00000001,
        [SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_POINT_SIZE] = 0x000000bd,
        [SVGA3D_DEVCAP_MAX_SHADER_TEXTURES] = 0x00000014,
        //        [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00008000,
        [SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH] = 0x00002000,
        //        [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00008000,
        [SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT] = 0x00002000,
        [SVGA3D_DEVCAP_MAX_VOLUME_EXTENT] = 0x00004000,
        [SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT] = 0x00008000,
        [SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO] = 0x00008000,
        [SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY] = 0x00000010,
        [SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT] = 0x001fffff,
        [SVGA3D_DEVCAP_MAX_VERTEX_INDEX] = 0x000fffff,
        [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS] = 0x0000ffff,
        [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS] = 0x0000ffff,
        [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS] = 0x00000020,
        [SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS] = 0x00000020,
        [SVGA3D_DEVCAP_TEXTURE_OPS] = 0x03ffffff,
        [SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8] = 0x0018ec1f,
        [SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8] = 0x0018e11f,
        [SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10] = 0x0008601f,
        [SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5] = 0x0008601f,
        [SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5] = 0x0008611f,
        [SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4] = 0x0000611f,
        [SVGA3D_DEVCAP_SURFACEFMT_R5G6B5] = 0x0018ec1f,
        [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16] = 0x0000601f,
        [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8] = 0x00006007,
        [SVGA3D_DEVCAP_SURFACEFMT_ALPHA8] = 0x0000601f,
        [SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8] = 0x0000601f,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_D16] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_DXT1] = 0x0000e005,
        [SVGA3D_DEVCAP_SURFACEFMT_DXT2] = 0x0000e005,
        [SVGA3D_DEVCAP_SURFACEFMT_DXT3] = 0x0000e005,
        [SVGA3D_DEVCAP_SURFACEFMT_DXT4] = 0x0000e005,
        [SVGA3D_DEVCAP_SURFACEFMT_DXT5] = 0x0000e005,
        [SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8] = 0x00014005,
        [SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10] = 0x00014007,
        [SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8] = 0x00014007,
        [SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8] = 0x00014005,
        [SVGA3D_DEVCAP_SURFACEFMT_CxV8U8] = 0x00014001,
        [SVGA3D_DEVCAP_SURFACEFMT_R_S10E5] = 0x0080601f,
        [SVGA3D_DEVCAP_SURFACEFMT_R_S23E8] = 0x0080601f,
        [SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5] = 0x0080601f,
        [SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8] = 0x0080601f,
        [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5] = 0x0080601f,
        [SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8] = 0x0080601f,
        [SVGA3D_DEVCAP_MISSING62] = 0x00000000,
        [SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES] = 0x00000004,
        [SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS] = 0x00000008,
        [SVGA3D_DEVCAP_SURFACEFMT_V16U16] = 0x00014007,
        [SVGA3D_DEVCAP_SURFACEFMT_G16R16] = 0x0000601f,
        [SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16] = 0x0000601f,
        [SVGA3D_DEVCAP_SURFACEFMT_UYVY] = 0x01246000,
        [SVGA3D_DEVCAP_SURFACEFMT_YUY2] = 0x01246000,
        [SVGA3D_DEVCAP_DEAD4] = 0x00000000,
        [SVGA3D_DEVCAP_DEAD5] = 0x00000000,
        [SVGA3D_DEVCAP_DEAD7] = 0x00000000,
        [SVGA3D_DEVCAP_DEAD6] = 0x00000000,
        [SVGA3D_DEVCAP_AUTOGENMIPMAPS] = 0x00000001,
        [SVGA3D_DEVCAP_SURFACEFMT_NV12] = 0x01246000,
        [SVGA3D_DEVCAP_SURFACEFMT_AYUV] = 0x00000000,
        [SVGA3D_DEVCAP_MAX_CONTEXT_IDS] = 0x00000100,
        [SVGA3D_DEVCAP_MAX_SURFACE_IDS] = 0x00008000,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_DF16] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_DF24] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT] = 0x000040c5,
        [SVGA3D_DEVCAP_SURFACEFMT_ATI1] = 0x00006005,
        [SVGA3D_DEVCAP_SURFACEFMT_ATI2] = 0x00006005,
        [SVGA3D_DEVCAP_DEAD1] = 0x00000000,
        [SVGA3D_DEVCAP_DEAD8] = 0x00000000,
        [SVGA3D_DEVCAP_DEAD9] = 0x00000000,
        [SVGA3D_DEVCAP_LINE_AA] = 0x00000001,
        [SVGA3D_DEVCAP_LINE_STIPPLE] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_LINE_WIDTH] = 0x0000000a,
        [SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH] = 0x0000000a,
        [SVGA3D_DEVCAP_SURFACEFMT_YV12] = 0x01246000,
        [SVGA3D_DEVCAP_DEAD3] = 0x00000000,
        [SVGA3D_DEVCAP_TS_COLOR_KEY] = 0x00000001,
        [SVGA3D_DEVCAP_DEAD2] = 0x00000000,
        [SVGA3D_DEVCAP_DXCONTEXT] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE] = 0x00000000,
        [SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS] = 0x00000010,
        [SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS] = 0x0000000f,
        [SVGA3D_DEVCAP_DX_PROVOKING_VERTEX] = 0x00000001,
        [SVGA3D_DEVCAP_DXFMT_X8R8G8B8] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_A8R8G8B8] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R5G6B5] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_X1R5G5B5] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_A1R5G5B5] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_A4R4G4B4] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_Z_D32] = 0x00000009,
        [SVGA3D_DEVCAP_DXFMT_Z_D16] = 0x0000026b,
        [SVGA3D_DEVCAP_DXFMT_Z_D24S8] = 0x0000026b,
        [SVGA3D_DEVCAP_DXFMT_Z_D15S1] = 0x0000000b,
        [SVGA3D_DEVCAP_DXFMT_LUMINANCE8] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_LUMINANCE16] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_DXT1] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_DXT2] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_DXT3] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_DXT4] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_DXT5] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_BUMPU8V8] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5] = 0x00000000,
        [SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1] = 0x00000000,
        [SVGA3D_DEVCAP_DXFMT_ARGB_S10E5] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_ARGB_S23E8] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_A2R10G10B10] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_V8U8] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_Q8W8V8U8] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_CxV8U8] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_X8L8V8U8] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_A2W10V10U10] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_ALPHA8] = 0x000000f7,
        [SVGA3D_DEVCAP_DXFMT_R_S10E5] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R_S23E8] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_RG_S10E5] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_RG_S23E8] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_BUFFER] = 0x00000001,
        [SVGA3D_DEVCAP_DXFMT_Z_D24X8] = 0x0000026b,
        [SVGA3D_DEVCAP_DXFMT_V16U16] = 0x000001e3,
        [SVGA3D_DEVCAP_DXFMT_G16R16] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_A16B16G16R16] = 0x000001f7,
        [SVGA3D_DEVCAP_DXFMT_UYVY] = 0x00000001,
        [SVGA3D_DEVCAP_DXFMT_YUY2] = 0x00000041,
        [SVGA3D_DEVCAP_DXFMT_NV12] = 0x00000041,
        [SVGA3D_DEVCAP_FORMAT_DEAD2] = 0x00000000,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT] = 0x000001e3,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT] = 0x000001e3,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT] = 0x000001e3,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R32G32_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32G32_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS] = 0x00000261,
        [SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT] = 0x00000269,
        [SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R16G16_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R16G16_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_D32_FLOAT] = 0x00000269,
        [SVGA3D_DEVCAP_DXFMT_R32_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R32_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS] = 0x00000261,
        [SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT] = 0x00000269,
        [SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_X24_G8_UINT] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R8G8_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R8G8_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R16_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R16_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R16_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R8_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_R8_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8_UINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_R8_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8_SINT] = 0x000003e7,
        [SVGA3D_DEVCAP_DXFMT_P8] = 0x00000001,
        [SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_ATI1] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_BC4_SNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_ATI2] = 0x00000063,
        [SVGA3D_DEVCAP_DXFMT_BC5_SNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM] = 0x00000045,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS] = 0x000002e1,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_Z_DF16] = 0x0000006b,
        [SVGA3D_DEVCAP_DXFMT_Z_DF24] = 0x0000006b,
        [SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT] = 0x0000006b,
        [SVGA3D_DEVCAP_DXFMT_YV12] = 0x00000001,
        [SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16G16_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R32_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R8G8_SNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_R16_FLOAT] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_D16_UNORM] = 0x00000269,
        [SVGA3D_DEVCAP_DXFMT_A8_UNORM] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_BC1_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC2_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC3_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM] = 0x000002f7,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM] = 0x000003f7,
        [SVGA3D_DEVCAP_DXFMT_BC4_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC5_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_SM41] = 0x00000001,
        [SVGA3D_DEVCAP_MULTISAMPLE_2X] = 0x00000001,
        [SVGA3D_DEVCAP_MULTISAMPLE_4X] = 0x00000001,
        [SVGA3D_DEVCAP_MS_FULL_QUALITY] = 0x00000001,
        [SVGA3D_DEVCAP_LOGICOPS] = 0x00000001,
        [SVGA3D_DEVCAP_LOGIC_BLENDOPS] = 0x00000001,
        [SVGA3D_DEVCAP_DEAD12] = 0x00000000,
        [SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_BC6H_UF16] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC6H_SF16] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS] = 0x000000e1,
        [SVGA3D_DEVCAP_DXFMT_BC7_UNORM] = 0x000000e3,
        [SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB] = 0x000000e3,
        [SVGA3D_DEVCAP_DEAD13] = 0x00000000,
        [SVGA3D_DEVCAP_SM5] = 0x00000001,
        [SVGA3D_DEVCAP_MULTISAMPLE_8X] = 0x00000001,
        [SVGA3D_DEVCAP_MAX_FORCED_SAMPLE_COUNT] = 0x00000010,
        [SVGA3D_DEVCAP_GL43] = 0x00000001};
    s->devcap_val = value >= SVGA3D_DEVCAP_MAX ? 0 : devcap[value];
    VPRINT("SVGA_REG_DEV_CAP register %u with the value of %u\n", s->index,
           value);
    break;
  default:
    VPRINT("default register %u with the value of %u\n", s->index, value);
  };
};
static uint32_t vmsvga_irqstatus_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_irqstatus_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_irqstatus_read %u %u\n", address, s->irq_status);
  return s->irq_status;
};
static void vmsvga_irqstatus_write(void *opaque, uint32_t address,
                                   uint32_t data) {
  VPRINT("vmsvga_irqstatus_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  s->irq_status &= ~data;
  VPRINT("vmsvga_irqstatus_write %u %u\n", address, data);
#ifndef RAISE_IRQ_OFF
  struct pci_vmsvga_state_s *pci_vmsvga =
      container_of(s, struct pci_vmsvga_state_s, chip);
  PCIDevice *pci_dev = PCI_DEVICE(pci_vmsvga);
#endif
  if (!((s->irq_status) & (s->irq_mask))) {
    VPRINT("PORT: Pci_set_irq=0\n");
#ifndef RAISE_IRQ_OFF
    pci_set_irq(pci_dev, 0);
#endif
  };
};
static uint32_t vmsvga_bios_read(void *opaque, uint32_t address) {
  VPRINT("vmsvga_bios_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  VPRINT("vmsvga_bios_read %u %u\n", address, s->bios);
  return s->bios;
};
static void vmsvga_bios_write(void *opaque, uint32_t address, uint32_t data) {
  VPRINT("vmsvga_bios_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  s->bios = data;
  VPRINT("vmsvga_bios_write %u %u\n", address, data);
};
static void vmsvga_update_display(void *opaque) {
  // VPRINT("vmsvga_update_display was just executed\n");
  struct vmsvga_state_s *s = opaque;
  if ((s->enable >= 1 || s->config >= 1) &&
      (s->new_width >= 1 && s->new_height >= 1 && s->new_depth >= 1)) {
    vmsvga_check_size(s);
    vmsvga_fifo_run(s);
    cursor_update_from_fifo(s);
  } else {
    s->vcs = s->vga;
    s->vga.hw_ops->gfx_update(&s->vcs);
  };
};
static void vmsvga_reset(DeviceState *dev) {
  VPRINT("vmsvga_reset was just executed\n");
  struct pci_vmsvga_state_s *pci = VMWARE_SVGA(dev);
  struct vmsvga_state_s *s = &pci->chip;
  s->enable = 0;
  s->config = 0;
};
static void vmsvga_invalidate_display(void *opaque) {
  VPRINT("vmsvga_invalidate_display was just executed\n");
};
static void vmsvga_text_update(void *opaque, console_ch_t *chardata) {
  VPRINT("vmsvga_text_update was just executed\n");
  struct vmsvga_state_s *s = opaque;
  if (s->vga.hw_ops->text_update) {
    s->vga.hw_ops->text_update(&s->vga, chardata);
  };
};
static int vmsvga_post_load(void *opaque, int version_id) {
  VPRINT("vmsvga_post_load was just executed\n");
  return 0;
};
static VMStateDescription vmstate_vmware_vga_internal = {
    .name = "vmware_vga_internal",
    .version_id = 1,
    .minimum_version_id = 0,
    .post_load = vmsvga_post_load,
    .fields = (const VMStateField[]){
        VMSTATE_UINT32_ARRAY(svgapalettebase, struct vmsvga_state_s,
                             SVGA_PALETTE_SIZE),
        VMSTATE_UINT32(enable, struct vmsvga_state_s),
        VMSTATE_UINT32(config, struct vmsvga_state_s),
        VMSTATE_UINT32(index, struct vmsvga_state_s),
        VMSTATE_UINT32(scratch_size, struct vmsvga_state_s),
        VMSTATE_UINT32(new_width, struct vmsvga_state_s),
        VMSTATE_UINT32(new_height, struct vmsvga_state_s),
        VMSTATE_UINT32(new_depth, struct vmsvga_state_s),
        VMSTATE_UINT32(num_gd, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_prim, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_x, struct vmsvga_state_s),
        VMSTATE_UINT32(disp_y, struct vmsvga_state_s),
        VMSTATE_UINT32(devcap_val, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrdesc, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrid, struct vmsvga_state_s),
        VMSTATE_UINT32(gmrpage, struct vmsvga_state_s),
        VMSTATE_UINT32(traces, struct vmsvga_state_s),
        VMSTATE_UINT32(cmd_low, struct vmsvga_state_s),
        VMSTATE_UINT32(cmd_high, struct vmsvga_state_s),
        VMSTATE_UINT32(guest, struct vmsvga_state_s),
        VMSTATE_UINT32(svgaid, struct vmsvga_state_s),
        VMSTATE_UINT32(thread, struct vmsvga_state_s),
        VMSTATE_UINT32(sync, struct vmsvga_state_s),
        VMSTATE_UINT32(bios, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_size, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_min, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_max, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_next, struct vmsvga_state_s),
        VMSTATE_UINT32(fifo_stop, struct vmsvga_state_s),
        VMSTATE_UINT32(irq_mask, struct vmsvga_state_s),
        VMSTATE_UINT32(irq_status, struct vmsvga_state_s),
        VMSTATE_UINT32(display_id, struct vmsvga_state_s),
        VMSTATE_UINT32(pitchlock, struct vmsvga_state_s),
        VMSTATE_UINT32(cursor, struct vmsvga_state_s),
        VMSTATE_UINT32(fc, struct vmsvga_state_s),
        VMSTATE_UINT32(ff, struct vmsvga_state_s),
        VMSTATE_END_OF_LIST()}};
static VMStateDescription vmstate_vmware_vga = {
    .name = "vmware_vga",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (const VMStateField[]){
        VMSTATE_PCI_DEVICE(parent_obj, struct pci_vmsvga_state_s),
        VMSTATE_STRUCT(chip, struct pci_vmsvga_state_s, 0,
                       vmstate_vmware_vga_internal, struct vmsvga_state_s),
        VMSTATE_END_OF_LIST()}};
static GraphicHwOps vmsvga_ops = {
    .invalidate = vmsvga_invalidate_display,
    .gfx_update = vmsvga_update_display,
    .text_update = vmsvga_text_update,
};
static void vmsvga_init(DeviceState *dev, struct vmsvga_state_s *s,
                        MemoryRegion *address_space, MemoryRegion *io) {
  VPRINT("vmsvga_init was just executed\n");
  s->scratch_size = 64;
  s->scratch = g_malloc(s->scratch_size * 4);
  s->vga.con = graphic_console_init(dev, 0, &vmsvga_ops, s);
  s->fifo_size = 262144;
  memory_region_init_ram(&s->fifo_ram, NULL, "vmsvga.fifo", s->fifo_size,
                         &error_fatal);
  s->fifo = (uint32_t *)memory_region_get_ram_ptr(&s->fifo_ram);
  vga_common_init(&s->vga, OBJECT(dev), &error_fatal);
  vga_init(&s->vga, OBJECT(dev), address_space, io, true);
#ifdef QEMU_V9_2_0
  vmstate_register_any(NULL, &vmstate_vga_common, &s->vga);
#else
  vmstate_register(NULL, 0, &vmstate_vga_common, &s->vga);
#endif
  if (s->thread < 1) {
    s->thread++;
    s->fifo[SVGA_FIFO_CURSOR_ON] = SVGA_CURSOR_ON_SHOW;
    s->new_width = 1024;
    s->new_height = 768;
    s->new_depth = 32;
    pthread_t threads[1];
    s->fc = 0xffffffff;
    s->ff = 0xffffffff;
#ifndef EXPCAPS
    // Enable FIFO capabilities needed for Windows 7 Aero:
    // s->ff -= SVGA_FIFO_FLAG_ACCELFRONT;     // Enable for Windows 7 Aero  
    // s->fc -= SVGA_FIFO_CAP_SCREEN_OBJECT;   // Enable for Windows 7 Aero
    // s->fc -= SVGA_FIFO_CAP_SCREEN_OBJECT_2; // Enable for Windows 7 Aero
#endif
    pthread_create(threads, NULL, vmsvga_loop, (void *)s);
  };
};
static uint64_t vmsvga_io_read(void *opaque, hwaddr addr, unsigned size) {
  VPRINT("vmsvga_io_read was just executed\n");
  struct vmsvga_state_s *s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    VPRINT("vmsvga_io_read SVGA_INDEX_PORT\n");
    return vmsvga_index_read(s, addr);
  case 1 * SVGA_VALUE_PORT:
    VPRINT("vmsvga_io_read SVGA_VALUE_PORT\n");
    return vmsvga_value_read(s, addr);
  case 1 * SVGA_BIOS_PORT:
    VPRINT("vmsvga_io_read SVGA_BIOS_PORT\n");
    return vmsvga_bios_read(s, addr);
  case 1 * SVGA_IRQSTATUS_PORT:
    VPRINT("vmsvga_io_read SVGA_IRQSTATUS_PORT\n");
    return vmsvga_irqstatus_read(s, addr);
  default:
    VPRINT("vmsvga_io_read default\n");
    return 0;
  };
};
static void vmsvga_io_write(void *opaque, hwaddr addr, uint64_t data,
                            unsigned size) {
  VPRINT("vmsvga_io_write was just executed\n");
  struct vmsvga_state_s *s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    VPRINT("vmsvga_io_write SVGA_INDEX_PORT\n");
    vmsvga_index_write(s, addr, data);
    break;
  case 1 * SVGA_VALUE_PORT:
    VPRINT("vmsvga_io_write SVGA_VALUE_PORT\n");
    vmsvga_value_write(s, addr, data);
    break;
  case 1 * SVGA_BIOS_PORT:
    VPRINT("vmsvga_io_write SVGA_BIOS_PORT\n");
    vmsvga_bios_write(s, addr, data);
    break;
  case 1 * SVGA_IRQSTATUS_PORT:
    VPRINT("vmsvga_io_write SVGA_IRQSTATUS_PORT\n");
    vmsvga_irqstatus_write(s, addr, data);
    break;
  default:
    VPRINT("vmsvga_io_write default\n");
    break;
  };
};
static MemoryRegionOps vmsvga_io_ops = {
    .read = vmsvga_io_read,
    .write = vmsvga_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid =
        {
            .min_access_size = 4,
            .max_access_size = 4,
            .unaligned = true,
        },
    .impl =
        {
            .unaligned = true,
        },
};
static void pci_vmsvga_realize(PCIDevice *dev, Error **errp) {
  VPRINT("pci_vmsvga_realize was just executed\n");
  struct pci_vmsvga_state_s *s = VMWARE_SVGA(dev);
  dev->config[PCI_INTERRUPT_PIN] = 1;
  dev->config[PCI_LATENCY_TIMER] = 64;
  dev->config[PCI_CACHE_LINE_SIZE] = 32;
  memory_region_init_io(&s->io_bar, OBJECT(dev), &vmsvga_io_ops, &s->chip,
                        "vmsvga-io", 0x10);
  memory_region_set_flush_coalesced(&s->io_bar);
  pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &s->io_bar);
  vmsvga_init(DEVICE(dev), &s->chip, pci_address_space(dev),
              pci_address_space_io(dev));
  pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->chip.vga.vram);
  pci_register_bar(dev, 2, PCI_BASE_ADDRESS_MEM_TYPE_32, &s->chip.fifo_ram);
};
static Property vga_vmware_properties[] = {
    DEFINE_PROP_UINT32("vgamem_mb", struct pci_vmsvga_state_s,
                       chip.vga.vram_size_mb, 128),
    DEFINE_PROP_BOOL("global-vmstate", struct pci_vmsvga_state_s,
                     chip.vga.global_vmstate, true),
    DEFINE_PROP_END_OF_LIST(),
};
static void vmsvga_class_init(ObjectClass *klass, void *data) {
  VPRINT("vmsvga_class_init was just executed\n");
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
#ifdef QEMU_V9_2_0
  device_class_set_legacy_reset(dc, vmsvga_reset);
#else
  dc->reset = vmsvga_reset;
#endif
  dc->vmsd = &vmstate_vmware_vga;
  device_class_set_props(dc, vga_vmware_properties);
  dc->hotpluggable = false;
  set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
};
static TypeInfo vmsvga_info = {
    .name = "vmware-svga",
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct pci_vmsvga_state_s),
    .class_init = vmsvga_class_init,
    .interfaces =
        (InterfaceInfo[]){
            {INTERFACE_CONVENTIONAL_PCI_DEVICE},
            {},
        },
};
static void vmsvga_register_types(void) {
  VPRINT("vmsvga_register_types was just executed\n");
  type_register_static(&vmsvga_info);
};
type_init(vmsvga_register_types)
