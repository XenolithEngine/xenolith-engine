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

#include <sprt/wrappers/windows/windows.h>

#include "dllloader.h"

// Preloaded string functions

extern "C" {
__SPRT_C_FUNC int memcmp(const void *s1, const void *s2, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memcmp)>(loader->ntdll.memcmp.fn)(s1, s2, len);
}

__SPRT_C_FUNC void *memcpy(void *__SPRT_RESTRICT dest, const void *__SPRT_RESTRICT source,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memcpy)>(loader->ntdll.memcpy.fn)(dest, source, size);
}

__SPRT_C_FUNC void *memmove(void *dst, const void *src, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memmove)>(loader->ntdll.memmove.fn)(dst, src, len);
}

__SPRT_C_FUNC void *memset(void *dst, int c, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memset)>(loader->ntdll.memset.fn)(dst, c, len);
}

__SPRT_C_FUNC char *strcpy(char *__SPRT_RESTRICT dest,
		const char *__SPRT_RESTRICT src) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strcpy)>(loader->ntdll.strcpy.fn)(dest, src);
}

__SPRT_C_FUNC size_t strlen(const char *s) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strlen)>(loader->ntdll.strlen.fn)(s);
}

__SPRT_C_FUNC char *strncpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strncpy)>(loader->ntdll.strncpy.fn)(dest, src, size);
}

__SPRT_C_FUNC size_t strnlen(const char *s, size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strnlen)>(loader->ntdll.strnlen.fn)(s, size);
}

__SPRT_C_FUNC const char *strstr(const char *str, const char *nstr) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strstr)>(loader->ntdll.strstr.fn)(str, nstr);
}

__SPRT_C_FUNC char *strchr(const char *s, int c) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strchr)>(loader->ntdll.strchr.fn)(s, c);
}

__SPRT_C_FUNC int strcmp(const void *s1, const void *s2) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strcmp)>(loader->ntdll.strcmp.fn)(s1, s2);
}

__SPRT_C_FUNC int strncmp(const void *s1, const void *s2, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strncmp)>(loader->ntdll.strncmp.fn)(s1, s2, len);
}

__SPRT_C_FUNC wchar_t *wcscpy(wchar_t *__SPRT_RESTRICT dest,
		const wchar_t *__SPRT_RESTRICT src) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcscpy)>(loader->ntdll.wcscpy.fn)(dest, src);
}

__SPRT_C_FUNC size_t wcslen(const wchar_t *s) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcslen)>(loader->ntdll.wcslen.fn)(s);
}

__SPRT_C_FUNC wchar_t *wcsncpy(wchar_t *__SPRT_RESTRICT dest, const wchar_t *__SPRT_RESTRICT src,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsncpy)>(loader->ntdll.wcsncpy.fn)(dest, src, size);
}

__SPRT_C_FUNC size_t wcsnlen(const wchar_t *s, size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsnlen)>(loader->ntdll.wcsnlen.fn)(s, size);
}

__SPRT_C_FUNC const wchar_t *wcsstr(const wchar_t *str, const wchar_t *nstr) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsstr)>(loader->ntdll.wcsstr.fn)(str, nstr);
}
}
