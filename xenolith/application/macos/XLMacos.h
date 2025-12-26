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

#ifndef XENOLITH_APPLICATION_MACOS_XLMACOS_H_
#define XENOLITH_APPLICATION_MACOS_XLMACOS_H_

#include "XLContextInfo.h"
#include "XLCoreTextInput.h"

// Set to 1 to enable debug logging
#ifndef XL_MACOS_DEBUG
#define XL_MACOS_DEBUG 0
#endif

#if __OBJC__

#define XL_OBJC_CALL(...) __VA_ARGS__
#define XL_OBJC_CALL_V(__TYPE__, ...) __TYPE__(__VA_ARGS__)

#define XL_OBJC_INTERFACE_FORWARD(__NAME__) @class __NAME__
#define XL_OBJC_INTERFACE_BEGIN(__NAME__) @interface __NAME__
#define XL_OBJC_INTERFACE_END(__NAME__) @end
#define XL_OBJC_IMPLEMENTATION_BEGIN(__NAME__) @implementation __NAME__
#define XL_OBJC_IMPLEMENTATION_END(__NAME__) @end

#else

#define __bridge

#define XL_OBJC_CALL(...) ((void *)0)
#define XL_OBJC_CALL_V(__TYPE__, ...) (__TYPE__)()
#define XL_OBJC_INTERFACE_FORWARD(__NAME__) typedef void __NAME__
#define XL_OBJC_INTERFACE_BEGIN(__NAME__) namespace { class __NAME__
#define XL_OBJC_INTERFACE_END(__NAME__) }
#define XL_OBJC_IMPLEMENTATION_BEGIN(__NAME__) namespace {
#define XL_OBJC_IMPLEMENTATION_END(__NAME__) }

#endif

#define NSSP STAPPLER_VERSIONIZED_NAMESPACE
#define NSXL STAPPLER_VERSIONIZED_NAMESPACE::xenolith
#define NSXLPL STAPPLER_VERSIONIZED_NAMESPACE::xenolith::platform

XL_OBJC_INTERFACE_FORWARD(XLMacosAppDelegate);
XL_OBJC_INTERFACE_FORWARD(XLMacosViewController);
XL_OBJC_INTERFACE_FORWARD(XLMacosView);
XL_OBJC_INTERFACE_FORWARD(XLMacosWindow);
XL_OBJC_INTERFACE_FORWARD(CAMetalLayer);
XL_OBJC_INTERFACE_FORWARD(NSScreen);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

SP_PUBLIC uint32_t getMacosButtonNumber(core::InputMouseButton btn);
SP_PUBLIC core::InputMouseButton getInputMouseButton(int32_t buttonNumber);

SP_PUBLIC void createKeyTables(core::InputKeyCode keycodes[256],
		uint16_t scancodes[stappler::toInt(core::InputKeyCode::Max)]);

SP_PUBLIC core::InputModifier getInputModifiers(uint32_t flags);

class MacosWindow;

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_MACOS_XLMACOS_H_
