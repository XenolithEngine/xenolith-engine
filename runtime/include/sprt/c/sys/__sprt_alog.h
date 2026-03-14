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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_ALOG_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_ALOG_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/__sprt_stdarg.h>

__SPRT_BEGIN_DECL

typedef enum __SPRT_ID(alog_priority) {
	__SPRT_ALOG_UNKNOWN = 0,
	__SPRT_ALOG_DEFAULT,
	__SPRT_ALOG_VERBOSE,
	__SPRT_ALOG_DEBUG,
	__SPRT_ALOG_INFO,
	__SPRT_ALOG_WARN,
	__SPRT_ALOG_ERROR,
	__SPRT_ALOG_FATAL,
	__SPRT_ALOG_SILENT,
} __SPRT_ID(alog_priority);

#if __SPRT_CONFIG_HAVE_ALOG || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_CONFIG_HAVE_ALOG_NOTICE
SPRT_API int __SPRT_ID(alog_print)(int prio, const char *tag, const char *fmt, ...);

__SPRT_CONFIG_HAVE_ALOG_NOTICE
SPRT_API int __SPRT_ID(
		alog_vprint)(int prio, const char *tag, const char *fmt, __SPRT_ID(va_list) ap);

#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_ALOG_H_
