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
// cross/macos_sprt/errno.h <-> Darwin <sys/errno.h> parity.
//
// The runtime routes its errno cell straight to libSystem's __error(), so every
// __SPRT_E* number MUST equal the native one -- EDEADLK is 11 here but 35 on
// Linux, EAGAIN 35 here but 11 on Linux, and a mismatch silently corrupts every
// errno comparison in the runtime.
//
// libc_wrapper/c/common/errno.cc already asserts this, but only in a *hosted*
// build: on +open that compares against the +open headers, never against Apple's.
// This TU closes that gap by pinning the table against the real SDK too.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <errno.h>
#include <sys/errno.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_errno.h>
#include "abi_check.h"

// === the full Darwin errno table (1..ELAST) =====================================
SPRT_CONST(EPERM);
SPRT_CONST(ENOENT);
SPRT_CONST(ESRCH);
SPRT_CONST(EINTR);
SPRT_CONST(EIO);
SPRT_CONST(ENXIO);
SPRT_CONST(E2BIG);
SPRT_CONST(ENOEXEC);
SPRT_CONST(EBADF);
SPRT_CONST(ECHILD);
SPRT_CONST(EDEADLK);
SPRT_CONST(ENOMEM);
SPRT_CONST(EACCES);
SPRT_CONST(EFAULT);
SPRT_CONST(ENOTBLK);
SPRT_CONST(EBUSY);
SPRT_CONST(EEXIST);
SPRT_CONST(EXDEV);
SPRT_CONST(ENODEV);
SPRT_CONST(ENOTDIR);
SPRT_CONST(EISDIR);
SPRT_CONST(EINVAL);
SPRT_CONST(ENFILE);
SPRT_CONST(EMFILE);
SPRT_CONST(ENOTTY);
SPRT_CONST(ETXTBSY);
SPRT_CONST(EFBIG);
SPRT_CONST(ENOSPC);
SPRT_CONST(ESPIPE);
SPRT_CONST(EROFS);
SPRT_CONST(EMLINK);
SPRT_CONST(EPIPE);
SPRT_CONST(EDOM);
SPRT_CONST(ERANGE);
SPRT_CONST(EAGAIN);
SPRT_CONST(EWOULDBLOCK);
SPRT_CONST(EINPROGRESS);
SPRT_CONST(EALREADY);
SPRT_CONST(ENOTSOCK);
SPRT_CONST(EDESTADDRREQ);
SPRT_CONST(EMSGSIZE);
SPRT_CONST(EPROTOTYPE);
SPRT_CONST(ENOPROTOOPT);
SPRT_CONST(EPROTONOSUPPORT);
SPRT_CONST(ESOCKTNOSUPPORT);
SPRT_CONST(ENOTSUP);
SPRT_CONST(EPFNOSUPPORT);
SPRT_CONST(EAFNOSUPPORT);
SPRT_CONST(EADDRINUSE);
SPRT_CONST(EADDRNOTAVAIL);
SPRT_CONST(ENETDOWN);
SPRT_CONST(ENETUNREACH);
SPRT_CONST(ENETRESET);
SPRT_CONST(ECONNABORTED);
SPRT_CONST(ECONNRESET);
SPRT_CONST(ENOBUFS);
SPRT_CONST(EISCONN);
SPRT_CONST(ENOTCONN);
SPRT_CONST(ESHUTDOWN);
SPRT_CONST(ETOOMANYREFS);
SPRT_CONST(ETIMEDOUT);
SPRT_CONST(ECONNREFUSED);
SPRT_CONST(ELOOP);
SPRT_CONST(ENAMETOOLONG);
SPRT_CONST(EHOSTDOWN);
SPRT_CONST(EHOSTUNREACH);
SPRT_CONST(ENOTEMPTY);
SPRT_CONST(EPROCLIM);
SPRT_CONST(EUSERS);
SPRT_CONST(EDQUOT);
SPRT_CONST(ESTALE);
SPRT_CONST(EREMOTE);
SPRT_CONST(EBADRPC);
SPRT_CONST(ERPCMISMATCH);
SPRT_CONST(EPROGUNAVAIL);
SPRT_CONST(EPROGMISMATCH);
SPRT_CONST(EPROCUNAVAIL);
SPRT_CONST(ENOLCK);
SPRT_CONST(ENOSYS);
SPRT_CONST(EFTYPE);
SPRT_CONST(EAUTH);
SPRT_CONST(ENEEDAUTH);
SPRT_CONST(EPWROFF);
SPRT_CONST(EDEVERR);
SPRT_CONST(EOVERFLOW);
SPRT_CONST(EBADEXEC);
SPRT_CONST(EBADARCH);
SPRT_CONST(ESHLIBVERS);
SPRT_CONST(EBADMACHO);
SPRT_CONST(ECANCELED);
SPRT_CONST(EIDRM);
SPRT_CONST(ENOMSG);
SPRT_CONST(EILSEQ);
SPRT_CONST(ENOATTR);
SPRT_CONST(EBADMSG);
SPRT_CONST(EMULTIHOP);
SPRT_CONST(ENODATA);
SPRT_CONST(ENOLINK);
SPRT_CONST(ENOSR);
SPRT_CONST(ENOSTR);
SPRT_CONST(EPROTO);
SPRT_CONST(ETIME);
SPRT_CONST(EOPNOTSUPP);
SPRT_CONST(ENOPOLICY);
SPRT_CONST(ENOTRECOVERABLE);
SPRT_CONST(EOWNERDEAD);
SPRT_CONST(EQFULL);

// ELAST is Darwin's "highest defined errno". Pinning it makes a table that
// grew or lost a tail entry a compile error rather than a silent drift.
SPRT_CONST_MAP(EQFULL, ELAST);

// __SPRT_ENOTCAPABLE (107) is deliberately NOT asserted: it is past Darwin's
// ELAST and <sys/errno.h> does not define ENOTCAPABLE at all (it is a Capsicum
// name xnu only uses internally). sprt carries it so the cross-platform Status
// mapping has a number to use; nothing hands it to libSystem.
