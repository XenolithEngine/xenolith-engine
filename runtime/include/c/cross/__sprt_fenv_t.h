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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FENV_T_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FENV_T_H_

#include <c/bits/__sprt_def.h>

__SPRT_BEGIN_DECL

#if SPRT_LINUX

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/linux/x86_64/fenv.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/linux/aarch64/fenv.h>
#else
#error "Unknown Linux arch"
#endif

#elif SPRT_WINDOWS

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/windows/x86_64/fenv.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/windows/aarch64/fenv.h>
#else
#error "Unknown Windows arch"
#endif

#elif SPRT_ANDROID

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/android/x86_64/fenv.h>
#elif defined(i386) || defined(__i386__) || defined(_M_IX86)
#include <c/cross/android/x86/fenv.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/android/arm64/fenv.h>
#elif defined(__arm__) || defined(_M_ARM)
#include <c/cross/android/arm/fenv.h>
#else
#error "Unknown Android arche"
#endif

#elif SPRT_MACOS

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/macos/x86_64/fenv.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/macos/aarch64/fenv.h>
#else
#error "Unknown Macos arch"
#endif

#else
#error "Unknown OS"
#endif

#ifndef __SPRT_FE_DFL_ENV

SPRT_API __sprt_fenv_t *__sprt_arch_FE_DFL_ENV_fn();

#define __SPRT_FE_DFL_ENV (__sprt_arch_FE_DFL_ENV_fn())

#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FENV_T_H_
