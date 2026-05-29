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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_

/*
	Dispatch header for <stdlib.h>:
	- hosted SPRT build      -> forwards to the system <stdlib.h> (#include_next)
	- __SPRT_AS_STD in C++   -> pulls <cstdlib>
	- otherwise              -> SPRT's own declarations via sprt/wrappers/libc/stdlib.h

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded):

	Macros:  NULL, EXIT_SUCCESS, EXIT_FAILURE, RAND_MAX, MB_CUR_MAX,
	         _malloca/_freea (when available), and MSVC locale aliases
	         _strtol_l/_strtoll_l/_strtoul_l/_strtoull_l/_strtof_l/_strtod_l/
	         _strtold_l/_strtoi64_l/_strtoui64_l

	Types:   div_t, ldiv_t, lldiv_t

	ISO C functions:
	  atoi        - convert string to int
	  atol        - convert string to long
	  atoll       - convert string to long long
	  atof        - convert string to double
	  strtof      - parse string to float, report end of parsed text
	  strtod      - parse string to double, report end of parsed text
	  strtold     - parse string to long double, report end of parsed text
	  strtol      - parse string to long in given base, report end
	  strtoul     - parse string to unsigned long in given base, report end
	  strtoll     - parse string to long long in given base, report end
	  strtoull    - parse string to unsigned long long in given base, report end
	  rand        - pseudo-random int in [0, RAND_MAX]
	  srand       - seed the rand() generator
	  malloc      - allocate uninitialized memory
	  calloc      - allocate zero-initialized array
	  realloc     - resize an existing allocation
	  free        - release an allocation
	  aligned_alloc - allocate memory with a given alignment
	  free_sized  - free an allocation of known size (C23)
	  free_aligned_sized - free an aligned allocation of known size/alignment (C23)
	  abort       - abnormal termination (raises SIGABRT)
	  atexit      - register a handler to run at normal exit
	  exit        - normal termination, runs atexit handlers
	  _Exit       - immediate termination, skips handlers/flush
	  at_quick_exit - register a handler for quick_exit
	  quick_exit  - terminate via at_quick_exit handlers
	  getenv      - look up an environment variable
	  system      - execute a shell command
	  bsearch     - binary search over a sorted array
	  qsort       - sort an array in place
	  abs         - absolute value of int (C / freestanding only)
	  labs        - absolute value of long (C / freestanding only)
	  llabs       - absolute value of long long (C / freestanding only)
	  div         - int division returning quotient and remainder
	  ldiv        - long division returning quotient and remainder
	  lldiv       - long long division returning quotient and remainder
	  mbstowcs    - convert multibyte string to wide string
	  mbtowc      - convert one multibyte char to a wide char
	  wctomb      - convert one wide char to a multibyte char
	  wcstombs    - convert wide string to multibyte string

	POSIX / extensions:
	  posix_memalign - allocate aligned memory via an out-pointer
	  aligned_free   - free aligned_alloc memory (MSVC-safe pairing)
	  qsort_r        - qsort variant taking a user context pointer
	  setenv         - set an environment variable
	  unsetenv       - remove an environment variable
	  mkstemp        - create and open a unique temporary file
	  mkostemp       - mkstemp with additional open() flags
	  mkdtemp        - create a unique temporary directory
	  getsubopt      - parse comma-separated suboption strings
	  rand_r         - reentrant rand() using caller-held state
	  realpath       - resolve a canonical absolute pathname
	  random         - higher-quality pseudo-random long
	  srandom        - seed the random() generator
	  strtol_l/strtoll_l/strtoul_l/strtoull_l/strtof_l/strtod_l/strtold_l
	                 - locale-aware variants of the strto* parsers

	MSVC CRT extensions:
	  _byteswap_ushort - reverse the bytes of a 16-bit value
	  _byteswap_ulong  - reverse the bytes of a 32-bit value
	  _byteswap_uint64 - reverse the bytes of a 64-bit value
	  qsort_s          - qsort with a context pointer and bounds checking
	  getenv_s         - bounds-checked getenv into a caller buffer
	  _msize           - report the usable size of a heap allocation
	  _wgetenv         - wide-character getenv

	In hosted C++ the ISO functions live in sprt::_cstdlib (and std:: under
	__SPRT_AS_STD); in freestanding / non-SPRT-build mode they are plain C
	declarations. CRT extensions and the locale-alias macros are always declared.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stdlib.h>

#elif defined(__SPRT_AS_STD) && defined(__cplusplus)

#include <cstdlib>

#else

#include <sprt/wrappers/libc/stdlib.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_
