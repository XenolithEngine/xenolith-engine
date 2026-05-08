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

#include "stdlib.h"

#include "stdlib/bsearch.cc"
#include "stdlib/floatscan.cc"
#include "stdlib/intscan.cc"
#include "stdlib/shgetc.cc"
#include "stdlib/strtod.cc"
#include "stdlib/strtol.cc"
#include "stdlib/wcstod.cc"
#include "stdlib/wcstol.cc"

#if SPRT_WINDOWS
#include "windows/stdlib.cc"
#endif

__SPRT_C_FUNC float __SPRT_ID(
		strtof_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return strtof(buf, out);
}

__SPRT_C_FUNC double __SPRT_ID(
		strtod_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return strtod(buf, out);
}

__SPRT_C_FUNC long double __SPRT_ID(
		strtold_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return strtold(buf, out);
}

__SPRT_C_FUNC void *__SPRT_ID(bsearch_impl)(const void *key, const void *base, size_t nmemb,
		size_t size, int (*compar)(const void *, const void *)) {
	return bsearch(key, base, nmemb, size, compar);
}

__SPRT_C_FUNC long __SPRT_ID(
		strtol_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return strtol(buf, out, base);
}

__SPRT_C_FUNC unsigned long __SPRT_ID(
		strtoul_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return strtoul(buf, out, base);
}

__SPRT_C_FUNC long long __SPRT_ID(
		strtoll_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return strtoll(buf, out, base);
}

__SPRT_C_FUNC unsigned long long __SPRT_ID(
		strtoull_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return strtoull(buf, out, base);
}
