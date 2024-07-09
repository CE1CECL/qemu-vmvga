 /*
  
  QEMU VMware Super Video Graphics Array 2 [SVGA-II]
  
  Copyright (c) 2007 Andrzej Zaborowski <balrog@zabor.org>
  
  Copyright (c) 2023-2024 Christopher Eric Lentocha <christopherericlentocha@gmail.com>
  
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
//#define VERBOSE
#include <pthread.h>
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
#include "vga_int.h"
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
#define SVGA_PIXMAP_SIZE(w, h, bpp)(((((w) * (bpp))) >> 5) * (h))
#define SVGA_CMD_RECT_FILL 2
#define SVGA_CMD_DISPLAY_CURSOR 20
#define SVGA_CMD_MOVE_CURSOR 21
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
#define SVGA_REG_MSHINT 81
#define SVGA_REG_MAX_PRIMARY_MEM 50
struct vmsvga_state_s {
  VGACommonState vga;
  int enable;
  int config;
  struct {
    uint32_t id;
    uint32_t x;
    uint32_t y;
    uint32_t on;
  }
  cursor;
  uint32_t last_fifo_cursor_count;
  int index;
  int scratch_size;
  uint32_t * scratch;
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
  uint32_t tracez;
  uint32_t cmd_low;
  uint32_t cmd_high;
  uint32_t guest;
  uint32_t svgaid;
  uint32_t thread;
  uint32_t fg;
  uint32_t pcisetirq;
  uint32_t pcisetirq0;
  uint32_t pcisetirq1;
  uint32_t sync1;
  uint32_t sync2;
  uint32_t sync3;
  uint32_t sync4;
  uint32_t sync5;
  uint32_t sync6;
  uint32_t sync7;
  uint32_t sync8;
  uint32_t sync9;
  uint32_t sync0;
  uint32_t sync;
  uint32_t bios;
  int syncing;
  MemoryRegion fifo_ram;
  unsigned int fifo_size;
  uint32_t * fifo;
  uint32_t fifo_min;
  uint32_t fifo_max;
  uint32_t fifo_next;
  uint32_t fifo_stop;
  struct vmsvga_rect_s {
    int x, y, w, h;
  }
  redraw_fifo[512];
  int redraw_fifo_last;
  uint32_t irq_mask;
  uint32_t irq_status;
  uint32_t display_id;
  uint32_t pitchlock;
  uint32_t svgabasea;
  uint32_t svgabaseb;
};
DECLARE_INSTANCE_CHECKER(struct pci_vmsvga_state_s, VMWARE_SVGA, "vmware-svga")
struct pci_vmsvga_state_s {
  PCIDevice parent_obj;
  struct vmsvga_state_s chip;
  MemoryRegion io_bar;
};
static void cursor_update_from_fifo(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: cursor_update_from_fifo was just executed\n");
  #endif
  uint32_t fifo_cursor_count;
  if (s -> config != 1 || s -> enable != 1) {
    s -> sync3--;
    return;
  }
  fifo_cursor_count = s -> fifo[SVGA_FIFO_CURSOR_COUNT];
  if (fifo_cursor_count == s -> last_fifo_cursor_count) {
    s -> sync3--;
    return;
  }
  s -> last_fifo_cursor_count = fifo_cursor_count;
  if (s -> fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_HIDE) {
    s -> cursor.on = SVGA_CURSOR_ON_HIDE;
  } else {
    s -> cursor.on = SVGA_CURSOR_ON_SHOW;
  }
  s -> cursor.x = s -> fifo[SVGA_FIFO_CURSOR_X];
  s -> cursor.y = s -> fifo[SVGA_FIFO_CURSOR_Y];
  dpy_mouse_set(s -> vga.con, s -> cursor.x, s -> cursor.y, s -> cursor.on);
  s -> sync3--;
}
static inline bool vmsvga_verify_rect(DisplaySurface * surface,
  const char * name,
    int x, int y, int w, int h) {
  #ifdef VERBOSE
  //	printf("vmvga: vmsvga_verify_rect was just executed\n");
  #endif
  if (x < 1) {
    return false;
  }
  if (x > SVGA_MAX_WIDTH) {
    return false;
  }
  if (w < 1) {
    return false;
  }
  if (w > SVGA_MAX_WIDTH) {
    return false;
  }
  if (x + w > surface_width(surface)) {
    return false;
  }
  if (y < 1) {
    return false;
  }
  if (y > SVGA_MAX_WIDTH) {
    return false;
  }
  if (h < 1) {
    return false;
  }
  if (h > SVGA_MAX_HEIGHT) {
    return false;
  }
  if (y + h > surface_height(surface)) {
    return false;
  }
  return true;
}
static inline void vmsvga_update_rect(struct vmsvga_state_s * s,
  int x, int y, int w, int h) {
  #ifdef VERBOSE
  //	printf("vmvga: vmsvga_update_rect was just executed\n");
  #endif
  DisplaySurface * surface = qemu_console_surface(s -> vga.con);
  int line;
  int bypl;
  int width;
  int start;
  uint8_t * src;
  uint8_t * dst;
  if (!vmsvga_verify_rect(surface, __func__, x, y, w, h)) {
    x = 0;
    y = 0;
    w = s -> new_width;
    h = s -> new_height;
  }
  bypl = surface_stride(surface);
  width = surface_bytes_per_pixel(surface) * w;
  start = surface_bytes_per_pixel(surface) * x + bypl * y;
  src = s -> vga.vram_ptr + start;
  dst = surface_data(surface) + start;
  for (line = h; line > 0; line--, src += bypl, dst += bypl) {
    memcpy(dst, src, width);
  }
  dpy_gfx_update(s -> vga.con, x, y, w, h);
}
static inline int vmsvga_copy_rect(struct vmsvga_state_s * s,
  int x0, int y0, int x1, int y1, int w, int h) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_copy_rect was just executed\n");
  #endif
  DisplaySurface * surface = qemu_console_surface(s -> vga.con);
  uint8_t * vram = s -> vga.vram_ptr;
  int bypl = surface_stride(surface);
  int bypp = surface_bytes_per_pixel(surface);
  int width = bypp * w;
  int line = h;
  uint8_t * ptr[2];
  if (!vmsvga_verify_rect(surface, "vmsvga_copy_rect/src", x0, y0, w, h)) {
    return 1;
  }
  if (!vmsvga_verify_rect(surface, "vmsvga_copy_rect/dst", x1, y1, w, h)) {
    return 1;
  }
  if (y1 > y0) {
    ptr[0] = vram + bypp * x0 + bypl * (y0 + h - 1);
    ptr[1] = vram + bypp * x1 + bypl * (y1 + h - 1);
    for (; line > 0; line--, ptr[0] -= bypl, ptr[1] -= bypl) {
      memmove(ptr[1], ptr[0], width);
    }
  } else {
    ptr[0] = vram + bypp * x0 + bypl * y0;
    ptr[1] = vram + bypp * x1 + bypl * y1;
    for (; line > 0; line--, ptr[0] += bypl, ptr[1] += bypl) {
      memmove(ptr[1], ptr[0], width);
    }
  }
  vmsvga_update_rect(s, x1, y1, w, h);
  return 0;
}
static inline int vmsvga_fill_rect(struct vmsvga_state_s * s,
  uint32_t c, int x, int y, int w, int h) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_fill_rect was just executed\n");
  #endif
  DisplaySurface * surface = qemu_console_surface(s -> vga.con);
  int bypl = surface_stride(surface);
  int width = surface_bytes_per_pixel(surface) * w;
  int line = h;
  int column;
  uint8_t * fst;
  uint8_t * dst;
  uint8_t * src;
  uint8_t col[4];
  if (!vmsvga_verify_rect(surface, __func__, x, y, w, h)) {
    return 1;
  }
  col[0] = c;
  col[1] = c >> 8;
  col[2] = c >> 16;
  col[3] = c >> 24;
  fst = s -> vga.vram_ptr + surface_bytes_per_pixel(surface) * x + bypl * y;
  if (line--) {
    dst = fst;
    src = col;
    for (column = width; column > 0; column--) {
      *(dst++) = * (src++);
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
  uint32_t id;
  uint32_t hot_x;
  uint32_t hot_y;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_mask[4096];
  uint32_t xor_mask[4096];
};
static inline void vmsvga_cursor_define(struct vmsvga_state_s * s,
  struct vmsvga_cursor_definition_s * c) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_cursor_define was just executed\n");
  #endif
  QEMUCursor * qc;
  qc = cursor_alloc(c -> width, c -> height);
  if (qc != NULL) {
    qc -> hot_x = c -> hot_x;
    qc -> hot_y = c -> hot_y;
    if (c -> xor_mask_bpp != 1 && c -> and_mask_bpp != 1) {
      uint32_t i = 0;
      uint32_t pixels = ((c -> width) * (c -> height));
      for (i = 0; i < pixels; i++) {
        qc -> data[i] = ((c -> xor_mask[i]) + (c -> and_mask[i]));
      }
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void * ) c -> xor_mask, 1, (void * ) c -> and_mask);
    }
    #ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_mono");
    printf("vmvga: vmsvga_cursor_define | xor_mask == %d : and_mask == %d\n", * c -> xor_mask, * c -> and_mask);
    #endif
    dpy_cursor_define(s -> vga.con, qc);
    cursor_put(qc);
  }
}
static inline void vmsvga_rgba_cursor_define(struct vmsvga_state_s * s,
  struct vmsvga_cursor_definition_s * c) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_rgba_cursor_define was just executed\n");
  #endif
  QEMUCursor * qc;
  qc = cursor_alloc(c -> width, c -> height);
  if (qc != NULL) {
    qc -> hot_x = c -> hot_x;
    qc -> hot_y = c -> hot_y;
    if (c -> xor_mask_bpp != 1 && c -> and_mask_bpp != 1) {
      uint32_t i = 0;
      uint32_t pixels = ((c -> width) * (c -> height));
      for (i = 0; i < pixels; i++) {
        qc -> data[i] = ((c -> xor_mask[i]) + (c -> and_mask[i]));
      }
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void * ) c -> xor_mask, 1, (void * ) c -> and_mask);
    }
    #ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_rgba");
    printf("vmvga: vmsvga_rgba_cursor_define | xor_mask == %d : and_mask == %d\n", * c -> xor_mask, * c -> and_mask);
    #endif
    dpy_cursor_define(s -> vga.con, qc);
    cursor_put(qc);
  }
}
static inline int vmsvga_fifo_length(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_fifo_length was just executed\n");
  #endif
  int num;
  if (s -> config != 1 || s -> enable != 1) {
    return 0;
  }
  s -> fifo_min = le32_to_cpu(s -> fifo[SVGA_FIFO_MIN]);
  s -> fifo_max = le32_to_cpu(s -> fifo[SVGA_FIFO_MAX]);
  s -> fifo_next = le32_to_cpu(s -> fifo[SVGA_FIFO_NEXT_CMD]);
  s -> fifo_stop = le32_to_cpu(s -> fifo[SVGA_FIFO_STOP]);
  if ((s -> fifo_min | s -> fifo_max | s -> fifo_next | s -> fifo_stop) & 3) {
    return 0;
  }
  if (s -> fifo_min < sizeof(uint32_t) * 4) {
    return 0;
  }
  if (s -> fifo_max > s -> fifo_size ||
    s -> fifo_min >= s -> fifo_size ||
    s -> fifo_stop >= s -> fifo_size ||
    s -> fifo_next >= s -> fifo_size) {
    return 0;
  }
  if (s -> fifo_max < s -> fifo_min + 10 * KiB) {
    return 0;
  }
  num = s -> fifo_next - s -> fifo_stop;
  if (num < 0) {
    num += s -> fifo_max - s -> fifo_min;
  }
  return num >> 2;
}
static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_fifo_read_raw was just executed\n");
  #endif
  uint32_t cmd = s -> fifo[s -> fifo_stop >> 2];
  s -> fifo_stop += 4;
  if (s -> fifo_stop >= s -> fifo_max) {
    s -> fifo_stop = s -> fifo_min;
  }
  s -> fifo[SVGA_FIFO_STOP] = cpu_to_le32(s -> fifo_stop);
  return cmd;
}
static inline uint32_t vmsvga_fifo_read(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_fifo_read was just executed\n");
  #endif
  return le32_to_cpu(vmsvga_fifo_read_raw(s));
}
static void vmsvga_fifo_run(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_fifo_run was just executed\n");
  #endif
  #ifdef VERBOSE
  int UnknownCommandA;
  int UnknownCommandB;
  int UnknownCommandC;
  int UnknownCommandD;
  int UnknownCommandE;
  int UnknownCommandF;
  int UnknownCommandG;
  int UnknownCommandH;
  int UnknownCommandI;
  int UnknownCommandJ;
  int UnknownCommandK;
  int UnknownCommandL;
  int UnknownCommandM;
  int UnknownCommandN;
  int UnknownCommandO;
  int UnknownCommandP;
  int UnknownCommandQ;
  int UnknownCommandR;
  int UnknownCommandS;
  int UnknownCommandT;
  int UnknownCommandU;
  int UnknownCommandV;
  int UnknownCommandW;
  int UnknownCommandX;
  int UnknownCommandY;
  int UnknownCommandZ;
  int UnknownCommandAA;
  int UnknownCommandAB;
  int UnknownCommandAC;
  int UnknownCommandAD;
  int UnknownCommandAE;
  int UnknownCommandAF;
  int UnknownCommandAG;
  int UnknownCommandAH;
  int UnknownCommandAI;
  int UnknownCommandAJ;
  int UnknownCommandAK;
  int UnknownCommandAL;
  int UnknownCommandAM;
  int UnknownCommandAN;
  int UnknownCommandAO;
  int UnknownCommandAP;
  int UnknownCommandAQ;
  int UnknownCommandAR;
  int UnknownCommandAS;
  int UnknownCommandAT;
  int UnknownCommandAU;
  int UnknownCommandAV;
  int UnknownCommandAW;
  int UnknownCommandAX;
  int UnknownCommandAY;
  int UnknownCommandAZ;
  int UnknownCommandBA;
  int UnknownCommandBB;
  int UnknownCommandBC;
  int UnknownCommandBD;
  int z, gmrIdCMD, offsetPages;
  #endif
  uint32_t cmd;
  int args, len, maxloop = 1024;
  int i, x, y, dx, dy, width, height;
  struct vmsvga_cursor_definition_s cursor;
  uint32_t cmd_start;
  uint32_t fence_arg;
  uint32_t flags, num_pages;
  len = vmsvga_fifo_length(s);
  while (len > 0 && --maxloop > 0) {
    cmd_start = s -> fifo_stop;
    #ifdef VERBOSE
    printf("%s: Unknown command in SVGA command FIFO\n", __func__);
    #endif
    switch (cmd = vmsvga_fifo_read(s)) {
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
      printf("%s: SVGA_CMD_UPDATE command in SVGA command FIFO %d %d %d %d \n", __func__, x, y, width, height);
      #endif
      break;
    case SVGA_CMD_UPDATE_VERBOSE:
      len -= 6;
      if (len < 0) {
        goto rewind;
      }
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      #ifdef VERBOSE
      z = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      #endif
      vmsvga_update_rect(s, x, y, width, height);
      args = 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_UPDATE_VERBOSE command in SVGA command FIFO %d %d %d %d %d\n", __func__, x, y, width, height, z);
      #endif
      break;
    case SVGA_CMD_RECT_FILL:
      len -= 6;
      if (len < 0) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAQ = vmsvga_fifo_read(s);
      UnknownCommandAR = vmsvga_fifo_read(s);
      UnknownCommandAS = vmsvga_fifo_read(s);
      UnknownCommandAT = vmsvga_fifo_read(s);
      UnknownCommandAU = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_RECT_FILL command in SVGA command FIFO %d %d %d %d %d \n", __func__, UnknownCommandAQ, UnknownCommandAR, UnknownCommandAS, UnknownCommandAT, UnknownCommandAU);
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
      printf("%s: SVGA_CMD_RECT_COPY command in SVGA command FIFO %d %d %d %d %d %d \n", __func__, x, y, dx, dy, width, height);
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
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = vmsvga_fifo_read(s);
      cursor.xor_mask_bpp = vmsvga_fifo_read(s);
      args = (SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.and_mask_bpp) + SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.xor_mask_bpp));
      if (cursor.width < 1 || cursor.height < 1 || cursor.width > 512 || cursor.height > 512 || cursor.and_mask_bpp < 1 || cursor.xor_mask_bpp < 1 || cursor.and_mask_bpp > 32 || cursor.xor_mask_bpp > 32) {
        #ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_CURSOR command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        #endif
        break;
      }
      len -= args;
      if (len < 0) {
        goto rewind;
      }
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.and_mask_bpp); args++) {
        cursor.and_mask[args] = vmsvga_fifo_read_raw(s);
        #ifdef VERBOSE
        printf("%s: cursor.and_mask[args] %d \n", __func__, cursor.and_mask[args]);
        #endif
      }
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.xor_mask_bpp); args++) {
        cursor.xor_mask[args] = vmsvga_fifo_read_raw(s);
        #ifdef VERBOSE
        printf("%s: cursor.xor_mask[args] %d \n", __func__, cursor.xor_mask[args]);
        #endif
      }
      vmsvga_cursor_define(s, & cursor);
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEFINE_CURSOR command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
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
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = 32;
      cursor.xor_mask_bpp = 32;
      args = ((cursor.width) * (cursor.height));
      if (cursor.width < 1 || cursor.height < 1 || cursor.width > 512 || cursor.height > 512 || cursor.and_mask_bpp < 1 || cursor.xor_mask_bpp < 1 || cursor.and_mask_bpp > 32 || cursor.xor_mask_bpp > 32) {
        #ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_ALPHA_CURSOR command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        #endif
        break;
      }
      len -= args;
      if (len < 0) {
        goto rewind;
      }
      for (i = 0; i < args; i++) {
        uint32_t rgba = vmsvga_fifo_read_raw(s);
        cursor.xor_mask[i] = rgba & 0x00ffffff;
        cursor.and_mask[i] = rgba & 0xff000000;
        #ifdef VERBOSE
        printf("%s: rgba %d \n", __func__, rgba);
        #endif
      }
      vmsvga_rgba_cursor_define(s, & cursor);
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEFINE_ALPHA_CURSOR command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
      #endif
      break;
    case SVGA_CMD_FENCE:
      len -= 2;
      if (len < 0) {
        goto rewind;
      }
      fence_arg = vmsvga_fifo_read(s);
      s -> fifo[SVGA_FIFO_FENCE] = cpu_to_le32(fence_arg);
      if (s -> irq_mask & (SVGA_IRQFLAG_ANY_FENCE)) {
        #ifdef VERBOSE
        printf("s->irq_status |= SVGA_IRQFLAG_ANY_FENCE\n");
        #else
        s -> irq_status |= SVGA_IRQFLAG_ANY_FENCE;
        #endif
      } else if ((s -> irq_mask & SVGA_IRQFLAG_FENCE_GOAL) && (s -> fifo[SVGA_FIFO_FENCE_GOAL] == fence_arg || s -> fg == fence_arg)) {
        #ifdef VERBOSE
        printf("s->irq_status |= SVGA_IRQFLAG_FENCE_GOAL\n");
        #endif
        s -> irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
      }
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_FENCE command in SVGA command FIFO %d %d %d %d \n", __func__, fence_arg, s -> irq_mask, s -> irq_status, cpu_to_le32(fence_arg));
      #endif
      break;
    case SVGA_CMD_DEFINE_GMR2:
      len -= 3;
      if (len < 0) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAW = vmsvga_fifo_read(s);
      UnknownCommandAX = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DEFINE_GMR2 command in SVGA command FIFO %d %d \n", __func__, UnknownCommandAW, UnknownCommandAX);
      #endif
      break;
    case SVGA_CMD_REMAP_GMR2:
      len -= 5;
      if (len < 0) {
        goto rewind;
      }
      #ifdef VERBOSE
      gmrIdCMD = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      #endif
      flags = vmsvga_fifo_read(s);
      #ifdef VERBOSE
      offsetPages = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      #endif
      num_pages = vmsvga_fifo_read(s);
      if (flags & SVGA_REMAP_GMR2_VIA_GMR) {
        args = 2;
      } else {
        args = (flags & SVGA_REMAP_GMR2_SINGLE_PPN) ? 1 : num_pages;
        if (flags & SVGA_REMAP_GMR2_PPN64)
          args *= 2;
      }
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_REMAP_GMR2 command in SVGA command FIFO %d %d %d %d \n", __func__, gmrIdCMD, flags, offsetPages, num_pages);
      #endif
      break;
    case SVGA_CMD_RECT_ROP_COPY:
      len -= 8;
      if (len < 0) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAY = vmsvga_fifo_read(s);
      UnknownCommandAZ = vmsvga_fifo_read(s);
      UnknownCommandBA = vmsvga_fifo_read(s);
      UnknownCommandBB = vmsvga_fifo_read(s);
      UnknownCommandBC = vmsvga_fifo_read(s);
      UnknownCommandBD = vmsvga_fifo_read(s);
      UnknownCommandM = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_RECT_ROP_COPY command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, UnknownCommandAY, UnknownCommandAZ, UnknownCommandBA, UnknownCommandBB, UnknownCommandBC, UnknownCommandBD, UnknownCommandM);
      #endif
      break;
    case SVGA_CMD_ESCAPE:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandA = vmsvga_fifo_read(s);
      UnknownCommandB = vmsvga_fifo_read(s);
      UnknownCommandAV = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ESCAPE command in SVGA command FIFO %d %d %d \n", __func__, UnknownCommandA, UnknownCommandB, UnknownCommandAV);
      #endif
      break;
    case SVGA_CMD_DEFINE_SCREEN:
      len -= 10;
      #ifdef VERBOSE
      UnknownCommandD = vmsvga_fifo_read(s);
      UnknownCommandE = vmsvga_fifo_read(s);
      UnknownCommandF = vmsvga_fifo_read(s);
      UnknownCommandG = vmsvga_fifo_read(s);
      UnknownCommandH = vmsvga_fifo_read(s);
      UnknownCommandI = vmsvga_fifo_read(s);
      UnknownCommandJ = vmsvga_fifo_read(s);
      UnknownCommandK = vmsvga_fifo_read(s);
      UnknownCommandL = vmsvga_fifo_read(s);
      s -> new_width = UnknownCommandG;
      s -> new_height = UnknownCommandH;
      printf("%s: SVGA_CMD_DEFINE_SCREEN command in SVGA command FIFO %d %d %d %d %d %d %d %d %d \n", __func__, UnknownCommandD, UnknownCommandE, UnknownCommandF, UnknownCommandG, UnknownCommandH, UnknownCommandI, UnknownCommandJ, UnknownCommandK, UnknownCommandL);
      #endif
      break;
    case SVGA_CMD_DISPLAY_CURSOR:
      len -= 3;
      #ifdef VERBOSE
      UnknownCommandC = vmsvga_fifo_read(s);
      UnknownCommandN = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DISPLAY_CURSOR command in SVGA command FIFO %d %d \n", __func__, UnknownCommandC, UnknownCommandN);
      #endif
      break;
    case SVGA_CMD_DESTROY_SCREEN:
      len -= 2;
      #ifdef VERBOSE
      UnknownCommandO = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DESTROY_SCREEN command in SVGA command FIFO %d \n", __func__, UnknownCommandO);
      #endif
      break;
    case SVGA_CMD_DEFINE_GMRFB:
      len -= 6;
      #ifdef VERBOSE
      UnknownCommandP = vmsvga_fifo_read(s);
      UnknownCommandQ = vmsvga_fifo_read(s);
      UnknownCommandR = vmsvga_fifo_read(s);
      UnknownCommandS = vmsvga_fifo_read(s);
      UnknownCommandT = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DEFINE_GMRFB command in SVGA command FIFO %d %d %d %d %d \n", __func__, UnknownCommandP, UnknownCommandQ, UnknownCommandR, UnknownCommandS, UnknownCommandT);
      #endif
      break;
    case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
      len -= 8;
      #ifdef VERBOSE
      UnknownCommandU = vmsvga_fifo_read(s);
      UnknownCommandV = vmsvga_fifo_read(s);
      UnknownCommandW = vmsvga_fifo_read(s);
      UnknownCommandX = vmsvga_fifo_read(s);
      UnknownCommandY = vmsvga_fifo_read(s);
      UnknownCommandZ = vmsvga_fifo_read(s);
      UnknownCommandAA = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_BLIT_GMRFB_TO_SCREEN command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, UnknownCommandU, UnknownCommandV, UnknownCommandW, UnknownCommandX, UnknownCommandY, UnknownCommandZ, UnknownCommandAA);
      #endif
      break;
    case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
      len -= 8;
      #ifdef VERBOSE
      UnknownCommandAB = vmsvga_fifo_read(s);
      UnknownCommandAC = vmsvga_fifo_read(s);
      UnknownCommandAD = vmsvga_fifo_read(s);
      UnknownCommandAE = vmsvga_fifo_read(s);
      UnknownCommandAF = vmsvga_fifo_read(s);
      UnknownCommandAG = vmsvga_fifo_read(s);
      UnknownCommandAH = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_BLIT_SCREEN_TO_GMRFB command in SVGA command FIFO %d %d %d %d %d %d %d \n", __func__, UnknownCommandAB, UnknownCommandAC, UnknownCommandAD, UnknownCommandAE, UnknownCommandAF, UnknownCommandAG, UnknownCommandAH);
      #endif
      break;
    case SVGA_CMD_ANNOTATION_FILL:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandAI = vmsvga_fifo_read(s);
      UnknownCommandAJ = vmsvga_fifo_read(s);
      UnknownCommandAK = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ANNOTATION_FILL command in SVGA command FIFO %d %d %d \n", __func__, UnknownCommandAI, UnknownCommandAJ, UnknownCommandAK);
      #endif
      break;
    case SVGA_CMD_ANNOTATION_COPY:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandAL = vmsvga_fifo_read(s);
      UnknownCommandAM = vmsvga_fifo_read(s);
      UnknownCommandAN = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ANNOTATION_COPY command in SVGA command FIFO %d %d %d \n", __func__, UnknownCommandAL, UnknownCommandAM, UnknownCommandAN);
      #endif
      break;
    case SVGA_CMD_MOVE_CURSOR:
      len -= 3;
      #ifdef VERBOSE
      UnknownCommandAO = vmsvga_fifo_read(s);
      UnknownCommandAP = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_MOVE_CURSOR command in SVGA command FIFO %d %d \n", __func__, UnknownCommandAO, UnknownCommandAP);
      #endif
      break;
    case SVGA_CMD_INVALID_CMD:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_INVALID_CMD command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_FRONT_ROP_FILL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_FRONT_ROP_FILL command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_DEAD:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEAD command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_DEAD_2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEAD_2 command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_NOP:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_NOP command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_NOP_ERROR:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_NOP_ERROR command in SVGA command FIFO \n", __func__);
      #endif
      break;
    case SVGA_CMD_MAX:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_MAX command in SVGA command FIFO \n", __func__);
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
      #ifdef VERBOSE
      printf("%s: default command in SVGA command FIFO\n", __func__);
      #endif
      break;
      rewind:
        s -> fifo_stop = cmd_start;
      s -> fifo[SVGA_FIFO_STOP] = cpu_to_le32(s -> fifo_stop);
      #ifdef VERBOSE
      printf("%s: rewind command in SVGA command FIFO\n", __func__);
      #endif
      break;
    }
  }
  if (s -> irq_mask & (SVGA_IRQFLAG_FIFO_PROGRESS)) {
    #ifdef VERBOSE
    printf("s->irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS\n");
    #endif
    s -> irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
  }
  #ifndef VERBOSE
  struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  if (((s -> irq_mask & s -> irq_status)) && ((s -> pcisetirq0 > s -> pcisetirq1)) && ((s -> pcisetirq == 0))) {
    #ifdef VERBOSE
    printf("Pci_set_irq=1\n");
    #endif
    pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
    s -> pcisetirq1++;
    s -> pcisetirq = 1;
  } else {
    #ifdef VERBOSE
    printf("Pci_set_irq=0\n");
    #endif
    pci_set_irq(PCI_DEVICE(pci_vmsvga), 0);
    s -> pcisetirq0++;
    s -> pcisetirq = 0;
  }
  #endif
  s -> syncing = 0;
  s -> sync2--;
}
static uint32_t vmsvga_index_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_index_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_index_read\n", __func__);
  #endif
  s -> sync4--;
  return s -> index;
}
static void vmsvga_index_write(void * opaque, uint32_t address, uint32_t index) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_index_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_index_write\n", __func__);
  #endif
  s -> index = index;
  s -> sync8--;
}
void * vmsvga_fifo_hack(void * arg);
void * vmsvga_fifo_hack(void * arg) {
  #ifdef VERBOSE
  //	printf("vmvga: vmsvga_fifo_hack was just executed\n");
  #endif
  struct vmsvga_state_s * s = (struct vmsvga_state_s * ) arg;
  int cx = 0;
  int cy = 0;
  while (true) {
    #ifdef VERBOSE
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 0) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 1) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 2) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 3) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 4) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 5) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 6) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000d;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 7) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 8) {
      #ifndef VERBOSE
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      #endif
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 9) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 10) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 11) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000004;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 12) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 13) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 14) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 15) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 16) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 17) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000bd;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 18) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000014;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 19) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 20) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 21) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00004000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 22) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 23) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 24) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 25) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x001fffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 26) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000fffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 27) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000ffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 28) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000ffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 29) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000020;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 30) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000020;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 31) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x03ffffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 32) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018ec1f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 33) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018e11f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 34) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 35) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 36) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008611f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 37) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000611f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 38) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018ec1f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 39) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 40) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 41) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 42) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 43) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 44) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 45) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 46) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 47) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 48) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 49) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 50) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 51) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 52) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 53) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 54) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 55) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 56) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 57) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 58) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 59) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 60) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 61) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 62) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 63) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000004;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 64) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 65) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 66) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 67) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 68) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 69) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 70) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 71) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 72) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 73) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 74) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 75) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 76) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 77) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000100;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 78) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 79) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 80) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 81) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 82) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 83) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 84) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 85) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 86) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 87) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 88) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 89) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000a;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 90) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000a;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 91) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 92) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 93) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 94) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 95) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 96) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 97) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 98) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 99) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 100) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 101) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 102) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 103) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 104) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 105) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 106) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000009;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 107) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 108) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 109) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 110) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 111) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 112) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 113) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 114) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 115) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 116) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 117) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 118) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 119) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 120) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 121) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 122) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 123) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 124) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 125) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 126) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 127) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 128) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 129) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 130) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 131) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 132) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 133) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 134) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 135) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 136) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 137) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 138) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 139) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 140) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 141) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 142) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000041;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 143) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000041;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 144) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 145) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 146) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 147) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 148) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 149) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 150) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 151) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 152) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 153) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 154) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 155) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 156) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 157) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 158) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 159) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000261;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 160) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 161) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 162) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 163) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 164) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 165) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 166) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 167) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 168) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 169) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 170) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 171) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 172) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 173) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 174) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 175) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 176) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 177) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 178) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000261;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 179) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 180) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 181) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 182) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 183) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 184) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 185) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 186) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 187) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 188) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 189) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 190) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 191) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 192) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 193) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 194) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 195) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 196) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 197) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 198) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 199) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 200) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 201) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 202) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 203) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 204) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 205) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 206) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 207) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 208) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 209) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 210) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 211) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 212) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000045;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 213) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 214) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 215) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 216) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 217) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 218) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 219) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 220) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 221) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 222) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 223) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 224) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 225) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 226) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 227) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 228) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 229) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 230) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 231) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 232) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 233) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 234) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 235) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 236) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 237) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 238) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 239) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 240) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 241) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 242) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 243) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 244) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 245) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 246) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 247) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 248) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 249) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 250) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 251) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 252) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 253) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 254) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 255) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 256) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 257) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 258) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 259) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 260) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 261) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] >= 262) {
      printf("s->fifo[SVGA_FIFO_3D_CAPS]==%d\n", s -> fifo[SVGA_FIFO_3D_CAPS]);
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    #else
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 0) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 1) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 2) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 3) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 4) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 5) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 6) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000d;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 7) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 8) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 9) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 10) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 11) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000004;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 12) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 13) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 14) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 15) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 16) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 17) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000bd;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 18) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000014;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 19) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 20) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 21) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00004000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 22) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 23) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 24) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 25) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x001fffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 26) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000fffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 27) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000ffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 28) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000ffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 29) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000020;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 30) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000020;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 31) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x03ffffff;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 32) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018ec1f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 33) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018e11f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 34) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 35) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 36) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0008611f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 37) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000611f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 38) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0018ec1f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 39) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 40) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 41) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 42) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 43) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 44) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 45) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 46) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 47) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 48) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 49) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 50) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000e005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 51) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 52) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 53) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 54) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 55) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 56) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 57) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 58) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 59) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 60) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 61) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0080601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 62) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 63) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000004;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 64) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000008;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 65) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00014007;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 66) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 67) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000601f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 68) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 69) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 70) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 71) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 72) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 73) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 74) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 75) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 76) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 77) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000100;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 78) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00008000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 79) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 80) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 81) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000040c5;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 82) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 83) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00006005;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 84) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 85) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 86) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 87) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 88) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 89) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000a;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 90) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000a;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 91) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x01246000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 92) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 93) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 94) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 95) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 96) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 97) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 98) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000f;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 99) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 100) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 101) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 102) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 103) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 104) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 105) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 106) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000009;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 107) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 108) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 109) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000000b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 110) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 111) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 112) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 113) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 114) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 115) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 116) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 117) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 118) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 119) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 120) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 121) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 122) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 123) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 124) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 125) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 126) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 127) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 128) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 129) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 130) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 131) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 132) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 133) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 134) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 135) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 136) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 137) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000026b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 138) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 139) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 140) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 141) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 142) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000041;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 143) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000041;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 144) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 145) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 146) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 147) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 148) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 149) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 150) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 151) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000001e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 152) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 153) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 154) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 155) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 156) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 157) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 158) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 159) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000261;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 160) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 161) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 162) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 163) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 164) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 165) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 166) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 167) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 168) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 169) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 170) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 171) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 172) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 173) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 174) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 175) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 176) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 177) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 178) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000261;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 179) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 180) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 181) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 182) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 183) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 184) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 185) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 186) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 187) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 188) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 189) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 190) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 191) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 192) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 193) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 194) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 195) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003e7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 196) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 197) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 198) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 199) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 200) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 201) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 202) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 203) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 204) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 205) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 206) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 207) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 208) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 209) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 210) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000063;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 211) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 212) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000045;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 213) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 214) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 215) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 216) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 217) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 218) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 219) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x0000006b;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 220) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 221) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 222) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 223) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 224) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 225) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 226) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 227) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 228) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 229) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 230) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 231) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 232) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 233) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000269;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 234) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 235) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 236) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 237) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 238) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 239) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000002f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 240) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 241) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000003f7;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 242) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 243) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 244) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 245) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 246) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 247) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 248) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 249) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 250) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 251) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 252) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 253) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 254) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e1;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 255) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 256) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x000000e3;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 257) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 258) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 259) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 260) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000010;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] == 261) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000001;
    };
    if (s -> fifo[SVGA_FIFO_3D_CAPS] >= 262) {
      s -> fifo[SVGA_FIFO_3D_CAPS] = 0x00000000;
    };
    #endif
    s -> fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_CURRENT;
    s -> fifo[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_CURRENT;
    #ifndef VERBOSE
    #ifdef VERBOSE
    s -> fifo[SVGA_FIFO_FLAGS] = SVGA_FIFO_FLAG_ACCELFRONT;
    #else
    s -> fifo[SVGA_FIFO_FLAGS] = SVGA_FIFO_FLAG_NONE;
    #endif
    #endif
    s -> fifo[SVGA_FIFO_BUSY] = s -> syncing;
    s -> fifo[SVGA_FIFO_CAPABILITIES] =
      SVGA_FIFO_CAP_NONE |
      SVGA_FIFO_CAP_FENCE |
      SVGA_FIFO_CAP_ACCELFRONT |
      SVGA_FIFO_CAP_PITCHLOCK |
      SVGA_FIFO_CAP_VIDEO |
      SVGA_FIFO_CAP_CURSOR_BYPASS_3 |
      SVGA_FIFO_CAP_ESCAPE |
      SVGA_FIFO_CAP_RESERVE |
      #ifdef VERBOSE
    SVGA_FIFO_CAP_SCREEN_OBJECT |
      #endif
    #ifdef VERBOSE
    SVGA_FIFO_CAP_GMR2 |
      #endif
    #ifdef VERBOSE
    SVGA_FIFO_CAP_SCREEN_OBJECT_2 |
      #endif
    SVGA_FIFO_CAP_DEAD;
    if (s -> enable != 0 && s -> config != 0) {
      vmsvga_update_rect(s, cx, cy, s -> new_width, s -> new_height);
    };
  };
};
static uint32_t vmsvga_value_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_value_read was just executed\n");
  #endif
  uint32_t caps;
  uint32_t cap2;
  struct vmsvga_state_s * s = opaque;
  uint32_t ret;
  #ifdef VERBOSE
  printf("%s: Unknown register %d\n", __func__, s -> index);
  #endif
  switch (s -> index) {
  case SVGA_REG_FENCE_GOAL:
    ret = s -> fg;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FENCE_GOAL register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_ID:
    ret = s -> svgaid;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ID register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_ENABLE:
    ret = s -> enable;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ENABLE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_WIDTH:
    ret = s -> new_width;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_WIDTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_HEIGHT:
    ret = s -> new_height;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HEIGHT register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_WIDTH:
    ret = SVGA_MAX_WIDTH;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_WIDTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCREENTARGET_MAX_WIDTH:
    ret = SVGA_MAX_WIDTH;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCREENTARGET_MAX_WIDTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_HEIGHT:
    ret = SVGA_MAX_HEIGHT;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_HEIGHT register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCREENTARGET_MAX_HEIGHT:
    ret = SVGA_MAX_HEIGHT;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCREENTARGET_MAX_HEIGHT register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    ret = s -> new_depth;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BITS_PER_PIXEL register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_HOST_BITS_PER_PIXEL:
    ret = s -> new_depth;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HOST_BITS_PER_PIXEL register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DEPTH:
    if (s -> new_depth == 32) {
      ret = 24;
    } else {
      ret = s -> new_depth;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEPTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_PSEUDOCOLOR:
    ret = (s -> new_depth == 8);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PSEUDOCOLOR register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_RED_MASK:
    if (s -> new_depth == 8) {
      ret = 0x00000007;
    } else if (s -> new_depth == 15) {
      ret = 0x0000001f;
    } else if (s -> new_depth == 16) {
      ret = 0x0000001f;
    } else {
      ret = 0x00ff0000;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_RED_MASK register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GREEN_MASK:
    if (s -> new_depth == 8) {
      ret = 0x00000038;
    } else if (s -> new_depth == 15) {
      ret = 0x000003e0;
    } else if (s -> new_depth == 16) {
      ret = 0x000007e0;
    } else {
      ret = 0x0000ff00;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GREEN_MASK register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BLUE_MASK:
    if (s -> new_depth == 8) {
      ret = 0x000000c0;
    } else if (s -> new_depth == 15) {
      ret = 0x00007c00;
    } else if (s -> new_depth == 16) {
      ret = 0x0000f800;
    } else {
      ret = 0x000000ff;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BLUE_MASK register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BYTES_PER_LINE:
    ret = ((((s -> new_depth) * (s -> new_width)) / (8)) * (s -> num_gd));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BYTES_PER_LINE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_FB_START: {
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_START register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  case SVGA_REG_FB_OFFSET:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_OFFSET register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BLANK_SCREEN_TARGETS:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BLANK_SCREEN_TARGETS register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_VRAM_SIZE:
    ret = s -> vga.vram_size;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_VRAM_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_FB_SIZE:
    ret = ((s -> new_height) * (((((s -> new_depth) * (s -> new_width)) / (8)) * (s -> num_gd))));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MOB_MAX_SIZE:
    ret = ((s -> vga.vram_size) * (s -> fifo_size));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MOB_MAX_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GBOBJECT_MEM_SIZE_KB:
    ret = ((s -> vga.vram_size) / (1024));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GBOBJECT_MEM_SIZE_KB register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
    ret = ((s -> vga.vram_size) / (1024));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MSHINT:
    ret = 1;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MSHINT register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_PRIMARY_MEM:
    ret = ((s -> vga.vram_size) + (s -> fifo_size));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_PRIMARY_MEM register %d with the return of %u\n", __func__, s -> index, ret);
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
    #ifdef VERBOSE
    caps |= SVGA_CAP_GMR;
    #endif
    caps |= SVGA_CAP_TRACES;
    #ifdef VERBOSE
    caps |= SVGA_CAP_GMR2;
    #endif
    #ifdef VERBOSE
    caps |= SVGA_CAP_SCREEN_OBJECT_2;
    #endif
    #ifndef VERBOSE
    #ifdef VERBOSE
    caps |= SVGA_CAP_COMMAND_BUFFERS;
    #endif
    #endif
    caps |= SVGA_CAP_DEAD1;
    #ifndef VERBOSE
    #ifdef VERBOSE
    caps |= SVGA_CAP_CMD_BUFFERS_2;
    #endif
    #endif
    #ifdef VERBOSE
    caps |= SVGA_CAP_GBOBJECTS;
    #endif
    #ifndef VERBOSE
    #ifdef VERBOSE
    caps |= SVGA_CAP_CMD_BUFFERS_3;
    #endif
    #endif
    caps |= SVGA_CAP_DX;
    caps |= SVGA_CAP_HP_CMD_QUEUE;
    caps |= SVGA_CAP_NO_BB_RESTRICTION;
    caps |= SVGA_CAP_CAP2_REGISTER;
    ret = caps;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CAPABILITIES register %d with the return of %u\n", __func__, s -> index, ret);
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
    printf("%s: SVGA_REG_CAP2 register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEM_START: {
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_START register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  case SVGA_REG_MEM_SIZE:
    ret = s -> fifo_size;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CONFIG_DONE:
    ret = s -> config;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CONFIG_DONE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SYNC:
    ret = s -> syncing;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SYNC register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BUSY:
    ret = s -> syncing;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BUSY register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GUEST_ID:
    ret = s -> guest;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GUEST_ID register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_ID:
    ret = s -> cursor.id;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ID register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_X:
    ret = s -> cursor.x;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_X register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_Y:
    ret = s -> cursor.y;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_Y register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_ON:
    ret = s -> cursor.on;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ON register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCRATCH_SIZE:
    ret = s -> scratch_size;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCRATCH_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEM_REGS:
    ret = 291;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_REGS register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_NUM_DISPLAYS:
    ret = s -> num_gd;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_DISPLAYS register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_PITCHLOCK:
    ret = ((((s -> new_depth) * (s -> new_width)) / (8)) * (s -> num_gd));
    s -> fifo[SVGA_FIFO_PITCHLOCK] = ((((s -> new_depth) * (s -> new_width)) / (8)) * (s -> num_gd));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PITCHLOCK register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_IRQMASK:
    ret = s -> irq_mask;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_IRQMASK register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    ret = s -> num_gd;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_ID:
    ret = s -> display_id;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_ID register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    ret = s -> disp_prim;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    ret = s -> disp_x;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_X register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    ret = s -> disp_y;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    ret = s -> new_width;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_WIDTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    ret = s -> new_height;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_HEIGHT register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMRS_MAX_PAGES:
    ret = s -> gmrpage;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMRS_MAX_PAGES register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_ID:
    ret = s -> gmrid;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_ID register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_DESCRIPTOR:
    ret = s -> gmrdesc;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_DESCRIPTOR register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_MAX_IDS:
    ret = s -> gmrid;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_MAX_IDS register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
    ret = s -> gmrdesc;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_TRACES:
    ret = s -> tracez;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_TRACES register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_COMMAND_LOW:
    ret = s -> cmd_low;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_LOW register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_COMMAND_HIGH:
    ret = s -> cmd_high;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_HIGH register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DEV_CAP:
    ret = s -> devcap_val;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEV_CAP register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEMORY_SIZE:
    ret = ((s -> vga.vram_size) - (s -> fifo_size));
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEMORY_SIZE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE...(SVGA_PALETTE_BASE + 767):
    ret = s -> svgabasea;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_SCRATCH_BASE:
    ret = s -> svgabaseb;
    #ifdef VERBOSE
    printf("%s: SVGA_SCRATCH_BASE register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  default:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: default register %d with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  s -> sync5--;
  return ret;
}
static void vmsvga_value_write(void * opaque, uint32_t address, uint32_t value) {
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: Unknown register %d with the value of %u\n", __func__, s -> index, value);
  #endif
  switch (s -> index) {
  case SVGA_REG_ID:
    if (value == SVGA_ID_0 || value == SVGA_ID_1 || value == SVGA_ID_2) {
      s -> svgaid = value;
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ID register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_FENCE_GOAL:
    s -> fg = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FENCE_GOAL register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE...(SVGA_PALETTE_BASE + 767):
    s -> svgabasea = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_SCRATCH_BASE:
    s -> svgabaseb = value;
    #ifdef VERBOSE
    printf("%s: SVGA_SCRATCH_BASE register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_ENABLE:
    s -> enable = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ENABLE register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_WIDTH:
    s -> new_width = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_WIDTH register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_HEIGHT:
    s -> new_height = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HEIGHT register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    s -> new_depth = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BITS_PER_PIXEL register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CONFIG_DONE:
    s -> config = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CONFIG_DONE register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_SYNC:
    s -> syncing = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SYNC register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_BUSY:
    s -> syncing = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BUSY register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GUEST_ID:
    s -> guest = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GUEST_ID register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_ID:
    s -> cursor.id = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ID register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_X:
    s -> cursor.x = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_X register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_Y:
    s -> cursor.y = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_Y register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_ON:
    if (value == SVGA_CURSOR_ON_HIDE) {
        s -> cursor.on = SVGA_CURSOR_ON_HIDE;
    } else {
        s -> cursor.on = SVGA_CURSOR_ON_SHOW;
    }
    dpy_mouse_set(s -> vga.con, s -> cursor.x, s -> cursor.y, s -> cursor.on);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ON register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_PITCHLOCK:
    s -> pitchlock = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PITCHLOCK register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_IRQMASK:
    s -> irq_mask = value;
    #ifndef VERBOSE
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    PCIDevice * pci_dev = PCI_DEVICE(pci_vmsvga);
    if (((value & s -> irq_status)) && ((s -> pcisetirq0 > s -> pcisetirq1)) && ((s -> pcisetirq == 0))) {
      #ifdef VERBOSE
      printf("pci_set_irq=1\n");
      #endif
      pci_set_irq(pci_dev, 1);
      s -> pcisetirq1++;
      s -> pcisetirq = 1;
    } else {
      #ifdef VERBOSE
      printf("pci_set_irq=0\n");
      #endif
      pci_set_irq(pci_dev, 0);
      s -> pcisetirq0++;
      s -> pcisetirq = 0;
    }
    #endif
    #ifdef VERBOSE
    printf("%s: SVGA_REG_IRQMASK register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    s -> num_gd = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    s -> disp_prim = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    s -> disp_x = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_X register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    s -> disp_y = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_ID:
    s -> display_id = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_ID register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    s -> new_width = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_WIDTH register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    s -> new_height = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_HEIGHT register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_TRACES:
    s -> tracez = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_TRACES register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_COMMAND_LOW:
    s -> cmd_low = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_LOW register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_COMMAND_HIGH:
    s -> cmd_high = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_HIGH register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GMR_ID:
    s -> gmrid = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_ID register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GMR_DESCRIPTOR:
    s -> gmrdesc = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_DESCRIPTOR register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DEV_CAP:
    if (value == 0) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 1) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 2) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 3) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 4) {
      s -> devcap_val = 0x00000007;
    };
    if (value == 5) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 6) {
      s -> devcap_val = 0x0000000d;
    };
    if (value == 7) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 8) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 9) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 10) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 11) {
      s -> devcap_val = 0x00000004;
    };
    if (value == 12) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 13) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 14) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 15) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 16) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 17) {
      s -> devcap_val = 0x000000bd;
    };
    if (value == 18) {
      s -> devcap_val = 0x00000014;
    };
    if (value == 19) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 20) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 21) {
      s -> devcap_val = 0x00004000;
    };
    if (value == 22) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 23) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 24) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 25) {
      s -> devcap_val = 0x001fffff;
    };
    if (value == 26) {
      s -> devcap_val = 0x000fffff;
    };
    if (value == 27) {
      s -> devcap_val = 0x0000ffff;
    };
    if (value == 28) {
      s -> devcap_val = 0x0000ffff;
    };
    if (value == 29) {
      s -> devcap_val = 0x00000020;
    };
    if (value == 30) {
      s -> devcap_val = 0x00000020;
    };
    if (value == 31) {
      s -> devcap_val = 0x03ffffff;
    };
    if (value == 32) {
      s -> devcap_val = 0x0018ec1f;
    };
    if (value == 33) {
      s -> devcap_val = 0x0018e11f;
    };
    if (value == 34) {
      s -> devcap_val = 0x0008601f;
    };
    if (value == 35) {
      s -> devcap_val = 0x0008601f;
    };
    if (value == 36) {
      s -> devcap_val = 0x0008611f;
    };
    if (value == 37) {
      s -> devcap_val = 0x0000611f;
    };
    if (value == 38) {
      s -> devcap_val = 0x0018ec1f;
    };
    if (value == 39) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 40) {
      s -> devcap_val = 0x00006007;
    };
    if (value == 41) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 42) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 43) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 44) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 45) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 46) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 47) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 48) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 49) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 50) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 51) {
      s -> devcap_val = 0x00014005;
    };
    if (value == 52) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 53) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 54) {
      s -> devcap_val = 0x00014005;
    };
    if (value == 55) {
      s -> devcap_val = 0x00014001;
    };
    if (value == 56) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 57) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 58) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 59) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 60) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 61) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 62) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 63) {
      s -> devcap_val = 0x00000004;
    };
    if (value == 64) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 65) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 66) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 67) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 68) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 69) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 70) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 71) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 72) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 73) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 74) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 75) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 76) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 77) {
      s -> devcap_val = 0x00000100;
    };
    if (value == 78) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 79) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 80) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 81) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 82) {
      s -> devcap_val = 0x00006005;
    };
    if (value == 83) {
      s -> devcap_val = 0x00006005;
    };
    if (value == 84) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 85) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 86) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 87) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 88) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 89) {
      s -> devcap_val = 0x0000000a;
    };
    if (value == 90) {
      s -> devcap_val = 0x0000000a;
    };
    if (value == 91) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 92) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 93) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 94) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 95) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 96) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 97) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 98) {
      s -> devcap_val = 0x0000000f;
    };
    if (value == 99) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 100) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 101) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 102) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 103) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 104) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 105) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 106) {
      s -> devcap_val = 0x00000009;
    };
    if (value == 107) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 108) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 109) {
      s -> devcap_val = 0x0000000b;
    };
    if (value == 110) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 111) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 112) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 113) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 114) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 115) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 116) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 117) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 118) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 119) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 120) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 121) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 122) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 123) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 124) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 125) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 126) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 127) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 128) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 129) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 130) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 131) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 132) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 133) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 134) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 135) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 136) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 137) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 138) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 139) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 140) {
      s -> devcap_val = 0x000001f7;
    };
    if (value == 141) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 142) {
      s -> devcap_val = 0x00000041;
    };
    if (value == 143) {
      s -> devcap_val = 0x00000041;
    };
    if (value == 144) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 145) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 146) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 147) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 148) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 149) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 150) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 151) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 152) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 153) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 154) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 155) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 156) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 157) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 158) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 159) {
      s -> devcap_val = 0x00000261;
    };
    if (value == 160) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 161) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 162) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 163) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 164) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 165) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 166) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 167) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 168) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 169) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 170) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 171) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 172) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 173) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 174) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 175) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 176) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 177) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 178) {
      s -> devcap_val = 0x00000261;
    };
    if (value == 179) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 180) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 181) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 182) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 183) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 184) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 185) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 186) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 187) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 188) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 189) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 190) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 191) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 192) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 193) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 194) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 195) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 196) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 197) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 198) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 199) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 200) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 201) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 202) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 203) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 204) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 205) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 206) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 207) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 208) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 209) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 210) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 211) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 212) {
      s -> devcap_val = 0x00000045;
    };
    if (value == 213) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 214) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 215) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 216) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 217) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 218) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 219) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 220) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 221) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 222) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 223) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 224) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 225) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 226) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 227) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 228) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 229) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 230) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 231) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 232) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 233) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 234) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 235) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 236) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 237) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 238) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 239) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 240) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 241) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 242) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 243) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 244) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 245) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 246) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 247) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 248) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 249) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 250) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 251) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 252) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 253) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 254) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 255) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 256) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 257) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 258) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 259) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 260) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 261) {
      s -> devcap_val = 0x00000001;
    };
    if (value >= 262) {
      s -> devcap_val = 0x00000000;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEV_CAP register %d with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  default:
    #ifdef VERBOSE
    printf("%s: default register %d with the value of %u\n", __func__, s -> index, value);
    #endif
  }
  s -> sync9--;
}
static uint32_t vmsvga_irqstatus_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_irqstatus_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_irqstatus_read\n", __func__);
  #endif
  s -> sync7--;
  return s -> irq_status;
}
static void vmsvga_irqstatus_write(void * opaque, uint32_t address, uint32_t data) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_irqstatus_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  s -> irq_status &= ~data;
  #ifdef VERBOSE
  printf("%s: vmsvga_irqstatus_write %d\n", __func__, data);
  #endif
  #ifndef VERBOSE
  struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  PCIDevice * pci_dev = PCI_DEVICE(pci_vmsvga);
  if ((!(s -> irq_mask & s -> irq_status))) {
    #ifdef VERBOSE
    printf("Pci_set_irq=O\n");
    #endif
    pci_set_irq(pci_dev, 0);
    s -> pcisetirq0++;
    s -> pcisetirq = 0;
  }
  #endif
  s -> sync--;
}
static uint32_t vmsvga_bios_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_bios_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_bios_read\n", __func__);
  #endif
  s -> sync6--;
  return s -> bios;
}
static void vmsvga_bios_write(void * opaque, uint32_t address, uint32_t data) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_bios_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  s -> bios = data;
  #ifdef VERBOSE
  printf("%s: vmsvga_bios_write %d\n", __func__, data);
  #endif
  s -> sync0--;
}
static inline void vmsvga_check_size(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_check_size was just executed\n");
  #endif
  DisplaySurface * surface = qemu_console_surface(s -> vga.con);
  uint32_t new_stride;
  if (s -> new_width < 1) {
    #ifdef VERBOSE
    printf("s->new_width < 1\n");
    #endif
    s -> sync1--;
    return;
  };
  if (s -> new_height < 1) {
    #ifdef VERBOSE
    printf("s->new_height < 1\n");
    #endif
    s -> sync1--;
    return;
  };
  if (s -> new_depth < 1) {
    #ifdef VERBOSE
    printf("s->new_depth < 1\n");
    #endif
    s -> sync1--;
    return;
  };
  if (s -> new_width > SVGA_MAX_WIDTH) {
    #ifdef VERBOSE
    printf("s->new_width > SVGA_MAX_WIDTH\n");
    #endif
    s -> sync1--;
    return;
  };
  if (s -> new_height > SVGA_MAX_HEIGHT) {
    #ifdef VERBOSE
    printf("s->new_height > SVGA_MAX_HEIGHT\n");
    #endif
    s -> sync1--;
    return;
  };
  if (s -> new_depth > 32) {
    #ifdef VERBOSE
    printf("s->new_depth > 32\n");
    #endif
    s -> sync1--;
    return;
  };
  new_stride = (s -> new_depth * s -> new_width) / 8;
  if (s -> new_width != surface_width(surface) ||
    s -> new_height != surface_height(surface) ||
    (new_stride != surface_stride(surface)) ||
    s -> new_depth != surface_bits_per_pixel(surface)) {
    pixman_format_code_t format = qemu_default_pixman_format(s -> new_depth, true);
    surface = qemu_create_displaysurface_from(s -> new_width, s -> new_height,
      format, new_stride,
      s -> vga.vram_ptr);
    dpy_gfx_replace_surface(s -> vga.con, surface);
  }
  s -> sync1--;
}
static void vmsvga_update_display(void * opaque) {
  #ifdef VERBOSE
  //	printf("vmvga: vmsvga_update_display was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  if (s -> enable == 0 && s -> config == 0) {
    s -> vga.hw_ops -> gfx_update( & s -> vga);
    return;
  }
  if (s -> sync1 < 1) {
    s -> sync1++;
    vmsvga_check_size(s);
  }
  if (s -> sync2 < 1) {
    s -> sync2++;
    vmsvga_fifo_run(s);
  }
  if (s -> sync3 < 1) {
    s -> sync3++;
    cursor_update_from_fifo(s);
  }
}
static void vmsvga_reset(DeviceState * dev) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_reset was just executed\n");
  #endif
  struct pci_vmsvga_state_s * pci = VMWARE_SVGA(dev);
  struct vmsvga_state_s * s = & pci -> chip;
  s -> enable = 0;
  s -> config = 0;
}
static void vmsvga_invalidate_display(void * opaque) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_invalidate_display was just executed\n");
  #endif
}
static void vmsvga_text_update(void * opaque, console_ch_t * chardata) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_text_update was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  if (s -> vga.hw_ops -> text_update) {
    s -> vga.hw_ops -> text_update( & s -> vga, chardata);
  }
}
static int vmsvga_post_load(void * opaque, int version_id) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_post_load was just executed\n");
  #endif
  return 0;
}
static
const VMStateDescription vmstate_vmware_vga_internal = {
  .name = "vmware_vga_internal",
  .version_id = 1,
  .minimum_version_id = 0,
  .post_load = vmsvga_post_load,
  .fields = (VMStateField[]) {
    VMSTATE_UINT32(new_depth, struct vmsvga_state_s),
      VMSTATE_INT32(enable, struct vmsvga_state_s),
      VMSTATE_INT32(config, struct vmsvga_state_s),
      VMSTATE_UINT32(cursor.id, struct vmsvga_state_s),
      VMSTATE_UINT32(cursor.x, struct vmsvga_state_s),
      VMSTATE_UINT32(cursor.y, struct vmsvga_state_s),
      VMSTATE_UINT32(cursor.on, struct vmsvga_state_s),
      VMSTATE_INT32(index, struct vmsvga_state_s),
      VMSTATE_VARRAY_INT32(scratch, struct vmsvga_state_s,
        scratch_size, 0, vmstate_info_uint32, uint32_t),
      VMSTATE_UINT32(new_width, struct vmsvga_state_s),
      VMSTATE_UINT32(new_height, struct vmsvga_state_s),
      VMSTATE_UINT32(guest, struct vmsvga_state_s),
      VMSTATE_UINT32(svgaid, struct vmsvga_state_s),
      VMSTATE_INT32(syncing, struct vmsvga_state_s),
      VMSTATE_UNUSED(4),
      VMSTATE_UINT32_V(irq_mask, struct vmsvga_state_s, 0),
      VMSTATE_UINT32_V(irq_status, struct vmsvga_state_s, 0),
      VMSTATE_UINT32_V(last_fifo_cursor_count, struct vmsvga_state_s, 0),
      VMSTATE_UINT32_V(display_id, struct vmsvga_state_s, 0),
      VMSTATE_UINT32_V(pitchlock, struct vmsvga_state_s, 0),
      VMSTATE_END_OF_LIST()
  }
};
static
const VMStateDescription vmstate_vmware_vga = {
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
static
const GraphicHwOps vmsvga_ops = {
  .invalidate = vmsvga_invalidate_display,
  .gfx_update = vmsvga_update_display,
  .text_update = vmsvga_text_update,
};
static void vmsvga_init(DeviceState * dev, struct vmsvga_state_s * s,
  MemoryRegion * address_space, MemoryRegion * io) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_init was just executed\n");
  #endif
  s -> scratch_size = 0x8000;
  s -> scratch = g_malloc(s -> scratch_size * 4);
  s -> vga.con = graphic_console_init(dev, 0, & vmsvga_ops, s);
  s -> fifo_size = 262144;
  memory_region_init_ram( & s -> fifo_ram, NULL, "vmsvga.fifo", s -> fifo_size, & error_fatal);
  s -> fifo = (uint32_t * ) memory_region_get_ram_ptr( & s -> fifo_ram);
  vga_common_init( & s -> vga, OBJECT(dev), & error_fatal);
  vga_init( & s -> vga, OBJECT(dev), address_space, io, true);
  vmstate_register(NULL, 0, & vmstate_vga_common, & s -> vga);
  if (s -> thread <= 0) {
    s -> thread++;
    s -> gmrid = 65536;
    s -> gmrpage = 64;
    s -> gmrdesc = 4096;
    s -> num_gd = 1;
    s -> new_width = 800;
    s -> new_height = 600;
    s -> new_depth = 32;
    pthread_t threads[1];
    pthread_create(threads, NULL, vmsvga_fifo_hack, (void * ) s);
  };
}
static uint64_t vmsvga_io_read(void * opaque, hwaddr addr, unsigned size) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_io_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    if (s -> sync4 < 1) {
      s -> sync4++;
      return vmsvga_index_read(s, addr);
    } else {
      return 0;
    }
  case 1 * SVGA_VALUE_PORT:
    if (s -> sync5 < 1) {
      s -> sync5++;
      return vmsvga_value_read(s, addr);
    } else {
      return 0;
    }
  case 1 * SVGA_BIOS_PORT:
    if (s -> sync6 < 1) {
      s -> sync6++;
      return vmsvga_bios_read(s, addr);
    } else {
      return 0;
    }
  case 1 * SVGA_IRQSTATUS_PORT:
    if (s -> sync7 < 1) {
      s -> sync7++;
      return vmsvga_irqstatus_read(s, addr);
    } else {
      return 0;
    }
  default:
    return 0;
  }
}
static void vmsvga_io_write(void * opaque, hwaddr addr,
  uint64_t data, unsigned size) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_io_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    if (s -> sync8 < 1) {
      s -> sync8++;
      vmsvga_index_write(s, addr, data);
    }
    break;
  case 1 * SVGA_VALUE_PORT:
    if (s -> sync9 < 1) {
      s -> sync9++;
      vmsvga_value_write(s, addr, data);
    }
    break;
  case 1 * SVGA_BIOS_PORT:
    if (s -> sync0 < 1) {
      s -> sync0++;
      vmsvga_bios_write(s, addr, data);
    }
    break;
  case 1 * SVGA_IRQSTATUS_PORT:
    if (s -> sync < 1) {
      s -> sync++;
      vmsvga_irqstatus_write(s, addr, data);
    }
    break;
  }
}
static
const MemoryRegionOps vmsvga_io_ops = {
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
static void pci_vmsvga_realize(PCIDevice * dev, Error ** errp) {
  #ifdef VERBOSE
  printf("vmvga: pci_vmsvga_realize was just executed\n");
  #endif
  struct pci_vmsvga_state_s * s = VMWARE_SVGA(dev);
  dev -> config[PCI_CACHE_LINE_SIZE] = 0x08;
  dev -> config[PCI_LATENCY_TIMER] = 0x40;
  dev -> config[PCI_INTERRUPT_LINE] = 0xff;
  dev -> config[PCI_INTERRUPT_PIN] = 1;
  memory_region_init_io( & s -> io_bar, OBJECT(dev), & vmsvga_io_ops, & s -> chip, "vmsvga-io", 0x10);
  memory_region_set_flush_coalesced( & s -> io_bar);
  pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, & s -> io_bar);
  vmsvga_init(DEVICE(dev), & s -> chip, pci_address_space(dev), pci_address_space_io(dev));
  pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_TYPE_32, & s -> chip.vga.vram);
  pci_register_bar(dev, 2, PCI_BASE_ADDRESS_MEM_PREFETCH, & s -> chip.fifo_ram);
}
static Property vga_vmware_properties[] = {
  DEFINE_PROP_UINT32("vgamem_mb", struct pci_vmsvga_state_s,
    chip.vga.vram_size_mb, 128),
  DEFINE_PROP_BOOL("global-vmstate", struct pci_vmsvga_state_s,
    chip.vga.global_vmstate, true),
  DEFINE_PROP_END_OF_LIST(),
};
static void vmsvga_class_init(ObjectClass * klass, void * data) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_class_init was just executed\n");
  #endif
  DeviceClass * dc = DEVICE_CLASS(klass);
  PCIDeviceClass * k = PCI_DEVICE_CLASS(klass);
  k -> realize = pci_vmsvga_realize;
  k -> romfile = "vgabios-vmware.bin";
  k -> vendor_id = PCI_VENDOR_ID_VMWARE;
  k -> device_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k -> class_id = PCI_CLASS_DISPLAY_VGA;
  k -> subsystem_vendor_id = PCI_VENDOR_ID_VMWARE;
  k -> subsystem_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k -> revision = 0x00;
  dc -> reset = vmsvga_reset;
  dc -> vmsd = & vmstate_vmware_vga;
  device_class_set_props(dc, vga_vmware_properties);
  dc -> hotpluggable = false;
  set_bit(DEVICE_CATEGORY_DISPLAY, dc -> categories);
}
static
const TypeInfo vmsvga_info = {
  .name = "vmware-svga",
  .parent = TYPE_PCI_DEVICE,
  .instance_size = sizeof(struct pci_vmsvga_state_s),
  .class_init = vmsvga_class_init,
  .interfaces = (InterfaceInfo[]) {
    {
      INTERFACE_CONVENTIONAL_PCI_DEVICE
    }, {},
  },
};
static void vmsvga_register_types(void) {
  #ifdef VERBOSE
  printf("vmvga: vmsvga_register_types was just executed\n");
  #endif
  type_register_static( & vmsvga_info);
}
type_init(vmsvga_register_types)
