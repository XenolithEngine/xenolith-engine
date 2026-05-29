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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_MALLOC_H_
#define CORE_RUNTIME_INCLUDE_LIBC_MALLOC_H_

/*
	Dispatch header for <malloc.h> (a non-standard, allocator-focused header):
	- hosted SPRT build      -> forwards to the system <malloc.h> (#include_next)
	- __SPRT_AS_STD in C++    -> includes <cstdlib>
	- otherwise               -> includes <stdlib.h>

	This header defines no functions of its own; the allocation family (malloc,
	calloc, realloc, free, aligned_alloc, aligned_free, ...) comes from the stdlib
	header it pulls in. It only adds two MSVC-compatibility macros:

	Macros:
	  _aligned_malloc(Size, Align) - allocate Size bytes aligned to Align;
	                                 maps to aligned_alloc(Align, Size)
	                                 (sprt::aligned_alloc under __SPRT_AS_STD)
	  _aligned_free(Ptr)           - free a block from _aligned_malloc;
	                                 maps to aligned_free (sprt::aligned_free
	                                 under __SPRT_AS_STD)

	Note: these macros are not defined on the hosted SPRT build path, which uses the
	platform's own <malloc.h>.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <malloc.h>

#elif defined(__SPRT_AS_STD) && defined(__cplusplus)

#include <cstdlib>

#define _aligned_malloc(Size, Align) sprt::aligned_alloc(Align, Size)
#define _aligned_free(Ptr) sprt::aligned_free(Ptr)

#else

#include <stdlib.h>

#define _aligned_malloc(Size, Align) aligned_alloc(Align, Size)
#define _aligned_free(Ptr) aligned_free(Ptr)

#endif

#endif
