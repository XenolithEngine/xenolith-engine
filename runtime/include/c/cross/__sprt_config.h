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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_CONFIG_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_CONFIG_H_

// Per-OS/Per-arch Stappler Runtime configuration
#include <c/bits/__sprt_def.h>

#if SPRT_LINUX
#include <c/cross/linux/config.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/linux/x86_64/config.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/linux/aarch64/config.h>
#else
#error "Unknown Linux arch"
#endif

#elif SPRT_WINDOWS
#include <c/cross/windows/config.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/windows/x86_64/config.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/windows/aarch64/config.h>
#else
#error "Unknown Windows arch"
#endif

#elif SPRT_ANDROID
#include <c/cross/android/config.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/android/x86_64/config.h>
#elif defined(i386) || defined(__i386__) || defined(_M_IX86)
#include <c/cross/android/x86/config.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/android/arm64/config.h>
#elif defined(__arm__) || defined(_M_ARM)
#include <c/cross/android/arm/config.h>
#else
#error "Unknown Android arche"
#endif

#elif SPRT_MACOS
#include <c/cross/macos/config.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <c/cross/macos/x86_64/config.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <c/cross/macos/aarch64/config.h>
#else
#error "Unknown Macos arch"
#endif

#else
#error "Unknown OS"
#endif

// All fallback values should be defined there

/*
	Use this list to create your own platform configurations
*/

#ifndef __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC
#define __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_CHOWN
#define __SPRT_CONFIG_HAVE_UNISTD_CHOWN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP
#define __SPRT_CONFIG_HAVE_UNISTD_DUP 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP3
#define __SPRT_CONFIG_HAVE_UNISTD_DUP3 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_EXEC
#define __SPRT_CONFIG_HAVE_UNISTD_EXEC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_FEXEC
#define __SPRT_CONFIG_HAVE_UNISTD_FEXEC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
#define __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
#define __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_NICE
#define __SPRT_CONFIG_HAVE_UNISTD_NICE 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_CONF
#define __SPRT_CONFIG_HAVE_UNISTD_CONF 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_GETPPID
#define __SPRT_CONFIG_HAVE_UNISTD_GETPPID 1
#endif

#ifndef __SPRT_CONFIG_HAVE_SELECT
#define __SPRT_CONFIG_HAVE_SELECT 1
#endif

#ifndef __SPRT_CONFIG_HAVE_ADJTIME
#define __SPRT_CONFIG_HAVE_ADJTIME 1
#endif

#endif
