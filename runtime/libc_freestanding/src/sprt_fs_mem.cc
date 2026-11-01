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

// Subunit of sprt_fs_string.cpp: memcpy memmove memset memcmp bcmp memchr memrchr.

#if SPRT_FS_C_VERSION(memcpy)
__SPRT_FS_DEF void *SPRT_FS_NAME(memcpy)(void *__SPRT_RESTRICT dest,
		const void *__SPRT_RESTRICT src, sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto d = static_cast<sprt::fs::uchar *>(dest);
	auto s = static_cast<const sprt::fs::uchar *>(src);
	for (; n; --n) {
		*d++ = *s++;
	}
	return dest;
}
#endif

#if SPRT_FS_C_VERSION(memmove)
__SPRT_FS_DEF void *SPRT_FS_NAME(memmove)(void *dest, const void *src,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto d = static_cast<sprt::fs::uchar *>(dest);
	auto s = static_cast<const sprt::fs::uchar *>(src);
	if (sprt::fs::uintptr_t(d) - sprt::fs::uintptr_t(s) >= n) {
		// d is below s, or the ranges do not overlap: a forward copy is safe.
		for (; n; --n) {
			*d++ = *s++;
		}
	} else {
		while (n) {
			--n;
			d[n] = s[n];
		}
	}
	return dest;
}
#endif

#if SPRT_FS_C_VERSION(memset)
__SPRT_FS_DEF void *SPRT_FS_NAME(memset)(void *dest, int c, sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto d = static_cast<sprt::fs::uchar *>(dest);
	for (; n; --n) {
		*d++ = sprt::fs::uchar(c);
	}
	return dest;
}
#endif

#if SPRT_FS_C_VERSION(memcmp)
__SPRT_FS_DEF int SPRT_FS_NAME(memcmp)(const void *l, const void *r,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto a = static_cast<const sprt::fs::uchar *>(l);
	auto b = static_cast<const sprt::fs::uchar *>(r);
	for (; n; --n, ++a, ++b) {
		if (*a != *b) {
			return int(*a) - int(*b);
		}
	}
	return 0;
}
#endif

#if SPRT_FS_C_VERSION(bcmp)
__SPRT_FS_DEF int SPRT_FS_NAME(bcmp)(const void *l, const void *r,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto a = static_cast<const sprt::fs::uchar *>(l);
	auto b = static_cast<const sprt::fs::uchar *>(r);
	for (; n; --n, ++a, ++b) {
		if (*a != *b) {
			return int(*a) - int(*b);
		}
	}
	return 0;
}
#endif

#if SPRT_FS_C_VERSION(memchr)
__SPRT_FS_DEF void *SPRT_FS_NAME(memchr)(const void *src, int c,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto s = static_cast<const sprt::fs::uchar *>(src);
	auto ch = sprt::fs::uchar(c);
	for (; n; --n, ++s) {
		if (*s == ch) {
			return const_cast<sprt::fs::uchar *>(s);
		}
	}
	return nullptr;
}
#endif

#if SPRT_FS_C_VERSION(memrchr)
__SPRT_FS_DEF void *SPRT_FS_NAME(memrchr)(const void *src, int c,
		sprt::fs::size_t n) __SPRT_NOEXCEPT {
	auto s = static_cast<const sprt::fs::uchar *>(src);
	auto ch = sprt::fs::uchar(c);
	while (n) {
		--n;
		if (s[n] == ch) {
			return const_cast<sprt::fs::uchar *>(s + n);
		}
	}
	return nullptr;
}
#endif
