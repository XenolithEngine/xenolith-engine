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
	- otherwise               -> includes <stdlib.h>

	This header defines no functions of its own; the allocation family (malloc,
	calloc, realloc, free, aligned_alloc, aligned_free, ...) comes from the stdlib
	header it pulls in. It only adds two MSVC-compatibility macros:

	Macros:
	  _aligned_malloc(Size, Align) - allocate Size bytes aligned to Align;
	                                 maps to aligned_alloc(Align, Size)
	  _aligned_free(Ptr)           - free a block from _aligned_malloc;
	                                 maps to aligned_free

	Note: these macros are not defined on the hosted SPRT build path, which uses the
	platform's own <malloc.h>.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <malloc.h>

#else

#include <stdlib.h>

// <stdlib.h> already surfaces these on Windows (mirroring MSVC's corecrt); guard so
// including <malloc.h> after <stdlib.h> is not a macro redefinition.
#ifndef _aligned_malloc
#define _aligned_malloc(Size, Align) aligned_alloc(Align, Size)
#endif
#ifndef _aligned_free
#define _aligned_free(Ptr) aligned_free(Ptr)
#endif

// MSVC heap-walk API (_HEAPINFO / _heapwalk), used by llvm's Process.inc
// GetMallocUsage. mimalloc is not a walkable free-list, so instead of enumerating
// blocks we report the whole current heap usage (__sprt_malloc_usage, summed by
// mimalloc's mi_heap_visit_blocks in the runtime) as a single virtual entry, then
// end. A caller that sums _size across the walk gets the real total.
#ifndef _HEAPOK
typedef struct _heapinfo {
	int *_pentry;
	size_t _size;
	int _useflag;
} _HEAPINFO;

#define _HEAPEMPTY (-1)
#define _HEAPOK (-2)
#define _HEAPBADBEGIN (-3)
#define _HEAPBADNODE (-4)
#define _HEAPEND (-5)
#define _HEAPBADPTR (-6)
#define _FREEENTRY 0
#define _USEDENTRY 1

#ifdef __cplusplus
extern "C" {
#endif
size_t __sprt_malloc_usage(void);
#ifdef __cplusplus
}
#endif

static inline int _heapwalk(_HEAPINFO *_EntryInfo) {
	if (_EntryInfo->_pentry == 0) {
		_EntryInfo->_pentry = (int *) (size_t) 1; // sentinel: total already reported
		_EntryInfo->_size = __sprt_malloc_usage();
		_EntryInfo->_useflag = _USEDENTRY;
		return _HEAPOK;
	}
	return _HEAPEND;
}
#endif // _HEAPOK

#endif

#endif
