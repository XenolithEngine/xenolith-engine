/*
   LZ4 - Fast LZ compression algorithm
   Copyright (C) 2011-2023, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/

/*-************************************
*  Tuning parameters
**************************************/
/*
 * LZ4_HEAPMODE :
 * Select how stateless compression functions like `LZ4_compress_default()`
 * allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#ifndef LZ4_HEAPMODE
#  define LZ4_HEAPMODE 0
#endif

/*
 * LZ4_ACCELERATION_DEFAULT :
 * Select "acceleration" for LZ4_compress_fast() when parameter value <= 0
 */
#define LZ4_ACCELERATION_DEFAULT 1
/*
 * LZ4_ACCELERATION_MAX :
 * Any "acceleration" value higher than this threshold
 * get treated as LZ4_ACCELERATION_MAX instead (fix #876)
 */
#define LZ4_ACCELERATION_MAX 65537


/*-************************************
*  CPU Feature Detection
**************************************/
/* LZ4_FORCE_MEMORY_ACCESS
 * By default, access to unaligned memory is controlled by `memcpy()`, which is safe and portable.
 * Unfortunately, on some target/compiler combinations, the generated assembly is sub-optimal.
 * The below switch allow to select different access method for improved performance.
 * Method 0 (default) : use `memcpy()`. Safe and portable.
 * Method 1 : `__packed` statement. It depends on compiler extension (ie, not portable).
 *            This method is safe if your compiler supports it, and *generally* as fast or faster than `memcpy`.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets which assembly generation depends on alignment.
 *            But in some circumstances, it's the only known way to get the most performance (ie GCC + ARMv6)
 * See https://fastcompression.blogspot.fr/2015/08/accessing-unaligned-memory.html for details.
 * Prefer these methods in priority order (0 > 1 > 2)
 */
#ifndef LZ4_FORCE_MEMORY_ACCESS   /* can be defined externally */
#  if defined(__GNUC__) && \
  ( defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
  || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) )
#    define LZ4_FORCE_MEMORY_ACCESS 2
#  elif (defined(__INTEL_COMPILER) && !defined(_WIN32)) || defined(__GNUC__) || defined(_MSC_VER)
#    define LZ4_FORCE_MEMORY_ACCESS 1
#  endif
#endif

/*
 * LZ4_FORCE_SW_BITCOUNT
 * Define this parameter if your target system or compiler does not support hardware bit count
 */
#if defined(_MSC_VER) && defined(_WIN32_WCE)   /* Visual Studio for WinCE doesn't support Hardware bit count */
#  undef  LZ4_FORCE_SW_BITCOUNT  /* avoid double def */
#  define LZ4_FORCE_SW_BITCOUNT
#endif



/*-************************************
*  Dependency
**************************************/
/*
 * LZ4_SRC_INCLUDED:
 * Amalgamation flag, whether lz4.c is included
 */
#ifndef LZ4_SRC_INCLUDED
#  define LZ4_SRC_INCLUDED 1
#endif

#ifndef LZ4_DISABLE_DEPRECATE_WARNINGS
#  define LZ4_DISABLE_DEPRECATE_WARNINGS /* due to LZ4_decompress_safe_withPrefix64k */
#endif

#ifndef LZ4_STATIC_LINKING_ONLY
#  define LZ4_STATIC_LINKING_ONLY
#endif
#include "lz4.h"
/* see also "memory routines" below */


/*-************************************
*  Compiler Options
**************************************/
#if defined(_MSC_VER) && (_MSC_VER >= 1400)  /* Visual Studio 2005+ */
#  include <intrin.h>               /* only present in VS2005+ */
#  pragma warning(disable : 4127)   /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 6237)   /* disable: C6237: conditional expression is always 0 */
#  pragma warning(disable : 6239)   /* disable: C6239: (<non-zero constant> && <expression>) always evaluates to the result of <expression> */
#  pragma warning(disable : 6240)   /* disable: C6240: (<expression> && <non-zero constant>) always evaluates to the result of <expression> */
#  pragma warning(disable : 6326)   /* disable: C6326: Potential comparison of a constant with another constant */
#endif  /* _MSC_VER */

#ifndef LZ4_FORCE_INLINE
#  if defined (_MSC_VER) && !defined (__clang__)    /* MSVC */
#    define LZ4_FORCE_INLINE static __forceinline
#  else
#    if defined (__cplusplus) || defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
#      if defined (__GNUC__) || defined (__clang__)
#        define LZ4_FORCE_INLINE static inline __attribute__((always_inline))
#      else
#        define LZ4_FORCE_INLINE static inline
#      endif
#    else
#      define LZ4_FORCE_INLINE static
#    endif /* __STDC_VERSION__ */
#  endif  /* _MSC_VER */
#endif /* LZ4_FORCE_INLINE */

/* LZ4_FORCE_O2 and LZ4_FORCE_INLINE
 * gcc on ppc64le generates an unrolled SIMDized loop for LZ4_wildCopy8,
 * together with a simple 8-byte copy loop as a fall-back path.
 * However, this optimization hurts the decompression speed by >30%,
 * because the execution does not go to the optimized loop
 * for typical compressible data, and all of the preamble checks
 * before going to the fall-back path become useless overhead.
 * This optimization happens only with the -O3 flag, and -O2 generates
 * a simple 8-byte copy loop.
 * With gcc on ppc64le, all of the LZ4_decompress_* and LZ4_wildCopy8
 * functions are annotated with __attribute__((optimize("O2"))),
 * and also LZ4_wildCopy8 is forcibly inlined, so that the O2 attribute
 * of LZ4_wildCopy8 does not affect the compression speed.
 */
#if defined(__PPC64__) && defined(__LITTLE_ENDIAN__) && defined(__GNUC__) && !defined(__clang__)
#  define LZ4_FORCE_O2  __attribute__((optimize("O2")))
#  undef LZ4_FORCE_INLINE
#  define LZ4_FORCE_INLINE  static __inline __attribute__((optimize("O2"),always_inline))
#else
#  define LZ4_FORCE_O2
#endif

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#ifndef likely
#define likely(expr)     expect((expr) != 0, 1)
#endif
#ifndef unlikely
#define unlikely(expr)   expect((expr) != 0, 0)
#endif

/* Should the alignment test prove unreliable, for some reason,
 * it can be disabled by setting LZ4_ALIGN_TEST to 0 */
#ifndef LZ4_ALIGN_TEST  /* can be externally provided */
# define LZ4_ALIGN_TEST 1
#endif


/*-************************************
*  Memory routines
**************************************/

/*! LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION :
 *  Disable relatively high-level LZ4/HC functions that use dynamic memory
 *  allocation functions (malloc(), calloc(), free()).
 *
 *  Note that this is a compile-time switch. And since it disables
 *  public/stable LZ4 v1 API functions, we don't recommend using this
 *  symbol to generate a library for distribution.
 *
 *  The following public functions are removed when this symbol is defined.
 *  - lz4   : LZ4_createStream, LZ4_freeStream,
 *            LZ4_createStreamDecode, LZ4_freeStreamDecode, LZ4_create (deprecated)
 *  - lz4hc : LZ4_createStreamHC, LZ4_freeStreamHC,
 *            LZ4_createHC (deprecated), LZ4_freeHC  (deprecated)
 *  - lz4frame, lz4file : All LZ4F_* functions
 */
#if defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
#  define ALLOC(s)          lz4_error_memory_allocation_is_disabled
#  define ALLOC_AND_ZERO(s) lz4_error_memory_allocation_is_disabled
#  define FREEMEM(p)        lz4_error_memory_allocation_is_disabled
#elif defined(LZ4_USER_MEMORY_FUNCTIONS)
/* memory management functions can be customized by user project.
 * Below functions must exist somewhere in the Project
 * and be available at link time */
void* LZ4_malloc(size_t s);
void* LZ4_calloc(size_t n, size_t s);
void  LZ4_free(void* p);
# define ALLOC(s)          LZ4_malloc(s)
# define ALLOC_AND_ZERO(s) LZ4_calloc(1,s)
# define FREEMEM(p)        LZ4_free(p)
#else
# include <stdlib.h>   /* malloc, calloc, free */
# define ALLOC(s)          malloc(s)
# define ALLOC_AND_ZERO(s) calloc(1,s)
# define FREEMEM(p)        free(p)
#endif

#if ! LZ4_FREESTANDING
#  include <string.h>   /* memset, memcpy */
#endif
#if !defined(LZ4_memset)
#  define LZ4_memset(p,v,s) memset((p),(v),(s))
#endif
#define MEM_INIT(p,v,s)   LZ4_memset((p),(v),(s))


/*-************************************
*  Common Constants
**************************************/
#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS   5   /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MFLIMIT       12   /* see ../doc/lz4_Block_format.md#parsing-restrictions */
#define MATCH_SAFEGUARD_DISTANCE  ((2*WILDCOPYLENGTH) - MINMATCH)   /* ensure it's possible to write 2 x wildcopyLength without overflowing output buffer */
#define FASTLOOP_SAFE_DISTANCE 64
static const int LZ4_minLength = (MFLIMIT+1);

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define LZ4_DISTANCE_ABSOLUTE_MAX 65535
#if (LZ4_DISTANCE_MAX > LZ4_DISTANCE_ABSOLUTE_MAX)   /* max supported by LZ4 format */
#  error "LZ4_DISTANCE_MAX is too big : must be <= 65535"
#endif

#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)


/*-************************************
*  Error detection
**************************************/
#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=1)
#  include <assert.h>
#else
#  ifndef assert
#    define assert(condition) ((void)0)
#  endif
#endif

#define LZ4_STATIC_ASSERT(c)   { enum { LZ4_static_assert = 1/(int)(!!(c)) }; }   /* use after variable declarations */

#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=2)
#  include <stdio.h>
   static int g_debuglog_enable = 1;
#  define DEBUGLOG(l, ...) {                          \
        if ((g_debuglog_enable) && (l<=LZ4_DEBUG)) {  \
            fprintf(stderr, __FILE__  " %i: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);             \
            fprintf(stderr, " \n");                   \
    }   }
#else
#  define DEBUGLOG(l, ...) {}    /* disabled */
#endif

static int LZ4_isAligned(const void* ptr, size_t alignment)
{
    return ((size_t)ptr & (alignment -1)) == 0;
}


/*-************************************
*  Types
**************************************/
#include <limits.h>
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# include <stdint.h>
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef uintptr_t uptrval;
#else
# if UINT_MAX != 4294967295UL
#   error "LZ4 code (when not C++ or C99) assumes that sizeof(int) == 4"
# endif
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef size_t              uptrval;   /* generally true, except OpenVMS-64 */
#endif

#if defined(__x86_64__)
  typedef U64    reg_t;   /* 64-bits in x32 mode */
#else
  typedef size_t reg_t;   /* 32-bits in x32 mode */
#endif

typedef enum {
    notLimited = 0,
    limitedOutput = 1,
    fillOutput = 2
} limitedOutput_directive;


/*-************************************
*  Reading and writing into memory
**************************************/

/**
 * LZ4 relies on memcpy with a constant size being inlined. In freestanding
 * environments, the compiler can't assume the implementation of memcpy() is
 * standard compliant, so it can't apply its specialized memcpy() inlining
 * logic. When possible, use __builtin_memcpy() to tell the compiler to analyze
 * memcpy() as if it were standard compliant, so it can inline it in freestanding
 * environments. This is needed when decompressing the Linux Kernel, for example.
 */
#if !defined(LZ4_memcpy)
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4_memcpy(dst, src, size) __builtin_memcpy(dst, src, size)
#  else
#    define LZ4_memcpy(dst, src, size) memcpy(dst, src, size)
#  endif
#endif

#if !defined(LZ4_memmove)
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4_memmove __builtin_memmove
#  else
#    define LZ4_memmove memmove
#  endif
#endif

static unsigned LZ4_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental */
    return one.c[0];
}

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define LZ4_PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define LZ4_PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==2)
/* lie to the compiler about data alignment; use with caution */

static U16 LZ4_read16(const void* memPtr) { return *(const U16*) memPtr; }
static U32 LZ4_read32(const void* memPtr) { return *(const U32*) memPtr; }
static reg_t LZ4_read_ARCH(const void* memPtr) { return *(const reg_t*) memPtr; }

static void LZ4_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
static void LZ4_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }

#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
LZ4_PACK(typedef struct { U16 u16; }) LZ4_unalign16;
LZ4_PACK(typedef struct { U32 u32; }) LZ4_unalign32;
LZ4_PACK(typedef struct { reg_t uArch; }) LZ4_unalignST;

static U16 LZ4_read16(const void* ptr) { return ((const LZ4_unalign16*)ptr)->u16; }
static U32 LZ4_read32(const void* ptr) { return ((const LZ4_unalign32*)ptr)->u32; }
static reg_t LZ4_read_ARCH(const void* ptr) { return ((const LZ4_unalignST*)ptr)->uArch; }

static void LZ4_write16(void* memPtr, U16 value) { ((LZ4_unalign16*)memPtr)->u16 = value; }
static void LZ4_write32(void* memPtr, U32 value) { ((LZ4_unalign32*)memPtr)->u32 = value; }

#else  /* safe and portable access using memcpy() */

static U16 LZ4_read16(const void* memPtr)
{
    U16 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static U32 LZ4_read32(const void* memPtr)
{
    U32 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static reg_t LZ4_read_ARCH(const void* memPtr)
{
    reg_t val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

static void LZ4_write16(void* memPtr, U16 value)
{
    LZ4_memcpy(memPtr, &value, sizeof(value));
}

static void LZ4_write32(void* memPtr, U32 value)
{
    LZ4_memcpy(memPtr, &value, sizeof(value));
}

#endif /* LZ4_FORCE_MEMORY_ACCESS */


static U16 LZ4_readLE16(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read16(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)((U16)p[0] | (p[1]<<8));
    }
}

#ifdef LZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT
static U32 LZ4_readLE32(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read32(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U32)p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    }
}
#endif

static void LZ4_writeLE16(void* memPtr, U16 value)
{
    if (LZ4_isLittleEndian()) {
        LZ4_write16(memPtr, value);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE) value;
        p[1] = (BYTE)(value>>8);
    }
}

/* customized variant of memcpy, which can overwrite up to 8 bytes beyond dstEnd */
LZ4_FORCE_INLINE
void LZ4_wildCopy8(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { LZ4_memcpy(d,s,8); d+=8; s+=8; } while (d<e);
}

static const unsigned inc32table[8] = {0, 1, 2,  1,  0,  4, 4, 4};
static const int      dec64table[8] = {0, 0, 0, -1, -4,  1, 2, 3};


#ifndef LZ4_FAST_DEC_LOOP
#  if defined __i386__ || defined _M_IX86 || defined __x86_64__ || defined _M_X64
#    define LZ4_FAST_DEC_LOOP 1
#  elif defined(__aarch64__) && defined(__APPLE__)
#    define LZ4_FAST_DEC_LOOP 1
#  elif defined(__aarch64__) && !defined(__clang__)
     /* On non-Apple aarch64, we disable this optimization for clang because
      * on certain mobile chipsets, performance is reduced with clang. For
      * more information refer to https://github.com/lz4/lz4/pull/707 */
#    define LZ4_FAST_DEC_LOOP 1
#  else
#    define LZ4_FAST_DEC_LOOP 0
#  endif
#endif

#if LZ4_FAST_DEC_LOOP

LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset_base(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    assert(srcPtr + offset == dstPtr);
    if (offset < 8) {
        LZ4_write32(dstPtr, 0);   /* silence an msan warning when offset==0 */
        dstPtr[0] = srcPtr[0];
        dstPtr[1] = srcPtr[1];
        dstPtr[2] = srcPtr[2];
        dstPtr[3] = srcPtr[3];
        srcPtr += inc32table[offset];
        LZ4_memcpy(dstPtr+4, srcPtr, 4);
        srcPtr -= dec64table[offset];
        dstPtr += 8;
    } else {
        LZ4_memcpy(dstPtr, srcPtr, 8);
        dstPtr += 8;
        srcPtr += 8;
    }

    LZ4_wildCopy8(dstPtr, srcPtr, dstEnd);
}

/* customized variant of memcpy, which can overwrite up to 32 bytes beyond dstEnd
 * this version copies two times 16 bytes (instead of one time 32 bytes)
 * because it must be compatible with offsets >= 16. */
LZ4_FORCE_INLINE void
LZ4_wildCopy32(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { LZ4_memcpy(d,s,16); LZ4_memcpy(d+16,s+16,16); d+=32; s+=32; } while (d<e);
}

/* LZ4_memcpy_using_offset()  presumes :
 * - dstEnd >= dstPtr + MINMATCH
 * - there is at least 12 bytes available to write after dstEnd */
LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    BYTE v[8];

    assert(dstEnd >= dstPtr + MINMATCH);

    switch(offset) {
    case 1:
        MEM_INIT(v, *srcPtr, 8);
        break;
    case 2:
        LZ4_memcpy(v, srcPtr, 2);
        LZ4_memcpy(&v[2], srcPtr, 2);
#if defined(_MSC_VER) && (_MSC_VER <= 1937) /* MSVC 2022 ver 17.7 or earlier */
#  pragma warning(push)
#  pragma warning(disable : 6385) /* warning C6385: Reading invalid data from 'v'. */
#endif
        LZ4_memcpy(&v[4], v, 4);
#if defined(_MSC_VER) && (_MSC_VER <= 1937) /* MSVC 2022 ver 17.7 or earlier */
#  pragma warning(pop)
#endif
        break;
    case 4:
        LZ4_memcpy(v, srcPtr, 4);
        LZ4_memcpy(&v[4], srcPtr, 4);
        break;
    default:
        LZ4_memcpy_using_offset_base(dstPtr, srcPtr, dstEnd, offset);
        return;
    }

    LZ4_memcpy(dstPtr, v, 8);
    dstPtr += 8;
    while (dstPtr < dstEnd) {
        LZ4_memcpy(dstPtr, v, 8);
        dstPtr += 8;
    }
}
#endif


/*-************************************
*  Common functions
**************************************/
static unsigned LZ4_NbCommonBytes (reg_t val)
{
    assert(val != 0);
    if (LZ4_isLittleEndian()) {
        if (sizeof(val) == 8) {
#       if defined(_MSC_VER) && (_MSC_VER >= 1800) && (defined(_M_AMD64) && !defined(_M_ARM64EC)) && !defined(LZ4_FORCE_SW_BITCOUNT)
/*-*************************************************************************************************
* ARM64EC is a Microsoft-designed ARM64 ABI compatible with AMD64 applications on ARM64 Windows 11.
* The ARM64EC ABI does not support AVX/AVX2/AVX512 instructions, nor their relevant intrinsics
* including _tzcnt_u64. Therefore, we need to neuter the _tzcnt_u64 code path for ARM64EC.
****************************************************************************************************/
#         if defined(__clang__) && (__clang_major__ < 10)
            /* Avoid undefined clang-cl intrinsics issue.
             * See https://github.com/lz4/lz4/pull/1017 for details. */
            return (unsigned)__builtin_ia32_tzcnt_u64(val) >> 3;
#         else
            /* x64 CPUS without BMI support interpret `TZCNT` as `REP BSF` */
            return (unsigned)_tzcnt_u64(val) >> 3;
#         endif
#       elif defined(_MSC_VER) && defined(_WIN64) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanForward64(&r, (U64)val);
            return (unsigned)r >> 3;
#       elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_ctzll((U64)val) >> 3;
#       else
            const U64 m = 0x0101010101010101ULL;
            val ^= val - 1;
            return (unsigned)(((U64)((val & (m - 1)) * m)) >> 56);
#       endif
        } else /* 32 bits */ {
#       if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r;
            _BitScanForward(&r, (U32)val);
            return (unsigned)r >> 3;
#       elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                        !defined(__TINYC__) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_ctz((U32)val) >> 3;
#       else
            const U32 m = 0x01010101;
            return (unsigned)((((val - 1) ^ val) & (m - 1)) * m) >> 24;
#       endif
        }
    } else   /* Big Endian CPU */ {
        if (sizeof(val)==8) {
#       if (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                        !defined(__TINYC__) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_clzll((U64)val) >> 3;
#       else
#if 1
            /* this method is probably faster,
             * but adds a 128 bytes lookup table */
            static const unsigned char ctz7_tab[128] = {
                7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
                4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
            };
            U64 const mask = 0x0101010101010101ULL;
            U64 const t = (((val >> 8) - mask) | val) & mask;
            return ctz7_tab[(t * 0x0080402010080402ULL) >> 57];
#else
            /* this method doesn't consume memory space like the previous one,
             * but it contains several branches,
             * that may end up slowing execution */
            static const U32 by32 = sizeof(val)*4;  /* 32 on 64 bits (goal), 16 on 32 bits.
            Just to avoid some static analyzer complaining about shift by 32 on 32-bits target.
            Note that this code path is never triggered in 32-bits mode. */
            unsigned r;
            if (!(val>>by32)) { r=4; } else { r=0; val>>=by32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#endif
#       endif
        } else /* 32 bits */ {
#       if (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
            return (unsigned)__builtin_clz((U32)val) >> 3;
#       else
            val >>= 8;
            val = ((((val + 0x00FFFF00) | 0x00FFFFFF) + val) |
              (val + 0x00FF0000)) >> 24;
            return (unsigned)val ^ 3;
#       endif
        }
    }
}


#define STEPSIZE sizeof(reg_t)
LZ4_FORCE_INLINE
unsigned LZ4_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    if (likely(pIn < pInLimit-(STEPSIZE-1))) {
        reg_t const diff = LZ4_read_ARCH(pMatch) ^ LZ4_read_ARCH(pIn);
        if (!diff) {
            pIn+=STEPSIZE; pMatch+=STEPSIZE;
        } else {
            return LZ4_NbCommonBytes(diff);
    }   }

    while (likely(pIn < pInLimit-(STEPSIZE-1))) {
        reg_t const diff = LZ4_read_ARCH(pMatch) ^ LZ4_read_ARCH(pIn);
        if (!diff) { pIn+=STEPSIZE; pMatch+=STEPSIZE; continue; }
        pIn += LZ4_NbCommonBytes(diff);
        return (unsigned)(pIn - pStart);
    }

    if ((STEPSIZE==8) && (pIn<(pInLimit-3)) && (LZ4_read32(pMatch) == LZ4_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (LZ4_read16(pMatch) == LZ4_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (unsigned)(pIn - pStart);
}


#ifndef LZ4_COMMONDEFS_ONLY
/*-************************************
*  Local Constants
**************************************/
static const int LZ4_64Klimit = ((64 KB) + (MFLIMIT-1));
static const U32 LZ4_skipTrigger = 6;  /* Increase this value ==> compression run slower on incompressible data */


/*-************************************
*  Local Structures and types
**************************************/
typedef enum { clearedTable = 0, byPtr, byU32, byU16 } tableType_t;

/**
 * This enum distinguishes several different modes of accessing previous
 * content in the stream.
 *
 * - noDict        : There is no preceding content.
 * - withPrefix64k : Table entries up to ctx->dictSize before the current blob
 *                   blob being compressed are valid and refer to the preceding
 *                   content (of length ctx->dictSize), which is available
 *                   contiguously preceding in memory the content currently
 *                   being compressed.
 * - usingExtDict  : Like withPrefix64k, but the preceding content is somewhere
 *                   else in memory, starting at ctx->dictionary with length
 *                   ctx->dictSize.
 * - usingDictCtx  : Everything concerning the preceding content is
 *                   in a separate context, pointed to by ctx->dictCtx.
 *                   ctx->dictionary, ctx->dictSize, and table entries
 *                   in the current context that refer to positions
 *                   preceding the beginning of the current compression are
 *                   ignored. Instead, ctx->dictCtx->dictionary and ctx->dictCtx
 *                   ->dictSize describe the location and size of the preceding
 *                   content, and matches are found by looking in the ctx
 *                   ->dictCtx->hashTable.
 */
typedef enum { noDict = 0, withPrefix64k, usingExtDict, usingDictCtx } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;


/*-************************************
*  Local Utils
**************************************/
int LZ4_versionNumber (void) { return LZ4_VERSION_NUMBER; }
const char* LZ4_versionString(void) { return LZ4_VERSION_STRING; }
int LZ4_compressBound(int isize)  { return LZ4_COMPRESSBOUND(isize); }
int LZ4_sizeofState(void) { return sizeof(LZ4_stream_t); }


/*-****************************************
*  Internal Definitions, used only in Tests
*******************************************/
#if defined (__cplusplus)
extern "C" {
#endif

int LZ4_compress_forceExtDict (LZ4_stream_t* LZ4_dict, const char* source, char* dest, int srcSize);

int LZ4_decompress_safe_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int maxOutputSize,
                                     const void* dictStart, size_t dictSize);
int LZ4_decompress_safe_partial_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int targetOutputSize, int dstCapacity,
                                     const void* dictStart, size_t dictSize);
#if defined (__cplusplus)
}
#endif

/*-******************************
*  Compression functions
********************************/
LZ4_FORCE_INLINE U32 LZ4_hash4(U32 sequence, tableType_t const tableType)
{
    if (tableType == byU16)
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-(LZ4_HASHLOG+1)));
    else
        return ((sequence * 2654435761U) >> ((MINMATCH*8)-LZ4_HASHLOG));
}

LZ4_FORCE_INLINE U32 LZ4_hash5(U64 sequence, tableType_t const tableType)
{
    const U32 hashLog = (tableType == byU16) ? LZ4_HASHLOG+1 : LZ4_HASHLOG;
    if (LZ4_isLittleEndian()) {
        const U64 prime5bytes = 889523592379ULL;
        return (U32)(((sequence << 24) * prime5bytes) >> (64 - hashLog));
    } else {
        const U64 prime8bytes = 11400714785074694791ULL;
        return (U32)(((sequence >> 24) * prime8bytes) >> (64 - hashLog));
    }
}

LZ4_FORCE_INLINE U32 LZ4_hashPosition(const void* const p, tableType_t const tableType)
{
    if ((sizeof(reg_t)==8) && (tableType != byU16)) return LZ4_hash5(LZ4_read_ARCH(p), tableType);

#ifdef LZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT
    return LZ4_hash4(LZ4_readLE32(p), tableType);
#else
    return LZ4_hash4(LZ4_read32(p), tableType);
#endif
}

LZ4_FORCE_INLINE void LZ4_clearHash(U32 h, void* tableBase, tableType_t const tableType)
{
    switch (tableType)
    {
    default: /* fallthrough */
    case clearedTable: { /* illegal! */ assert(0); return; }
    case byPtr: { const BYTE** hashTable = (const BYTE**)tableBase; hashTable[h] = NULL; return; }
    case byU32: { U32* hashTable = (U32*) tableBase; hashTable[h] = 0; return; }
    case byU16: { U16* hashTable = (U16*) tableBase; hashTable[h] = 0; return; }
    }
}

LZ4_FORCE_INLINE void LZ4_putIndexOnHash(U32 idx, U32 h, void* tableBase, tableType_t const tableType)
{
    switch (tableType)
    {
    default: /* fallthrough */
    case clearedTable: /* fallthrough */
    case byPtr: { /* illegal! */ assert(0); return; }
    case byU32: { U32* hashTable = (U32*) tableBase; hashTable[h] = idx; return; }
    case byU16: { U16* hashTable = (U16*) tableBase; assert(idx < 65536); hashTable[h] = (U16)idx; return; }
    }
}

/* LZ4_putPosition*() : only used in byPtr mode */
LZ4_FORCE_INLINE void LZ4_putPositionOnHash(const BYTE* p, U32 h,
                                  void* tableBase, tableType_t const tableType)
{
    const BYTE** const hashTable = (const BYTE**)tableBase;
    assert(tableType == byPtr); (void)tableType;
    hashTable[h] = p;
}

LZ4_FORCE_INLINE void LZ4_putPosition(const BYTE* p, void* tableBase, tableType_t tableType)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    LZ4_putPositionOnHash(p, h, tableBase, tableType);
}

/* LZ4_getIndexOnHash() :
 * Index of match position registered in hash table.
 * hash position must be calculated by using base+index, or dictBase+index.
 * Assumption 1 : only valid if tableType == byU32 or byU16.
 * Assumption 2 : h is presumed valid (within limits of hash table)
 */
LZ4_FORCE_INLINE U32 LZ4_getIndexOnHash(U32 h, const void* tableBase, tableType_t tableType)
{
    LZ4_STATIC_ASSERT(LZ4_MEMORY_USAGE > 2);
    if (tableType == byU32) {
        const U32* const hashTable = (const U32*) tableBase;
        assert(h < (1U << (LZ4_MEMORY_USAGE-2)));
        return hashTable[h];
    }
    if (tableType == byU16) {
        const U16* const hashTable = (const U16*) tableBase;
        assert(h < (1U << (LZ4_MEMORY_USAGE-1)));
        return hashTable[h];
    }
    assert(0); return 0;  /* forbidden case */
}

static const BYTE* LZ4_getPositionOnHash(U32 h, const void* tableBase, tableType_t tableType)
{
    assert(tableType == byPtr); (void)tableType;
    { const BYTE* const* hashTable = (const BYTE* const*) tableBase; return hashTable[h]; }
}

LZ4_FORCE_INLINE const BYTE*
LZ4_getPosition(const BYTE* p,
                const void* tableBase, tableType_t tableType)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    return LZ4_getPositionOnHash(h, tableBase, tableType);
}

LZ4_FORCE_INLINE void
LZ4_prepareTable(LZ4_stream_t_internal* const cctx,
           const int inputSize,
           const tableType_t tableType) {
    /* If the table hasn't been used, it's guaranteed to be zeroed out, and is
     * therefore safe to use no matter what mode we're in. Otherwise, we figure
     * out if it's safe to leave as is or whether it needs to be reset.
     */
    if ((tableType_t)cctx->tableType != clearedTable) {
        assert(inputSize >= 0);
        if ((tableType_t)cctx->tableType != tableType
          || ((tableType == byU16) && cctx->currentOffset + (unsigned)inputSize >= 0xFFFFU)
          || ((tableType == byU32) && cctx->currentOffset > 1 GB)
          || tableType == byPtr
          || inputSize >= 4 KB)
        {
            DEBUGLOG(4, "LZ4_prepareTable: Resetting table in %p", cctx);
            MEM_INIT(cctx->hashTable, 0, LZ4_HASHTABLESIZE);
            cctx->currentOffset = 0;
            cctx->tableType = (U32)clearedTable;
        } else {
            DEBUGLOG(4, "LZ4_prepareTable: Re-use hash table (no reset)");
        }
    }

    /* Adding a gap, so all previous entries are > LZ4_DISTANCE_MAX back,
     * is faster than compressing without a gap.
     * However, compressing with currentOffset == 0 is faster still,
     * so we preserve that case.
     */
    if (cctx->currentOffset != 0 && tableType == byU32) {
        DEBUGLOG(5, "LZ4_prepareTable: adding 64KB to currentOffset");
        cctx->currentOffset += 64 KB;
    }

    /* Finally, clear history */
    cctx->dictCtx = NULL;
    cctx->dictionary = NULL;
    cctx->dictSize = 0;
}

/** LZ4_compress_generic_validated() :
 *  inlined, to ensure branches are decided at compilation time.
 *  The following conditions are presumed already validated:
 *  - source != NULL
 *  - inputSize > 0
 */
LZ4_FORCE_INLINE int LZ4_compress_generic_validated(
                 LZ4_stream_t_internal* const cctx,
                 const char* const source,
                 char* const dest,
                 const int inputSize,
                 int*  inputConsumed, /* only written when outputDirective == fillOutput */
                 const int maxOutputSize,
                 const limitedOutput_directive outputDirective,
                 const tableType_t tableType,
                 const dict_directive dictDirective,
                 const dictIssue_directive dictIssue,
                 const int acceleration)
{
    int result;
    const BYTE* ip = (const BYTE*)source;

    U32 const startIndex = cctx->currentOffset;
    const BYTE* base = (const BYTE*)source - startIndex;
    const BYTE* lowLimit;

    const LZ4_stream_t_internal* dictCtx = (const LZ4_stream_t_internal*) cctx->dictCtx;
    const BYTE* const dictionary =
        dictDirective == usingDictCtx ? dictCtx->dictionary : cctx->dictionary;
    const U32 dictSize =
        dictDirective == usingDictCtx ? dictCtx->dictSize : cctx->dictSize;
    const U32 dictDelta =
        (dictDirective == usingDictCtx) ? startIndex - dictCtx->currentOffset : 0;   /* make indexes in dictCtx comparable with indexes in current context */

    int const maybe_extMem = (dictDirective == usingExtDict) || (dictDirective == usingDictCtx);
    U32 const prefixIdxLimit = startIndex - dictSize;   /* used when dictDirective == dictSmall */
    const BYTE* const dictEnd = dictionary ? dictionary + dictSize : dictionary;
    const BYTE* anchor = (const BYTE*) source;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimitPlusOne = iend - MFLIMIT + 1;
    const BYTE* const matchlimit = iend - LASTLITERALS;

    /* the dictCtx currentOffset is indexed on the start of the dictionary,
     * while a dictionary in the current context precedes the currentOffset */
    const BYTE* dictBase = (dictionary == NULL) ? NULL :
                           (dictDirective == usingDictCtx) ?
                            dictionary + dictSize - dictCtx->currentOffset :
                            dictionary + dictSize - startIndex;

    BYTE* op = (BYTE*) dest;
    BYTE* const olimit = op + maxOutputSize;

    U32 offset = 0;
    U32 forwardH;

    DEBUGLOG(5, "LZ4_compress_generic_validated: srcSize=%i, tableType=%u", inputSize, tableType);
    assert(ip != NULL);
    if (tableType == byU16) assert(inputSize<LZ4_64Klimit);  /* Size too large (not within 64K limit) */
    if (tableType == byPtr) assert(dictDirective==noDict);   /* only supported use case with byPtr */
    /* If init conditions are not met, we don't have to mark stream
     * as having dirty context, since no action was taken yet */
    if (outputDirective == fillOutput && maxOutputSize < 1) { return 0; } /* Impossible to store anything */
    assert(acceleration >= 1);

    lowLimit = (const BYTE*)source - (dictDirective == withPrefix64k ? dictSize : 0);

    /* Update context state */
    if (dictDirective == usingDictCtx) {
        /* Subsequent linked blocks can't use the dictionary. */
        /* Instead, they use the block we just compressed. */
        cctx->dictCtx = NULL;
        cctx->dictSize = (U32)inputSize;
    } else {
        cctx->dictSize += (U32)inputSize;
    }
    cctx->currentOffset += (U32)inputSize;
    cctx->tableType = (U32)tableType;

    if (inputSize<LZ4_minLength) goto _last_literals;        /* Input too small, no compression (all literals) */

    /* First Byte */
    {   U32 const h = LZ4_hashPosition(ip, tableType);
        if (tableType == byPtr) {
            LZ4_putPositionOnHash(ip, h, cctx->hashTable, byPtr);
        } else {
            LZ4_putIndexOnHash(startIndex, h, cctx->hashTable, tableType);
    }   }
    ip++; forwardH = LZ4_hashPosition(ip, tableType);

    /* Main Loop */
    for ( ; ; ) {
        const BYTE* match;
        BYTE* token;
        const BYTE* filledIp;

        /* Find a match */
        if (tableType == byPtr) {
            const BYTE* forwardIp = ip;
            int step = 1;
            int searchMatchNb = acceleration << LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimitPlusOne)) goto _last_literals;
                assert(ip < mflimitPlusOne);

                match = LZ4_getPositionOnHash(h, cctx->hashTable, tableType);
                forwardH = LZ4_hashPosition(forwardIp, tableType);
                LZ4_putPositionOnHash(ip, h, cctx->hashTable, tableType);

            } while ( (match+LZ4_DISTANCE_MAX < ip)
                   || (LZ4_read32(match) != LZ4_read32(ip)) );

        } else {   /* byU32, byU16 */

            const BYTE* forwardIp = ip;
            int step = 1;
            int searchMatchNb = acceleration << LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                U32 const current = (U32)(forwardIp - base);
                U32 matchIndex = LZ4_getIndexOnHash(h, cctx->hashTable, tableType);
                assert(matchIndex <= current);
                assert(forwardIp - base < (ptrdiff_t)(2 GB - 1));
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimitPlusOne)) goto _last_literals;
                assert(ip < mflimitPlusOne);

                if (dictDirective == usingDictCtx) {
                    if (matchIndex < startIndex) {
                        /* there was no match, try the dictionary */
                        assert(tableType == byU32);
                        matchIndex = LZ4_getIndexOnHash(h, dictCtx->hashTable, byU32);
                        match = dictBase + matchIndex;
                        matchIndex += dictDelta;   /* make dictCtx index comparable with current context */
                        lowLimit = dictionary;
                    } else {
                        match = base + matchIndex;
                        lowLimit = (const BYTE*)source;
                    }
                } else if (dictDirective == usingExtDict) {
                    if (matchIndex < startIndex) {
                        DEBUGLOG(7, "extDict candidate: matchIndex=%5u  <  startIndex=%5u", matchIndex, startIndex);
                        assert(startIndex - matchIndex >= MINMATCH);
                        assert(dictBase);
                        match = dictBase + matchIndex;
                        lowLimit = dictionary;
                    } else {
                        match = base + matchIndex;
                        lowLimit = (const BYTE*)source;
                    }
                } else {   /* single continuous memory segment */
                    match = base + matchIndex;
                }
                forwardH = LZ4_hashPosition(forwardIp, tableType);
                LZ4_putIndexOnHash(current, h, cctx->hashTable, tableType);

                DEBUGLOG(7, "candidate at pos=%u  (offset=%u \n", matchIndex, current - matchIndex);
                if ((dictIssue == dictSmall) && (matchIndex < prefixIdxLimit)) { continue; }    /* match outside of valid area */
                assert(matchIndex < current);
                if ( ((tableType != byU16) || (LZ4_DISTANCE_MAX < LZ4_DISTANCE_ABSOLUTE_MAX))
                  && (matchIndex+LZ4_DISTANCE_MAX < current)) {
                    continue;
                } /* too far */
                assert((current - matchIndex) <= LZ4_DISTANCE_MAX);  /* match now expected within distance */

                if (LZ4_read32(match) == LZ4_read32(ip)) {
                    if (maybe_extMem) offset = current - matchIndex;
                    break;   /* match found */
                }

            } while(1);
        }

        /* Catch up */
        filledIp = ip;
        assert(ip > anchor); /* this is always true as ip has been advanced before entering the main loop */
        if ((match > lowLimit) && unlikely(ip[-1] == match[-1])) {
            do { ip--; match--; } while (((ip > anchor) & (match > lowLimit)) && (unlikely(ip[-1] == match[-1])));
        }

        /* Encode Literals */
        {   unsigned const litLength = (unsigned)(ip - anchor);
            token = op++;
            if ((outputDirective == limitedOutput) &&  /* Check output buffer overflow */
                (unlikely(op + litLength + (2 + 1 + LASTLITERALS) + (litLength/255) > olimit)) ) {
                return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
            }
            if ((outputDirective == fillOutput) &&
                (unlikely(op + (litLength+240)/255 /* litlen */ + litLength /* literals */ + 2 /* offset */ + 1 /* token */ + MFLIMIT - MINMATCH /* min last literals so last match is <= end - MFLIMIT */ > olimit))) {
                op--;
                goto _last_literals;
            }
            if (litLength >= RUN_MASK) {
                unsigned len = litLength - RUN_MASK;
                *token = (RUN_MASK<<ML_BITS);
                for(; len >= 255 ; len-=255) *op++ = 255;
                *op++ = (BYTE)len;
            }
            else *token = (BYTE)(litLength<<ML_BITS);

            /* Copy Literals */
            LZ4_wildCopy8(op, anchor, op+litLength);
            op+=litLength;
            DEBUGLOG(6, "seq.start:%i, literals=%u, match.start:%i",
                        (int)(anchor-(const BYTE*)source), litLength, (int)(ip-(const BYTE*)source));
        }

_next_match:
        /* at this stage, the following variables must be correctly set :
         * - ip : at start of LZ operation
         * - match : at start of previous pattern occurrence; can be within current prefix, or within extDict
         * - offset : if maybe_ext_memSegment==1 (constant)
         * - lowLimit : must be == dictionary to mean "match is within extDict"; must be == source otherwise
         * - token and *token : position to write 4-bits for match length; higher 4-bits for literal length supposed already written
         */

        if ((outputDirective == fillOutput) &&
            (op + 2 /* offset */ + 1 /* token */ + MFLIMIT - MINMATCH /* min last literals so last match is <= end - MFLIMIT */ > olimit)) {
            /* the match was too close to the end, rewind and go to last literals */
            op = token;
            goto _last_literals;
        }

        /* Encode Offset */
        if (maybe_extMem) {   /* static test */
            DEBUGLOG(6, "             with offset=%u  (ext if > %i)", offset, (int)(ip - (const BYTE*)source));
            assert(offset <= LZ4_DISTANCE_MAX && offset > 0);
            LZ4_writeLE16(op, (U16)offset); op+=2;
        } else  {
            DEBUGLOG(6, "             with offset=%u  (same segment)", (U32)(ip - match));
            assert(ip-match <= LZ4_DISTANCE_MAX);
            LZ4_writeLE16(op, (U16)(ip - match)); op+=2;
        }

        /* Encode MatchLength */
        {   unsigned matchCode;

            if ( (dictDirective==usingExtDict || dictDirective==usingDictCtx)
              && (lowLimit==dictionary) /* match within extDict */ ) {
                const BYTE* limit = ip + (dictEnd-match);
                assert(dictEnd > match);
                if (limit > matchlimit) limit = matchlimit;
                matchCode = LZ4_count(ip+MINMATCH, match+MINMATCH, limit);
                ip += (size_t)matchCode + MINMATCH;
                if (ip==limit) {
                    unsigned const more = LZ4_count(limit, (const BYTE*)source, matchlimit);
                    matchCode += more;
                    ip += more;
                }
                DEBUGLOG(6, "             with matchLength=%u starting in extDict", matchCode+MINMATCH);
            } else {
                matchCode = LZ4_count(ip+MINMATCH, match+MINMATCH, matchlimit);
                ip += (size_t)matchCode + MINMATCH;
                DEBUGLOG(6, "             with matchLength=%u", matchCode+MINMATCH);
            }

            if ((outputDirective) &&    /* Check output buffer overflow */
                (unlikely(op + (1 + LASTLITERALS) + (matchCode+240)/255 > olimit)) ) {
                if (outputDirective == fillOutput) {
                    /* Match description too long : reduce it */
                    U32 newMatchCode = 15 /* in token */ - 1 /* to avoid needing a zero byte */ + ((U32)(olimit - op) - 1 - LASTLITERALS) * 255;
                    ip -= matchCode - newMatchCode;
                    assert(newMatchCode < matchCode);
                    matchCode = newMatchCode;
                    if (unlikely(ip <= filledIp)) {
                        /* We have already filled up to filledIp so if ip ends up less than filledIp
                         * we have positions in the hash table beyond the current position. This is
                         * a problem if we reuse the hash table. So we have to remove these positions
                         * from the hash table.
                         */
                        const BYTE* ptr;
                        DEBUGLOG(5, "Clearing %u positions", (U32)(filledIp - ip));
                        for (ptr = ip; ptr <= filledIp; ++ptr) {
                            U32 const h = LZ4_hashPosition(ptr, tableType);
                            LZ4_clearHash(h, cctx->hashTable, tableType);
                        }
                    }
                } else {
                    assert(outputDirective == limitedOutput);
                    return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
                }
            }
            if (matchCode >= ML_MASK) {
                *token += ML_MASK;
                matchCode -= ML_MASK;
                LZ4_write32(op, 0xFFFFFFFF);
                while (matchCode >= 4*255) {
                    op+=4;
                    LZ4_write32(op, 0xFFFFFFFF);
                    matchCode -= 4*255;
                }
                op += matchCode / 255;
                *op++ = (BYTE)(matchCode % 255);
            } else
                *token += (BYTE)(matchCode);
        }
        /* Ensure we have enough space for the last literals. */
        assert(!(outputDirective == fillOutput && op + 1 + LASTLITERALS > olimit));

        anchor = ip;

        /* Test end of chunk */
        if (ip >= mflimitPlusOne) break;

        /* Fill table */
        {   U32 const h = LZ4_hashPosition(ip-2, tableType);
            if (tableType == byPtr) {
                LZ4_putPositionOnHash(ip-2, h, cctx->hashTable, byPtr);
            } else {
                U32 const idx = (U32)((ip-2) - base);
                LZ4_putIndexOnHash(idx, h, cctx->hashTable, tableType);
        }   }

        /* Test next position */
        if (tableType == byPtr) {

            match = LZ4_getPosition(ip, cctx->hashTable, tableType);
            LZ4_putPosition(ip, cctx->hashTable, tableType);
            if ( (match+LZ4_DISTANCE_MAX >= ip)
              && (LZ4_read32(match) == LZ4_read32(ip)) )
            { token=op++; *token=0; goto _next_match; }

        } else {   /* byU32, byU16 */

            U32 const h = LZ4_hashPosition(ip, tableType);
            U32 const current = (U32)(ip-base);
            U32 matchIndex = LZ4_getIndexOnHash(h, cctx->hashTable, tableType);
            assert(matchIndex < current);
            if (dictDirective == usingDictCtx) {
                if (matchIndex < startIndex) {
                    /* there was no match, try the dictionary */
                    assert(tableType == byU32);
                    matchIndex = LZ4_getIndexOnHash(h, dictCtx->hashTable, byU32);
                    match = dictBase + matchIndex;
                    lowLimit = dictionary;   /* required for match length counter */
                    matchIndex += dictDelta;
                } else {
                    match = base + matchIndex;
                    lowLimit = (const BYTE*)source;  /* required for match length counter */
                }
            } else if (dictDirective==usingExtDict) {
                if (matchIndex < startIndex) {
                    assert(dictBase);
                    match = dictBase + matchIndex;
                    lowLimit = dictionary;   /* required for match length counter */
                } else {
                    match = base + matchIndex;
                    lowLimit = (const BYTE*)source;   /* required for match length counter */
                }
            } else {   /* single memory segment */
                match = base + matchIndex;
            }
            LZ4_putIndexOnHash(current, h, cctx->hashTable, tableType);
            assert(matchIndex < current);
            if ( ((dictIssue==dictSmall) ? (matchIndex >= prefixIdxLimit) : 1)
              && (((tableType==byU16) && (LZ4_DISTANCE_MAX == LZ4_DISTANCE_ABSOLUTE_MAX)) ? 1 : (matchIndex+LZ4_DISTANCE_MAX >= current))
              && (LZ4_read32(match) == LZ4_read32(ip)) ) {
                token=op++;
                *token=0;
                if (maybe_extMem) offset = current - matchIndex;
                DEBUGLOG(6, "seq.start:%i, literals=%u, match.start:%i",
                            (int)(anchor-(const BYTE*)source), 0, (int)(ip-(const BYTE*)source));
                goto _next_match;
            }
        }

        /* Prepare next loop */
        forwardH = LZ4_hashPosition(++ip, tableType);

    }

_last_literals:
    /* Encode Last Literals */
    {   size_t lastRun = (size_t)(iend - anchor);
        if ( (outputDirective) &&  /* Check output buffer overflow */
            (op + lastRun + 1 + ((lastRun+255-RUN_MASK)/255) > olimit)) {
            if (outputDirective == fillOutput) {
                /* adapt lastRun to fill 'dst' */
                assert(olimit >= op);
                lastRun  = (size_t)(olimit-op) - 1/*token*/;
                lastRun -= (lastRun + 256 - RUN_MASK) / 256;  /*additional length tokens*/
            } else {
                assert(outputDirective == limitedOutput);
                return 0;   /* cannot compress within `dst` budget. Stored indexes in hash table are nonetheless fine */
            }
        }
        DEBUGLOG(6, "Final literal run : %i literals", (int)lastRun);
        if (lastRun >= RUN_MASK) {
            size_t accumulator = lastRun - RUN_MASK;
            *op++ = RUN_MASK << ML_BITS;
            for(; accumulator >= 255 ; accumulator-=255) *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRun<<ML_BITS);
        }
        LZ4_memcpy(op, anchor, lastRun);
        ip = anchor + lastRun;
        op += lastRun;
    }

    if (outputDirective == fillOutput) {
        *inputConsumed = (int) (((const char*)ip)-source);
    }
    result = (int)(((char*)op) - dest);
    assert(result > 0);
    DEBUGLOG(5, "LZ4_compress_generic: compressed %i bytes into %i bytes", inputSize, result);
    return result;
}

/** LZ4_compress_generic() :
 *  inlined, to ensure branches are decided at compilation time;
 *  takes care of src == (NULL, 0)
 *  and forward the rest to LZ4_compress_generic_validated */
LZ4_FORCE_INLINE int LZ4_compress_generic(
                 LZ4_stream_t_internal* const cctx,
                 const char* const src,
                 char* const dst,
                 const int srcSize,
                 int *inputConsumed, /* only written when outputDirective == fillOutput */
                 const int dstCapacity,
                 const limitedOutput_directive outputDirective,
                 const tableType_t tableType,
                 const dict_directive dictDirective,
                 const dictIssue_directive dictIssue,
                 const int acceleration)
{
    DEBUGLOG(5, "LZ4_compress_generic: srcSize=%i, dstCapacity=%i",
                srcSize, dstCapacity);

    if ((U32)srcSize > (U32)LZ4_MAX_INPUT_SIZE) { return 0; }  /* Unsupported srcSize, too large (or negative) */
    if (srcSize == 0) {   /* src == NULL supported if srcSize == 0 */
        if (outputDirective != notLimited && dstCapacity <= 0) return 0;  /* no output, can't write anything */
        DEBUGLOG(5, "Generating an empty block");
        assert(outputDirective == notLimited || dstCapacity >= 1);
        assert(dst != NULL);
        dst[0] = 0;
        if (outputDirective == fillOutput) {
            assert (inputConsumed != NULL);
            *inputConsumed = 0;
        }
        return 1;
    }
    assert(src != NULL);

    return LZ4_compress_generic_validated(cctx, src, dst, srcSize,
                inputConsumed, /* only written into if outputDirective == fillOutput */
                dstCapacity, outputDirective,
                tableType, dictDirective, dictIssue, acceleration);
}


int LZ4_compress_fast_extState(void* state, const char* source, char* dest, int inputSize, int maxOutputSize, int acceleration)
{
    LZ4_stream_t_internal* const ctx = & LZ4_initStream(state, sizeof(LZ4_stream_t)) -> internal_donotuse;
    assert(ctx != NULL);
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;
    if (maxOutputSize >= LZ4_compressBound(inputSize)) {
        if (inputSize < LZ4_64Klimit) {
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, 0, notLimited, byU16, noDict, noDictIssue, acceleration);
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)source > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
        }
    } else {
        if (inputSize < LZ4_64Klimit) {
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, byU16, noDict, noDictIssue, acceleration);
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)source > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(ctx, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, noDict, noDictIssue, acceleration);
        }
    }
}

/**
 * LZ4_compress_fast_extState_fastReset() :
 * A variant of LZ4_compress_fast_extState().
 *
 * Using this variant avoids an expensive initialization step. It is only safe
 * to call if the state buffer is known to be correctly initialized already
 * (see comment in lz4.h on LZ4_resetStream_fast() for a definition of
 * "correctly initialized").
 */
int LZ4_compress_fast_extState_fastReset(void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration)
{
    LZ4_stream_t_internal* const ctx = &((LZ4_stream_t*)state)->internal_donotuse;
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;
    assert(ctx != NULL);

    if (dstCapacity >= LZ4_compressBound(srcSize)) {
        if (srcSize < LZ4_64Klimit) {
            const tableType_t tableType = byU16;
            LZ4_prepareTable(ctx, srcSize, tableType);
            if (ctx->currentOffset) {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, dictSmall, acceleration);
            } else {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
            }
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            LZ4_prepareTable(ctx, srcSize, tableType);
            return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, 0, notLimited, tableType, noDict, noDictIssue, acceleration);
        }
    } else {
        if (srcSize < LZ4_64Klimit) {
            const tableType_t tableType = byU16;
            LZ4_prepareTable(ctx, srcSize, tableType);
            if (ctx->currentOffset) {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, dictSmall, acceleration);
            } else {
                return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, noDictIssue, acceleration);
            }
        } else {
            const tableType_t tableType = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            LZ4_prepareTable(ctx, srcSize, tableType);
            return LZ4_compress_generic(ctx, src, dst, srcSize, NULL, dstCapacity, limitedOutput, tableType, noDict, noDictIssue, acceleration);
        }
    }
}


int LZ4_compress_fast(const char* src, char* dest, int srcSize, int dstCapacity, int acceleration)
{
    int result;
#if (LZ4_HEAPMODE)
    LZ4_stream_t* const ctxPtr = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));   /* malloc-calloc always properly aligned */
    if (ctxPtr == NULL) return 0;
#else
    LZ4_stream_t ctx;
    LZ4_stream_t* const ctxPtr = &ctx;
#endif
    result = LZ4_compress_fast_extState(ctxPtr, src, dest, srcSize, dstCapacity, acceleration);

#if (LZ4_HEAPMODE)
    FREEMEM(ctxPtr);
#endif
    return result;
}


int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity)
{
    return LZ4_compress_fast(src, dst, srcSize, dstCapacity, 1);
}


/* Note!: This function leaves the stream in an unclean/broken state!
 * It is not safe to subsequently use the same state with a _fastReset() or
 * _continue() call without resetting it. */
static int LZ4_compress_destSize_extState_internal(LZ4_stream_t* state, const char* src, char* dst, int* srcSizePtr, int targetDstSize, int acceleration)
{
    void* const s = LZ4_initStream(state, sizeof (*state));
    assert(s != NULL); (void)s;

    if (targetDstSize >= LZ4_compressBound(*srcSizePtr)) {  /* compression success is guaranteed */
        return LZ4_compress_fast_extState(state, src, dst, *srcSizePtr, targetDstSize, acceleration);
    } else {
        if (*srcSizePtr < LZ4_64Klimit) {
            return LZ4_compress_generic(&state->internal_donotuse, src, dst, *srcSizePtr, srcSizePtr, targetDstSize, fillOutput, byU16, noDict, noDictIssue, acceleration);
        } else {
            tableType_t const addrMode = ((sizeof(void*)==4) && ((uptrval)src > LZ4_DISTANCE_MAX)) ? byPtr : byU32;
            return LZ4_compress_generic(&state->internal_donotuse, src, dst, *srcSizePtr, srcSizePtr, targetDstSize, fillOutput, addrMode, noDict, noDictIssue, acceleration);
    }   }
}

int LZ4_compress_destSize_extState(void* state, const char* src, char* dst, int* srcSizePtr, int targetDstSize, int acceleration)
{
    int const r = LZ4_compress_destSize_extState_internal((LZ4_stream_t*)state, src, dst, srcSizePtr, targetDstSize, acceleration);
    /* clean the state on exit */
    LZ4_initStream(state, sizeof (LZ4_stream_t));
    return r;
}


int LZ4_compress_destSize(const char* src, char* dst, int* srcSizePtr, int targetDstSize)
{
#if (LZ4_HEAPMODE)
    LZ4_stream_t* const ctx = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));   /* malloc-calloc always properly aligned */
    if (ctx == NULL) return 0;
#else
    LZ4_stream_t ctxBody;
    LZ4_stream_t* const ctx = &ctxBody;
#endif

    int result = LZ4_compress_destSize_extState_internal(ctx, src, dst, srcSizePtr, targetDstSize, 1);

#if (LZ4_HEAPMODE)
    FREEMEM(ctx);
#endif
    return result;
}



/*-******************************
*  Streaming functions
********************************/

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4_stream_t* LZ4_createStream(void)
{
    LZ4_stream_t* const lz4s = (LZ4_stream_t*)ALLOC(sizeof(LZ4_stream_t));
    LZ4_STATIC_ASSERT(sizeof(LZ4_stream_t) >= sizeof(LZ4_stream_t_internal));
    DEBUGLOG(4, "LZ4_createStream %p", lz4s);
    if (lz4s == NULL) return NULL;
    LZ4_initStream(lz4s, sizeof(*lz4s));
    return lz4s;
}
#endif

static size_t LZ4_stream_t_alignment(void)
{
#if LZ4_ALIGN_TEST
    typedef struct { char c; LZ4_stream_t t; } t_a;
    return sizeof(t_a) - sizeof(LZ4_stream_t);
#else
    return 1;  /* effectively disabled */
#endif
}

LZ4_stream_t* LZ4_initStream (void* buffer, size_t size)
{
    DEBUGLOG(5, "LZ4_initStream");
    if (buffer == NULL) { return NULL; }
    if (size < sizeof(LZ4_stream_t)) { return NULL; }
    if (!LZ4_isAligned(buffer, LZ4_stream_t_alignment())) return NULL;
    MEM_INIT(buffer, 0, sizeof(LZ4_stream_t_internal));
    return (LZ4_stream_t*)buffer;
}

/* resetStream is now deprecated,
 * prefer initStream() which is more general */
void LZ4_resetStream (LZ4_stream_t* LZ4_stream)
{
    DEBUGLOG(5, "LZ4_resetStream (ctx:%p)", LZ4_stream);
    MEM_INIT(LZ4_stream, 0, sizeof(LZ4_stream_t_internal));
}

void LZ4_resetStream_fast(LZ4_stream_t* ctx) {
    LZ4_prepareTable(&(ctx->internal_donotuse), 0, byU32);
}

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
int LZ4_freeStream (LZ4_stream_t* LZ4_stream)
{
    if (!LZ4_stream) return 0;   /* support free on NULL */
    DEBUGLOG(5, "LZ4_freeStream %p", LZ4_stream);
    FREEMEM(LZ4_stream);
    return (0);
}
#endif


typedef enum { _ld_fast, _ld_slow } LoadDict_mode_e;
#define HASH_UNIT sizeof(reg_t)
int LZ4_loadDict_internal(LZ4_stream_t* LZ4_dict,
                    const char* dictionary, int dictSize,
                    LoadDict_mode_e _ld)
{
    LZ4_stream_t_internal* const dict = &LZ4_dict->internal_donotuse;
    const tableType_t tableType = byU32;
    const BYTE* p = (const BYTE*)dictionary;
    const BYTE* const dictEnd = p + dictSize;
    U32 idx32;

    DEBUGLOG(4, "LZ4_loadDict (%i bytes from %p into %p)", dictSize, dictionary, LZ4_dict);

    /* It's necessary to reset the context,
     * and not just continue it with prepareTable()
     * to avoid any risk of generating overflowing matchIndex
     * when compressing using this dictionary */
    LZ4_resetStream(LZ4_dict);

    /* We always increment the offset by 64 KB, since, if the dict is longer,
     * we truncate it to the last 64k, and if it's shorter, we still want to
     * advance by a whole window length so we can provide the guarantee that
     * there are only valid offsets in the window, which allows an optimization
     * in LZ4_compress_fast_continue() where it uses noDictIssue even when the
     * dictionary isn't a full 64k. */
    dict->currentOffset += 64 KB;

    if (dictSize < (int)HASH_UNIT) {
        return 0;
    }

    if ((dictEnd - p) > 64 KB) p = dictEnd - 64 KB;
    dict->dictionary = p;
    dict->dictSize = (U32)(dictEnd - p);
    dict->tableType = (U32)tableType;
    idx32 = dict->currentOffset - dict->dictSize;

    while (p <= dictEnd-HASH_UNIT) {
        U32 const h = LZ4_hashPosition(p, tableType);
        /* Note: overwriting => favors positions end of dictionary */
        LZ4_putIndexOnHash(idx32, h, dict->hashTable, tableType);
        p+=3; idx32+=3;
    }

    if (_ld == _ld_slow) {
        /* Fill hash table with additional references, to improve compression capability */
        p = dict->dictionary;
        idx32 = dict->currentOffset - dict->dictSize;
        while (p <= dictEnd-HASH_UNIT) {
            U32 const h = LZ4_hashPosition(p, tableType);
            U32 const limit = dict->currentOffset - 64 KB;
            if (LZ4_getIndexOnHash(h, dict->hashTable, tableType) <= limit) {
                /* Note: not overwriting => favors positions beginning of dictionary */
                LZ4_putIndexOnHash(idx32, h, dict->hashTable, tableType);
            }
            p++; idx32++;
        }
    }

    return (int)dict->dictSize;
}

int LZ4_loadDict(LZ4_stream_t* LZ4_dict, const char* dictionary, int dictSize)
{
    return LZ4_loadDict_internal(LZ4_dict, dictionary, dictSize, _ld_fast);
}

int LZ4_loadDictSlow(LZ4_stream_t* LZ4_dict, const char* dictionary, int dictSize)
{
    return LZ4_loadDict_internal(LZ4_dict, dictionary, dictSize, _ld_slow);
}

void LZ4_attach_dictionary(LZ4_stream_t* workingStream, const LZ4_stream_t* dictionaryStream)
{
    const LZ4_stream_t_internal* dictCtx = (dictionaryStream == NULL) ? NULL :
        &(dictionaryStream->internal_donotuse);

    DEBUGLOG(4, "LZ4_attach_dictionary (%p, %p, size %u)",
             workingStream, dictionaryStream,
             dictCtx != NULL ? dictCtx->dictSize : 0);

    if (dictCtx != NULL) {
        /* If the current offset is zero, we will never look in the
         * external dictionary context, since there is no value a table
         * entry can take that indicate a miss. In that case, we need
         * to bump the offset to something non-zero.
         */
        if (workingStream->internal_donotuse.currentOffset == 0) {
            workingStream->internal_donotuse.currentOffset = 64 KB;
        }

        /* Don't actually attach an empty dictionary.
         */
        if (dictCtx->dictSize == 0) {
            dictCtx = NULL;
        }
    }
    workingStream->internal_donotuse.dictCtx = dictCtx;
}


static void LZ4_renormDictT(LZ4_stream_t_internal* LZ4_dict, int nextSize)
{
    assert(nextSize >= 0);
    if (LZ4_dict->currentOffset + (unsigned)nextSize > 0x80000000) {   /* potential ptrdiff_t overflow (32-bits mode) */
        /* rescale hash table */
        U32 const delta = LZ4_dict->currentOffset - 64 KB;
        const BYTE* dictEnd = LZ4_dict->dictionary + LZ4_dict->dictSize;
        int i;
        DEBUGLOG(4, "LZ4_renormDictT");
        for (i=0; i<LZ4_HASH_SIZE_U32; i++) {
            if (LZ4_dict->hashTable[i] < delta) LZ4_dict->hashTable[i]=0;
            else LZ4_dict->hashTable[i] -= delta;
        }
        LZ4_dict->currentOffset = 64 KB;
        if (LZ4_dict->dictSize > 64 KB) LZ4_dict->dictSize = 64 KB;
        LZ4_dict->dictionary = dictEnd - LZ4_dict->dictSize;
    }
}


int LZ4_compress_fast_continue (LZ4_stream_t* LZ4_stream,
                                const char* source, char* dest,
                                int inputSize, int maxOutputSize,
                                int acceleration)
{
    const tableType_t tableType = byU32;
    LZ4_stream_t_internal* const streamPtr = &LZ4_stream->internal_donotuse;
    const char* dictEnd = streamPtr->dictSize ? (const char*)streamPtr->dictionary + streamPtr->dictSize : NULL;

    DEBUGLOG(5, "LZ4_compress_fast_continue (inputSize=%i, dictSize=%u)", inputSize, streamPtr->dictSize);

    LZ4_renormDictT(streamPtr, inputSize);   /* fix index overflow */
    if (acceleration < 1) acceleration = LZ4_ACCELERATION_DEFAULT;
    if (acceleration > LZ4_ACCELERATION_MAX) acceleration = LZ4_ACCELERATION_MAX;

    /* invalidate tiny dictionaries */
    if ( (streamPtr->dictSize < 4)     /* tiny dictionary : not enough for a hash */
      && (dictEnd != source)           /* prefix mode */
      && (inputSize > 0)               /* tolerance : don't lose history, in case next invocation would use prefix mode */
      && (streamPtr->dictCtx == NULL)  /* usingDictCtx */
      ) {
        DEBUGLOG(5, "LZ4_compress_fast_continue: dictSize(%u) at addr:%p is too small", streamPtr->dictSize, streamPtr->dictionary);
        /* remove dictionary existence from history, to employ faster prefix mode */
        streamPtr->dictSize = 0;
        streamPtr->dictionary = (const BYTE*)source;
        dictEnd = source;
    }

    /* Check overlapping input/dictionary space */
    {   const char* const sourceEnd = source + inputSize;
        if ((sourceEnd > (const char*)streamPtr->dictionary) && (sourceEnd < dictEnd)) {
            streamPtr->dictSize = (U32)(dictEnd - sourceEnd);
            if (streamPtr->dictSize > 64 KB) streamPtr->dictSize = 64 KB;
            if (streamPtr->dictSize < 4) streamPtr->dictSize = 0;
            streamPtr->dictionary = (const BYTE*)dictEnd - streamPtr->dictSize;
        }
    }

    /* prefix mode : source data follows dictionary */
    if (dictEnd == source) {
        if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset))
            return LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, withPrefix64k, dictSmall, acceleration);
        else
            return LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, withPrefix64k, noDictIssue, acceleration);
    }

    /* external dictionary mode */
    {   int result;
        if (streamPtr->dictCtx) {
            /* We depend here on the fact that dictCtx'es (produced by
             * LZ4_loadDict) guarantee that their tables contain no references
             * to offsets between dictCtx->currentOffset - 64 KB and
             * dictCtx->currentOffset - dictCtx->dictSize. This makes it safe
             * to use noDictIssue even when the dict isn't a full 64 KB.
             */
            if (inputSize > 4 KB) {
                /* For compressing large blobs, it is faster to pay the setup
                 * cost to copy the dictionary's tables into the active context,
                 * so that the compression loop is only looking into one table.
                 */
                LZ4_memcpy(streamPtr, streamPtr->dictCtx, sizeof(*streamPtr));
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, noDictIssue, acceleration);
            } else {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingDictCtx, noDictIssue, acceleration);
            }
        } else {  /* small data <= 4 KB */
            if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset)) {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, dictSmall, acceleration);
            } else {
                result = LZ4_compress_generic(streamPtr, source, dest, inputSize, NULL, maxOutputSize, limitedOutput, tableType, usingExtDict, noDictIssue, acceleration);
            }
        }
        streamPtr->dictionary = (const BYTE*)source;
        streamPtr->dictSize = (U32)inputSize;
        return result;
    }
}


/* Hidden debug function, to force-test external dictionary mode */
int LZ4_compress_forceExtDict (LZ4_stream_t* LZ4_dict, const char* source, char* dest, int srcSize)
{
    LZ4_stream_t_internal* const streamPtr = &LZ4_dict->internal_donotuse;
    int result;

    LZ4_renormDictT(streamPtr, srcSize);

    if ((streamPtr->dictSize < 64 KB) && (streamPtr->dictSize < streamPtr->currentOffset)) {
        result = LZ4_compress_generic(streamPtr, source, dest, srcSize, NULL, 0, notLimited, byU32, usingExtDict, dictSmall, 1);
    } else {
        result = LZ4_compress_generic(streamPtr, source, dest, srcSize, NULL, 0, notLimited, byU32, usingExtDict, noDictIssue, 1);
    }

    streamPtr->dictionary = (const BYTE*)source;
    streamPtr->dictSize = (U32)srcSize;

    return result;
}


/*! LZ4_saveDict() :
 *  If previously compressed data block is not guaranteed to remain available at its memory location,
 *  save it into a safer place (char* safeBuffer).
 *  Note : no need to call LZ4_loadDict() afterwards, dictionary is immediately usable,
 *         one can therefore call LZ4_compress_fast_continue() right after.
 * @return : saved dictionary size in bytes (necessarily <= dictSize), or 0 if error.
 */
int LZ4_saveDict (LZ4_stream_t* LZ4_dict, char* safeBuffer, int dictSize)
{
    LZ4_stream_t_internal* const dict = &LZ4_dict->internal_donotuse;

    DEBUGLOG(5, "LZ4_saveDict : dictSize=%i, safeBuffer=%p", dictSize, safeBuffer);

    if ((U32)dictSize > 64 KB) { dictSize = 64 KB; } /* useless to define a dictionary > 64 KB */
    if ((U32)dictSize > dict->dictSize) { dictSize = (int)dict->dictSize; }

    if (safeBuffer == NULL) assert(dictSize == 0);
    if (dictSize > 0) {
        const BYTE* const previousDictEnd = dict->dictionary + dict->dictSize;
        assert(dict->dictionary);
        LZ4_memmove(safeBuffer, previousDictEnd - dictSize, (size_t)dictSize);
    }

    dict->dictionary = (const BYTE*)safeBuffer;
    dict->dictSize = (U32)dictSize;

    return dictSize;
}



/*-*******************************
 *  Decompression functions
 ********************************/

typedef enum { decode_full_block = 0, partial_decode = 1 } earlyEnd_directive;

#undef MIN
#define MIN(a,b)    ( (a) < (b) ? (a) : (b) )


/* variant for decompress_unsafe()
 * does not know end of input
 * presumes input is well formed
 * note : will consume at least one byte */
static size_t read_long_length_no_check(const BYTE** pp)
{
    size_t b, l = 0;
    do { b = **pp; (*pp)++; l += b; } while (b==255);
    DEBUGLOG(6, "read_long_length_no_check: +length=%zu using %zu input bytes", l, l/255 + 1)
    return l;
}

/* core decoder variant for LZ4_decompress_fast*()
 * for legacy support only : these entry points are deprecated.
 * - Presumes input is correctly formed (no defense vs malformed inputs)
 * - Does not know input size (presume input buffer is "large enough")
 * - Decompress a full block (only)
 * @return : nb of bytes read from input.
 * Note : this variant is not optimized for speed, just for maintenance.
 *        the goal is to remove support of decompress_fast*() variants by v2.0
**/
LZ4_FORCE_INLINE int
LZ4_decompress_unsafe_generic(
                 const BYTE* const istart,
                 BYTE* const ostart,
                 int decompressedSize,

                 size_t prefixSize,
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note: =0 if dictStart==NULL */
                 )
{
    const BYTE* ip = istart;
    BYTE* op = (BYTE*)ostart;
    BYTE* const oend = ostart + decompressedSize;
    const BYTE* const prefixStart = ostart - prefixSize;

    DEBUGLOG(5, "LZ4_decompress_unsafe_generic");
    if (dictStart == NULL) assert(dictSize == 0);

    while (1) {
        /* start new sequence */
        unsigned token = *ip++;

        /* literals */
        {   size_t ll = token >> ML_BITS;
            if (ll==15) {
                /* long literal length */
                ll += read_long_length_no_check(&ip);
            }
            if ((size_t)(oend-op) < ll) return -1; /* output buffer overflow */
            LZ4_memmove(op, ip, ll); /* support in-place decompression */
            op += ll;
            ip += ll;
            if ((size_t)(oend-op) < MFLIMIT) {
                if (op==oend) break;  /* end of block */
                DEBUGLOG(5, "invalid: literals end at distance %zi from end of block", oend-op);
                /* incorrect end of block :
                 * last match must start at least MFLIMIT==12 bytes before end of output block */
                return -1;
        }   }

        /* match */
        {   size_t ml = token & 15;
            size_t const offset = LZ4_readLE16(ip);
            ip+=2;

            if (ml==15) {
                /* long literal length */
                ml += read_long_length_no_check(&ip);
            }
            ml += MINMATCH;

            if ((size_t)(oend-op) < ml) return -1; /* output buffer overflow */

            {   const BYTE* match = op - offset;

                /* out of range */
                if (offset > (size_t)(op - prefixStart) + dictSize) {
                    DEBUGLOG(6, "offset out of range");
                    return -1;
                }

                /* check special case : extDict */
                if (offset > (size_t)(op - prefixStart)) {
                    /* extDict scenario */
                    const BYTE* const dictEnd = dictStart + dictSize;
                    const BYTE* extMatch = dictEnd - (offset - (size_t)(op-prefixStart));
                    size_t const extml = (size_t)(dictEnd - extMatch);
                    if (extml > ml) {
                        /* match entirely within extDict */
                        LZ4_memmove(op, extMatch, ml);
                        op += ml;
                        ml = 0;
                    } else {
                        /* match split between extDict & prefix */
                        LZ4_memmove(op, extMatch, extml);
                        op += extml;
                        ml -= extml;
                    }
                    match = prefixStart;
                }

                /* match copy - slow variant, supporting overlap copy */
                {   size_t u;
                    for (u=0; u<ml; u++) {
                        op[u] = match[u];
            }   }   }
            op += ml;
            if ((size_t)(oend-op) < LASTLITERALS) {
                DEBUGLOG(5, "invalid: match ends at distance %zi from end of block", oend-op);
                /* incorrect end of block :
                 * last match must stop at least LASTLITERALS==5 bytes before end of output block */
                return -1;
            }
        } /* match */
    } /* main loop */
    return (int)(ip - istart);
}


/* Read the variable-length literal or match length.
 *
 * @ip : input pointer
 * @ilimit : position after which if length is not decoded, the input is necessarily corrupted.
 * @initial_check - check ip >= ipmax before start of loop.  Returns initial_error if so.
 * @error (output) - error code.  Must be set to 0 before call.
**/
typedef size_t Rvl_t;
static const Rvl_t rvl_error = (Rvl_t)(-1);
LZ4_FORCE_INLINE Rvl_t
read_variable_length(const BYTE** ip, const BYTE* ilimit,
                     int initial_check)
{
    Rvl_t s, length = 0;
    assert(ip != NULL);
    assert(*ip !=  NULL);
    assert(ilimit != NULL);
    if (initial_check && unlikely((*ip) >= ilimit)) {    /* read limit reached */
        return rvl_error;
    }
    s = **ip;
    (*ip)++;
    length += s;
    if (unlikely((*ip) > ilimit)) {    /* read limit reached */
        return rvl_error;
    }
    /* accumulator overflow detection (32-bit mode only) */
    if ((sizeof(length) < 8) && unlikely(length > ((Rvl_t)(-1)/2)) ) {
        return rvl_error;
    }
    if (likely(s != 255)) return length;
    do {
        s = **ip;
        (*ip)++;
        length += s;
        if (unlikely((*ip) > ilimit)) {    /* read limit reached */
            return rvl_error;
        }
        /* accumulator overflow detection (32-bit mode only) */
        if ((sizeof(length) < 8) && unlikely(length > ((Rvl_t)(-1)/2)) ) {
            return rvl_error;
        }
    } while (s == 255);

    return length;
}

/*! LZ4_decompress_generic() :
 *  This generic decompression function covers all use cases.
 *  It shall be instantiated several times, using different sets of directives.
 *  Note that it is important for performance that this function really get inlined,
 *  in order to remove useless branches during compilation optimization.
 */
LZ4_FORCE_INLINE int
LZ4_decompress_generic(
                 const char* const src,
                 char* const dst,
                 int srcSize,
                 int outputSize,         /* If endOnInput==endOnInputSize, this value is `dstCapacity` */

                 earlyEnd_directive partialDecoding,  /* full, partial */
                 dict_directive dict,                 /* noDict, withPrefix64k, usingExtDict */
                 const BYTE* const lowPrefix,  /* always <= dst, == dst when no prefix */
                 const BYTE* const dictStart,  /* only if dict==usingExtDict */
                 const size_t dictSize         /* note : = 0 if noDict */
                 )
{
    if ((src == NULL) || (outputSize < 0)) { return -1; }

    {   const BYTE* ip = (const BYTE*) src;
        const BYTE* const iend = ip + srcSize;

        BYTE* op = (BYTE*) dst;
        BYTE* const oend = op + outputSize;
        BYTE* cpy;

        const BYTE* const dictEnd = (dictStart == NULL) ? NULL : dictStart + dictSize;

        const int checkOffset = (dictSize < (int)(64 KB));


        /* Set up the "end" pointers for the shortcut. */
        const BYTE* const shortiend = iend - 14 /*maxLL*/ - 2 /*offset*/;
        const BYTE* const shortoend = oend - 14 /*maxLL*/ - 18 /*maxML*/;

        const BYTE* match;
        size_t offset;
        unsigned token;
        size_t length;


        DEBUGLOG(5, "LZ4_decompress_generic (srcSize:%i, dstSize:%i)", srcSize, outputSize);

        /* Special cases */
        assert(lowPrefix <= op);
        if (unlikely(outputSize==0)) {
            /* Empty output buffer */
            if (partialDecoding) return 0;
            return ((srcSize==1) && (*ip==0)) ? 0 : -1;
        }
        if (unlikely(srcSize==0)) { return -1; }

    /* LZ4_FAST_DEC_LOOP:
     * designed for modern OoO performance cpus,
     * where copying reliably 32-bytes is preferable to an unpredictable branch.
     * note : fast loop may show a regression for some client arm chips. */
#if LZ4_FAST_DEC_LOOP
        if ((oend - op) < FASTLOOP_SAFE_DISTANCE) {
            DEBUGLOG(6, "move to safe decode loop");
            goto safe_decode;
        }

        /* Fast loop : decode sequences as long as output < oend-FASTLOOP_SAFE_DISTANCE */
        DEBUGLOG(6, "using fast decode loop");
        while (1) {
            /* Main fastloop assertion: We can always wildcopy FASTLOOP_SAFE_DISTANCE */
            assert(oend - op >= FASTLOOP_SAFE_DISTANCE);
            assert(ip < iend);
            token = *ip++;
            length = token >> ML_BITS;  /* literal length */
            DEBUGLOG(7, "blockPos%6u: litLength token = %u", (unsigned)(op-(BYTE*)dst), (unsigned)length);

            /* decode literal length */
            if (length == RUN_MASK) {
                size_t const addl = read_variable_length(&ip, iend-RUN_MASK, 1);
                if (addl == rvl_error) {
                    DEBUGLOG(6, "error reading long literal length");
                    goto _output_error;
                }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; } /* overflow detection */
                if (unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; } /* overflow detection */

                /* copy literals */
                LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
                if ((op+length>oend-32) || (ip+length>iend-32)) { goto safe_literal_copy; }
                LZ4_wildCopy32(op, ip, op+length);
                ip += length; op += length;
            } else if (ip <= iend-(16 + 1/*max lit + offset + nextToken*/)) {
                /* We don't need to check oend, since we check it once for each loop below */
                DEBUGLOG(7, "copy %u bytes in a 16-bytes stripe", (unsigned)length);
                /* Literals can only be <= 14, but hope compilers optimize better when copy by a register size */
                LZ4_memcpy(op, ip, 16);
                ip += length; op += length;
            } else {
                goto safe_literal_copy;
            }

            /* get offset */
            offset = LZ4_readLE16(ip); ip+=2;
            DEBUGLOG(6, "blockPos%6u: offset = %u", (unsigned)(op-(BYTE*)dst), (unsigned)offset);
            match = op - offset;
            assert(match <= op);  /* overflow check */

            /* get matchlength */
            length = token & ML_MASK;
            DEBUGLOG(7, "  match length token = %u (len==%u)", (unsigned)length, (unsigned)length+MINMATCH);

            if (length == ML_MASK) {
                size_t const addl = read_variable_length(&ip, iend - LASTLITERALS + 1, 0);
                if (addl == rvl_error) {
                    DEBUGLOG(5, "error reading long match length");
                    goto _output_error;
                }
                length += addl;
                length += MINMATCH;
                DEBUGLOG(7, "  long match length == %u", (unsigned)length);
                if (unlikely((uptrval)(op)+length<(uptrval)op)) { goto _output_error; } /* overflow detection */
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    goto safe_match_copy;
                }
            } else {
                length += MINMATCH;
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    DEBUGLOG(7, "moving to safe_match_copy (ml==%u)", (unsigned)length);
                    goto safe_match_copy;
                }

                /* Fastpath check: skip LZ4_wildCopy32 when true */
                if ((dict == withPrefix64k) || (match >= lowPrefix)) {
                    if (offset >= 8) {
                        assert(match >= lowPrefix);
                        assert(match <= op);
                        assert(op + 18 <= oend);

                        LZ4_memcpy(op, match, 8);
                        LZ4_memcpy(op+8, match+8, 8);
                        LZ4_memcpy(op+16, match+16, 2);
                        op += length;
                        continue;
            }   }   }

            if ( checkOffset && (unlikely(match + dictSize < lowPrefix)) ) {
                DEBUGLOG(5, "Error : pos=%zi, offset=%zi => outside buffers", op-lowPrefix, op-match);
                goto _output_error;
            }
            /* match starting within external dictionary */
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                assert(dictEnd != NULL);
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) {
                        DEBUGLOG(7, "partialDecoding: dictionary match, close to dstEnd");
                        length = MIN(length, (size_t)(oend-op));
                    } else {
                        DEBUGLOG(6, "end-of-block condition violated")
                        goto _output_error;
                }   }

                if (length <= (size_t)(lowPrefix-match)) {
                    /* match fits entirely within external dictionary : just copy */
                    LZ4_memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    /* match stretches into both external dictionary and current block */
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {  /* overlap copy */
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) { *op++ = *copyFrom++; }
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }

            /* copy match within block */
            cpy = op + length;

            assert((op <= oend) && (oend-op >= 32));
            if (unlikely(offset<16)) {
                LZ4_memcpy_using_offset(op, match, cpy, offset);
            } else {
                LZ4_wildCopy32(op, match, cpy);
            }

            op = cpy;   /* wildcopy correction */
        }
    safe_decode:
#endif

        /* Main Loop : decode remaining sequences where output < FASTLOOP_SAFE_DISTANCE */
        DEBUGLOG(6, "using safe decode loop");
        while (1) {
            assert(ip < iend);
            token = *ip++;
            length = token >> ML_BITS;  /* literal length */
            DEBUGLOG(7, "blockPos%6u: litLength token = %u", (unsigned)(op-(BYTE*)dst), (unsigned)length);

            /* A two-stage shortcut for the most common case:
             * 1) If the literal length is 0..14, and there is enough space,
             * enter the shortcut and copy 16 bytes on behalf of the literals
             * (in the fast mode, only 8 bytes can be safely copied this way).
             * 2) Further if the match length is 4..18, copy 18 bytes in a similar
             * manner; but we ensure that there's enough space in the output for
             * those 18 bytes earlier, upon entering the shortcut (in other words,
             * there is a combined check for both stages).
             */
            if ( (length != RUN_MASK)
                /* strictly "less than" on input, to re-enter the loop with at least one byte */
              && likely((ip < shortiend) & (op <= shortoend)) ) {
                /* Copy the literals */
                LZ4_memcpy(op, ip, 16);
                op += length; ip += length;

                /* The second stage: prepare for match copying, decode full info.
                 * If it doesn't work out, the info won't be wasted. */
                length = token & ML_MASK; /* match length */
                DEBUGLOG(7, "blockPos%6u: matchLength token = %u (len=%u)", (unsigned)(op-(BYTE*)dst), (unsigned)length, (unsigned)length + 4);
                offset = LZ4_readLE16(ip); ip += 2;
                match = op - offset;
                assert(match <= op); /* check overflow */

                /* Do not deal with overlapping matches. */
                if ( (length != ML_MASK)
                  && (offset >= 8)
                  && (dict==withPrefix64k || match >= lowPrefix) ) {
                    /* Copy the match. */
                    LZ4_memcpy(op + 0, match + 0, 8);
                    LZ4_memcpy(op + 8, match + 8, 8);
                    LZ4_memcpy(op +16, match +16, 2);
                    op += length + MINMATCH;
                    /* Both stages worked, load the next token. */
                    continue;
                }

                /* The second stage didn't work out, but the info is ready.
                 * Propel it right to the point of match copying. */
                goto _copy_match;
            }

            /* decode literal length */
            if (length == RUN_MASK) {
                size_t const addl = read_variable_length(&ip, iend-RUN_MASK, 1);
                if (addl == rvl_error) { goto _output_error; }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; } /* overflow detection */
                if (unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; } /* overflow detection */
            }

#if LZ4_FAST_DEC_LOOP
        safe_literal_copy:
#endif
            /* copy literals */
            cpy = op+length;

            LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
            if ((cpy>oend-MFLIMIT) || (ip+length>iend-(2+1+LASTLITERALS))) {
                /* We've either hit the input parsing restriction or the output parsing restriction.
                 * In the normal scenario, decoding a full block, it must be the last sequence,
                 * otherwise it's an error (invalid input or dimensions).
                 * In partialDecoding scenario, it's necessary to ensure there is no buffer overflow.
                 */
                if (partialDecoding) {
                    /* Since we are partial decoding we may be in this block because of the output parsing
                     * restriction, which is not valid since the output buffer is allowed to be undersized.
                     */
                    DEBUGLOG(7, "partialDecoding: copying literals, close to input or output end")
                    DEBUGLOG(7, "partialDecoding: literal length = %u", (unsigned)length);
                    DEBUGLOG(7, "partialDecoding: remaining space in dstBuffer : %i", (int)(oend - op));
                    DEBUGLOG(7, "partialDecoding: remaining space in srcBuffer : %i", (int)(iend - ip));
                    /* Finishing in the middle of a literals segment,
                     * due to lack of input.
                     */
                    if (ip+length > iend) {
                        length = (size_t)(iend-ip);
                        cpy = op + length;
                    }
                    /* Finishing in the middle of a literals segment,
                     * due to lack of output space.
                     */
                    if (cpy > oend) {
                        cpy = oend;
                        assert(op<=oend);
                        length = (size_t)(oend-op);
                    }
                } else {
                     /* We must be on the last sequence (or invalid) because of the parsing limitations
                      * so check that we exactly consume the input and don't overrun the output buffer.
                      */
                    if ((ip+length != iend) || (cpy > oend)) {
                        DEBUGLOG(5, "should have been last run of literals")
                        DEBUGLOG(5, "ip(%p) + length(%i) = %p != iend (%p)", ip, (int)length, ip+length, iend);
                        DEBUGLOG(5, "or cpy(%p) > (oend-MFLIMIT)(%p)", cpy, oend-MFLIMIT);
                        DEBUGLOG(5, "after writing %u bytes / %i bytes available", (unsigned)(op-(BYTE*)dst), outputSize);
                        goto _output_error;
                    }
                }
                LZ4_memmove(op, ip, length);  /* supports overlapping memory regions, for in-place decompression scenarios */
                ip += length;
                op += length;
                /* Necessarily EOF when !partialDecoding.
                 * When partialDecoding, it is EOF if we've either
                 * filled the output buffer or
                 * can't proceed with reading an offset for following match.
                 */
                if (!partialDecoding || (cpy == oend) || (ip >= (iend-2))) {
                    break;
                }
            } else {
                LZ4_wildCopy8(op, ip, cpy);   /* can overwrite up to 8 bytes beyond cpy */
                ip += length; op = cpy;
            }

            /* get offset */
            offset = LZ4_readLE16(ip); ip+=2;
            match = op - offset;

            /* get matchlength */
            length = token & ML_MASK;
            DEBUGLOG(7, "blockPos%6u: matchLength token = %u", (unsigned)(op-(BYTE*)dst), (unsigned)length);

    _copy_match:
            if (length == ML_MASK) {
                size_t const addl = read_variable_length(&ip, iend - LASTLITERALS + 1, 0);
                if (addl == rvl_error) { goto _output_error; }
                length += addl;
                if (unlikely((uptrval)(op)+length<(uptrval)op)) goto _output_error;   /* overflow detection */
            }
            length += MINMATCH;

#if LZ4_FAST_DEC_LOOP
        safe_match_copy:
#endif
            if ((checkOffset) && (unlikely(match + dictSize < lowPrefix))) goto _output_error;   /* Error : offset outside buffers */
            /* match starting within external dictionary */
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                assert(dictEnd != NULL);
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) length = MIN(length, (size_t)(oend-op));
                    else goto _output_error;   /* doesn't respect parsing restriction */
                }

                if (length <= (size_t)(lowPrefix-match)) {
                    /* match fits entirely within external dictionary : just copy */
                    LZ4_memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    /* match stretches into both external dictionary and current block */
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {  /* overlap copy */
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) *op++ = *copyFrom++;
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }
            assert(match >= lowPrefix);

            /* copy match within block */
            cpy = op + length;

            /* partialDecoding : may end anywhere within the block */
            assert(op<=oend);
            if (partialDecoding && (cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                size_t const mlen = MIN(length, (size_t)(oend-op));
                const BYTE* const matchEnd = match + mlen;
                BYTE* const copyEnd = op + mlen;
                if (matchEnd > op) {   /* overlap copy */
                    while (op < copyEnd) { *op++ = *match++; }
                } else {
                    LZ4_memcpy(op, match, mlen);
                }
                op = copyEnd;
                if (op == oend) { break; }
                continue;
            }

            if (unlikely(offset<8)) {
                LZ4_write32(op, 0);   /* silence msan warning when offset==0 */
                op[0] = match[0];
                op[1] = match[1];
                op[2] = match[2];
                op[3] = match[3];
                match += inc32table[offset];
                LZ4_memcpy(op+4, match, 4);
                match -= dec64table[offset];
            } else {
                LZ4_memcpy(op, match, 8);
                match += 8;
            }
            op += 8;

            if (unlikely(cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                BYTE* const oCopyLimit = oend - (WILDCOPYLENGTH-1);
                if (cpy > oend-LASTLITERALS) { goto _output_error; } /* Error : last LASTLITERALS bytes must be literals (uncompressed) */
                if (op < oCopyLimit) {
                    LZ4_wildCopy8(op, match, oCopyLimit);
                    match += oCopyLimit - op;
                    op = oCopyLimit;
                }
                while (op < cpy) { *op++ = *match++; }
            } else {
                LZ4_memcpy(op, match, 8);
                if (length > 16) { LZ4_wildCopy8(op+8, match+8, cpy); }
            }
            op = cpy;   /* wildcopy correction */
        }

        /* end of decoding */
        DEBUGLOG(5, "decoded %i bytes", (int) (((char*)op)-dst));
        return (int) (((char*)op)-dst);     /* Nb of output bytes decoded */

        /* Overflow error detected */
    _output_error:
        return (int) (-(((const char*)ip)-src))-1;
    }
}


/*===== Instantiate the API decoding functions. =====*/

LZ4_FORCE_O2
int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxDecompressedSize,
                                  decode_full_block, noDict,
                                  (BYTE*)dest, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_partial(const char* src, char* dst, int compressedSize, int targetOutputSize, int dstCapacity)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(src, dst, compressedSize, dstCapacity,
                                  partial_decode,
                                  noDict, (BYTE*)dst, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_fast(const char* source, char* dest, int originalSize)
{
    DEBUGLOG(5, "LZ4_decompress_fast");
    return LZ4_decompress_unsafe_generic(
                (const BYTE*)source, (BYTE*)dest, originalSize,
                0, NULL, 0);
}

/*===== Instantiate a few more decoding cases, used more than once. =====*/

LZ4_FORCE_O2 /* Exported, an obsolete API function. */
int LZ4_decompress_safe_withPrefix64k(const char* source, char* dest, int compressedSize, int maxOutputSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, withPrefix64k,
                                  (BYTE*)dest - 64 KB, NULL, 0);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_partial_withPrefix64k(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(source, dest, compressedSize, dstCapacity,
                                  partial_decode, withPrefix64k,
                                  (BYTE*)dest - 64 KB, NULL, 0);
}

/* Another obsolete API function, paired with the previous one. */
int LZ4_decompress_fast_withPrefix64k(const char* source, char* dest, int originalSize)
{
    return LZ4_decompress_unsafe_generic(
                (const BYTE*)source, (BYTE*)dest, originalSize,
                64 KB, NULL, 0);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_withSmallPrefix(const char* source, char* dest, int compressedSize, int maxOutputSize,
                                               size_t prefixSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, noDict,
                                  (BYTE*)dest-prefixSize, NULL, 0);
}

LZ4_FORCE_O2
static int LZ4_decompress_safe_partial_withSmallPrefix(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity,
                                               size_t prefixSize)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(source, dest, compressedSize, dstCapacity,
                                  partial_decode, noDict,
                                  (BYTE*)dest-prefixSize, NULL, 0);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int maxOutputSize,
                                     const void* dictStart, size_t dictSize)
{
    DEBUGLOG(5, "LZ4_decompress_safe_forceExtDict");
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, usingExtDict,
                                  (BYTE*)dest, (const BYTE*)dictStart, dictSize);
}

LZ4_FORCE_O2
int LZ4_decompress_safe_partial_forceExtDict(const char* source, char* dest,
                                     int compressedSize, int targetOutputSize, int dstCapacity,
                                     const void* dictStart, size_t dictSize)
{
    dstCapacity = MIN(targetOutputSize, dstCapacity);
    return LZ4_decompress_generic(source, dest, compressedSize, dstCapacity,
                                  partial_decode, usingExtDict,
                                  (BYTE*)dest, (const BYTE*)dictStart, dictSize);
}

LZ4_FORCE_O2
static int LZ4_decompress_fast_extDict(const char* source, char* dest, int originalSize,
                                       const void* dictStart, size_t dictSize)
{
    return LZ4_decompress_unsafe_generic(
                (const BYTE*)source, (BYTE*)dest, originalSize,
                0, (const BYTE*)dictStart, dictSize);
}

/* The "double dictionary" mode, for use with e.g. ring buffers: the first part
 * of the dictionary is passed as prefix, and the second via dictStart + dictSize.
 * These routines are used only once, in LZ4_decompress_*_continue().
 */
LZ4_FORCE_INLINE
int LZ4_decompress_safe_doubleDict(const char* source, char* dest, int compressedSize, int maxOutputSize,
                                   size_t prefixSize, const void* dictStart, size_t dictSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxOutputSize,
                                  decode_full_block, usingExtDict,
                                  (BYTE*)dest-prefixSize, (const BYTE*)dictStart, dictSize);
}

/*===== streaming decompression functions =====*/

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4_streamDecode_t* LZ4_createStreamDecode(void)
{
    LZ4_STATIC_ASSERT(sizeof(LZ4_streamDecode_t) >= sizeof(LZ4_streamDecode_t_internal));
    return (LZ4_streamDecode_t*) ALLOC_AND_ZERO(sizeof(LZ4_streamDecode_t));
}

int LZ4_freeStreamDecode (LZ4_streamDecode_t* LZ4_stream)
{
    if (LZ4_stream == NULL) { return 0; }  /* support free on NULL */
    FREEMEM(LZ4_stream);
    return 0;
}
#endif

/*! LZ4_setStreamDecode() :
 *  Use this function to instruct where to find the dictionary.
 *  This function is not necessary if previous data is still available where it was decoded.
 *  Loading a size of 0 is allowed (same effect as no dictionary).
 * @return : 1 if OK, 0 if error
 */
int LZ4_setStreamDecode (LZ4_streamDecode_t* LZ4_streamDecode, const char* dictionary, int dictSize)
{
    LZ4_streamDecode_t_internal* lz4sd = &LZ4_streamDecode->internal_donotuse;
    lz4sd->prefixSize = (size_t)dictSize;
    if (dictSize) {
        assert(dictionary != NULL);
        lz4sd->prefixEnd = (const BYTE*) dictionary + dictSize;
    } else {
        lz4sd->prefixEnd = (const BYTE*) dictionary;
    }
    lz4sd->externalDict = NULL;
    lz4sd->extDictSize  = 0;
    return 1;
}

/*! LZ4_decoderRingBufferSize() :
 *  when setting a ring buffer for streaming decompression (optional scenario),
 *  provides the minimum size of this ring buffer
 *  to be compatible with any source respecting maxBlockSize condition.
 *  Note : in a ring buffer scenario,
 *  blocks are presumed decompressed next to each other.
 *  When not enough space remains for next block (remainingSize < maxBlockSize),
 *  decoding resumes from beginning of ring buffer.
 * @return : minimum ring buffer size,
 *           or 0 if there is an error (invalid maxBlockSize).
 */
int LZ4_decoderRingBufferSize(int maxBlockSize)
{
    if (maxBlockSize < 0) return 0;
    if (maxBlockSize > LZ4_MAX_INPUT_SIZE) return 0;
    if (maxBlockSize < 16) maxBlockSize = 16;
    return LZ4_DECODER_RING_BUFFER_SIZE(maxBlockSize);
}

/*
*_continue() :
    These decoding functions allow decompression of multiple blocks in "streaming" mode.
    Previously decoded blocks must still be available at the memory position where they were decoded.
    If it's not possible, save the relevant part of decoded data into a safe buffer,
    and indicate where it stands using LZ4_setStreamDecode()
*/
LZ4_FORCE_O2
int LZ4_decompress_safe_continue (LZ4_streamDecode_t* LZ4_streamDecode, const char* source, char* dest, int compressedSize, int maxOutputSize)
{
    LZ4_streamDecode_t_internal* lz4sd = &LZ4_streamDecode->internal_donotuse;
    int result;

    if (lz4sd->prefixSize == 0) {
        /* The first call, no dictionary yet. */
        assert(lz4sd->extDictSize == 0);
        result = LZ4_decompress_safe(source, dest, compressedSize, maxOutputSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)result;
        lz4sd->prefixEnd = (BYTE*)dest + result;
    } else if (lz4sd->prefixEnd == (BYTE*)dest) {
        /* They're rolling the current segment. */
        if (lz4sd->prefixSize >= 64 KB - 1)
            result = LZ4_decompress_safe_withPrefix64k(source, dest, compressedSize, maxOutputSize);
        else if (lz4sd->extDictSize == 0)
            result = LZ4_decompress_safe_withSmallPrefix(source, dest, compressedSize, maxOutputSize,
                                                         lz4sd->prefixSize);
        else
            result = LZ4_decompress_safe_doubleDict(source, dest, compressedSize, maxOutputSize,
                                                    lz4sd->prefixSize, lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize += (size_t)result;
        lz4sd->prefixEnd  += result;
    } else {
        /* The buffer wraps around, or they're switching to another buffer. */
        lz4sd->extDictSize = lz4sd->prefixSize;
        lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
        result = LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize,
                                                  lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)result;
        lz4sd->prefixEnd  = (BYTE*)dest + result;
    }

    return result;
}

LZ4_FORCE_O2 int
LZ4_decompress_fast_continue (LZ4_streamDecode_t* LZ4_streamDecode,
                        const char* source, char* dest, int originalSize)
{
    LZ4_streamDecode_t_internal* const lz4sd =
        (assert(LZ4_streamDecode!=NULL), &LZ4_streamDecode->internal_donotuse);
    int result;

    DEBUGLOG(5, "LZ4_decompress_fast_continue (toDecodeSize=%i)", originalSize);
    assert(originalSize >= 0);

    if (lz4sd->prefixSize == 0) {
        DEBUGLOG(5, "first invocation : no prefix nor extDict");
        assert(lz4sd->extDictSize == 0);
        result = LZ4_decompress_fast(source, dest, originalSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)originalSize;
        lz4sd->prefixEnd = (BYTE*)dest + originalSize;
    } else if (lz4sd->prefixEnd == (BYTE*)dest) {
        DEBUGLOG(5, "continue using existing prefix");
        result = LZ4_decompress_unsafe_generic(
                        (const BYTE*)source, (BYTE*)dest, originalSize,
                        lz4sd->prefixSize,
                        lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize += (size_t)originalSize;
        lz4sd->prefixEnd  += originalSize;
    } else {
        DEBUGLOG(5, "prefix becomes extDict");
        lz4sd->extDictSize = lz4sd->prefixSize;
        lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
        result = LZ4_decompress_fast_extDict(source, dest, originalSize,
                                             lz4sd->externalDict, lz4sd->extDictSize);
        if (result <= 0) return result;
        lz4sd->prefixSize = (size_t)originalSize;
        lz4sd->prefixEnd  = (BYTE*)dest + originalSize;
    }

    return result;
}


/*
Advanced decoding functions :
*_usingDict() :
    These decoding functions work the same as "_continue" ones,
    the dictionary must be explicitly provided within parameters
*/

int LZ4_decompress_safe_usingDict(const char* source, char* dest, int compressedSize, int maxOutputSize, const char* dictStart, int dictSize)
{
    if (dictSize==0)
        return LZ4_decompress_safe(source, dest, compressedSize, maxOutputSize);
    if (dictStart+dictSize == dest) {
        if (dictSize >= 64 KB - 1) {
            return LZ4_decompress_safe_withPrefix64k(source, dest, compressedSize, maxOutputSize);
        }
        assert(dictSize >= 0);
        return LZ4_decompress_safe_withSmallPrefix(source, dest, compressedSize, maxOutputSize, (size_t)dictSize);
    }
    assert(dictSize >= 0);
    return LZ4_decompress_safe_forceExtDict(source, dest, compressedSize, maxOutputSize, dictStart, (size_t)dictSize);
}

int LZ4_decompress_safe_partial_usingDict(const char* source, char* dest, int compressedSize, int targetOutputSize, int dstCapacity, const char* dictStart, int dictSize)
{
    if (dictSize==0)
        return LZ4_decompress_safe_partial(source, dest, compressedSize, targetOutputSize, dstCapacity);
    if (dictStart+dictSize == dest) {
        if (dictSize >= 64 KB - 1) {
            return LZ4_decompress_safe_partial_withPrefix64k(source, dest, compressedSize, targetOutputSize, dstCapacity);
        }
        assert(dictSize >= 0);
        return LZ4_decompress_safe_partial_withSmallPrefix(source, dest, compressedSize, targetOutputSize, dstCapacity, (size_t)dictSize);
    }
    assert(dictSize >= 0);
    return LZ4_decompress_safe_partial_forceExtDict(source, dest, compressedSize, targetOutputSize, dstCapacity, dictStart, (size_t)dictSize);
}

int LZ4_decompress_fast_usingDict(const char* source, char* dest, int originalSize, const char* dictStart, int dictSize)
{
    if (dictSize==0 || dictStart+dictSize == dest)
        return LZ4_decompress_unsafe_generic(
                        (const BYTE*)source, (BYTE*)dest, originalSize,
                        (size_t)dictSize, NULL, 0);
    assert(dictSize >= 0);
    return LZ4_decompress_fast_extDict(source, dest, originalSize, dictStart, (size_t)dictSize);
}


/*=*************************************************
*  Obsolete Functions
***************************************************/
/* obsolete compression functions */
int LZ4_compress_limitedOutput(const char* source, char* dest, int inputSize, int maxOutputSize)
{
    return LZ4_compress_default(source, dest, inputSize, maxOutputSize);
}
int LZ4_compress(const char* src, char* dest, int srcSize)
{
    return LZ4_compress_default(src, dest, srcSize, LZ4_compressBound(srcSize));
}
int LZ4_compress_limitedOutput_withState (void* state, const char* src, char* dst, int srcSize, int dstSize)
{
    return LZ4_compress_fast_extState(state, src, dst, srcSize, dstSize, 1);
}
int LZ4_compress_withState (void* state, const char* src, char* dst, int srcSize)
{
    return LZ4_compress_fast_extState(state, src, dst, srcSize, LZ4_compressBound(srcSize), 1);
}
int LZ4_compress_limitedOutput_continue (LZ4_stream_t* LZ4_stream, const char* src, char* dst, int srcSize, int dstCapacity)
{
    return LZ4_compress_fast_continue(LZ4_stream, src, dst, srcSize, dstCapacity, 1);
}
int LZ4_compress_continue (LZ4_stream_t* LZ4_stream, const char* source, char* dest, int inputSize)
{
    return LZ4_compress_fast_continue(LZ4_stream, source, dest, inputSize, LZ4_compressBound(inputSize), 1);
}

/*
These decompression functions are deprecated and should no longer be used.
They are only provided here for compatibility with older user programs.
- LZ4_uncompress is totally equivalent to LZ4_decompress_fast
- LZ4_uncompress_unknownOutputSize is totally equivalent to LZ4_decompress_safe
*/
int LZ4_uncompress (const char* source, char* dest, int outputSize)
{
    return LZ4_decompress_fast(source, dest, outputSize);
}
int LZ4_uncompress_unknownOutputSize (const char* source, char* dest, int isize, int maxOutputSize)
{
    return LZ4_decompress_safe(source, dest, isize, maxOutputSize);
}

/* Obsolete Streaming functions */

int LZ4_sizeofStreamState(void) { return sizeof(LZ4_stream_t); }

int LZ4_resetStreamState(void* state, char* inputBuffer)
{
    (void)inputBuffer;
    LZ4_resetStream((LZ4_stream_t*)state);
    return 0;
}

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
void* LZ4_create (char* inputBuffer)
{
    (void)inputBuffer;
    return LZ4_createStream();
}
#endif

char* LZ4_slideInputBuffer (void* state)
{
    /* avoid const char * -> char * conversion warning */
    return (char *)(uptrval)((LZ4_stream_t*)state)->internal_donotuse.dictionary;
}

#endif   /* LZ4_COMMONDEFS_ONLY */

/*
    LZ4 HC - High Compression Mode of LZ4
    Copyright (C) 2011-2020, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
       - LZ4 source repository : https://github.com/lz4/lz4
       - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/* note : lz4hc is not an independent module, it requires lz4.h/lz4.c for proper compilation */


/* *************************************
*  Tuning Parameter
***************************************/

/*! HEAPMODE :
 *  Select how stateless HC compression functions like `LZ4_compress_HC()`
 *  allocate memory for their workspace:
 *  in stack (0:fastest), or in heap (1:default, requires malloc()).
 *  Since workspace is rather large, heap mode is recommended.
**/
#ifndef LZ4HC_HEAPMODE
#  define LZ4HC_HEAPMODE 1
#endif


/*===    Dependency    ===*/
#define LZ4_HC_STATIC_LINKING_ONLY
#include "lz4hc.h"
#include <limits.h>


/*===   Shared lz4.c code   ===*/
#ifndef LZ4_SRC_INCLUDED
# if defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wunused-function"
# endif
# if defined (__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
# endif
# define LZ4_COMMONDEFS_ONLY
# include "lz4.c"   /* LZ4_count, constants, mem */
#endif


/*===   Enums   ===*/
typedef enum { noDictCtx, usingDictCtxHc } dictCtx_directive;


/*===   Constants   ===*/
#define OPTIMAL_ML (int)((ML_MASK-1)+MINMATCH)
#define LZ4_OPT_NUM   (1<<12)


/*===   Macros   ===*/
#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )
#define MAX(a,b)   ( (a) > (b) ? (a) : (b) )


/*===   Levels definition   ===*/
typedef enum { lz4mid, lz4hc, lz4opt } lz4hc_strat_e;
typedef struct {
    lz4hc_strat_e strat;
    int nbSearches;
    U32 targetLength;
} cParams_t;
static const cParams_t k_clTable[LZ4HC_CLEVEL_MAX+1] = {
    { lz4mid,    2, 16 },  /* 0, unused */
    { lz4mid,    2, 16 },  /* 1, unused */
    { lz4mid,    2, 16 },  /* 2 */
    { lz4hc,     4, 16 },  /* 3 */
    { lz4hc,     8, 16 },  /* 4 */
    { lz4hc,    16, 16 },  /* 5 */
    { lz4hc,    32, 16 },  /* 6 */
    { lz4hc,    64, 16 },  /* 7 */
    { lz4hc,   128, 16 },  /* 8 */
    { lz4hc,   256, 16 },  /* 9 */
    { lz4opt,   96, 64 },  /*10==LZ4HC_CLEVEL_OPT_MIN*/
    { lz4opt,  512,128 },  /*11 */
    { lz4opt,16384,LZ4_OPT_NUM },  /* 12==LZ4HC_CLEVEL_MAX */
};

static cParams_t LZ4HC_getCLevelParams(int cLevel)
{
    /* note : clevel convention is a bit different from lz4frame,
     * possibly something worth revisiting for consistency */
    if (cLevel < 1)
        cLevel = LZ4HC_CLEVEL_DEFAULT;
    cLevel = MIN(LZ4HC_CLEVEL_MAX, cLevel);
    return k_clTable[cLevel];
}


/*===   Hashing   ===*/
#define LZ4HC_HASHSIZE 4
#define HASH_FUNCTION(i)      (((i) * 2654435761U) >> ((MINMATCH*8)-LZ4HC_HASH_LOG))
static U32 LZ4HC_hashPtr(const void* ptr) { return HASH_FUNCTION(LZ4_read32(ptr)); }

#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==2)
/* lie to the compiler about data alignment; use with caution */
static U64 LZ4_read64(const void* memPtr) { return *(const U64*) memPtr; }

#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==1)
/* __pack instructions are safer, but compiler specific */
LZ4_PACK(typedef struct { U64 u64; }) LZ4_unalign64;
static U64 LZ4_read64(const void* ptr) { return ((const LZ4_unalign64*)ptr)->u64; }

#else  /* safe and portable access using memcpy() */
static U64 LZ4_read64(const void* memPtr)
{
    U64 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}

#endif /* LZ4_FORCE_MEMORY_ACCESS */

#define LZ4MID_HASHSIZE 8
#define LZ4MID_HASHLOG (LZ4HC_HASH_LOG-1)
#define LZ4MID_HASHTABLESIZE (1 << LZ4MID_HASHLOG)

static U32 LZ4MID_hash4(U32 v) { return (v * 2654435761U) >> (32-LZ4MID_HASHLOG); }
static U32 LZ4MID_hash4Ptr(const void* ptr) { return LZ4MID_hash4(LZ4_read32(ptr)); }
/* note: hash7 hashes the lower 56-bits.
 * It presumes input was read using little endian.*/
static U32 LZ4MID_hash7(U64 v) { return (U32)(((v  << (64-56)) * 58295818150454627ULL) >> (64-LZ4MID_HASHLOG)) ; }
static U64 LZ4_readLE64(const void* memPtr);
static U32 LZ4MID_hash8Ptr(const void* ptr) { return LZ4MID_hash7(LZ4_readLE64(ptr)); }

static U64 LZ4_readLE64(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read64(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        /* note: relies on the compiler to simplify this expression */
        return (U64)p[0] | ((U64)p[1]<<8) | ((U64)p[2]<<16) | ((U64)p[3]<<24)
            | ((U64)p[4]<<32) | ((U64)p[5]<<40) | ((U64)p[6]<<48) | ((U64)p[7]<<56);
    }
}


/*===   Count match length   ===*/
LZ4_FORCE_INLINE
unsigned LZ4HC_NbCommonBytes32(U32 val)
{
    assert(val != 0);
    if (LZ4_isLittleEndian()) {
#     if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(LZ4_FORCE_SW_BITCOUNT)
        unsigned long r;
        _BitScanReverse(&r, val);
        return (unsigned)((31 - r) >> 3);
#     elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
        return (unsigned)__builtin_clz(val) >> 3;
#     else
        val >>= 8;
        val = ((((val + 0x00FFFF00) | 0x00FFFFFF) + val) |
              (val + 0x00FF0000)) >> 24;
        return (unsigned)val ^ 3;
#     endif
    } else {
#     if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(LZ4_FORCE_SW_BITCOUNT)
        unsigned long r;
        _BitScanForward(&r, val);
        return (unsigned)(r >> 3);
#     elif (defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ > 3) || \
                            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))))) && \
                                        !defined(LZ4_FORCE_SW_BITCOUNT)
        return (unsigned)__builtin_ctz(val) >> 3;
#     else
        const U32 m = 0x01010101;
        return (unsigned)((((val - 1) ^ val) & (m - 1)) * m) >> 24;
#     endif
    }
}

/** LZ4HC_countBack() :
 * @return : negative value, nb of common bytes before ip/match */
LZ4_FORCE_INLINE
int LZ4HC_countBack(const BYTE* const ip, const BYTE* const match,
                    const BYTE* const iMin, const BYTE* const mMin)
{
    int back = 0;
    int const min = (int)MAX(iMin - ip, mMin - match);
    assert(min <= 0);
    assert(ip >= iMin); assert((size_t)(ip-iMin) < (1U<<31));
    assert(match >= mMin); assert((size_t)(match - mMin) < (1U<<31));

    while ((back - min) > 3) {
        U32 const v = LZ4_read32(ip + back - 4) ^ LZ4_read32(match + back - 4);
        if (v) {
            return (back - (int)LZ4HC_NbCommonBytes32(v));
        } else back -= 4; /* 4-byte step */
    }
    /* check remainder if any */
    while ( (back > min)
         && (ip[back-1] == match[back-1]) )
            back--;
    return back;
}

/*===   Chain table updates   ===*/
#define DELTANEXTU16(table, pos) table[(U16)(pos)]   /* faster */
/* Make fields passed to, and updated by LZ4HC_encodeSequence explicit */
#define UPDATABLE(ip, op, anchor) &ip, &op, &anchor


/**************************************
*  Init
**************************************/
static void LZ4HC_clearTables (LZ4HC_CCtx_internal* hc4)
{
    MEM_INIT(hc4->hashTable, 0, sizeof(hc4->hashTable));
    MEM_INIT(hc4->chainTable, 0xFF, sizeof(hc4->chainTable));
}

static void LZ4HC_init_internal (LZ4HC_CCtx_internal* hc4, const BYTE* start)
{
    size_t const bufferSize = (size_t)(hc4->end - hc4->prefixStart);
    size_t newStartingOffset = bufferSize + hc4->dictLimit;
    DEBUGLOG(5, "LZ4HC_init_internal");
    assert(newStartingOffset >= bufferSize);  /* check overflow */
    if (newStartingOffset > 1 GB) {
        LZ4HC_clearTables(hc4);
        newStartingOffset = 0;
    }
    newStartingOffset += 64 KB;
    hc4->nextToUpdate = (U32)newStartingOffset;
    hc4->prefixStart = start;
    hc4->end = start;
    hc4->dictStart = start;
    hc4->dictLimit = (U32)newStartingOffset;
    hc4->lowLimit = (U32)newStartingOffset;
}


/**************************************
*  Encode
**************************************/
/* LZ4HC_encodeSequence() :
 * @return : 0 if ok,
 *           1 if buffer issue detected */
LZ4_FORCE_INLINE int LZ4HC_encodeSequence (
    const BYTE** _ip,
    BYTE** _op,
    const BYTE** _anchor,
    int matchLength,
    int offset,
    limitedOutput_directive limit,
    BYTE* oend)
{
#define ip      (*_ip)
#define op      (*_op)
#define anchor  (*_anchor)

    size_t length;
    BYTE* const token = op++;

#if defined(LZ4_DEBUG) && (LZ4_DEBUG >= 6)
    static const BYTE* start = NULL;
    static U32 totalCost = 0;
    U32 const pos = (start==NULL) ? 0 : (U32)(anchor - start);
    U32 const ll = (U32)(ip - anchor);
    U32 const llAdd = (ll>=15) ? ((ll-15) / 255) + 1 : 0;
    U32 const mlAdd = (matchLength>=19) ? ((matchLength-19) / 255) + 1 : 0;
    U32 const cost = 1 + llAdd + ll + 2 + mlAdd;
    if (start==NULL) start = anchor;  /* only works for single segment */
    /* g_debuglog_enable = (pos >= 2228) & (pos <= 2262); */
    DEBUGLOG(6, "pos:%7u -- literals:%4u, match:%4i, offset:%5i, cost:%4u + %5u",
                pos,
                (U32)(ip - anchor), matchLength, offset,
                cost, totalCost);
    totalCost += cost;
#endif

    /* Encode Literal length */
    length = (size_t)(ip - anchor);
    LZ4_STATIC_ASSERT(notLimited == 0);
    /* Check output limit */
    if (limit && ((op + (length / 255) + length + (2 + 1 + LASTLITERALS)) > oend)) {
        DEBUGLOG(6, "Not enough room to write %i literals (%i bytes remaining)",
                (int)length, (int)(oend - op));
        return 1;
    }
    if (length >= RUN_MASK) {
        size_t len = length - RUN_MASK;
        *token = (RUN_MASK << ML_BITS);
        for(; len >= 255 ; len -= 255) *op++ = 255;
        *op++ = (BYTE)len;
    } else {
        *token = (BYTE)(length << ML_BITS);
    }

    /* Copy Literals */
    LZ4_wildCopy8(op, anchor, op + length);
    op += length;

    /* Encode Offset */
    assert(offset <= LZ4_DISTANCE_MAX );
    assert(offset > 0);
    LZ4_writeLE16(op, (U16)(offset)); op += 2;

    /* Encode MatchLength */
    assert(matchLength >= MINMATCH);
    length = (size_t)matchLength - MINMATCH;
    if (limit && (op + (length / 255) + (1 + LASTLITERALS) > oend)) {
        DEBUGLOG(6, "Not enough room to write match length");
        return 1;   /* Check output limit */
    }
    if (length >= ML_MASK) {
        *token += ML_MASK;
        length -= ML_MASK;
        for(; length >= 510 ; length -= 510) { *op++ = 255; *op++ = 255; }
        if (length >= 255) { length -= 255; *op++ = 255; }
        *op++ = (BYTE)length;
    } else {
        *token += (BYTE)(length);
    }

    /* Prepare next loop */
    ip += matchLength;
    anchor = ip;

    return 0;

#undef ip
#undef op
#undef anchor
}


typedef struct {
    int off;
    int len;
    int back;  /* negative value */
} LZ4HC_match_t;

LZ4HC_match_t LZ4HC_searchExtDict(const BYTE* ip, U32 ipIndex,
        const BYTE* const iLowLimit, const BYTE* const iHighLimit,
        const LZ4HC_CCtx_internal* dictCtx, U32 gDictEndIndex,
        int currentBestML, int nbAttempts)
{
    size_t const lDictEndIndex = (size_t)(dictCtx->end - dictCtx->prefixStart) + dictCtx->dictLimit;
    U32 lDictMatchIndex = dictCtx->hashTable[LZ4HC_hashPtr(ip)];
    U32 matchIndex = lDictMatchIndex + gDictEndIndex - (U32)lDictEndIndex;
    int offset = 0, sBack = 0;
    assert(lDictEndIndex <= 1 GB);
    if (lDictMatchIndex>0)
        DEBUGLOG(7, "lDictEndIndex = %zu, lDictMatchIndex = %u", lDictEndIndex, lDictMatchIndex);
    while (ipIndex - matchIndex <= LZ4_DISTANCE_MAX && nbAttempts--) {
        const BYTE* const matchPtr = dictCtx->prefixStart - dictCtx->dictLimit + lDictMatchIndex;

        if (LZ4_read32(matchPtr) == LZ4_read32(ip)) {
            int mlt;
            int back = 0;
            const BYTE* vLimit = ip + (lDictEndIndex - lDictMatchIndex);
            if (vLimit > iHighLimit) vLimit = iHighLimit;
            mlt = (int)LZ4_count(ip+MINMATCH, matchPtr+MINMATCH, vLimit) + MINMATCH;
            back = (ip > iLowLimit) ? LZ4HC_countBack(ip, matchPtr, iLowLimit, dictCtx->prefixStart) : 0;
            mlt -= back;
            if (mlt > currentBestML) {
                currentBestML = mlt;
                offset = (int)(ipIndex - matchIndex);
                sBack = back;
                DEBUGLOG(7, "found match of length %i within extDictCtx", currentBestML);
        }   }

        {   U32 const nextOffset = DELTANEXTU16(dictCtx->chainTable, lDictMatchIndex);
            lDictMatchIndex -= nextOffset;
            matchIndex -= nextOffset;
    }   }

    {   LZ4HC_match_t md;
        md.len = currentBestML;
        md.off = offset;
        md.back = sBack;
        return md;
    }
}

typedef LZ4HC_match_t (*LZ4MID_searchIntoDict_f)(const BYTE* ip, U32 ipIndex,
        const BYTE* const iHighLimit,
        const LZ4HC_CCtx_internal* dictCtx, U32 gDictEndIndex);

static LZ4HC_match_t LZ4MID_searchHCDict(const BYTE* ip, U32 ipIndex,
        const BYTE* const iHighLimit,
        const LZ4HC_CCtx_internal* dictCtx, U32 gDictEndIndex)
{
    return LZ4HC_searchExtDict(ip,ipIndex,
                            ip, iHighLimit,
                            dictCtx, gDictEndIndex,
                            MINMATCH-1, 2);
}

static LZ4HC_match_t LZ4MID_searchExtDict(const BYTE* ip, U32 ipIndex,
        const BYTE* const iHighLimit,
        const LZ4HC_CCtx_internal* dictCtx, U32 gDictEndIndex)
{
    size_t const lDictEndIndex = (size_t)(dictCtx->end - dictCtx->prefixStart) + dictCtx->dictLimit;
    const U32* const hash4Table = dictCtx->hashTable;
    const U32* const hash8Table = hash4Table + LZ4MID_HASHTABLESIZE;
    DEBUGLOG(7, "LZ4MID_searchExtDict (ipIdx=%u)", ipIndex);

    /* search long match first */
    {   U32 l8DictMatchIndex = hash8Table[LZ4MID_hash8Ptr(ip)];
        U32 m8Index = l8DictMatchIndex + gDictEndIndex - (U32)lDictEndIndex;
        assert(lDictEndIndex <= 1 GB);
        if (ipIndex - m8Index <= LZ4_DISTANCE_MAX) {
            const BYTE* const matchPtr = dictCtx->prefixStart - dictCtx->dictLimit + l8DictMatchIndex;
            const size_t safeLen = MIN(lDictEndIndex - l8DictMatchIndex, (size_t)(iHighLimit - ip));
            int mlt = (int)LZ4_count(ip, matchPtr, ip + safeLen);
            if (mlt >= MINMATCH) {
                LZ4HC_match_t md;
                DEBUGLOG(7, "Found long ExtDict match of len=%u", mlt);
                md.len = mlt;
                md.off = (int)(ipIndex - m8Index);
                md.back = 0;
                return md;
            }
        }
    }

    /* search for short match second */
    {   U32 l4DictMatchIndex = hash4Table[LZ4MID_hash4Ptr(ip)];
        U32 m4Index = l4DictMatchIndex + gDictEndIndex - (U32)lDictEndIndex;
        if (ipIndex - m4Index <= LZ4_DISTANCE_MAX) {
            const BYTE* const matchPtr = dictCtx->prefixStart - dictCtx->dictLimit + l4DictMatchIndex;
            const size_t safeLen = MIN(lDictEndIndex - l4DictMatchIndex, (size_t)(iHighLimit - ip));
            int mlt = (int)LZ4_count(ip, matchPtr, ip + safeLen);
            if (mlt >= MINMATCH) {
                LZ4HC_match_t md;
                DEBUGLOG(7, "Found short ExtDict match of len=%u", mlt);
                md.len = mlt;
                md.off = (int)(ipIndex - m4Index);
                md.back = 0;
                return md;
            }
        }
    }

    /* nothing found */
    {   LZ4HC_match_t const md = {0, 0, 0 };
        return md;
    }
}

/**************************************
*  Mid Compression (level 2)
**************************************/

LZ4_FORCE_INLINE void
LZ4MID_addPosition(U32* hTable, U32 hValue, U32 index)
{
    hTable[hValue] = index;
}

#define ADDPOS8(_p, _idx) LZ4MID_addPosition(hash8Table, LZ4MID_hash8Ptr(_p), _idx)
#define ADDPOS4(_p, _idx) LZ4MID_addPosition(hash4Table, LZ4MID_hash4Ptr(_p), _idx)

/* Fill hash tables with references into dictionary.
 * The resulting table is only exploitable by LZ4MID (level 2) */
static void
LZ4MID_fillHTable (LZ4HC_CCtx_internal* cctx, const void* dict, size_t size)
{
    U32* const hash4Table = cctx->hashTable;
    U32* const hash8Table = hash4Table + LZ4MID_HASHTABLESIZE;
    const BYTE* const prefixPtr = (const BYTE*)dict;
    U32 const prefixIdx = cctx->dictLimit;
    U32 const target = prefixIdx + (U32)size - LZ4MID_HASHSIZE;
    U32 idx = cctx->nextToUpdate;
    assert(dict == cctx->prefixStart);
    DEBUGLOG(4, "LZ4MID_fillHTable (size:%zu)", size);
    if (size <= LZ4MID_HASHSIZE)
        return;

    for (; idx < target; idx += 3) {
        ADDPOS4(prefixPtr+idx-prefixIdx, idx);
        ADDPOS8(prefixPtr+idx+1-prefixIdx, idx+1);
    }

    idx = (size > 32 KB + LZ4MID_HASHSIZE) ? target - 32 KB : cctx->nextToUpdate;
    for (; idx < target; idx += 1) {
        ADDPOS8(prefixPtr+idx-prefixIdx, idx);
    }

    cctx->nextToUpdate = target;
}

static LZ4MID_searchIntoDict_f select_searchDict_function(const LZ4HC_CCtx_internal* dictCtx)
{
    if (dictCtx == NULL) return NULL;
    if (LZ4HC_getCLevelParams(dictCtx->compressionLevel).strat == lz4mid)
        return LZ4MID_searchExtDict;
    return LZ4MID_searchHCDict;
}

static int LZ4MID_compress (
    LZ4HC_CCtx_internal* const ctx,
    const char* const src,
    char* const dst,
    int* srcSizePtr,
    int const maxOutputSize,
    const limitedOutput_directive limit,
    const dictCtx_directive dict
    )
{
    U32* const hash4Table = ctx->hashTable;
    U32* const hash8Table = hash4Table + LZ4MID_HASHTABLESIZE;
    const BYTE* ip = (const BYTE*)src;
    const BYTE* anchor = ip;
    const BYTE* const iend = ip + *srcSizePtr;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = (iend - LASTLITERALS);
    const BYTE* const ilimit = (iend - LZ4MID_HASHSIZE);
    BYTE* op = (BYTE*)dst;
    BYTE* oend = op + maxOutputSize;

    const BYTE* const prefixPtr = ctx->prefixStart;
    const U32 prefixIdx = ctx->dictLimit;
    const U32 ilimitIdx = (U32)(ilimit - prefixPtr) + prefixIdx;
    const BYTE* const dictStart = ctx->dictStart;
    const U32 dictIdx = ctx->lowLimit;
    const U32 gDictEndIndex = ctx->lowLimit;
    const LZ4MID_searchIntoDict_f searchIntoDict = (dict == usingDictCtxHc) ? select_searchDict_function(ctx->dictCtx) : NULL;
    unsigned matchLength;
    unsigned matchDistance;

    /* input sanitization */
    DEBUGLOG(5, "LZ4MID_compress (%i bytes)", *srcSizePtr);
    if (dict == usingDictCtxHc) DEBUGLOG(5, "usingDictCtxHc");
    assert(*srcSizePtr >= 0);
    if (*srcSizePtr) assert(src != NULL);
    if (maxOutputSize) assert(dst != NULL);
    if (*srcSizePtr < 0) return 0;  /* invalid */
    if (maxOutputSize < 0) return 0; /* invalid */
    if (*srcSizePtr > LZ4_MAX_INPUT_SIZE) {
        /* forbidden: no input is allowed to be that large */
        return 0;
    }
    if (limit == fillOutput) oend -= LASTLITERALS;  /* Hack for support LZ4 format restriction */
    if (*srcSizePtr < LZ4_minLength)
        goto _lz4mid_last_literals;  /* Input too small, no compression (all literals) */

    /* main loop */
    while (ip <= mflimit) {
        const U32 ipIndex = (U32)(ip - prefixPtr) + prefixIdx;
        /* search long match */
        {   U32 const h8 = LZ4MID_hash8Ptr(ip);
            U32 const pos8 = hash8Table[h8];
            assert(h8 < LZ4MID_HASHTABLESIZE);
            assert(pos8 < ipIndex);
            LZ4MID_addPosition(hash8Table, h8, ipIndex);
            if (ipIndex - pos8 <= LZ4_DISTANCE_MAX) {
                /* match candidate found */
                if (pos8 >= prefixIdx) {
                    const BYTE* const matchPtr = prefixPtr + pos8 - prefixIdx;
                    assert(matchPtr < ip);
                    matchLength = LZ4_count(ip, matchPtr, matchlimit);
                    if (matchLength >= MINMATCH) {
                        DEBUGLOG(7, "found long match at pos %u (len=%u)", pos8, matchLength);
                        matchDistance = ipIndex - pos8;
                        goto _lz4mid_encode_sequence;
                    }
                } else {
                    if (pos8 >= dictIdx) {
                        /* extDict match candidate */
                        const BYTE* const matchPtr = dictStart + (pos8 - dictIdx);
                        const size_t safeLen = MIN(prefixIdx - pos8, (size_t)(matchlimit - ip));
                        matchLength = LZ4_count(ip, matchPtr, ip + safeLen);
                        if (matchLength >= MINMATCH) {
                            DEBUGLOG(7, "found long match at ExtDict pos %u (len=%u)", pos8, matchLength);
                            matchDistance = ipIndex - pos8;
                            goto _lz4mid_encode_sequence;
                        }
                    }
                }
        }   }
        /* search short match */
        {   U32 const h4 = LZ4MID_hash4Ptr(ip);
            U32 const pos4 = hash4Table[h4];
            assert(h4 < LZ4MID_HASHTABLESIZE);
            assert(pos4 < ipIndex);
            LZ4MID_addPosition(hash4Table, h4, ipIndex);
            if (ipIndex - pos4 <= LZ4_DISTANCE_MAX) {
                /* match candidate found */
                if (pos4 >= prefixIdx) {
                /* only search within prefix */
                    const BYTE* const matchPtr = prefixPtr + (pos4 - prefixIdx);
                    assert(matchPtr < ip);
                    assert(matchPtr >= prefixPtr);
                    matchLength = LZ4_count(ip, matchPtr, matchlimit);
                    if (matchLength >= MINMATCH) {
                        /* short match found, let's just check ip+1 for longer */
                        U32 const h8 = LZ4MID_hash8Ptr(ip+1);
                        U32 const pos8 = hash8Table[h8];
                        U32 const m2Distance = ipIndex + 1 - pos8;
                        matchDistance = ipIndex - pos4;
                        if ( m2Distance <= LZ4_DISTANCE_MAX
                        && pos8 >= prefixIdx /* only search within prefix */
                        && likely(ip < mflimit)
                        ) {
                            const BYTE* const m2Ptr = prefixPtr + (pos8 - prefixIdx);
                            unsigned ml2 = LZ4_count(ip+1, m2Ptr, matchlimit);
                            if (ml2 > matchLength) {
                                LZ4MID_addPosition(hash8Table, h8, ipIndex+1);
                                ip++;
                                matchLength = ml2;
                                matchDistance = m2Distance;
                        }   }
                        goto _lz4mid_encode_sequence;
                    }
                } else {
                    if (pos4 >= dictIdx) {
                        /* extDict match candidate */
                        const BYTE* const matchPtr = dictStart + (pos4 - dictIdx);
                        const size_t safeLen = MIN(prefixIdx - pos4, (size_t)(matchlimit - ip));
                        matchLength = LZ4_count(ip, matchPtr, ip + safeLen);
                        if (matchLength >= MINMATCH) {
                            DEBUGLOG(7, "found match at ExtDict pos %u (len=%u)", pos4, matchLength);
                            matchDistance = ipIndex - pos4;
                            goto _lz4mid_encode_sequence;
                        }
                    }
                }
        }   }
        /* no match found in prefix */
        if ( (dict == usingDictCtxHc)
          && (ipIndex - gDictEndIndex < LZ4_DISTANCE_MAX - 8) ) {
            /* search a match into external dictionary */
            LZ4HC_match_t dMatch = searchIntoDict(ip, ipIndex,
                    matchlimit,
                    ctx->dictCtx, gDictEndIndex);
            if (dMatch.len >= MINMATCH) {
                DEBUGLOG(7, "found Dictionary match (offset=%i)", dMatch.off);
                assert(dMatch.back == 0);
                matchLength = (unsigned)dMatch.len;
                matchDistance = (unsigned)dMatch.off;
                goto _lz4mid_encode_sequence;
            }
        }
        /* no match found */
        ip += 1 + ((ip-anchor) >> 9);  /* skip faster over incompressible data */
        continue;

_lz4mid_encode_sequence:
        /* catch back */
        while (((ip > anchor) & ((U32)(ip-prefixPtr) > matchDistance)) && (unlikely(ip[-1] == ip[-(int)matchDistance-1]))) {
            ip--;  matchLength++;
        };

        /* fill table with beginning of match */
        ADDPOS8(ip+1, ipIndex+1);
        ADDPOS8(ip+2, ipIndex+2);
        ADDPOS4(ip+1, ipIndex+1);

        /* encode */
        {   BYTE* const saved_op = op;
            /* LZ4HC_encodeSequence always updates @op; on success, it updates @ip and @anchor */
            if (LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                    (int)matchLength, (int)matchDistance,
                    limit, oend) ) {
                op = saved_op;  /* restore @op value before failed LZ4HC_encodeSequence */
                goto _lz4mid_dest_overflow;
            }
        }

        /* fill table with end of match */
        {   U32 endMatchIdx = (U32)(ip-prefixPtr) + prefixIdx;
            U32 pos_m2 = endMatchIdx - 2;
            if (pos_m2 < ilimitIdx) {
                if (likely(ip - prefixPtr > 5)) {
                    ADDPOS8(ip-5, endMatchIdx - 5);
                }
                ADDPOS8(ip-3, endMatchIdx - 3);
                ADDPOS8(ip-2, endMatchIdx - 2);
                ADDPOS4(ip-2, endMatchIdx - 2);
                ADDPOS4(ip-1, endMatchIdx - 1);
            }
        }
    }

_lz4mid_last_literals:
    /* Encode Last Literals */
    {   size_t lastRunSize = (size_t)(iend - anchor);  /* literals */
        size_t llAdd = (lastRunSize + 255 - RUN_MASK) / 255;
        size_t const totalSize = 1 + llAdd + lastRunSize;
        if (limit == fillOutput) oend += LASTLITERALS;  /* restore correct value */
        if (limit && (op + totalSize > oend)) {
            if (limit == limitedOutput) return 0;  /* not enough space in @dst */
            /* adapt lastRunSize to fill 'dest' */
            lastRunSize  = (size_t)(oend - op) - 1 /*token*/;
            llAdd = (lastRunSize + 256 - RUN_MASK) / 256;
            lastRunSize -= llAdd;
        }
        DEBUGLOG(6, "Final literal run : %i literals", (int)lastRunSize);
        ip = anchor + lastRunSize;  /* can be != iend if limit==fillOutput */

        if (lastRunSize >= RUN_MASK) {
            size_t accumulator = lastRunSize - RUN_MASK;
            *op++ = (RUN_MASK << ML_BITS);
            for(; accumulator >= 255 ; accumulator -= 255)
                *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRunSize << ML_BITS);
        }
        assert(lastRunSize <= (size_t)(oend - op));
        LZ4_memcpy(op, anchor, lastRunSize);
        op += lastRunSize;
    }

    /* End */
    DEBUGLOG(5, "compressed %i bytes into %i bytes", *srcSizePtr, (int)((char*)op - dst));
    assert(ip >= (const BYTE*)src);
    assert(ip <= iend);
    *srcSizePtr = (int)(ip - (const BYTE*)src);
    assert((char*)op >= dst);
    assert(op <= oend);
    assert((char*)op - dst < INT_MAX);
    return (int)((char*)op - dst);

_lz4mid_dest_overflow:
    if (limit == fillOutput) {
        /* Assumption : @ip, @anchor, @optr and @matchLength must be set correctly */
        size_t const ll = (size_t)(ip - anchor);
        size_t const ll_addbytes = (ll + 240) / 255;
        size_t const ll_totalCost = 1 + ll_addbytes + ll;
        BYTE* const maxLitPos = oend - 3; /* 2 for offset, 1 for token */
        DEBUGLOG(6, "Last sequence is overflowing : %u literals, %u remaining space",
                (unsigned)ll, (unsigned)(oend-op));
        if (op + ll_totalCost <= maxLitPos) {
            /* ll validated; now adjust match length */
            size_t const bytesLeftForMl = (size_t)(maxLitPos - (op+ll_totalCost));
            size_t const maxMlSize = MINMATCH + (ML_MASK-1) + (bytesLeftForMl * 255);
            assert(maxMlSize < INT_MAX);
            if ((size_t)matchLength > maxMlSize) matchLength= (unsigned)maxMlSize;
            if ((oend + LASTLITERALS) - (op + ll_totalCost + 2) - 1 + matchLength >= MFLIMIT) {
            DEBUGLOG(6, "Let's encode a last sequence (ll=%u, ml=%u)", (unsigned)ll, matchLength);
                LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                        (int)matchLength, (int)matchDistance,
                        notLimited, oend);
        }   }
        DEBUGLOG(6, "Let's finish with a run of literals (%u bytes left)", (unsigned)(oend-op));
        goto _lz4mid_last_literals;
    }
    /* compression failed */
    return 0;
}


/**************************************
*  HC Compression - Search
**************************************/

/* Update chains up to ip (excluded) */
LZ4_FORCE_INLINE void LZ4HC_Insert (LZ4HC_CCtx_internal* hc4, const BYTE* ip)
{
    U16* const chainTable = hc4->chainTable;
    U32* const hashTable  = hc4->hashTable;
    const BYTE* const prefixPtr = hc4->prefixStart;
    U32 const prefixIdx = hc4->dictLimit;
    U32 const target = (U32)(ip - prefixPtr) + prefixIdx;
    U32 idx = hc4->nextToUpdate;
    assert(ip >= prefixPtr);
    assert(target >= prefixIdx);

    while (idx < target) {
        U32 const h = LZ4HC_hashPtr(prefixPtr+idx-prefixIdx);
        size_t delta = idx - hashTable[h];
        if (delta>LZ4_DISTANCE_MAX) delta = LZ4_DISTANCE_MAX;
        DELTANEXTU16(chainTable, idx) = (U16)delta;
        hashTable[h] = idx;
        idx++;
    }

    hc4->nextToUpdate = target;
}

#if defined(_MSC_VER)
#  define LZ4HC_rotl32(x,r) _rotl(x,r)
#else
#  define LZ4HC_rotl32(x,r) ((x << r) | (x >> (32 - r)))
#endif


static U32 LZ4HC_rotatePattern(size_t const rotate, U32 const pattern)
{
    size_t const bitsToRotate = (rotate & (sizeof(pattern) - 1)) << 3;
    if (bitsToRotate == 0) return pattern;
    return LZ4HC_rotl32(pattern, (int)bitsToRotate);
}

/* LZ4HC_countPattern() :
 * pattern32 must be a sample of repetitive pattern of length 1, 2 or 4 (but not 3!) */
static unsigned
LZ4HC_countPattern(const BYTE* ip, const BYTE* const iEnd, U32 const pattern32)
{
    const BYTE* const iStart = ip;
    reg_t const pattern = (sizeof(pattern)==8) ?
        (reg_t)pattern32 + (((reg_t)pattern32) << (sizeof(pattern)*4)) : pattern32;

    while (likely(ip < iEnd-(sizeof(pattern)-1))) {
        reg_t const diff = LZ4_read_ARCH(ip) ^ pattern;
        if (!diff) { ip+=sizeof(pattern); continue; }
        ip += LZ4_NbCommonBytes(diff);
        return (unsigned)(ip - iStart);
    }

    if (LZ4_isLittleEndian()) {
        reg_t patternByte = pattern;
        while ((ip<iEnd) && (*ip == (BYTE)patternByte)) {
            ip++; patternByte >>= 8;
        }
    } else {  /* big endian */
        U32 bitOffset = (sizeof(pattern)*8) - 8;
        while (ip < iEnd) {
            BYTE const byte = (BYTE)(pattern >> bitOffset);
            if (*ip != byte) break;
            ip ++; bitOffset -= 8;
    }   }

    return (unsigned)(ip - iStart);
}

/* LZ4HC_reverseCountPattern() :
 * pattern must be a sample of repetitive pattern of length 1, 2 or 4 (but not 3!)
 * read using natural platform endianness */
static unsigned
LZ4HC_reverseCountPattern(const BYTE* ip, const BYTE* const iLow, U32 pattern)
{
    const BYTE* const iStart = ip;

    while (likely(ip >= iLow+4)) {
        if (LZ4_read32(ip-4) != pattern) break;
        ip -= 4;
    }
    {   const BYTE* bytePtr = (const BYTE*)(&pattern) + 3; /* works for any endianness */
        while (likely(ip>iLow)) {
            if (ip[-1] != *bytePtr) break;
            ip--; bytePtr--;
    }   }
    return (unsigned)(iStart - ip);
}

/* LZ4HC_protectDictEnd() :
 * Checks if the match is in the last 3 bytes of the dictionary, so reading the
 * 4 byte MINMATCH would overflow.
 * @returns true if the match index is okay.
 */
static int LZ4HC_protectDictEnd(U32 const dictLimit, U32 const matchIndex)
{
    return ((U32)((dictLimit - 1) - matchIndex) >= 3);
}

typedef enum { rep_untested, rep_not, rep_confirmed } repeat_state_e;
typedef enum { favorCompressionRatio=0, favorDecompressionSpeed } HCfavor_e;


LZ4_FORCE_INLINE LZ4HC_match_t
LZ4HC_InsertAndGetWiderMatch (
        LZ4HC_CCtx_internal* const hc4,
        const BYTE* const ip,
        const BYTE* const iLowLimit, const BYTE* const iHighLimit,
        int longest,
        const int maxNbAttempts,
        const int patternAnalysis, const int chainSwap,
        const dictCtx_directive dict,
        const HCfavor_e favorDecSpeed)
{
    U16* const chainTable = hc4->chainTable;
    U32* const hashTable = hc4->hashTable;
    const LZ4HC_CCtx_internal* const dictCtx = hc4->dictCtx;
    const BYTE* const prefixPtr = hc4->prefixStart;
    const U32 prefixIdx = hc4->dictLimit;
    const U32 ipIndex = (U32)(ip - prefixPtr) + prefixIdx;
    const int withinStartDistance = (hc4->lowLimit + (LZ4_DISTANCE_MAX + 1) > ipIndex);
    const U32 lowestMatchIndex = (withinStartDistance) ? hc4->lowLimit : ipIndex - LZ4_DISTANCE_MAX;
    const BYTE* const dictStart = hc4->dictStart;
    const U32 dictIdx = hc4->lowLimit;
    const BYTE* const dictEnd = dictStart + prefixIdx - dictIdx;
    int const lookBackLength = (int)(ip-iLowLimit);
    int nbAttempts = maxNbAttempts;
    U32 matchChainPos = 0;
    U32 const pattern = LZ4_read32(ip);
    U32 matchIndex;
    repeat_state_e repeat = rep_untested;
    size_t srcPatternLength = 0;
    int offset = 0, sBack = 0;

    DEBUGLOG(7, "LZ4HC_InsertAndGetWiderMatch");
    /* First Match */
    LZ4HC_Insert(hc4, ip);  /* insert all prior positions up to ip (excluded) */
    matchIndex = hashTable[LZ4HC_hashPtr(ip)];
    DEBUGLOG(7, "First candidate match for pos %u found at index %u / %u (lowestMatchIndex)",
                ipIndex, matchIndex, lowestMatchIndex);

    while ((matchIndex>=lowestMatchIndex) && (nbAttempts>0)) {
        int matchLength=0;
        nbAttempts--;
        assert(matchIndex < ipIndex);
        if (favorDecSpeed && (ipIndex - matchIndex < 8)) {
            /* do nothing:
             * favorDecSpeed intentionally skips matches with offset < 8 */
        } else if (matchIndex >= prefixIdx) {   /* within current Prefix */
            const BYTE* const matchPtr = prefixPtr + (matchIndex - prefixIdx);
            assert(matchPtr < ip);
            assert(longest >= 1);
            if (LZ4_read16(iLowLimit + longest - 1) == LZ4_read16(matchPtr - lookBackLength + longest - 1)) {
                if (LZ4_read32(matchPtr) == pattern) {
                    int const back = lookBackLength ? LZ4HC_countBack(ip, matchPtr, iLowLimit, prefixPtr) : 0;
                    matchLength = MINMATCH + (int)LZ4_count(ip+MINMATCH, matchPtr+MINMATCH, iHighLimit);
                    matchLength -= back;
                    if (matchLength > longest) {
                        longest = matchLength;
                        offset = (int)(ipIndex - matchIndex);
                        sBack = back;
                        DEBUGLOG(7, "Found match of len=%i within prefix, offset=%i, back=%i", longest, offset, -back);
            }   }   }
        } else {   /* lowestMatchIndex <= matchIndex < dictLimit : within Ext Dict */
            const BYTE* const matchPtr = dictStart + (matchIndex - dictIdx);
            assert(matchIndex >= dictIdx);
            if ( likely(matchIndex <= prefixIdx - 4)
              && (LZ4_read32(matchPtr) == pattern) ) {
                int back = 0;
                const BYTE* vLimit = ip + (prefixIdx - matchIndex);
                if (vLimit > iHighLimit) vLimit = iHighLimit;
                matchLength = (int)LZ4_count(ip+MINMATCH, matchPtr+MINMATCH, vLimit) + MINMATCH;
                if ((ip+matchLength == vLimit) && (vLimit < iHighLimit))
                    matchLength += LZ4_count(ip+matchLength, prefixPtr, iHighLimit);
                back = lookBackLength ? LZ4HC_countBack(ip, matchPtr, iLowLimit, dictStart) : 0;
                matchLength -= back;
                if (matchLength > longest) {
                    longest = matchLength;
                    offset = (int)(ipIndex - matchIndex);
                    sBack = back;
                    DEBUGLOG(7, "Found match of len=%i within dict, offset=%i, back=%i", longest, offset, -back);
        }   }   }

        if (chainSwap && matchLength==longest) {   /* better match => select a better chain */
            assert(lookBackLength==0);   /* search forward only */
            if (matchIndex + (U32)longest <= ipIndex) {
                int const kTrigger = 4;
                U32 distanceToNextMatch = 1;
                int const end = longest - MINMATCH + 1;
                int step = 1;
                int accel = 1 << kTrigger;
                int pos;
                for (pos = 0; pos < end; pos += step) {
                    U32 const candidateDist = DELTANEXTU16(chainTable, matchIndex + (U32)pos);
                    step = (accel++ >> kTrigger);
                    if (candidateDist > distanceToNextMatch) {
                        distanceToNextMatch = candidateDist;
                        matchChainPos = (U32)pos;
                        accel = 1 << kTrigger;
                }   }
                if (distanceToNextMatch > 1) {
                    if (distanceToNextMatch > matchIndex) break;   /* avoid overflow */
                    matchIndex -= distanceToNextMatch;
                    continue;
        }   }   }

        {   U32 const distNextMatch = DELTANEXTU16(chainTable, matchIndex);
            if (patternAnalysis && distNextMatch==1 && matchChainPos==0) {
                U32 const matchCandidateIdx = matchIndex-1;
                /* may be a repeated pattern */
                if (repeat == rep_untested) {
                    if ( ((pattern & 0xFFFF) == (pattern >> 16))
                      &  ((pattern & 0xFF)   == (pattern >> 24)) ) {
                        DEBUGLOG(7, "Repeat pattern detected, char %02X", pattern >> 24);
                        repeat = rep_confirmed;
                        srcPatternLength = LZ4HC_countPattern(ip+sizeof(pattern), iHighLimit, pattern) + sizeof(pattern);
                    } else {
                        repeat = rep_not;
                }   }
                if ( (repeat == rep_confirmed) && (matchCandidateIdx >= lowestMatchIndex)
                  && LZ4HC_protectDictEnd(prefixIdx, matchCandidateIdx) ) {
                    const int extDict = matchCandidateIdx < prefixIdx;
                    const BYTE* const matchPtr = extDict ? dictStart + (matchCandidateIdx - dictIdx) : prefixPtr + (matchCandidateIdx - prefixIdx);
                    if (LZ4_read32(matchPtr) == pattern) {  /* good candidate */
                        const BYTE* const iLimit = extDict ? dictEnd : iHighLimit;
                        size_t forwardPatternLength = LZ4HC_countPattern(matchPtr+sizeof(pattern), iLimit, pattern) + sizeof(pattern);
                        if (extDict && matchPtr + forwardPatternLength == iLimit) {
                            U32 const rotatedPattern = LZ4HC_rotatePattern(forwardPatternLength, pattern);
                            forwardPatternLength += LZ4HC_countPattern(prefixPtr, iHighLimit, rotatedPattern);
                        }
                        {   const BYTE* const lowestMatchPtr = extDict ? dictStart : prefixPtr;
                            size_t backLength = LZ4HC_reverseCountPattern(matchPtr, lowestMatchPtr, pattern);
                            size_t currentSegmentLength;
                            if (!extDict
                              && matchPtr - backLength == prefixPtr
                              && dictIdx < prefixIdx) {
                                U32 const rotatedPattern = LZ4HC_rotatePattern((U32)(-(int)backLength), pattern);
                                backLength += LZ4HC_reverseCountPattern(dictEnd, dictStart, rotatedPattern);
                            }
                            /* Limit backLength not go further than lowestMatchIndex */
                            backLength = matchCandidateIdx - MAX(matchCandidateIdx - (U32)backLength, lowestMatchIndex);
                            assert(matchCandidateIdx - backLength >= lowestMatchIndex);
                            currentSegmentLength = backLength + forwardPatternLength;
                            /* Adjust to end of pattern if the source pattern fits, otherwise the beginning of the pattern */
                            if ( (currentSegmentLength >= srcPatternLength)   /* current pattern segment large enough to contain full srcPatternLength */
                              && (forwardPatternLength <= srcPatternLength) ) { /* haven't reached this position yet */
                                U32 const newMatchIndex = matchCandidateIdx + (U32)forwardPatternLength - (U32)srcPatternLength;  /* best position, full pattern, might be followed by more match */
                                if (LZ4HC_protectDictEnd(prefixIdx, newMatchIndex))
                                    matchIndex = newMatchIndex;
                                else {
                                    /* Can only happen if started in the prefix */
                                    assert(newMatchIndex >= prefixIdx - 3 && newMatchIndex < prefixIdx && !extDict);
                                    matchIndex = prefixIdx;
                                }
                            } else {
                                U32 const newMatchIndex = matchCandidateIdx - (U32)backLength;   /* farthest position in current segment, will find a match of length currentSegmentLength + maybe some back */
                                if (!LZ4HC_protectDictEnd(prefixIdx, newMatchIndex)) {
                                    assert(newMatchIndex >= prefixIdx - 3 && newMatchIndex < prefixIdx && !extDict);
                                    matchIndex = prefixIdx;
                                } else {
                                    matchIndex = newMatchIndex;
                                    if (lookBackLength==0) {  /* no back possible */
                                        size_t const maxML = MIN(currentSegmentLength, srcPatternLength);
                                        if ((size_t)longest < maxML) {
                                            assert(prefixPtr - prefixIdx + matchIndex != ip);
                                            if ((size_t)(ip - prefixPtr) + prefixIdx - matchIndex > LZ4_DISTANCE_MAX) break;
                                            assert(maxML < 2 GB);
                                            longest = (int)maxML;
                                            offset = (int)(ipIndex - matchIndex);
                                            assert(sBack == 0);
                                            DEBUGLOG(7, "Found repeat pattern match of len=%i, offset=%i", longest, offset);
                                        }
                                        {   U32 const distToNextPattern = DELTANEXTU16(chainTable, matchIndex);
                                            if (distToNextPattern > matchIndex) break;  /* avoid overflow */
                                            matchIndex -= distToNextPattern;
                        }   }   }   }   }
                        continue;
                }   }
        }   }   /* PA optimization */

        /* follow current chain */
        matchIndex -= DELTANEXTU16(chainTable, matchIndex + matchChainPos);

    }  /* while ((matchIndex>=lowestMatchIndex) && (nbAttempts)) */

    if ( dict == usingDictCtxHc
      && nbAttempts > 0
      && withinStartDistance) {
        size_t const dictEndOffset = (size_t)(dictCtx->end - dictCtx->prefixStart) + dictCtx->dictLimit;
        U32 dictMatchIndex = dictCtx->hashTable[LZ4HC_hashPtr(ip)];
        assert(dictEndOffset <= 1 GB);
        matchIndex = dictMatchIndex + lowestMatchIndex - (U32)dictEndOffset;
        if (dictMatchIndex>0) DEBUGLOG(7, "dictEndOffset = %zu, dictMatchIndex = %u => relative matchIndex = %i", dictEndOffset, dictMatchIndex, (int)dictMatchIndex - (int)dictEndOffset);
        while (ipIndex - matchIndex <= LZ4_DISTANCE_MAX && nbAttempts--) {
            const BYTE* const matchPtr = dictCtx->prefixStart - dictCtx->dictLimit + dictMatchIndex;

            if (LZ4_read32(matchPtr) == pattern) {
                int mlt;
                int back = 0;
                const BYTE* vLimit = ip + (dictEndOffset - dictMatchIndex);
                if (vLimit > iHighLimit) vLimit = iHighLimit;
                mlt = (int)LZ4_count(ip+MINMATCH, matchPtr+MINMATCH, vLimit) + MINMATCH;
                back = lookBackLength ? LZ4HC_countBack(ip, matchPtr, iLowLimit, dictCtx->prefixStart) : 0;
                mlt -= back;
                if (mlt > longest) {
                    longest = mlt;
                    offset = (int)(ipIndex - matchIndex);
                    sBack = back;
                    DEBUGLOG(7, "found match of length %i within extDictCtx", longest);
            }   }

            {   U32 const nextOffset = DELTANEXTU16(dictCtx->chainTable, dictMatchIndex);
                dictMatchIndex -= nextOffset;
                matchIndex -= nextOffset;
    }   }   }

    {   LZ4HC_match_t md;
        assert(longest >= 0);
        md.len = longest;
        md.off = offset;
        md.back = sBack;
        return md;
    }
}

LZ4_FORCE_INLINE LZ4HC_match_t
LZ4HC_InsertAndFindBestMatch(LZ4HC_CCtx_internal* const hc4,   /* Index table will be updated */
                       const BYTE* const ip, const BYTE* const iLimit,
                       const int maxNbAttempts,
                       const int patternAnalysis,
                       const dictCtx_directive dict)
{
    DEBUGLOG(7, "LZ4HC_InsertAndFindBestMatch");
    /* note : LZ4HC_InsertAndGetWiderMatch() is able to modify the starting position of a match (*startpos),
     * but this won't be the case here, as we define iLowLimit==ip,
     * so LZ4HC_InsertAndGetWiderMatch() won't be allowed to search past ip */
    return LZ4HC_InsertAndGetWiderMatch(hc4, ip, ip, iLimit, MINMATCH-1, maxNbAttempts, patternAnalysis, 0 /*chainSwap*/, dict, favorCompressionRatio);
}


LZ4_FORCE_INLINE int LZ4HC_compress_hashChain (
    LZ4HC_CCtx_internal* const ctx,
    const char* const source,
    char* const dest,
    int* srcSizePtr,
    int const maxOutputSize,
    int maxNbAttempts,
    const limitedOutput_directive limit,
    const dictCtx_directive dict
    )
{
    const int inputSize = *srcSizePtr;
    const int patternAnalysis = (maxNbAttempts > 128);   /* levels 9+ */

    const BYTE* ip = (const BYTE*) source;
    const BYTE* anchor = ip;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = (iend - LASTLITERALS);

    BYTE* optr = (BYTE*) dest;
    BYTE* op = (BYTE*) dest;
    BYTE* oend = op + maxOutputSize;

    const BYTE* start0;
    const BYTE* start2 = NULL;
    const BYTE* start3 = NULL;
    LZ4HC_match_t m0, m1, m2, m3;
    const LZ4HC_match_t nomatch = {0, 0, 0};

    /* init */
    DEBUGLOG(5, "LZ4HC_compress_hashChain (dict?=>%i)", dict);
    *srcSizePtr = 0;
    if (limit == fillOutput) oend -= LASTLITERALS;                  /* Hack for support LZ4 format restriction */
    if (inputSize < LZ4_minLength) goto _last_literals;             /* Input too small, no compression (all literals) */

    /* Main Loop */
    while (ip <= mflimit) {
        m1 = LZ4HC_InsertAndFindBestMatch(ctx, ip, matchlimit, maxNbAttempts, patternAnalysis, dict);
        if (m1.len<MINMATCH) { ip++; continue; }

        /* saved, in case we would skip too much */
        start0 = ip; m0 = m1;

_Search2:
        DEBUGLOG(7, "_Search2 (currently found match of size %i)", m1.len);
        if (ip+m1.len <= mflimit) {
            start2 = ip + m1.len - 2;
            m2 = LZ4HC_InsertAndGetWiderMatch(ctx,
                            start2, ip + 0, matchlimit, m1.len,
                            maxNbAttempts, patternAnalysis, 0, dict, favorCompressionRatio);
            start2 += m2.back;
        } else {
            m2 = nomatch;  /* do not search further */
        }

        if (m2.len <= m1.len) { /* No better match => encode ML1 immediately */
            optr = op;
            if (LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                    m1.len, m1.off,
                    limit, oend) )
                goto _dest_overflow;
            continue;
        }

        if (start0 < ip) {   /* first match was skipped at least once */
            if (start2 < ip + m0.len) {  /* squeezing ML1 between ML0(original ML1) and ML2 */
                ip = start0; m1 = m0;  /* restore initial Match1 */
        }   }

        /* Here, start0==ip */
        if ((start2 - ip) < 3) {  /* First Match too small : removed */
            ip = start2;
            m1 = m2;
            goto _Search2;
        }

_Search3:
        if ((start2 - ip) < OPTIMAL_ML) {
            int correction;
            int new_ml = m1.len;
            if (new_ml > OPTIMAL_ML) new_ml = OPTIMAL_ML;
            if (ip+new_ml > start2 + m2.len - MINMATCH)
                new_ml = (int)(start2 - ip) + m2.len - MINMATCH;
            correction = new_ml - (int)(start2 - ip);
            if (correction > 0) {
                start2 += correction;
                m2.len -= correction;
            }
        }

        if (start2 + m2.len <= mflimit) {
            start3 = start2 + m2.len - 3;
            m3 = LZ4HC_InsertAndGetWiderMatch(ctx,
                            start3, start2, matchlimit, m2.len,
                            maxNbAttempts, patternAnalysis, 0, dict, favorCompressionRatio);
            start3 += m3.back;
        } else {
            m3 = nomatch;  /* do not search further */
        }

        if (m3.len <= m2.len) {  /* No better match => encode ML1 and ML2 */
            /* ip & ref are known; Now for ml */
            if (start2 < ip+m1.len) m1.len = (int)(start2 - ip);
            /* Now, encode 2 sequences */
            optr = op;
            if (LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                    m1.len, m1.off,
                    limit, oend) )
                goto _dest_overflow;
            ip = start2;
            optr = op;
            if (LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                    m2.len, m2.off,
                    limit, oend) ) {
                m1 = m2;
                goto _dest_overflow;
            }
            continue;
        }

        if (start3 < ip+m1.len+3) {  /* Not enough space for match 2 : remove it */
            if (start3 >= (ip+m1.len)) {  /* can write Seq1 immediately ==> Seq2 is removed, so Seq3 becomes Seq1 */
                if (start2 < ip+m1.len) {
                    int correction = (int)(ip+m1.len - start2);
                    start2 += correction;
                    m2.len -= correction;
                    if (m2.len < MINMATCH) {
                        start2 = start3;
                        m2 = m3;
                    }
                }

                optr = op;
                if (LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                        m1.len, m1.off,
                        limit, oend) )
                    goto _dest_overflow;
                ip  = start3;
                m1 = m3;

                start0 = start2;
                m0 = m2;
                goto _Search2;
            }

            start2 = start3;
            m2 = m3;
            goto _Search3;
        }

        /*
        * OK, now we have 3 ascending matches;
        * let's write the first one ML1.
        * ip & ref are known; Now decide ml.
        */
        if (start2 < ip+m1.len) {
            if ((start2 - ip) < OPTIMAL_ML) {
                int correction;
                if (m1.len > OPTIMAL_ML) m1.len = OPTIMAL_ML;
                if (ip + m1.len > start2 + m2.len - MINMATCH)
                    m1.len = (int)(start2 - ip) + m2.len - MINMATCH;
                correction = m1.len - (int)(start2 - ip);
                if (correction > 0) {
                    start2 += correction;
                    m2.len -= correction;
                }
            } else {
                m1.len = (int)(start2 - ip);
            }
        }
        optr = op;
        if ( LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor),
                m1.len, m1.off,
                limit, oend) )
            goto _dest_overflow;

        /* ML2 becomes ML1 */
        ip = start2; m1 = m2;

        /* ML3 becomes ML2 */
        start2 = start3; m2 = m3;

        /* let's find a new ML3 */
        goto _Search3;
    }

_last_literals:
    /* Encode Last Literals */
    {   size_t lastRunSize = (size_t)(iend - anchor);  /* literals */
        size_t llAdd = (lastRunSize + 255 - RUN_MASK) / 255;
        size_t const totalSize = 1 + llAdd + lastRunSize;
        if (limit == fillOutput) oend += LASTLITERALS;  /* restore correct value */
        if (limit && (op + totalSize > oend)) {
            if (limit == limitedOutput) return 0;
            /* adapt lastRunSize to fill 'dest' */
            lastRunSize  = (size_t)(oend - op) - 1 /*token*/;
            llAdd = (lastRunSize + 256 - RUN_MASK) / 256;
            lastRunSize -= llAdd;
        }
        DEBUGLOG(6, "Final literal run : %i literals", (int)lastRunSize);
        ip = anchor + lastRunSize;  /* can be != iend if limit==fillOutput */

        if (lastRunSize >= RUN_MASK) {
            size_t accumulator = lastRunSize - RUN_MASK;
            *op++ = (RUN_MASK << ML_BITS);
            for(; accumulator >= 255 ; accumulator -= 255) *op++ = 255;
            *op++ = (BYTE) accumulator;
        } else {
            *op++ = (BYTE)(lastRunSize << ML_BITS);
        }
        LZ4_memcpy(op, anchor, lastRunSize);
        op += lastRunSize;
    }

    /* End */
    *srcSizePtr = (int) (((const char*)ip) - source);
    return (int) (((char*)op)-dest);

_dest_overflow:
    if (limit == fillOutput) {
        /* Assumption : @ip, @anchor, @optr and @m1 must be set correctly */
        size_t const ll = (size_t)(ip - anchor);
        size_t const ll_addbytes = (ll + 240) / 255;
        size_t const ll_totalCost = 1 + ll_addbytes + ll;
        BYTE* const maxLitPos = oend - 3; /* 2 for offset, 1 for token */
        DEBUGLOG(6, "Last sequence overflowing");
        op = optr;  /* restore correct out pointer */
        if (op + ll_totalCost <= maxLitPos) {
            /* ll validated; now adjust match length */
            size_t const bytesLeftForMl = (size_t)(maxLitPos - (op+ll_totalCost));
            size_t const maxMlSize = MINMATCH + (ML_MASK-1) + (bytesLeftForMl * 255);
            assert(maxMlSize < INT_MAX); assert(m1.len >= 0);
            if ((size_t)m1.len > maxMlSize) m1.len = (int)maxMlSize;
            if ((oend + LASTLITERALS) - (op + ll_totalCost + 2) - 1 + m1.len >= MFLIMIT) {
                LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor), m1.len, m1.off, notLimited, oend);
        }   }
        goto _last_literals;
    }
    /* compression failed */
    return 0;
}


static int LZ4HC_compress_optimal( LZ4HC_CCtx_internal* ctx,
    const char* const source, char* dst,
    int* srcSizePtr, int dstCapacity,
    int const nbSearches, size_t sufficient_len,
    const limitedOutput_directive limit, int const fullUpdate,
    const dictCtx_directive dict,
    const HCfavor_e favorDecSpeed);

LZ4_FORCE_INLINE int
LZ4HC_compress_generic_internal (
            LZ4HC_CCtx_internal* const ctx,
            const char* const src,
            char* const dst,
            int* const srcSizePtr,
            int const dstCapacity,
            int cLevel,
            const limitedOutput_directive limit,
            const dictCtx_directive dict
            )
{
    DEBUGLOG(5, "LZ4HC_compress_generic_internal(src=%p, srcSize=%d)",
                src, *srcSizePtr);

    if (limit == fillOutput && dstCapacity < 1) return 0;   /* Impossible to store anything */
    if ((U32)*srcSizePtr > (U32)LZ4_MAX_INPUT_SIZE) return 0;  /* Unsupported input size (too large or negative) */

    ctx->end += *srcSizePtr;
    {   cParams_t const cParam = LZ4HC_getCLevelParams(cLevel);
        HCfavor_e const favor = ctx->favorDecSpeed ? favorDecompressionSpeed : favorCompressionRatio;
        int result;

        if (cParam.strat == lz4mid) {
            result = LZ4MID_compress(ctx,
                                src, dst, srcSizePtr, dstCapacity,
                                limit, dict);
        } else if (cParam.strat == lz4hc) {
            result = LZ4HC_compress_hashChain(ctx,
                                src, dst, srcSizePtr, dstCapacity,
                                cParam.nbSearches, limit, dict);
        } else {
            assert(cParam.strat == lz4opt);
            result = LZ4HC_compress_optimal(ctx,
                                src, dst, srcSizePtr, dstCapacity,
                                cParam.nbSearches, cParam.targetLength, limit,
                                cLevel >= LZ4HC_CLEVEL_MAX,   /* ultra mode */
                                dict, favor);
        }
        if (result <= 0) ctx->dirty = 1;
        return result;
    }
}

static void LZ4HC_setExternalDict(LZ4HC_CCtx_internal* ctxPtr, const BYTE* newBlock);

static int
LZ4HC_compress_generic_noDictCtx (
        LZ4HC_CCtx_internal* const ctx,
        const char* const src,
        char* const dst,
        int* const srcSizePtr,
        int const dstCapacity,
        int cLevel,
        limitedOutput_directive limit
        )
{
    assert(ctx->dictCtx == NULL);
    return LZ4HC_compress_generic_internal(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit, noDictCtx);
}

static int isStateCompatible(const LZ4HC_CCtx_internal* ctx1, const LZ4HC_CCtx_internal* ctx2)
{
    int const isMid1 = LZ4HC_getCLevelParams(ctx1->compressionLevel).strat == lz4mid;
    int const isMid2 = LZ4HC_getCLevelParams(ctx2->compressionLevel).strat == lz4mid;
    return !(isMid1 ^ isMid2);
}

static int
LZ4HC_compress_generic_dictCtx (
        LZ4HC_CCtx_internal* const ctx,
        const char* const src,
        char* const dst,
        int* const srcSizePtr,
        int const dstCapacity,
        int cLevel,
        limitedOutput_directive limit
        )
{
    const size_t position = (size_t)(ctx->end - ctx->prefixStart) + (ctx->dictLimit - ctx->lowLimit);
    assert(ctx->dictCtx != NULL);
    if (position >= 64 KB) {
        ctx->dictCtx = NULL;
        return LZ4HC_compress_generic_noDictCtx(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit);
    } else if (position == 0 && *srcSizePtr > 4 KB && isStateCompatible(ctx, ctx->dictCtx)) {
        LZ4_memcpy(ctx, ctx->dictCtx, sizeof(LZ4HC_CCtx_internal));
        LZ4HC_setExternalDict(ctx, (const BYTE *)src);
        ctx->compressionLevel = (short)cLevel;
        return LZ4HC_compress_generic_noDictCtx(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit);
    } else {
        return LZ4HC_compress_generic_internal(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit, usingDictCtxHc);
    }
}

static int
LZ4HC_compress_generic (
        LZ4HC_CCtx_internal* const ctx,
        const char* const src,
        char* const dst,
        int* const srcSizePtr,
        int const dstCapacity,
        int cLevel,
        limitedOutput_directive limit
        )
{
    if (ctx->dictCtx == NULL) {
        return LZ4HC_compress_generic_noDictCtx(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit);
    } else {
        return LZ4HC_compress_generic_dictCtx(ctx, src, dst, srcSizePtr, dstCapacity, cLevel, limit);
    }
}


int LZ4_sizeofStateHC(void) { return (int)sizeof(LZ4_streamHC_t); }

static size_t LZ4_streamHC_t_alignment(void)
{
#if LZ4_ALIGN_TEST
    typedef struct { char c; LZ4_streamHC_t t; } t_a;
    return sizeof(t_a) - sizeof(LZ4_streamHC_t);
#else
    return 1;  /* effectively disabled */
#endif
}

/* state is presumed correctly initialized,
 * in which case its size and alignment have already been validate */
int LZ4_compress_HC_extStateHC_fastReset (void* state, const char* src, char* dst, int srcSize, int dstCapacity, int compressionLevel)
{
    LZ4HC_CCtx_internal* const ctx = &((LZ4_streamHC_t*)state)->internal_donotuse;
    if (!LZ4_isAligned(state, LZ4_streamHC_t_alignment())) return 0;
    LZ4_resetStreamHC_fast((LZ4_streamHC_t*)state, compressionLevel);
    LZ4HC_init_internal (ctx, (const BYTE*)src);
    if (dstCapacity < LZ4_compressBound(srcSize))
        return LZ4HC_compress_generic (ctx, src, dst, &srcSize, dstCapacity, compressionLevel, limitedOutput);
    else
        return LZ4HC_compress_generic (ctx, src, dst, &srcSize, dstCapacity, compressionLevel, notLimited);
}

int LZ4_compress_HC_extStateHC (void* state, const char* src, char* dst, int srcSize, int dstCapacity, int compressionLevel)
{
    LZ4_streamHC_t* const ctx = LZ4_initStreamHC(state, sizeof(*ctx));
    if (ctx==NULL) return 0;   /* init failure */
    return LZ4_compress_HC_extStateHC_fastReset(state, src, dst, srcSize, dstCapacity, compressionLevel);
}

int LZ4_compress_HC(const char* src, char* dst, int srcSize, int dstCapacity, int compressionLevel)
{
    int cSize;
#if defined(LZ4HC_HEAPMODE) && LZ4HC_HEAPMODE==1
    LZ4_streamHC_t* const statePtr = (LZ4_streamHC_t*)ALLOC(sizeof(LZ4_streamHC_t));
    if (statePtr==NULL) return 0;
#else
    LZ4_streamHC_t state;
    LZ4_streamHC_t* const statePtr = &state;
#endif
    DEBUGLOG(5, "LZ4_compress_HC")
    cSize = LZ4_compress_HC_extStateHC(statePtr, src, dst, srcSize, dstCapacity, compressionLevel);
#if defined(LZ4HC_HEAPMODE) && LZ4HC_HEAPMODE==1
    FREEMEM(statePtr);
#endif
    return cSize;
}

/* state is presumed sized correctly (>= sizeof(LZ4_streamHC_t)) */
int LZ4_compress_HC_destSize(void* state, const char* source, char* dest, int* sourceSizePtr, int targetDestSize, int cLevel)
{
    LZ4_streamHC_t* const ctx = LZ4_initStreamHC(state, sizeof(*ctx));
    if (ctx==NULL) return 0;   /* init failure */
    LZ4HC_init_internal(&ctx->internal_donotuse, (const BYTE*) source);
    LZ4_setCompressionLevel(ctx, cLevel);
    return LZ4HC_compress_generic(&ctx->internal_donotuse, source, dest, sourceSizePtr, targetDestSize, cLevel, fillOutput);
}



/**************************************
*  Streaming Functions
**************************************/
/* allocation */
#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
LZ4_streamHC_t* LZ4_createStreamHC(void)
{
    LZ4_streamHC_t* const state =
        (LZ4_streamHC_t*)ALLOC_AND_ZERO(sizeof(LZ4_streamHC_t));
    if (state == NULL) return NULL;
    LZ4_setCompressionLevel(state, LZ4HC_CLEVEL_DEFAULT);
    return state;
}

int LZ4_freeStreamHC (LZ4_streamHC_t* LZ4_streamHCPtr)
{
    DEBUGLOG(4, "LZ4_freeStreamHC(%p)", LZ4_streamHCPtr);
    if (!LZ4_streamHCPtr) return 0;  /* support free on NULL */
    FREEMEM(LZ4_streamHCPtr);
    return 0;
}
#endif


LZ4_streamHC_t* LZ4_initStreamHC (void* buffer, size_t size)
{
    LZ4_streamHC_t* const LZ4_streamHCPtr = (LZ4_streamHC_t*)buffer;
    DEBUGLOG(4, "LZ4_initStreamHC(%p, %u)", buffer, (unsigned)size);
    /* check conditions */
    if (buffer == NULL) return NULL;
    if (size < sizeof(LZ4_streamHC_t)) return NULL;
    if (!LZ4_isAligned(buffer, LZ4_streamHC_t_alignment())) return NULL;
    /* init */
    { LZ4HC_CCtx_internal* const hcstate = &(LZ4_streamHCPtr->internal_donotuse);
      MEM_INIT(hcstate, 0, sizeof(*hcstate)); }
    LZ4_setCompressionLevel(LZ4_streamHCPtr, LZ4HC_CLEVEL_DEFAULT);
    return LZ4_streamHCPtr;
}

/* just a stub */
void LZ4_resetStreamHC (LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel)
{
    LZ4_initStreamHC(LZ4_streamHCPtr, sizeof(*LZ4_streamHCPtr));
    LZ4_setCompressionLevel(LZ4_streamHCPtr, compressionLevel);
}

void LZ4_resetStreamHC_fast (LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel)
{
    LZ4HC_CCtx_internal* const s = &LZ4_streamHCPtr->internal_donotuse;
    DEBUGLOG(5, "LZ4_resetStreamHC_fast(%p, %d)", LZ4_streamHCPtr, compressionLevel);
    if (s->dirty) {
        LZ4_initStreamHC(LZ4_streamHCPtr, sizeof(*LZ4_streamHCPtr));
    } else {
        assert(s->end >= s->prefixStart);
        s->dictLimit += (U32)(s->end - s->prefixStart);
        s->prefixStart = NULL;
        s->end = NULL;
        s->dictCtx = NULL;
    }
    LZ4_setCompressionLevel(LZ4_streamHCPtr, compressionLevel);
}

void LZ4_setCompressionLevel(LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel)
{
    DEBUGLOG(5, "LZ4_setCompressionLevel(%p, %d)", LZ4_streamHCPtr, compressionLevel);
    if (compressionLevel < 1) compressionLevel = LZ4HC_CLEVEL_DEFAULT;
    if (compressionLevel > LZ4HC_CLEVEL_MAX) compressionLevel = LZ4HC_CLEVEL_MAX;
    LZ4_streamHCPtr->internal_donotuse.compressionLevel = (short)compressionLevel;
}

void LZ4_favorDecompressionSpeed(LZ4_streamHC_t* LZ4_streamHCPtr, int favor)
{
    LZ4_streamHCPtr->internal_donotuse.favorDecSpeed = (favor!=0);
}

/* LZ4_loadDictHC() :
 * LZ4_streamHCPtr is presumed properly initialized */
int LZ4_loadDictHC (LZ4_streamHC_t* LZ4_streamHCPtr,
              const char* dictionary, int dictSize)
{
    LZ4HC_CCtx_internal* const ctxPtr = &LZ4_streamHCPtr->internal_donotuse;
    cParams_t cp;
    DEBUGLOG(4, "LZ4_loadDictHC(ctx:%p, dict:%p, dictSize:%d, clevel=%d)", LZ4_streamHCPtr, dictionary, dictSize, ctxPtr->compressionLevel);
    assert(dictSize >= 0);
    assert(LZ4_streamHCPtr != NULL);
    if (dictSize > 64 KB) {
        dictionary += (size_t)dictSize - 64 KB;
        dictSize = 64 KB;
    }
    /* need a full initialization, there are bad side-effects when using resetFast() */
    {   int const cLevel = ctxPtr->compressionLevel;
        LZ4_initStreamHC(LZ4_streamHCPtr, sizeof(*LZ4_streamHCPtr));
        LZ4_setCompressionLevel(LZ4_streamHCPtr, cLevel);
        cp = LZ4HC_getCLevelParams(cLevel);
    }
    LZ4HC_init_internal (ctxPtr, (const BYTE*)dictionary);
    ctxPtr->end = (const BYTE*)dictionary + dictSize;
    if (cp.strat == lz4mid) {
        LZ4MID_fillHTable (ctxPtr, dictionary, (size_t)dictSize);
    } else {
        if (dictSize >= LZ4HC_HASHSIZE) LZ4HC_Insert (ctxPtr, ctxPtr->end-3);
    }
    return dictSize;
}

void LZ4_attach_HC_dictionary(LZ4_streamHC_t *working_stream, const LZ4_streamHC_t *dictionary_stream) {
    working_stream->internal_donotuse.dictCtx = dictionary_stream != NULL ? &(dictionary_stream->internal_donotuse) : NULL;
}

/* compression */

static void LZ4HC_setExternalDict(LZ4HC_CCtx_internal* ctxPtr, const BYTE* newBlock)
{
    DEBUGLOG(4, "LZ4HC_setExternalDict(%p, %p)", ctxPtr, newBlock);
    if ( (ctxPtr->end >= ctxPtr->prefixStart + 4)
      && (LZ4HC_getCLevelParams(ctxPtr->compressionLevel).strat != lz4mid) ) {
        LZ4HC_Insert (ctxPtr, ctxPtr->end-3);  /* Referencing remaining dictionary content */
    }

    /* Only one memory segment for extDict, so any previous extDict is lost at this stage */
    ctxPtr->lowLimit  = ctxPtr->dictLimit;
    ctxPtr->dictStart  = ctxPtr->prefixStart;
    ctxPtr->dictLimit += (U32)(ctxPtr->end - ctxPtr->prefixStart);
    ctxPtr->prefixStart = newBlock;
    ctxPtr->end  = newBlock;
    ctxPtr->nextToUpdate = ctxPtr->dictLimit;   /* match referencing will resume from there */

    /* cannot reference an extDict and a dictCtx at the same time */
    ctxPtr->dictCtx = NULL;
}

static int
LZ4_compressHC_continue_generic (LZ4_streamHC_t* LZ4_streamHCPtr,
                                 const char* src, char* dst,
                                 int* srcSizePtr, int dstCapacity,
                                 limitedOutput_directive limit)
{
    LZ4HC_CCtx_internal* const ctxPtr = &LZ4_streamHCPtr->internal_donotuse;
    DEBUGLOG(5, "LZ4_compressHC_continue_generic(ctx=%p, src=%p, srcSize=%d, limit=%d)",
                LZ4_streamHCPtr, src, *srcSizePtr, limit);
    assert(ctxPtr != NULL);
    /* auto-init if forgotten */
    if (ctxPtr->prefixStart == NULL)
        LZ4HC_init_internal (ctxPtr, (const BYTE*) src);

    /* Check overflow */
    if ((size_t)(ctxPtr->end - ctxPtr->prefixStart) + ctxPtr->dictLimit > 2 GB) {
        size_t dictSize = (size_t)(ctxPtr->end - ctxPtr->prefixStart);
        if (dictSize > 64 KB) dictSize = 64 KB;
        LZ4_loadDictHC(LZ4_streamHCPtr, (const char*)(ctxPtr->end) - dictSize, (int)dictSize);
    }

    /* Check if blocks follow each other */
    if ((const BYTE*)src != ctxPtr->end)
        LZ4HC_setExternalDict(ctxPtr, (const BYTE*)src);

    /* Check overlapping input/dictionary space */
    {   const BYTE* sourceEnd = (const BYTE*) src + *srcSizePtr;
        const BYTE* const dictBegin = ctxPtr->dictStart;
        const BYTE* const dictEnd   = ctxPtr->dictStart + (ctxPtr->dictLimit - ctxPtr->lowLimit);
        if ((sourceEnd > dictBegin) && ((const BYTE*)src < dictEnd)) {
            if (sourceEnd > dictEnd) sourceEnd = dictEnd;
            ctxPtr->lowLimit += (U32)(sourceEnd - ctxPtr->dictStart);
            ctxPtr->dictStart += (U32)(sourceEnd - ctxPtr->dictStart);
            /* invalidate dictionary is it's too small */
            if (ctxPtr->dictLimit - ctxPtr->lowLimit < LZ4HC_HASHSIZE) {
                ctxPtr->lowLimit = ctxPtr->dictLimit;
                ctxPtr->dictStart = ctxPtr->prefixStart;
    }   }   }

    return LZ4HC_compress_generic (ctxPtr, src, dst, srcSizePtr, dstCapacity, ctxPtr->compressionLevel, limit);
}

int LZ4_compress_HC_continue (LZ4_streamHC_t* LZ4_streamHCPtr, const char* src, char* dst, int srcSize, int dstCapacity)
{
    DEBUGLOG(5, "LZ4_compress_HC_continue");
    if (dstCapacity < LZ4_compressBound(srcSize))
        return LZ4_compressHC_continue_generic (LZ4_streamHCPtr, src, dst, &srcSize, dstCapacity, limitedOutput);
    else
        return LZ4_compressHC_continue_generic (LZ4_streamHCPtr, src, dst, &srcSize, dstCapacity, notLimited);
}

int LZ4_compress_HC_continue_destSize (LZ4_streamHC_t* LZ4_streamHCPtr, const char* src, char* dst, int* srcSizePtr, int targetDestSize)
{
    return LZ4_compressHC_continue_generic(LZ4_streamHCPtr, src, dst, srcSizePtr, targetDestSize, fillOutput);
}


/* LZ4_saveDictHC :
 * save history content
 * into a user-provided buffer
 * which is then used to continue compression
 */
int LZ4_saveDictHC (LZ4_streamHC_t* LZ4_streamHCPtr, char* safeBuffer, int dictSize)
{
    LZ4HC_CCtx_internal* const streamPtr = &LZ4_streamHCPtr->internal_donotuse;
    int const prefixSize = (int)(streamPtr->end - streamPtr->prefixStart);
    DEBUGLOG(5, "LZ4_saveDictHC(%p, %p, %d)", LZ4_streamHCPtr, safeBuffer, dictSize);
    assert(prefixSize >= 0);
    if (dictSize > 64 KB) dictSize = 64 KB;
    if (dictSize < 4) dictSize = 0;
    if (dictSize > prefixSize) dictSize = prefixSize;
    if (safeBuffer == NULL) assert(dictSize == 0);
    if (dictSize > 0)
        LZ4_memmove(safeBuffer, streamPtr->end - dictSize, (size_t)dictSize);
    {   U32 const endIndex = (U32)(streamPtr->end - streamPtr->prefixStart) + streamPtr->dictLimit;
        streamPtr->end = (safeBuffer == NULL) ? NULL : (const BYTE*)safeBuffer + dictSize;
        streamPtr->prefixStart = (const BYTE*)safeBuffer;
        streamPtr->dictLimit = endIndex - (U32)dictSize;
        streamPtr->lowLimit = endIndex - (U32)dictSize;
        streamPtr->dictStart = streamPtr->prefixStart;
        if (streamPtr->nextToUpdate < streamPtr->dictLimit)
            streamPtr->nextToUpdate = streamPtr->dictLimit;
    }
    return dictSize;
}


/* ================================================
 *  LZ4 Optimal parser (levels [LZ4HC_CLEVEL_OPT_MIN - LZ4HC_CLEVEL_MAX])
 * ===============================================*/
typedef struct {
    int price;
    int off;
    int mlen;
    int litlen;
} LZ4HC_optimal_t;

/* price in bytes */
LZ4_FORCE_INLINE int LZ4HC_literalsPrice(int const litlen)
{
    int price = litlen;
    assert(litlen >= 0);
    if (litlen >= (int)RUN_MASK)
        price += 1 + ((litlen-(int)RUN_MASK) / 255);
    return price;
}

/* requires mlen >= MINMATCH */
LZ4_FORCE_INLINE int LZ4HC_sequencePrice(int litlen, int mlen)
{
    int price = 1 + 2 ; /* token + 16-bit offset */
    assert(litlen >= 0);
    assert(mlen >= MINMATCH);

    price += LZ4HC_literalsPrice(litlen);

    if (mlen >= (int)(ML_MASK+MINMATCH))
        price += 1 + ((mlen-(int)(ML_MASK+MINMATCH)) / 255);

    return price;
}

LZ4_FORCE_INLINE LZ4HC_match_t
LZ4HC_FindLongerMatch(LZ4HC_CCtx_internal* const ctx,
                      const BYTE* ip, const BYTE* const iHighLimit,
                      int minLen, int nbSearches,
                      const dictCtx_directive dict,
                      const HCfavor_e favorDecSpeed)
{
    LZ4HC_match_t const match0 = { 0 , 0, 0 };
    /* note : LZ4HC_InsertAndGetWiderMatch() is able to modify the starting position of a match (*startpos),
     * but this won't be the case here, as we define iLowLimit==ip,
    ** so LZ4HC_InsertAndGetWiderMatch() won't be allowed to search past ip */
    LZ4HC_match_t md = LZ4HC_InsertAndGetWiderMatch(ctx, ip, ip, iHighLimit, minLen, nbSearches, 1 /*patternAnalysis*/, 1 /*chainSwap*/, dict, favorDecSpeed);
    assert(md.back == 0);
    if (md.len <= minLen) return match0;
    if (favorDecSpeed) {
        if ((md.len>18) & (md.len<=36)) md.len=18;   /* favor dec.speed (shortcut) */
    }
    return md;
}


static int LZ4HC_compress_optimal ( LZ4HC_CCtx_internal* ctx,
                                    const char* const source,
                                    char* dst,
                                    int* srcSizePtr,
                                    int dstCapacity,
                                    int const nbSearches,
                                    size_t sufficient_len,
                                    const limitedOutput_directive limit,
                                    int const fullUpdate,
                                    const dictCtx_directive dict,
                                    const HCfavor_e favorDecSpeed)
{
    int retval = 0;
#define TRAILING_LITERALS 3
#if defined(LZ4HC_HEAPMODE) && LZ4HC_HEAPMODE==1
    LZ4HC_optimal_t* const opt = (LZ4HC_optimal_t*)ALLOC(sizeof(LZ4HC_optimal_t) * (LZ4_OPT_NUM + TRAILING_LITERALS));
#else
    LZ4HC_optimal_t opt[LZ4_OPT_NUM + TRAILING_LITERALS];   /* ~64 KB, which is a bit large for stack... */
#endif

    const BYTE* ip = (const BYTE*) source;
    const BYTE* anchor = ip;
    const BYTE* const iend = ip + *srcSizePtr;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = iend - LASTLITERALS;
    BYTE* op = (BYTE*) dst;
    BYTE* opSaved = (BYTE*) dst;
    BYTE* oend = op + dstCapacity;
    int ovml = MINMATCH;  /* overflow - last sequence */
    int ovoff = 0;

    /* init */
#if defined(LZ4HC_HEAPMODE) && LZ4HC_HEAPMODE==1
    if (opt == NULL) goto _return_label;
#endif
    DEBUGLOG(5, "LZ4HC_compress_optimal(dst=%p, dstCapa=%u)", dst, (unsigned)dstCapacity);
    *srcSizePtr = 0;
    if (limit == fillOutput) oend -= LASTLITERALS;   /* Hack for support LZ4 format restriction */
    if (sufficient_len >= LZ4_OPT_NUM) sufficient_len = LZ4_OPT_NUM-1;

    /* Main Loop */
    while (ip <= mflimit) {
         int const llen = (int)(ip - anchor);
         int best_mlen, best_off;
         int cur, last_match_pos = 0;

         LZ4HC_match_t const firstMatch = LZ4HC_FindLongerMatch(ctx, ip, matchlimit, MINMATCH-1, nbSearches, dict, favorDecSpeed);
         if (firstMatch.len==0) { ip++; continue; }

         if ((size_t)firstMatch.len > sufficient_len) {
             /* good enough solution : immediate encoding */
             int const firstML = firstMatch.len;
             opSaved = op;
             if ( LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor), firstML, firstMatch.off, limit, oend) ) {  /* updates ip, op and anchor */
                 ovml = firstML;
                 ovoff = firstMatch.off;
                 goto _dest_overflow;
             }
             continue;
         }

         /* set prices for first positions (literals) */
         {   int rPos;
             for (rPos = 0 ; rPos < MINMATCH ; rPos++) {
                 int const cost = LZ4HC_literalsPrice(llen + rPos);
                 opt[rPos].mlen = 1;
                 opt[rPos].off = 0;
                 opt[rPos].litlen = llen + rPos;
                 opt[rPos].price = cost;
                 DEBUGLOG(7, "rPos:%3i => price:%3i (litlen=%i) -- initial setup",
                             rPos, cost, opt[rPos].litlen);
         }   }
         /* set prices using initial match */
         {   int const matchML = firstMatch.len;   /* necessarily < sufficient_len < LZ4_OPT_NUM */
             int const offset = firstMatch.off;
             int mlen;
             assert(matchML < LZ4_OPT_NUM);
             for (mlen = MINMATCH ; mlen <= matchML ; mlen++) {
                 int const cost = LZ4HC_sequencePrice(llen, mlen);
                 opt[mlen].mlen = mlen;
                 opt[mlen].off = offset;
                 opt[mlen].litlen = llen;
                 opt[mlen].price = cost;
                 DEBUGLOG(7, "rPos:%3i => price:%3i (matchlen=%i) -- initial setup",
                             mlen, cost, mlen);
         }   }
         last_match_pos = firstMatch.len;
         {   int addLit;
             for (addLit = 1; addLit <= TRAILING_LITERALS; addLit ++) {
                 opt[last_match_pos+addLit].mlen = 1; /* literal */
                 opt[last_match_pos+addLit].off = 0;
                 opt[last_match_pos+addLit].litlen = addLit;
                 opt[last_match_pos+addLit].price = opt[last_match_pos].price + LZ4HC_literalsPrice(addLit);
                 DEBUGLOG(7, "rPos:%3i => price:%3i (litlen=%i) -- initial setup",
                             last_match_pos+addLit, opt[last_match_pos+addLit].price, addLit);
         }   }

         /* check further positions */
         for (cur = 1; cur < last_match_pos; cur++) {
             const BYTE* const curPtr = ip + cur;
             LZ4HC_match_t newMatch;

             if (curPtr > mflimit) break;
             DEBUGLOG(7, "rPos:%u[%u] vs [%u]%u",
                     cur, opt[cur].price, opt[cur+1].price, cur+1);
             if (fullUpdate) {
                 /* not useful to search here if next position has same (or lower) cost */
                 if ( (opt[cur+1].price <= opt[cur].price)
                   /* in some cases, next position has same cost, but cost rises sharply after, so a small match would still be beneficial */
                   && (opt[cur+MINMATCH].price < opt[cur].price + 3/*min seq price*/) )
                     continue;
             } else {
                 /* not useful to search here if next position has same (or lower) cost */
                 if (opt[cur+1].price <= opt[cur].price) continue;
             }

             DEBUGLOG(7, "search at rPos:%u", cur);
             if (fullUpdate)
                 newMatch = LZ4HC_FindLongerMatch(ctx, curPtr, matchlimit, MINMATCH-1, nbSearches, dict, favorDecSpeed);
             else
                 /* only test matches of minimum length; slightly faster, but misses a few bytes */
                 newMatch = LZ4HC_FindLongerMatch(ctx, curPtr, matchlimit, last_match_pos - cur, nbSearches, dict, favorDecSpeed);
             if (!newMatch.len) continue;

             if ( ((size_t)newMatch.len > sufficient_len)
               || (newMatch.len + cur >= LZ4_OPT_NUM) ) {
                 /* immediate encoding */
                 best_mlen = newMatch.len;
                 best_off = newMatch.off;
                 last_match_pos = cur + 1;
                 goto encode;
             }

             /* before match : set price with literals at beginning */
             {   int const baseLitlen = opt[cur].litlen;
                 int litlen;
                 for (litlen = 1; litlen < MINMATCH; litlen++) {
                     int const price = opt[cur].price - LZ4HC_literalsPrice(baseLitlen) + LZ4HC_literalsPrice(baseLitlen+litlen);
                     int const pos = cur + litlen;
                     if (price < opt[pos].price) {
                         opt[pos].mlen = 1; /* literal */
                         opt[pos].off = 0;
                         opt[pos].litlen = baseLitlen+litlen;
                         opt[pos].price = price;
                         DEBUGLOG(7, "rPos:%3i => price:%3i (litlen=%i)",
                                     pos, price, opt[pos].litlen);
             }   }   }

             /* set prices using match at position = cur */
             {   int const matchML = newMatch.len;
                 int ml = MINMATCH;

                 assert(cur + newMatch.len < LZ4_OPT_NUM);
                 for ( ; ml <= matchML ; ml++) {
                     int const pos = cur + ml;
                     int const offset = newMatch.off;
                     int price;
                     int ll;
                     DEBUGLOG(7, "testing price rPos %i (last_match_pos=%i)",
                                 pos, last_match_pos);
                     if (opt[cur].mlen == 1) {
                         ll = opt[cur].litlen;
                         price = ((cur > ll) ? opt[cur - ll].price : 0)
                               + LZ4HC_sequencePrice(ll, ml);
                     } else {
                         ll = 0;
                         price = opt[cur].price + LZ4HC_sequencePrice(0, ml);
                     }

                    assert((U32)favorDecSpeed <= 1);
                     if (pos > last_match_pos+TRAILING_LITERALS
                      || price <= opt[pos].price - (int)favorDecSpeed) {
                         DEBUGLOG(7, "rPos:%3i => price:%3i (matchlen=%i)",
                                     pos, price, ml);
                         assert(pos < LZ4_OPT_NUM);
                         if ( (ml == matchML)  /* last pos of last match */
                           && (last_match_pos < pos) )
                             last_match_pos = pos;
                         opt[pos].mlen = ml;
                         opt[pos].off = offset;
                         opt[pos].litlen = ll;
                         opt[pos].price = price;
             }   }   }
             /* complete following positions with literals */
             {   int addLit;
                 for (addLit = 1; addLit <= TRAILING_LITERALS; addLit ++) {
                     opt[last_match_pos+addLit].mlen = 1; /* literal */
                     opt[last_match_pos+addLit].off = 0;
                     opt[last_match_pos+addLit].litlen = addLit;
                     opt[last_match_pos+addLit].price = opt[last_match_pos].price + LZ4HC_literalsPrice(addLit);
                     DEBUGLOG(7, "rPos:%3i => price:%3i (litlen=%i)", last_match_pos+addLit, opt[last_match_pos+addLit].price, addLit);
             }   }
         }  /* for (cur = 1; cur <= last_match_pos; cur++) */

         assert(last_match_pos < LZ4_OPT_NUM + TRAILING_LITERALS);
         best_mlen = opt[last_match_pos].mlen;
         best_off = opt[last_match_pos].off;
         cur = last_match_pos - best_mlen;

encode: /* cur, last_match_pos, best_mlen, best_off must be set */
         assert(cur < LZ4_OPT_NUM);
         assert(last_match_pos >= 1);  /* == 1 when only one candidate */
         DEBUGLOG(6, "reverse traversal, looking for shortest path (last_match_pos=%i)", last_match_pos);
         {   int candidate_pos = cur;
             int selected_matchLength = best_mlen;
             int selected_offset = best_off;
             while (1) {  /* from end to beginning */
                 int const next_matchLength = opt[candidate_pos].mlen;  /* can be 1, means literal */
                 int const next_offset = opt[candidate_pos].off;
                 DEBUGLOG(7, "pos %i: sequence length %i", candidate_pos, selected_matchLength);
                 opt[candidate_pos].mlen = selected_matchLength;
                 opt[candidate_pos].off = selected_offset;
                 selected_matchLength = next_matchLength;
                 selected_offset = next_offset;
                 if (next_matchLength > candidate_pos) break; /* last match elected, first match to encode */
                 assert(next_matchLength > 0);  /* can be 1, means literal */
                 candidate_pos -= next_matchLength;
         }   }

         /* encode all recorded sequences in order */
         {   int rPos = 0;  /* relative position (to ip) */
             while (rPos < last_match_pos) {
                 int const ml = opt[rPos].mlen;
                 int const offset = opt[rPos].off;
                 if (ml == 1) { ip++; rPos++; continue; }  /* literal; note: can end up with several literals, in which case, skip them */
                 rPos += ml;
                 assert(ml >= MINMATCH);
                 assert((offset >= 1) && (offset <= LZ4_DISTANCE_MAX));
                 opSaved = op;
                 if ( LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor), ml, offset, limit, oend) ) {  /* updates ip, op and anchor */
                     ovml = ml;
                     ovoff = offset;
                     goto _dest_overflow;
         }   }   }
     }  /* while (ip <= mflimit) */

_last_literals:
     /* Encode Last Literals */
     {   size_t lastRunSize = (size_t)(iend - anchor);  /* literals */
         size_t llAdd = (lastRunSize + 255 - RUN_MASK) / 255;
         size_t const totalSize = 1 + llAdd + lastRunSize;
         if (limit == fillOutput) oend += LASTLITERALS;  /* restore correct value */
         if (limit && (op + totalSize > oend)) {
             if (limit == limitedOutput) { /* Check output limit */
                retval = 0;
                goto _return_label;
             }
             /* adapt lastRunSize to fill 'dst' */
             lastRunSize  = (size_t)(oend - op) - 1 /*token*/;
             llAdd = (lastRunSize + 256 - RUN_MASK) / 256;
             lastRunSize -= llAdd;
         }
         DEBUGLOG(6, "Final literal run : %i literals", (int)lastRunSize);
         ip = anchor + lastRunSize; /* can be != iend if limit==fillOutput */

         if (lastRunSize >= RUN_MASK) {
             size_t accumulator = lastRunSize - RUN_MASK;
             *op++ = (RUN_MASK << ML_BITS);
             for(; accumulator >= 255 ; accumulator -= 255) *op++ = 255;
             *op++ = (BYTE) accumulator;
         } else {
             *op++ = (BYTE)(lastRunSize << ML_BITS);
         }
         LZ4_memcpy(op, anchor, lastRunSize);
         op += lastRunSize;
     }

     /* End */
     *srcSizePtr = (int) (((const char*)ip) - source);
     retval = (int) ((char*)op-dst);
     goto _return_label;

_dest_overflow:
if (limit == fillOutput) {
     /* Assumption : ip, anchor, ovml and ovref must be set correctly */
     size_t const ll = (size_t)(ip - anchor);
     size_t const ll_addbytes = (ll + 240) / 255;
     size_t const ll_totalCost = 1 + ll_addbytes + ll;
     BYTE* const maxLitPos = oend - 3; /* 2 for offset, 1 for token */
     DEBUGLOG(6, "Last sequence overflowing (only %i bytes remaining)", (int)(oend-1-opSaved));
     op = opSaved;  /* restore correct out pointer */
     if (op + ll_totalCost <= maxLitPos) {
         /* ll validated; now adjust match length */
         size_t const bytesLeftForMl = (size_t)(maxLitPos - (op+ll_totalCost));
         size_t const maxMlSize = MINMATCH + (ML_MASK-1) + (bytesLeftForMl * 255);
         assert(maxMlSize < INT_MAX); assert(ovml >= 0);
         if ((size_t)ovml > maxMlSize) ovml = (int)maxMlSize;
         if ((oend + LASTLITERALS) - (op + ll_totalCost + 2) - 1 + ovml >= MFLIMIT) {
             DEBUGLOG(6, "Space to end : %i + ml (%i)", (int)((oend + LASTLITERALS) - (op + ll_totalCost + 2) - 1), ovml);
             DEBUGLOG(6, "Before : ip = %p, anchor = %p", ip, anchor);
             LZ4HC_encodeSequence(UPDATABLE(ip, op, anchor), ovml, ovoff, notLimited, oend);
             DEBUGLOG(6, "After : ip = %p, anchor = %p", ip, anchor);
     }   }
     goto _last_literals;
}
_return_label:
#if defined(LZ4HC_HEAPMODE) && LZ4HC_HEAPMODE==1
     if (opt) FREEMEM(opt);
#endif
     return retval;
}


/***************************************************
*  Deprecated Functions
***************************************************/

/* These functions currently generate deprecation warnings */

/* Wrappers for deprecated compression functions */
int LZ4_compressHC(const char* src, char* dst, int srcSize) { return LZ4_compress_HC (src, dst, srcSize, LZ4_compressBound(srcSize), 0); }
int LZ4_compressHC_limitedOutput(const char* src, char* dst, int srcSize, int maxDstSize) { return LZ4_compress_HC(src, dst, srcSize, maxDstSize, 0); }
int LZ4_compressHC2(const char* src, char* dst, int srcSize, int cLevel) { return LZ4_compress_HC (src, dst, srcSize, LZ4_compressBound(srcSize), cLevel); }
int LZ4_compressHC2_limitedOutput(const char* src, char* dst, int srcSize, int maxDstSize, int cLevel) { return LZ4_compress_HC(src, dst, srcSize, maxDstSize, cLevel); }
int LZ4_compressHC_withStateHC (void* state, const char* src, char* dst, int srcSize) { return LZ4_compress_HC_extStateHC (state, src, dst, srcSize, LZ4_compressBound(srcSize), 0); }
int LZ4_compressHC_limitedOutput_withStateHC (void* state, const char* src, char* dst, int srcSize, int maxDstSize) { return LZ4_compress_HC_extStateHC (state, src, dst, srcSize, maxDstSize, 0); }
int LZ4_compressHC2_withStateHC (void* state, const char* src, char* dst, int srcSize, int cLevel) { return LZ4_compress_HC_extStateHC(state, src, dst, srcSize, LZ4_compressBound(srcSize), cLevel); }
int LZ4_compressHC2_limitedOutput_withStateHC (void* state, const char* src, char* dst, int srcSize, int maxDstSize, int cLevel) { return LZ4_compress_HC_extStateHC(state, src, dst, srcSize, maxDstSize, cLevel); }
int LZ4_compressHC_continue (LZ4_streamHC_t* ctx, const char* src, char* dst, int srcSize) { return LZ4_compress_HC_continue (ctx, src, dst, srcSize, LZ4_compressBound(srcSize)); }
int LZ4_compressHC_limitedOutput_continue (LZ4_streamHC_t* ctx, const char* src, char* dst, int srcSize, int maxDstSize) { return LZ4_compress_HC_continue (ctx, src, dst, srcSize, maxDstSize); }


/* Deprecated streaming functions */
int LZ4_sizeofStreamStateHC(void) { return sizeof(LZ4_streamHC_t); }

/* state is presumed correctly sized, aka >= sizeof(LZ4_streamHC_t)
 * @return : 0 on success, !=0 if error */
int LZ4_resetStreamStateHC(void* state, char* inputBuffer)
{
    LZ4_streamHC_t* const hc4 = LZ4_initStreamHC(state, sizeof(*hc4));
    if (hc4 == NULL) return 1;   /* init failed */
    LZ4HC_init_internal (&hc4->internal_donotuse, (const BYTE*)inputBuffer);
    return 0;
}

#if !defined(LZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION)
void* LZ4_createHC (const char* inputBuffer)
{
    LZ4_streamHC_t* const hc4 = LZ4_createStreamHC();
    if (hc4 == NULL) return NULL;   /* not enough memory */
    LZ4HC_init_internal (&hc4->internal_donotuse, (const BYTE*)inputBuffer);
    return hc4;
}

int LZ4_freeHC (void* LZ4HC_Data)
{
    if (!LZ4HC_Data) return 0;  /* support free on NULL */
    FREEMEM(LZ4HC_Data);
    return 0;
}
#endif

int LZ4_compressHC2_continue (void* LZ4HC_Data, const char* src, char* dst, int srcSize, int cLevel)
{
    return LZ4HC_compress_generic (&((LZ4_streamHC_t*)LZ4HC_Data)->internal_donotuse, src, dst, &srcSize, 0, cLevel, notLimited);
}

int LZ4_compressHC2_limitedOutput_continue (void* LZ4HC_Data, const char* src, char* dst, int srcSize, int dstCapacity, int cLevel)
{
    return LZ4HC_compress_generic (&((LZ4_streamHC_t*)LZ4HC_Data)->internal_donotuse, src, dst, &srcSize, dstCapacity, cLevel, limitedOutput);
}

char* LZ4_slideInputBufferHC(void* LZ4HC_Data)
{
    LZ4HC_CCtx_internal* const s = &((LZ4_streamHC_t*)LZ4HC_Data)->internal_donotuse;
    const BYTE* const bufferStart = s->prefixStart - s->dictLimit + s->lowLimit;
    LZ4_resetStreamHC_fast((LZ4_streamHC_t*)LZ4HC_Data, s->compressionLevel);
    /* ugly conversion trick, required to evade (const char*) -> (char*) cast-qual warning :( */
    return (char*)(uptrval)bufferStart;
}
