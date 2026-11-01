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

// Subunit of sprt_fs_string.cpp: strlen strnlen strcmp strncmp strchr strrchr.

#if SPRT_FS_C_VERSION(strlen)
__SPRT_FS_DEF sprt::fs::size_t SPRT_FS_NAME(strlen)(const char *s) __SPRT_NOEXCEPT {
	sprt::fs::size_t n = 0;
	while (s[n]) {
		++n;
	}
	return n;
}
#endif

#if SPRT_FS_C_VERSION(strnlen)
__SPRT_FS_DEF sprt::fs::size_t SPRT_FS_NAME(strnlen)(const char *s,
		sprt::fs::size_t max) __SPRT_NOEXCEPT {
	sprt::fs::size_t n = 0;
	while (n < max && s[n]) {
		++n;
	}
	return n;
}
#endif

#if SPRT_FS_C_VERSION(strcmp)
__SPRT_FS_DEF int SPRT_FS_NAME(strcmp)(const char *l, const char *r) __SPRT_NOEXCEPT {
	auto a = reinterpret_cast<const sprt::fs::uchar *>(l);
	auto b = reinterpret_cast<const sprt::fs::uchar *>(r);
	for (; *a && *a == *b; ++a, ++b) { }
	return int(*a) - int(*b);
}
#endif

#if SPRT_FS_C_VERSION(strncmp)
__SPRT_FS_DEF int SPRT_FS_NAME(strncmp)(const char *l, const char *r,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto a = reinterpret_cast<const sprt::fs::uchar *>(l);
	auto b = reinterpret_cast<const sprt::fs::uchar *>(r);
	for (; n; --n, ++a, ++b) {
		if (*a != *b || !*a) {
			return int(*a) - int(*b);
		}
	}
	return 0;
}
#endif

#if SPRT_FS_C_VERSION(strchr)
__SPRT_FS_DEF char *SPRT_FS_NAME(strchr)(const char *s, int c) __SPRT_NOEXCEPT {
	auto ch = char(c);
	for (;; ++s) {
		if (*s == ch) {
			return const_cast<char *>(s);
		}
		if (!*s) {
			return nullptr;
		}
	}
}
#endif

#if SPRT_FS_C_VERSION(strrchr)
__SPRT_FS_DEF char *SPRT_FS_NAME(strrchr)(const char *s, int c) __SPRT_NOEXCEPT {
	auto ch = char(c);
	const char *found = nullptr;
	for (;; ++s) {
		if (*s == ch) {
			found = s;
		}
		if (!*s) {
			return const_cast<char *>(found);
		}
	}
}
#endif
