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

// Global Stappler Runtime configuration

/*
	Global configuration options have default settings and can be overridden at compile time.
	When compiled and used, the values ​​must match
*/

/*
	Stappler runtime is designed to work in conjunction with a compiler in the toolchain,
	therefore, if the compiler has preferences regarding limits, we use them first

	If __SPRT_CONFIG_COMPILER_ASSISTED_INTS = 1, int types and limits will
	be acquired from compiler's definition when possible
*/
#ifndef __SPRT_CONFIG_COMPILER_ASSISTED_INTS
#define __SPRT_CONFIG_COMPILER_ASSISTED_INTS 1
#endif

/*
	If a function is not available on the platform, we can either not define it in the API,
	or when calling it, report unavailability via errno (ENOSYS)
	If __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS = 0 - functions are not defined
	If __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS = 1 - the option with errno is used
*/
#ifndef __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
#define __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS 1
#endif

/*
	Use alloca or new[] for temporary allocations
*/
#ifndef __SPRT_CONFIG_USE_ALLOCA_FOR_TEMPORRY
#define __SPRT_CONFIG_USE_ALLOCA_FOR_TEMPORRY 1
#endif

/*
	Define library functions as inlined builtins where possible
*/
#ifndef __SPRT_CONFIG_BUILTIN_INLINES
#define __SPRT_CONFIG_BUILTIN_INLINES 0
#endif

/*
	To work with llvm-libc++, you need to disable receiving symbols from the standard library
	This must be specified during compilation of llvm-libc++
*/
#ifndef _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
#define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
#endif
