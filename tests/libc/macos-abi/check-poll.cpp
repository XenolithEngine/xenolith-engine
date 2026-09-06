// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// cross/macos_sprt/polltypes.h <-> Darwin <poll.h> parity.
//
// The poll() shim passes the events/revents bitmask through untouched, so the
// bits must be Darwin's. Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <poll.h>
#include <sys/poll.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_polltypes.h>
#include "abi_check.h"

// === poll() event bits =====================================================
SPRT_CONST(POLLIN);
SPRT_CONST(POLLPRI);
SPRT_CONST(POLLOUT);
SPRT_CONST(POLLRDNORM);
SPRT_CONST(POLLWRNORM);
SPRT_CONST(POLLRDBAND);
SPRT_CONST(POLLWRBAND);
SPRT_CONST(POLLERR);
SPRT_CONST(POLLHUP);
SPRT_CONST(POLLNVAL);

// struct pollfd is handed to libSystem's poll() as-is.
SPRT_SIZE(__sprt_pollfd, pollfd);
SPRT_OFFSET(__sprt_pollfd, pollfd, fd);
SPRT_OFFSET(__sprt_pollfd, pollfd, events);
SPRT_OFFSET(__sprt_pollfd, pollfd, revents);
