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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOS_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOS_H_

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/input.h>

// NB: this is the preliminary iOS window backend. The macOS (AppKit) backend in
// ../macos is the reference; the iOS counterpart will eventually drive UIKit
// (UIApplication / UIWindow / UIView + CAMetalLayer). For now everything here is
// a stub so that the runtime builds and links for the *-apple-ios* targets.
#if SPRT_IOS

// Set to 1 to enable debug logging
#ifndef XL_IOS_DEBUG
#define XL_IOS_DEBUG 0
#endif

#if __OBJC__

#define SPRT_OBJC_CALL(...) __VA_ARGS__
#define SPRT_OBJC_INTERFACE_FORWARD(__NAME__) @class __NAME__

#else

#define __bridge

#define SPRT_OBJC_CALL(...) ((void *)0)
#define SPRT_OBJC_INTERFACE_FORWARD(__NAME__) typedef void __NAME__

#endif

#define NSSP ::sprt
#define NSSPWIN ::sprt::window

SPRT_OBJC_INTERFACE_FORWARD(SPRTIosAppDelegate);
SPRT_OBJC_INTERFACE_FORWARD(CAMetalLayer);

namespace sprt::window {

class IosWindow;

} // namespace sprt::window

#endif // SPRT_IOS

#endif // CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOS_H_
