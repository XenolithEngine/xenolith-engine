/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LINUX_FB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LINUX_FB_H_

/*
	Dispatch header for the Linux framebuffer UAPI <linux/fb.h>:
	- hosted SPRT build -> forwards to the system <linux/fb.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Only the read-out surface is declared: the two screeninfo structures, the
	bitfield that describes a channel, the three FBIO requests and the handful
	of FB_TYPE / FB_VISUAL values a packed-pixel truecolour device reports.
	Palettes, panning, hardware cursors and the accelerator interface are not
	here because nothing on this side can answer them.

	Public surface provided by the SPRT-own path:

	Macros:
	  FBIOGET_VSCREENINFO, FBIOPUT_VSCREENINFO, FBIOGET_FSCREENINFO
	  (the numbers are Linux's, and Embox happens to agree on all three)
	  FB_TYPE_PACKED_PIXELS, FB_VISUAL_MONO01, FB_VISUAL_TRUECOLOR,
	  FB_VISUAL_PSEUDOCOLOR, FB_VISUAL_DIRECTCOLOR
	  FB_ACTIVATE_NOW, FB_VMODE_NONINTERLACED

	Types:
	  struct fb_bitfield        - one colour channel: offset, length, msb_right
	  struct fb_var_screeninfo  - geometry, timing and the channel layout
	  struct fb_fix_screeninfo  - the buffer itself: address, length, stride

	The layout is Linux's, not the host kernel's. On Embox EL0 these structures
	do NOT match the kernel's own (Embox carries a shortened fb_var_screeninfo
	ending in an `enum pix_fmt`), and that is the point: the syscall boundary
	speaks Linux and the kernel translates, the same way it does for errno and
	O_* -- see xenolith-os board/embox-qemu/drivers/xlsyscall/xl_abi.h.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <linux/fb.h>

#else

#include <sprt/c/bits/__sprt_def.h>
#include <stdint.h>

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

#define FB_TYPE_PACKED_PIXELS 0

#define FB_VISUAL_MONO01        0
#define FB_VISUAL_MONO10        1
#define FB_VISUAL_TRUECOLOR     2
#define FB_VISUAL_PSEUDOCOLOR   3
#define FB_VISUAL_DIRECTCOLOR   4
#define FB_VISUAL_STATIC_PSEUDOCOLOR 5

#define FB_ACTIVATE_NOW 0

#define FB_VMODE_NONINTERLACED 0
#define FB_VMODE_INTERLACED    1
#define FB_VMODE_DOUBLE        2

/* One colour channel inside a pixel. `offset` is the bit position of the least
 * significant bit of the channel, counted from the least significant bit of the
 * pixel word; `length` is how many bits it occupies. msb_right is 0 on every
 * device this runtime meets. */
struct fb_bitfield {
	uint32_t offset;
	uint32_t length;
	uint32_t msb_right;
};

struct fb_var_screeninfo {
	uint32_t xres;
	uint32_t yres;
	uint32_t xres_virtual;
	uint32_t yres_virtual;
	uint32_t xoffset;
	uint32_t yoffset;

	uint32_t bits_per_pixel;
	uint32_t grayscale;

	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;

	uint32_t nonstd;
	uint32_t activate;
	uint32_t height;
	uint32_t width;
	uint32_t accel_flags;

	uint32_t pixclock;
	uint32_t left_margin;
	uint32_t right_margin;
	uint32_t upper_margin;
	uint32_t lower_margin;
	uint32_t hsync_len;
	uint32_t vsync_len;
	uint32_t sync;
	uint32_t vmode;
	uint32_t rotate;
	uint32_t colorspace;
	uint32_t reserved[4];
};

struct fb_fix_screeninfo {
	char id[16];
	unsigned long smem_start; /* physical start of the buffer, as the device sees it */
	uint32_t smem_len;
	uint32_t type;
	uint32_t type_aux;
	uint32_t visual;
	uint16_t xpanstep;
	uint16_t ypanstep;
	uint16_t ywrapstep;
	uint32_t line_length; /* bytes per row, which is NOT always xres * bpp / 8 */
	unsigned long mmio_start;
	uint32_t mmio_len;
	uint32_t accel;
	uint16_t capabilities;
	uint16_t reserved[2];
};

#endif /* __SPRT_BUILD && __STDC_HOSTED__ */

#endif /* CORE_RUNTIME_INCLUDE_LIBC_LINUX_FB_H_ */
