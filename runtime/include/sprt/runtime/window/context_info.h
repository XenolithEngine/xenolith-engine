/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTEXT_INFO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTEXT_INFO_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/window/config.h>

namespace sprt::window {

using NativeContextHandle = void;

enum class ContextFlags : uint32_t {
	None = 0,

	Headless = 1 << 0,

	// Application shold be terminated when all it's windows were closed
	DestroyWhenAllWindowsClosed = 1 << 1,
};

SPRT_DEFINE_ENUM_AS_MASK(ContextFlags)

struct SPRT_API ContextInfo final : public Ref {
	memory::dynstring bundleName = "org.stappler.xenolith.test"; // application reverce-domain name
	memory::dynstring appName = "Xenolith"; // application human-readable name
	memory::dynstring appVersion = "0.0.1"; // application human-readable version
	memory::dynstring userLanguage = ""; //"ru-ru"; // current locale name
	memory::dynstring userAgent = "XenolithApp"; // networking user agent
	memory::dynstring launchUrl; // initial launch URL (deep link)

	uint32_t appVersionCode = 0; // version code in Vulkan format (see SP_MAKE_API_VERSION)

	// Application event update interval (NOT screen update interval)
	time_t appUpdateInterval = config::getDefaultAppUpdateInterval();

	uint16_t mainThreadsCount = config::getDefaultMainThreads(); // threads for general and gl tasks
	uint16_t appThreadsCount = config::getDefaultAppThreads(); // threads for app-level tasks

	ContextFlags flags = ContextFlags::None;
};

} // namespace sprt::window

#endif
