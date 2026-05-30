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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_stdlib.h>
#include <stdlib.h>

namespace sprt {

struct __qsort_s_wrapper {
	void *ctx;
	int (*comp)(void *, const void *, const void *);
};

#if SPRT_ANDROID
thread_local __qsort_s_wrapper tl_qsort_s_wrapper;
#endif

__SPRT_C_FUNC int qsort_s(void *ptr, __SPRT_ID(rsize_t) count, __SPRT_ID(rsize_t) size,
		int (*comp)(void *, const void *, const void *), void *context) __SPRT_NOEXCEPT {

#if SPRT_ANDROID
	tl_qsort_s_wrapper = {context, comp};
	qsort(ptr, count, size, [](const void *l, const void *r) {
		return tl_qsort_s_wrapper.comp(tl_qsort_s_wrapper.ctx, l, r);
	});
#else
	__qsort_s_wrapper w = {context, comp};

	qsort_r(ptr, count, size, [](const void *l, const void *r, void *ptr) {
		auto w = (__qsort_s_wrapper *)ptr;
		return w->comp(w->ctx, l, r);
	}, &w);
#endif
	return 0;
}

} // namespace sprt

#if SPRT_ANDROID

struct __qsort_r_wrapper {
	void *ctx;
	int (*comp)(const void *, const void *, void *);
};

thread_local __qsort_r_wrapper tl_qsort_r_wrapper;

__SPRT_C_FUNC void qsort_r(void *ptr, size_t count, size_t size,
		int (*cmp)(const void *, const void *, void *), void *ctx) {
	tl_qsort_r_wrapper = {ctx, cmp};
	qsort(ptr, count, size, [](const void *l, const void *r) {
		return tl_qsort_r_wrapper.comp(l, r, tl_qsort_r_wrapper.ctx);
	});
}

#endif
