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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONFIG_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONFIG_H_

#include <sprt/runtime/init.h>
#include <sprt/runtime/dispatch/thread_info.h>

#include <sprt/cxx/thread>

namespace sprt::window::config {

static inline time_t getDefaultAppUpdateInterval() { return 1'000'000; }

static inline uint16_t getDefaultMainThreads() {
	auto n = static_cast<uint16_t>(thread::hardware_concurrency());
	if (n <= 1) {
		return 0;
	} else if (n == 2) {
		return 1;
	}
	return static_cast<uint16_t>(n / 2 + 1);
}
static inline uint16_t getDefaultAppThreads() {
	auto n = static_cast<uint16_t>(thread::hardware_concurrency());
	if (n <= 1) {
		return 0;
	}
	auto half = static_cast<uint16_t>(n / 2);
	return half > 1 ? static_cast<uint16_t>(half - 1) : uint16_t(1);
}

} // namespace sprt::window::config

#endif
