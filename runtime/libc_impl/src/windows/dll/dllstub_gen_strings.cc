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
	return reinterpret_cast<decltype(&::memcmp)>(SPWIN_NTDLL_PRELOADED_FN(loader, memcmp))(s1, s2, len);
}

__SPRT_C_FUNC void *memcpy(void *__SPRT_RESTRICT dest, const void *__SPRT_RESTRICT source,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memcpy)>(SPWIN_NTDLL_PRELOADED_FN(loader, memcpy))(dest, source, size);
}

__SPRT_C_FUNC void *memmove(void *dst, const void *src, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memmove)>(SPWIN_NTDLL_PRELOADED_FN(loader, memmove))(dst, src, len);
}

__SPRT_C_FUNC void *memset(void *dst, int c, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::memset)>(SPWIN_NTDLL_PRELOADED_FN(loader, memset))(dst, c, len);
}

__SPRT_C_FUNC char *strcpy(char *__SPRT_RESTRICT dest,
		const char *__SPRT_RESTRICT src) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strcpy)>(SPWIN_NTDLL_PRELOADED_FN(loader, strcpy))(dest, src);
}

__SPRT_C_FUNC size_t strlen(const char *s) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strlen)>(SPWIN_NTDLL_PRELOADED_FN(loader, strlen))(s);
}

__SPRT_C_FUNC char *strncpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strncpy)>(SPWIN_NTDLL_PRELOADED_FN(loader, strncpy))(dest, src, size);
}

__SPRT_C_FUNC size_t strnlen(const char *s, size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strnlen)>(SPWIN_NTDLL_PRELOADED_FN(loader, strnlen))(s, size);
}

__SPRT_C_FUNC const char *strstr(const char *str, const char *nstr) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strstr)>(SPWIN_NTDLL_PRELOADED_FN(loader, strstr))(str, nstr);
}

__SPRT_C_FUNC char *strchr(const char *s, int c) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strchr)>(SPWIN_NTDLL_PRELOADED_FN(loader, strchr))(s, c);
}

__SPRT_C_FUNC int strcmp(const void *s1, const void *s2) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strcmp)>(SPWIN_NTDLL_PRELOADED_FN(loader, strcmp))(s1, s2);
}

__SPRT_C_FUNC int strncmp(const void *s1, const void *s2, size_t len) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::strncmp)>(SPWIN_NTDLL_PRELOADED_FN(loader, strncmp))(s1, s2, len);
}

__SPRT_C_FUNC wchar_t *wcscpy(wchar_t *__SPRT_RESTRICT dest,
		const wchar_t *__SPRT_RESTRICT src) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcscpy)>(SPWIN_NTDLL_PRELOADED_FN(loader, wcscpy))(dest, src);
}

__SPRT_C_FUNC size_t wcslen(const wchar_t *s) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcslen)>(SPWIN_NTDLL_PRELOADED_FN(loader, wcslen))(s);
}

__SPRT_C_FUNC wchar_t *wcsncpy(wchar_t *__SPRT_RESTRICT dest, const wchar_t *__SPRT_RESTRICT src,
		size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsncpy)>(SPWIN_NTDLL_PRELOADED_FN(loader, wcsncpy))(dest, src, size);
}

__SPRT_C_FUNC size_t wcsnlen(const wchar_t *s, size_t size) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsnlen)>(SPWIN_NTDLL_PRELOADED_FN(loader, wcsnlen))(s, size);
}

__SPRT_C_FUNC const wchar_t *wcsstr(const wchar_t *str, const wchar_t *nstr) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return reinterpret_cast<decltype(&::wcsstr)>(SPWIN_NTDLL_PRELOADED_FN(loader, wcsstr))(str, nstr);
}
}
