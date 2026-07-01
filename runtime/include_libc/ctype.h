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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_
#define CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_

/*
	Dispatch header for <ctype.h>:
	- hosted SPRT build -> forwards to the system <ctype.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/ctype.h

	This header declares only functions/macros (no public types). In C++ the
	classification predicates and tolower_c/toupper_c are constexpr overloads in
	namespace sprt::_cctype (pulled into the global scope when hosted-and-not-SPRT-
	build, and into std:: via the include_libc/stl wrappers). Note: the runtime's primary locale is
	UTF-8, so classifying 8-bit characters needs no locale; the _l functions consult an
	explicit locale only if the underlying libc supports others.

	Classification predicates (return non-zero on match):
	  isalnum  - alphanumeric        isalpha  - alphabetic
	  isblank  - blank (space/tab)   iscntrl  - control character
	  isdigit  - decimal digit       isgraph  - printable, non-space
	  islower  - lowercase           isprint  - printable (incl. space)
	  ispunct  - punctuation         isspace  - whitespace
	  isupper  - uppercase           isxdigit - hexadecimal digit
	  isascii  - 7-bit ASCII value (XSI legacy)

	Case / value conversion:
	  tolower  - convert a character to lowercase
	  toupper  - convert a character to uppercase
	  toascii  - mask a value down to 7-bit ASCII (XSI legacy)

	Locale-aware variants (each takes an explicit locale_t):
	  isalnum_l, isalpha_l, isblank_l, iscntrl_l, isdigit_l, isgraph_l, islower_l,
	  isprint_l, ispunct_l, isspace_l, isupper_l, isxdigit_l, tolower_l, toupper_l
	  (MSVC-named aliases _isalnum_l ... _toupper_l map onto the above)

	C++-only additions (in the sprt namespaces):
	  constexpr tolower_c / toupper_c overloads for char, char16_t and char32_t
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <ctype.h>

#else

#include <sprt/wrappers/libc/ctype.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_
