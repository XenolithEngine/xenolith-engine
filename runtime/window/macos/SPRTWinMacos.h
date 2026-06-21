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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOS_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOS_H_

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/input.h>

#if SPRT_MACOS

// Set to 1 to enable debug logging
#ifndef XL_MACOS_DEBUG
#define XL_MACOS_DEBUG 0
#endif

#if __OBJC__

#define SPRT_OBJC_CALL(...) __VA_ARGS__
#define SPRT_OBJC_CALL_V(__TYPE__, ...) __TYPE__(__VA_ARGS__)

#define SPRT_OBJC_INTERFACE_FORWARD(__NAME__) @class __NAME__
#define SPRT_OBJC_INTERFACE_BEGIN(__NAME__) @interface __NAME__
#define SPRT_OBJC_INTERFACE_END(__NAME__) @end
#define SPRT_OBJC_IMPLEMENTATION_BEGIN(__NAME__) @implementation __NAME__
#define SPRT_OBJC_IMPLEMENTATION_END(__NAME__) @end

#else

#define __bridge

#define SPRT_OBJC_CALL(...) ((void *)0)
#define SPRT_OBJC_CALL_V(__TYPE__, ...) (__TYPE__)()
#define SPRT_OBJC_INTERFACE_FORWARD(__NAME__) typedef void __NAME__
#define SPRT_OBJC_INTERFACE_BEGIN(__NAME__) namespace { class __NAME__
#define SPRT_OBJC_INTERFACE_END(__NAME__) }
#define SPRT_OBJC_IMPLEMENTATION_BEGIN(__NAME__) namespace {
#define SPRT_OBJC_IMPLEMENTATION_END(__NAME__) }

#endif

#define NSSP ::sprt
#define NSSPWIN ::sprt::window

SPRT_OBJC_INTERFACE_FORWARD(SPRTMacosAppDelegate);
SPRT_OBJC_INTERFACE_FORWARD(SPRTMacosViewController);
SPRT_OBJC_INTERFACE_FORWARD(SPRTMacosView);
SPRT_OBJC_INTERFACE_FORWARD(SPRTMacosWindow);
SPRT_OBJC_INTERFACE_FORWARD(CAMetalLayer);
SPRT_OBJC_INTERFACE_FORWARD(NSScreen);

namespace sprt::window {

SPRT_API uint32_t getMacosButtonNumber(InputMouseButton btn);
SPRT_API InputMouseButton getInputMouseButton(int32_t buttonNumber);

SPRT_API void createKeyTables(InputKeyCode keycodes[256],
		uint16_t scancodes[toInt(InputKeyCode::Max)]);

SPRT_API InputModifier getInputModifiers(uint32_t flags);

class MacosWindow;

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOS_H_
