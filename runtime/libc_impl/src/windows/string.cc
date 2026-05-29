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

#include <sprt/c/__sprt_locale.h>

#include "string.h"
#include "wchar.h"
#include "../../include/__impl_libc.h"

extern "C" {
int strcoll(const char *l, const char *r) __SPRT_NOEXCEPT {
	if (!l || !r) {
		return 0;
	}

	auto map = sprt::__get_effective_locale_map(__SPRT_LC_COLLATE);
	if (map == sprt::__get_default_locale()) { }

	int ret = 0;
	sprt::unicode::toUtf16([&](sprt::WideStringView wl) {
		sprt::unicode::toUtf16([&](sprt::WideStringView wr) {
			ret = wcscoll((const wchar_t *)wl.data(), (const wchar_t *)wr.data());
		}, r);
	}, l);

	return ret;
}
}
