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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
#define CORE_RUNTIME_INCLUDE_LIBC_STRING_H_

/*
	Dispatch header for <string.h>:
	- hosted SPRT build -> forwards to the system <string.h> (#include_next)
	- otherwise         -> SPRT's own declarations via sprt/wrappers/libc/string.h
	                       (which in C++ routes into sprt::_cstring_dll + sprt::_cstring,
	                       and into std:: under __SPRT_AS_STD)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	NULL comes in via <stddef.h>; no other public macros or types are defined here.
	On Windows, when __SPRT_WIN_USE_IMPORT_STRING_LIB is set, the core memory and
	string functions are taken from a DLL import and are not redeclared here.

	ISO C functions:
	  memcpy      - copy non-overlapping memory
	  memmove     - copy possibly-overlapping memory
	  memset      - fill memory with a byte value
	  memcmp      - compare two memory blocks
	  memchr      - find a byte within a memory block
	  strcpy      - copy a NUL-terminated string
	  strncpy     - copy at most n characters
	  strcat      - append one string to another
	  strncat     - append at most n characters
	  strcmp      - compare two strings
	  strncmp     - compare at most n characters
	  strcoll     - locale-aware string comparison
	  strxfrm     - transform a string for locale-aware comparison
	  strchr      - find the first occurrence of a character
	  strrchr     - find the last occurrence of a character
	  strspn      - length of the initial run of accepted characters
	  strcspn     - length of the initial run of rejected characters
	  strpbrk     - find the first character from a given set
	  strstr      - find a substring
	  strtok      - split a string into tokens (stateful)
	  strlen      - length of a NUL-terminated string
	  strerror    - message string for an errno value

	POSIX / extensions:
	  strnlen     - length bounded by a maximum
	  strtok_r    - reentrant strtok with caller-held state
	  strverscmp  - version-aware (natural-order) comparison (GNU)
	  strdup      - duplicate a string into a new allocation
	  _strdup     - MSVC-named alias of strdup
	  strcasecmp  - case-insensitive comparison
	  strncasecmp - case-insensitive comparison of at most n characters
	  strcasecmp_l/strncasecmp_l - locale-aware case-insensitive comparisons

	C++-only additions (in the sprt namespaces):
	  constexpr strlen/strnlen overloads for char16_t, char32_t and wchar_t

	MSVC bounds-checked extensions:
	  strerror_s  - bounds-checked strerror into a caller buffer
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <string.h>

#else

#include <sprt/wrappers/libc/string.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
