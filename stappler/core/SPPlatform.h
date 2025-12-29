/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_CORE_SPPLATFORM_H_
#define CORE_CORE_SPPLATFORM_H_

#include "SPCore.h"
#include <sprt/runtime/platform.h>

namespace STAPPLER_VERSIONIZED stappler {

using sprt::platform::ClockType;

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::platform {

SP_PUBLIC inline size_t makeRandomBytes(uint8_t *buf, size_t count) {
	return sprt::platform::makeRandomBytes(buf, count);
}

// current time in microseconds
SP_PUBLIC inline uint64_t clock(ClockType c = ClockType::Default) {
	return sprt::platform::clock(c);
}

// current time in nanoseconds
SP_PUBLIC inline uint64_t nanoclock(ClockType c = ClockType::Default) {
	return sprt::platform::nanoclock(c);
}

// sleep for the microseconds
SP_PUBLIC inline void sleep(uint64_t microseconds) { return sprt::platform::sleep(microseconds); }

SP_PUBLIC inline uint32_t getMemoryPageSize() { return sprt::platform::getMemoryPageSize(); }

} // namespace stappler::platform

#endif /* CORE_CORE_SPPLATFORM_H_ */
