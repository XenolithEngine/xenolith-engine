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

#ifndef CORE_RUNTIME_PRIVATE_SPRTFILENAME_H_
#define CORE_RUNTIME_PRIVATE_SPRTFILENAME_H_

#include <c/__sprt_string.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_errno.h>

#include "SPRuntimeInvoke.h"

namespace sprt::internal {

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif

template <typename Result, typename Callback>
static inline auto performWithNativePath(const char *path, const Callback &cb,
		const Result &error) {
	static_assert(is_invocable_v<Callback, const char *>);

	auto pathlen = path ? __sprt_strlen(path) : 0;
	auto isNative = __sprt_fpath_is_native(path, pathlen);
	if (!path || isNative) {
		return cb(path);
	} else {
#if __SPRT_CONFIG_USE_ALLOCA_FOR_TEMPORRY
		auto buf = (char *)__builtin_alloca(pathlen + 1);
		if (__sprt_fpath_to_native(path, pathlen, buf, pathlen + 1) > 0) {
			return cb(buf);
		}
#else
		auto buf = new char[pathlen + 1];
		if (__sprt_fpath_to_native(path, pathlen, buf, pathlen + 1) > 0) {
			auto ret = cb(buf);
			delete[] buf;
			return ret;
		}
		delete[] buf;
#endif
	}
	*__sprt___errno_location() = EINVAL;
	return error;
}

template <typename Result, typename Callback>
static inline auto performWithPosixePath(const char *path, const Callback &cb,
		const Result &error) {
	static_assert(is_invocable_v<Callback, const char *>);

	auto pathlen = path ? __sprt_strlen(path) : 0;
	auto isPosix = __sprt_fpath_is_posix(path, pathlen);
	if (!path || isPosix) {
		return cb(path);
	} else {
#if __SPRT_CONFIG_USE_ALLOCA_FOR_TEMPORRY
		auto buf = (char *)__builtin_alloca(pathlen + 1);
		if (__sprt_fpath_to_posix(path, pathlen, buf, pathlen + 1) > 0) {
			return cb(buf);
		}
#else
		auto buf = new char[pathlen + 1];
		if (__sprt_fpath_to_posix(path, pathlen, buf, pathlen + 1) > 0) {
			auto ret = cb(buf);
			delete[] buf;
			return ret;
		}
		delete[] buf;
#endif
	}
	*__sprt___errno_location() = EINVAL;
	return error;
}

#if __clang__
#pragma clang diagnostic pop
#endif

} // namespace sprt::internal

#endif
