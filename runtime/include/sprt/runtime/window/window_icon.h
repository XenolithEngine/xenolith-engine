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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_ICON_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_ICON_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/types.h>

// The window icon an application hands to the window system at creation time.
//
// This lives in the runtime, which has no image decoder: runtime_window is PRIVATE_STANDALONE and
// builds -nostdinc++ against sprt's own libcxx, so it cannot depend on stappler_bitmap. Decoding
// therefore happens in application code (xenolith::makeWindowIcon) and only raw pixels cross down
// here - the same split ClipboardData uses, where the app encodes and the platform sees bytes.

namespace sprt::window {

// One raster of a window icon.
//
// Tightly packed (no row padding), top-left origin, straight (unpremultiplied) RGBA8 - byte order
// R, G, B, A. That is exactly what a PNG decodes to, so the application side converts nothing and
// every conversion sits next to the platform call that needs it.
//
// `extent` must be square. xdg_toplevel_icon_v1::add_buffer raises `invalid_buffer` for anything
// else, and neither X11 nor Win32 has a use for a non-square icon.
struct WindowIconImage {
	Extent2 extent;
	Vector<uint8_t> data;

	// Bytes a well-formed image of this extent occupies.
	size_t getDataSize() const { return size_t(extent.width) * size_t(extent.height) * 4; }

	bool isValid() const {
		return extent.width > 0 && extent.width == extent.height && data.size() == getDataSize();
	}
};

// A set of rasters at different sizes, plus an optional freedesktop theme name.
//
// Multi-size is not a nicety: _NET_WM_ICON carries the whole set in one property and lets the WM
// choose, Win32 wants a separate ICON_BIG and ICON_SMALL, and xdg_toplevel_icon_v1 asks the client
// for "all icon sizes and scales that it can provide".
//
// Shared by Rc so one decoded icon can serve every window an application opens.
struct SPRT_API WindowIcon final : public Ref {
	Vector<WindowIconImage> images;

	// Icon name resolved through the XDG icon theme specification. Wayland's
	// xdg_toplevel_icon_v1::set_name takes it, and a compositor may prefer it over pixel data.
	// Ignored everywhere else.
	String name;

	// Smallest image at or above `size`; the largest one when every image is smaller.
	// Null only when `images` is empty.
	const WindowIconImage *getBestImage(uint32_t size) const;
};

// Pack `img` into `out` as host-endian 0xAARRGGBB words with PREMULTIPLIED alpha.
// `out` must have room for extent.width * extent.height words.
//
// One helper serves two backends because the two formats are the same 32-bit word: X11's
// _NET_WM_ICON is CARDINAL[]/32 holding ARGB, and WL_SHM_FORMAT_ARGB8888 is a host-endian 32-bit
// word (see the note in SPRTWinLinuxWaylandSoftwareSurface.cc). Both expect premultiplied alpha.
SPRT_API void packIconArgbPremultiplied(const WindowIconImage &img, uint32_t *out);

// Pack `img` into `out` as premultiplied BGRA8 bytes, bottom-up.
// `out` must have room for img.getDataSize() bytes.
//
// This is what a Win32 32bpp DDB wants for CreateIconIndirect: BGRA byte order, and rows starting
// at the bottom because a bitmap is bottom-up unless it is an explicitly top-down DIB.
SPRT_API void packIconBgraPremultipliedBottomUp(const WindowIconImage &img, uint8_t *out);

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_WINDOW_ICON_H_
