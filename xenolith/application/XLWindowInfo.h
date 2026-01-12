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

using sprt::window::WindowCursor;
using sprt::window::WindowLayerFlags;
using sprt::window::WindowLayer;
using sprt::window::WindowCreationFlags;
using sprt::window::WindowAttributes;
using sprt::window::WindowCapabilities;
using sprt::window::WindowInfo;

SP_PUBLIC Value encodeWindowInfo(const WindowInfo &info);
SP_PUBLIC StringView getWindowCursorName(WindowCursor);

inline const CallbackStream &operator<<(const CallbackStream &stream, WindowCursor t) {
	stream << getWindowCursorName(t);
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, WindowCursor t) {
	stream << getWindowCursorName(t);
	return stream;
}

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLWINDOWINFO_H_
