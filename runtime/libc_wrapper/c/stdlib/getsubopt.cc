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
#include <sprt/cxx/cstring>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(getsubopt)(char **opt, char *const *keys, char **val) {
	char *s = *opt;
	int i;

	*val = nullptr;
	*opt = strchr(s, ',');
	if (*opt) {
		*(*opt)++ = 0;
	} else {
		*opt = s + strlen(s);
	}

	for (i = 0; keys[i]; i++) {
		size_t l = strlen(keys[i]);
		if (strncmp(keys[i], s, l)) {
			continue;
		}
		if (s[l] == '=') {
			*val = s + l + 1;
		} else if (s[l]) {
			continue;
		}
		return i;
	}
	return -1;
}

} // namespace sprt
