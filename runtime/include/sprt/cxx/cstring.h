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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_CSTRING_H_
#define RUNTIME_INCLUDE_SPRT_CXX_CSTRING_H_

#include <sprt/c/__sprt_string.h>
#include <sprt/runtime/init.h>
#include <sprt/runtime/detail/constexpr.h>

namespace sprt {

SPRT_FORCEINLINE void *memcpy(void *s1, const void *s2, size_t n) {
	return __builtin_memcpy(s1, s2, n);
}
SPRT_FORCEINLINE void *memmove(void *s1, const void *s2, size_t n) {
	return __builtin_memmove(s1, s2, n);
}
SPRT_FORCEINLINE char *strcpy(char *s1, const char *s2) { return __builtin_strcpy(s1, s2); }
SPRT_FORCEINLINE char *strncpy(char *s1, const char *s2, size_t n) {
	return __builtin_strncpy(s1, s2, n);
}
SPRT_FORCEINLINE char *strcat(char *s1, const char *s2) { return __builtin_strcat(s1, s2); }
SPRT_FORCEINLINE char *strncat(char *s1, const char *s2, size_t n) {
	return __builtin_strncat(s1, s2, n);
}
SPRT_FORCEINLINE int memcmp(const void *s1, const void *s2, size_t n) {
	return __builtin_memcmp(s1, s2, n);
}
SPRT_FORCEINLINE int strcoll(const char *s1, const char *s2) { return __sprt_strcoll(s1, s2); }

SPRT_FORCEINLINE size_t strxfrm(char *s1, const char *s2, size_t n) {
	return __sprt_strxfrm(s1, s2, n);
}
SPRT_FORCEINLINE const void *memchr(const void *s, int c, size_t n) {
	return __builtin_memchr(s, c, n);
}
SPRT_FORCEINLINE void *memchr(void *s, int c, size_t n) { return __builtin_memchr(s, c, n); }
SPRT_FORCEINLINE const char *strchr(const char *s, int c) { return __builtin_strchr(s, c); }
SPRT_FORCEINLINE char *strchr(char *s, int c) { return __builtin_strchr(s, c); }
SPRT_FORCEINLINE size_t strcspn(const char *s1, const char *s2) {
	return __builtin_strcspn(s1, s2);
}
SPRT_FORCEINLINE const char *strpbrk(const char *s1, const char *s2) {
	return __builtin_strpbrk(s1, s2);
}
SPRT_FORCEINLINE char *strpbrk(char *s1, const char *s2) { return __builtin_strpbrk(s1, s2); }
SPRT_FORCEINLINE const char *strrchr(const char *s, int c) { return __builtin_strrchr(s, c); }
SPRT_FORCEINLINE char *strrchr(char *s, int c) { return __builtin_strrchr(s, c); }
SPRT_FORCEINLINE size_t strspn(const char *s1, const char *s2) { return __builtin_strspn(s1, s2); }
SPRT_FORCEINLINE const char *strstr(const char *s1, const char *s2) {
	return __builtin_strstr(s1, s2);
}
SPRT_FORCEINLINE char *strstr(char *s1, const char *s2) { return __builtin_strstr(s1, s2); }
SPRT_FORCEINLINE char *strtok(char *s1, const char *s2) { return __sprt_strtok(s1, s2); }
SPRT_FORCEINLINE void *memset(void *s, int c, size_t n) { return __builtin_memset(s, c, n); }
SPRT_FORCEINLINE char *strerror(int errnum) { return __sprt_strerror(errnum); }

SPRT_FORCEINLINE constexpr size_t strlen(const char *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const char16_t *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const char32_t *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const wchar_t *s) { return __constexpr_strlen(s); }

SPRT_FORCEINLINE int strcmp(const char *s1, const char *s2) { return __builtin_strcmp(s1, s2); }

SPRT_FORCEINLINE int strncmp(const char *s1, const char *s2, size_t n) {
	return __builtin_strncmp(s1, s2, n);
}
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_CSTRING_H_
