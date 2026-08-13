/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_XLWINDOWINFO_H_
#define XENOLITH_APPLICATION_XLWINDOWINFO_H_

#include "XLApplicationConfig.h" // IWYU pragma: keep
#include "XLCorePresentationEngine.h" // IWYU pragma: keep
#include "SPFilepath.h" // IWYU pragma: keep

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

using core::ModeInfo;
using core::MonitorId;
using core::MonitorInfo;
using core::ScreenInfo;
using core::FullscreenFlags;
using core::FullscreenInfo;
using core::ViewConstraints;
using core::WindowState;

using sprt::window::WindowType;
using sprt::window::WindowAnchor;
using sprt::window::WindowPlacementAdjustment;
using sprt::window::WindowPlacement;
using sprt::window::getWindowTypeName;
using sprt::window::WindowCursor;
using sprt::window::WindowLayerFlags;
using sprt::window::WindowLayer;
using sprt::window::WindowCreationFlags;
using sprt::window::WindowAttributes;
using sprt::window::WindowCapabilities;
using sprt::window::WindowInfo;
using sprt::window::WindowIcon;
using sprt::window::WindowIconImage;

SP_PUBLIC Value encodeWindowInfo(const WindowInfo &info);
SP_PUBLIC StringView getWindowCursorName(WindowCursor);

// Decode an image into a WindowInfo::icon, producing one square raster per requested size.
//
// This lives here, and not in the runtime, because runtime_window is PRIVATE_STANDALONE and has no
// image decoder - see the note on sprt::window::WindowIcon.
//
// `sizes` defaults to getDefaultWindowIconSizes(). A requested size larger than the source is
// skipped rather than upscaled: a blurry raster is worse than none, since the window system picks
// from whatever set it is given. The source's own size is always emitted, so a single-size source
// still produces a usable icon. A non-square source is center-cropped to its shorter side.
//
// Returns nullptr when the file is missing or does not decode.
SP_PUBLIC Rc<WindowIcon> makeWindowIcon(const FileInfo &, SpanView<uint32_t> sizes = SpanView<uint32_t>());
SP_PUBLIC Rc<WindowIcon> makeWindowIcon(BytesView imageData,
		SpanView<uint32_t> sizes = SpanView<uint32_t>());

// 16..256: the sizes desktop window systems actually ask for (Win32 SM_CXSMICON/SM_CXICON are 16
// and 32, WMs pick from _NET_WM_ICON, compositors advertise sizes via xdg_toplevel_icon_manager).
SP_PUBLIC SpanView<uint32_t> getDefaultWindowIconSizes();

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLWINDOWINFO_H_
