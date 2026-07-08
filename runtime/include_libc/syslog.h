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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYSLOG_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYSLOG_H_

/*
	POSIX <syslog.h> - system logging.
	- hosted SPRT build -> forwards to the system <syslog.h> (#include_next)
	- otherwise         -> the priorities/facilities/options + prototypes below.

	There is no system logger on freestanding wasm; the sprt libc backs syslog()
	with a no-op stub (optionally routed to the JS console). The macros and
	prototypes still follow the Linux/musl values so callers compile unchanged.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <syslog.h>

#else

#include <sprt/c/bits/__sprt_def.h>
#include <stdarg.h>

#ifdef SPRT_WASM

// clang-format off
// priorities (highest to lowest)
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define LOG_PRIMASK 0x07
#define LOG_PRI(p)     ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(f, p) ((f) | (p))

#define LOG_MASK(pri) (1 << (pri))
#define LOG_UPTO(pri) ((1 << ((pri) + 1)) - 1)

// facility codes
#define LOG_KERN     (0 << 3)
#define LOG_USER     (1 << 3)
#define LOG_MAIL     (2 << 3)
#define LOG_DAEMON   (3 << 3)
#define LOG_AUTH     (4 << 3)
#define LOG_SYSLOG   (5 << 3)
#define LOG_LPR      (6 << 3)
#define LOG_NEWS     (7 << 3)
#define LOG_UUCP     (8 << 3)
#define LOG_CRON     (9 << 3)
#define LOG_AUTHPRIV (10 << 3)
#define LOG_FTP      (11 << 3)
#define LOG_LOCAL0   (16 << 3)
#define LOG_LOCAL1   (17 << 3)
#define LOG_LOCAL2   (18 << 3)
#define LOG_LOCAL3   (19 << 3)
#define LOG_LOCAL4   (20 << 3)
#define LOG_LOCAL5   (21 << 3)
#define LOG_LOCAL6   (22 << 3)
#define LOG_LOCAL7   (23 << 3)

#define LOG_NFACILITIES 24
#define LOG_FACMASK  0x03f8
#define LOG_FAC(p)   (((p) & LOG_FACMASK) >> 3)

// openlog() options
#define LOG_PID    0x01
#define LOG_CONS   0x02
#define LOG_ODELAY 0x04
#define LOG_NDELAY 0x08
#define LOG_NOWAIT 0x10
#define LOG_PERROR 0x20
// clang-format on

__SPRT_BEGIN_DECL

void openlog(const char *__ident, int __option, int __facility);
void closelog(void);
int setlogmask(int __mask);
void syslog(int __priority, const char *__format, ...);
void vsyslog(int __priority, const char *__format, va_list __ap);

__SPRT_END_DECL

#endif // SPRT_WASM

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYSLOG_H_
