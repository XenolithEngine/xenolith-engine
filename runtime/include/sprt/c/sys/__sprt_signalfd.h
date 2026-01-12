/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SIGNALFD_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SIGNALFD_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/bits/__sprt_sigset_t.h>

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_fcntl.h>

#define __SPRT_SFD_CLOEXEC __SPRT_O_CLOEXEC
#define __SPRT_SFD_NONBLOCK __SPRT_O_NONBLOCK

#ifdef __SPRT_BUILD
#define __SPRT_SIGNALFD_SIGINFO_NAME __SPRT_ID(signalfd_siginfo)
#else
#define __SPRT_SIGNALFD_SIGINFO_NAME signalfd_siginfo
#endif

struct __SPRT_SIGNALFD_SIGINFO_NAME {
	__SPRT_ID(uint32_t) ssi_signo;
	__SPRT_ID(int32_t) ssi_errno;
	__SPRT_ID(int32_t) ssi_code;
	__SPRT_ID(uint32_t) ssi_pid;
	__SPRT_ID(uint32_t) ssi_uid;
	__SPRT_ID(int32_t) ssi_fd;
	__SPRT_ID(uint32_t) ssi_tid;
	__SPRT_ID(uint32_t) ssi_band;
	__SPRT_ID(uint32_t) ssi_overrun;
	__SPRT_ID(uint32_t) ssi_trapno;
	__SPRT_ID(int32_t) ssi_status;
	__SPRT_ID(int32_t) ssi_int;
	__SPRT_ID(uint64_t) ssi_ptr;
	__SPRT_ID(uint64_t) ssi_utime;
	__SPRT_ID(uint64_t) ssi_stime;
	__SPRT_ID(uint64_t) ssi_addr;
	__SPRT_ID(uint16_t) ssi_addr_lsb;
	__SPRT_ID(uint16_t) __pad2;
	__SPRT_ID(int32_t) ssi_syscall;
	__SPRT_ID(uint64_t) ssi_call_addr;
	__SPRT_ID(uint32_t) ssi_arch;
	__SPRT_ID(uint8_t) __pad[128 - 14 * 4 - 5 * 8 - 2 * 2];
};

#if __SPRT_CONFIG_HAVE_SIGNALFD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(signalfd)(int, const __SPRT_ID(sigset_t) *, int);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SIGNALFD_H_
