/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_CORE_DETAIL_SPLOGINIT_H_
#define CORE_CORE_DETAIL_SPLOGINIT_H_

#include "SPPlatformInit.h"
#include <sprt/runtime/log.h>

// GCC-specific formatting attribute
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define SPPRINTF(formatPos, argPos) __attribute__((__format__(printf, formatPos, argPos)))
#define SP_COVERAGE_TRIVIAL __attribute__ ((no_profile_instrument_function))
#elif defined(__has_attribute)
#if __has_attribute(format)
#define SPPRINTF(formatPos, argPos) __attribute__((__format__(printf, formatPos, argPos)))
#endif // __has_attribute(format)
#define SP_COVERAGE_TRIVIAL
#else
#define SPPRINTF(formatPos, argPos)
#define SP_COVERAGE_TRIVIAL
#endif

namespace STAPPLER_VERSIONIZED stappler::log {

using LogType = sprt::log::LogType;
using enum LogType;

SP_PUBLIC void format(LogType, const char *tag, const sprt::source_location &source, const char *,
		...) SPPRINTF(4, 5);
SP_PUBLIC void text(LogType, const char *tag, const sprt::source_location &source, const char *);

} // namespace stappler::log

#endif /* CORE_CORE_DETAIL_SPLOGINIT_H_ */
