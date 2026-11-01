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

#ifndef RUNTIME_LIBC_FREESTANDING_INCLUDE_SPRT_FS_CONFIG_H_
#define RUNTIME_LIBC_FREESTANDING_INCLUDE_SPRT_FS_CONFIG_H_

// Shared by the C sources and the assembly: only preprocessor definitions here.
//
// SPRT_FS_PREFIX renames every exported function (memcpy -> <prefix>memcpy), so a
// test can link several implementations beside the platform libc. SPRT_FS_NO_ASM
// selects the C version of every function.

#define __SPRT_FS_CAT2(a, b) a##b
#define __SPRT_FS_CAT(a, b) __SPRT_FS_CAT2(a, b)

#ifdef SPRT_FS_PREFIX
#define SPRT_FS_NAME(name) __SPRT_FS_CAT(SPRT_FS_PREFIX, name)
#else
#define SPRT_FS_NAME(name) name
#endif

#ifndef __ASSEMBLER__
#include <sprt/c/bits/__sprt_def.h>

#if SPRT_WINDOWS
#include <sprt/wrappers/windows/__sprt_config.h>
#endif
#endif // __ASSEMBLER__

// Which functions this build provides. Windows imports most of them from ntdll
// (__SPRT_WIN_USE_IMPORT_STRING_LIB), the rest come from here.
#if !defined(__ASSEMBLER__) && SPRT_WINDOWS && __SPRT_WIN_USE_IMPORT_STRING_LIB && !defined(SPRT_FS_PREFIX)
#define SPRT_FS_PROVIDE_NTDLL_SET 0
#else
#define SPRT_FS_PROVIDE_NTDLL_SET 1
#endif

#define SPRT_FS_PROVIDE_memcpy SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_memmove SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_memset SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_memcmp SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_bcmp 1
#define SPRT_FS_PROVIDE_memchr 1
#define SPRT_FS_PROVIDE_memrchr 1
#define SPRT_FS_PROVIDE_strlen SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_strnlen SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_strcmp SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_strncmp SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_strchr SPRT_FS_PROVIDE_NTDLL_SET
#define SPRT_FS_PROVIDE_strrchr 1

// The assembly is written for the ELF ABIs of x86_64 (SysV), aarch64 (AAPCS64),
// riscv64 and loongarch64 (LP64D). It has two levels: general registers only, and
// SIMD, chosen by __SSE2__ / __ARM_NEON (which -mgeneral-regs-only removes),
// __riscv_vector (RVV 1.0, or SPRT_FS_FORCE_RVV) and __loongarch_sx (LSX).
// SPRT_FS_FORCE_GPR selects the first level whatever the flags.
#if !defined(SPRT_FS_NO_ASM) && defined(__ELF__) \
		&& (defined(__x86_64__) || defined(__aarch64__) \
				|| (defined(__riscv) && __riscv_xlen == 64) || defined(__loongarch64))
#define SPRT_FS_HAVE_ASM 1
#else
#define SPRT_FS_HAVE_ASM 0
#endif

#if SPRT_FS_HAVE_ASM && !defined(SPRT_FS_FORCE_GPR) \
		&& (defined(__SSE2__) || defined(__ARM_NEON) || defined(__riscv_vector) \
				|| defined(SPRT_FS_FORCE_RVV) || defined(__loongarch_sx))
#define SPRT_FS_ASM_SIMD 1
#else
#define SPRT_FS_ASM_SIMD 0
#endif

// riscv64 at the general-register level uses Zbb (orc.b, ctz) for the searches when
// the flags enable it (or SPRT_FS_FORCE_ZBB).
#if SPRT_FS_HAVE_ASM && defined(__riscv) && (defined(__riscv_zbb) || defined(SPRT_FS_FORCE_ZBB))
#define SPRT_FS_RV_ZBB 1
#else
#define SPRT_FS_RV_ZBB 0
#endif

// x86_64 at the SIMD level adds AVX2 paths for long inputs. 2: always, when the flags
// guarantee AVX2 (or SPRT_FS_FORCE_AVX2); 1: chosen on the first call that needs them,
// from CPUID and XGETBV (the OS must save the YMM state); 0: never (SPRT_FS_NO_AVX2).
#if SPRT_FS_ASM_SIMD && defined(__x86_64__) && !defined(SPRT_FS_NO_AVX2)
#if defined(__AVX2__) || defined(SPRT_FS_FORCE_AVX2)
#define SPRT_FS_X86_AVX2 2
#else
#define SPRT_FS_X86_AVX2 1
#endif
#else
#define SPRT_FS_X86_AVX2 0
#endif

// aarch64 at the SIMD level takes SVE for the searches and comparisons when the flags
// enable it (or SPRT_FS_FORCE_SVE). There is no run time choice: reading the ID
// registers from EL0 traps on systems that do not emulate it.
#if SPRT_FS_ASM_SIMD && defined(__aarch64__) && !defined(SPRT_FS_NO_SVE) \
		&& (defined(__ARM_FEATURE_SVE) || defined(SPRT_FS_FORCE_SVE))
#define SPRT_FS_A64_SVE 1
#else
#define SPRT_FS_A64_SVE 0
#endif

// Which functions the assembly defines; the C version of a function is compiled
// only when its SPRT_FS_ASM_<name> is 0.
#define SPRT_FS_ASM_memcpy SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_memmove SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_memset SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_memcmp SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_bcmp SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_memchr SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_memrchr 0
#define SPRT_FS_ASM_strlen SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_strnlen 0
#define SPRT_FS_ASM_strcmp SPRT_FS_HAVE_ASM
#define SPRT_FS_ASM_strncmp 0
#define SPRT_FS_ASM_strchr 0
#define SPRT_FS_ASM_strrchr 0

#define SPRT_FS_C_VERSION(name) (SPRT_FS_PROVIDE_##name && !SPRT_FS_ASM_##name)

// The floating-point environment (<fenv.h>). The primitives (feclearexcept
// feraiseexcept fetestexcept fegetround __fesetround fegetenv fesetenv) are musl's
// assembly wherever code may touch the FP control registers: the SIMD level of
// x86_64 and aarch64 (their general-register level is code built with FP disabled),
// and hard-float riscv64 and loongarch64. Elsewhere (WASM, soft float) they are
// musl's stubs for a machine without an FP environment. Windows has its own
// primitives in libc_impl/asm/Windows; the wrappers (fegetexceptflag
// fesetexceptflag feholdexcept feupdateenv fesetround __flt_rounds) are C everywhere.
#if SPRT_FS_HAVE_ASM && !defined(SPRT_FS_FORCE_GPR) \
		&& (((defined(__x86_64__) || defined(__aarch64__)) && SPRT_FS_ASM_SIMD) \
				|| (defined(__riscv) && defined(__riscv_flen) && !defined(__riscv_float_abi_soft)) \
				|| (defined(__loongarch64) && !defined(__loongarch_soft_float)))
#define SPRT_FS_ASM_FENV 1
#else
#define SPRT_FS_ASM_FENV 0
#endif

#define SPRT_FS_PROVIDE_FENV_PRIMITIVES SPRT_FS_PROVIDE_NTDLL_SET

#endif // RUNTIME_LIBC_FREESTANDING_INCLUDE_SPRT_FS_CONFIG_H_
