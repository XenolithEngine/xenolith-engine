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

#define __SPRT_BUILD 1

#if __STDC_HOSTED__ == 0

#include <sprt/c/__sprt_errno.h>

#else

// Pull the system errno numbers first (so the bare Exxx names resolve to the host values),
// then our internal per-platform set (__SPRT_Exxx). The public Exxx mapping in our headers is
// #ifndef EPERM-guarded, so the system definitions above win and there is no macro
// redefinition — leaving a clean `host Exxx vs our __SPRT_Exxx` comparison below. The runtime
// routes its errno cell to the host's errno (see __errno_location), so any divergence here
// would silently corrupt errno checks (e.g. EAGAIN is 11 on Linux but 35 on macOS); these
// assertions make such a mismatch a compile error instead.
#include <errno.h>
#include <sprt/c/__sprt_errno.h>

#define SPRT_ASSERT_ERRNO(name) \
	static_assert((name) == (__SPRT_##name), "sprt errno number differs from the host system: " #name)

#ifdef __SPRT_E2BIG
SPRT_ASSERT_ERRNO(E2BIG);
#endif
#ifdef __SPRT_EACCES
SPRT_ASSERT_ERRNO(EACCES);
#endif
#ifdef __SPRT_EADDRINUSE
SPRT_ASSERT_ERRNO(EADDRINUSE);
#endif
#ifdef __SPRT_EADDRNOTAVAIL
SPRT_ASSERT_ERRNO(EADDRNOTAVAIL);
#endif
#if defined(__SPRT_EADV) && (!SPRT_EMBOX || defined(EADV)) // Embox has no EADV
SPRT_ASSERT_ERRNO(EADV);
#endif
#ifdef __SPRT_EAFNOSUPPORT
SPRT_ASSERT_ERRNO(EAFNOSUPPORT);
#endif
#ifdef __SPRT_EAGAIN
SPRT_ASSERT_ERRNO(EAGAIN);
#endif
#ifdef __SPRT_EALREADY
SPRT_ASSERT_ERRNO(EALREADY);
#endif
#ifdef __SPRT_EAUTH
SPRT_ASSERT_ERRNO(EAUTH);
#endif
#ifdef __SPRT_EBADARCH
SPRT_ASSERT_ERRNO(EBADARCH);
#endif
#if defined(__SPRT_EBADE) && (!SPRT_EMBOX || defined(EBADE)) // Embox has no EBADE
SPRT_ASSERT_ERRNO(EBADE);
#endif
#ifdef __SPRT_EBADEXEC
SPRT_ASSERT_ERRNO(EBADEXEC);
#endif
#ifdef __SPRT_EBADF
SPRT_ASSERT_ERRNO(EBADF);
#endif
#if defined(__SPRT_EBADFD) && (!SPRT_EMBOX || defined(EBADFD)) // Embox has no EBADFD
SPRT_ASSERT_ERRNO(EBADFD);
#endif
#ifdef __SPRT_EBADMACHO
SPRT_ASSERT_ERRNO(EBADMACHO);
#endif
#ifdef __SPRT_EBADMSG
SPRT_ASSERT_ERRNO(EBADMSG);
#endif
#if defined(__SPRT_EBADR) && (!SPRT_EMBOX || defined(EBADR)) // Embox has no EBADR
SPRT_ASSERT_ERRNO(EBADR);
#endif
#ifdef __SPRT_EBADRPC
SPRT_ASSERT_ERRNO(EBADRPC);
#endif
#if defined(__SPRT_EBADRQC) && (!SPRT_EMBOX || defined(EBADRQC)) // Embox has no EBADRQC
SPRT_ASSERT_ERRNO(EBADRQC);
#endif
#if defined(__SPRT_EBADSLT) && (!SPRT_EMBOX || defined(EBADSLT)) // Embox has no EBADSLT
SPRT_ASSERT_ERRNO(EBADSLT);
#endif
#if defined(__SPRT_EBFONT) && (!SPRT_EMBOX || defined(EBFONT)) // Embox has no EBFONT
SPRT_ASSERT_ERRNO(EBFONT);
#endif
#ifdef __SPRT_EBUSY
SPRT_ASSERT_ERRNO(EBUSY);
#endif
#ifdef __SPRT_ECANCELED
SPRT_ASSERT_ERRNO(ECANCELED);
#endif
#ifdef __SPRT_ECHILD
SPRT_ASSERT_ERRNO(ECHILD);
#endif
#if defined(__SPRT_ECHRNG) && (!SPRT_EMBOX || defined(ECHRNG)) // Embox has no ECHRNG
SPRT_ASSERT_ERRNO(ECHRNG);
#endif
#if defined(__SPRT_ECOMM) && (!SPRT_EMBOX || defined(ECOMM)) // Embox has no ECOMM
SPRT_ASSERT_ERRNO(ECOMM);
#endif
#ifdef __SPRT_ECONNABORTED
SPRT_ASSERT_ERRNO(ECONNABORTED);
#endif
#ifdef __SPRT_ECONNREFUSED
SPRT_ASSERT_ERRNO(ECONNREFUSED);
#endif
#ifdef __SPRT_ECONNRESET
SPRT_ASSERT_ERRNO(ECONNRESET);
#endif
#ifdef __SPRT_EDEADLK
SPRT_ASSERT_ERRNO(EDEADLK);
#endif
#ifdef __SPRT_EDEADLOCK
SPRT_ASSERT_ERRNO(EDEADLOCK);
#endif
#ifdef __SPRT_EDESTADDRREQ
SPRT_ASSERT_ERRNO(EDESTADDRREQ);
#endif
#ifdef __SPRT_EDEVERR
SPRT_ASSERT_ERRNO(EDEVERR);
#endif
#ifdef __SPRT_EDOM
SPRT_ASSERT_ERRNO(EDOM);
#endif
#if defined(__SPRT_EDOTDOT) && (!SPRT_EMBOX || defined(EDOTDOT)) // Embox has no EDOTDOT
SPRT_ASSERT_ERRNO(EDOTDOT);
#endif
#ifdef __SPRT_EDQUOT
SPRT_ASSERT_ERRNO(EDQUOT);
#endif
#ifdef __SPRT_EEXIST
SPRT_ASSERT_ERRNO(EEXIST);
#endif
#ifdef __SPRT_EFAULT
SPRT_ASSERT_ERRNO(EFAULT);
#endif
#ifdef __SPRT_EFBIG
SPRT_ASSERT_ERRNO(EFBIG);
#endif
#ifdef __SPRT_EFTYPE
SPRT_ASSERT_ERRNO(EFTYPE);
#endif
#ifdef __SPRT_EHOSTDOWN
SPRT_ASSERT_ERRNO(EHOSTDOWN);
#endif
#ifdef __SPRT_EHOSTUNREACH
SPRT_ASSERT_ERRNO(EHOSTUNREACH);
#endif
#if defined(__SPRT_EHWPOISON) && (!SPRT_EMBOX || defined(EHWPOISON)) // Embox has no EHWPOISON
SPRT_ASSERT_ERRNO(EHWPOISON);
#endif
#ifdef __SPRT_EIDRM
SPRT_ASSERT_ERRNO(EIDRM);
#endif
#ifdef __SPRT_EILSEQ
SPRT_ASSERT_ERRNO(EILSEQ);
#endif
#ifdef __SPRT_EINPROGRESS
SPRT_ASSERT_ERRNO(EINPROGRESS);
#endif
#ifdef __SPRT_EINTR
SPRT_ASSERT_ERRNO(EINTR);
#endif
#ifdef __SPRT_EINVAL
SPRT_ASSERT_ERRNO(EINVAL);
#endif
#ifdef __SPRT_EIO
SPRT_ASSERT_ERRNO(EIO);
#endif
#ifdef __SPRT_EISCONN
SPRT_ASSERT_ERRNO(EISCONN);
#endif
#ifdef __SPRT_EISDIR
SPRT_ASSERT_ERRNO(EISDIR);
#endif
#if defined(__SPRT_EISNAM) && (!SPRT_EMBOX || defined(EISNAM)) // Embox has no EISNAM
SPRT_ASSERT_ERRNO(EISNAM);
#endif
#if defined(__SPRT_EKEYEXPIRED) && (!SPRT_EMBOX || defined(EKEYEXPIRED)) // Embox has no EKEYEXPIRED
SPRT_ASSERT_ERRNO(EKEYEXPIRED);
#endif
#if defined(__SPRT_EKEYREJECTED) && (!SPRT_EMBOX || defined(EKEYREJECTED)) // Embox has no EKEYREJECTED
SPRT_ASSERT_ERRNO(EKEYREJECTED);
#endif
#if defined(__SPRT_EKEYREVOKED) && (!SPRT_EMBOX || defined(EKEYREVOKED)) // Embox has no EKEYREVOKED
SPRT_ASSERT_ERRNO(EKEYREVOKED);
#endif
#if defined(__SPRT_EL2HLT) && (!SPRT_EMBOX || defined(EL2HLT)) // Embox has no EL2HLT
SPRT_ASSERT_ERRNO(EL2HLT);
#endif
#if defined(__SPRT_EL2NSYNC) && (!SPRT_EMBOX || defined(EL2NSYNC)) // Embox has no EL2NSYNC
SPRT_ASSERT_ERRNO(EL2NSYNC);
#endif
#if defined(__SPRT_EL3HLT) && (!SPRT_EMBOX || defined(EL3HLT)) // Embox has no EL3HLT
SPRT_ASSERT_ERRNO(EL3HLT);
#endif
#if defined(__SPRT_EL3RST) && (!SPRT_EMBOX || defined(EL3RST)) // Embox has no EL3RST
SPRT_ASSERT_ERRNO(EL3RST);
#endif
#if defined(__SPRT_ELIBACC) && (!SPRT_EMBOX || defined(ELIBACC)) // Embox has no ELIBACC
SPRT_ASSERT_ERRNO(ELIBACC);
#endif
#if defined(__SPRT_ELIBBAD) && (!SPRT_EMBOX || defined(ELIBBAD)) // Embox has no ELIBBAD
SPRT_ASSERT_ERRNO(ELIBBAD);
#endif
#if defined(__SPRT_ELIBEXEC) && (!SPRT_EMBOX || defined(ELIBEXEC)) // Embox has no ELIBEXEC
SPRT_ASSERT_ERRNO(ELIBEXEC);
#endif
#if defined(__SPRT_ELIBMAX) && (!SPRT_EMBOX || defined(ELIBMAX)) // Embox has no ELIBMAX
SPRT_ASSERT_ERRNO(ELIBMAX);
#endif
#if defined(__SPRT_ELIBSCN) && (!SPRT_EMBOX || defined(ELIBSCN)) // Embox has no ELIBSCN
SPRT_ASSERT_ERRNO(ELIBSCN);
#endif
#if defined(__SPRT_ELNRNG) && (!SPRT_EMBOX || defined(ELNRNG)) // Embox has no ELNRNG
SPRT_ASSERT_ERRNO(ELNRNG);
#endif
#ifdef __SPRT_ELOOP
SPRT_ASSERT_ERRNO(ELOOP);
#endif
#if defined(__SPRT_EMEDIUMTYPE) && (!SPRT_EMBOX || defined(EMEDIUMTYPE)) // Embox has no EMEDIUMTYPE
SPRT_ASSERT_ERRNO(EMEDIUMTYPE);
#endif
#ifdef __SPRT_EMFILE
SPRT_ASSERT_ERRNO(EMFILE);
#endif
#ifdef __SPRT_EMLINK
SPRT_ASSERT_ERRNO(EMLINK);
#endif
#ifdef __SPRT_EMSGSIZE
SPRT_ASSERT_ERRNO(EMSGSIZE);
#endif
#if defined(__SPRT_EMULTIHOP) && (!SPRT_EMBOX || defined(EMULTIHOP)) // Embox has no EMULTIHOP
SPRT_ASSERT_ERRNO(EMULTIHOP);
#endif
#ifdef __SPRT_ENAMETOOLONG
SPRT_ASSERT_ERRNO(ENAMETOOLONG);
#endif
#if defined(__SPRT_ENAVAIL) && (!SPRT_EMBOX || defined(ENAVAIL)) // Embox has no ENAVAIL
SPRT_ASSERT_ERRNO(ENAVAIL);
#endif
#ifdef __SPRT_ENEEDAUTH
SPRT_ASSERT_ERRNO(ENEEDAUTH);
#endif
#ifdef __SPRT_ENETDOWN
SPRT_ASSERT_ERRNO(ENETDOWN);
#endif
#ifdef __SPRT_ENETRESET
SPRT_ASSERT_ERRNO(ENETRESET);
#endif
#ifdef __SPRT_ENETUNREACH
SPRT_ASSERT_ERRNO(ENETUNREACH);
#endif
#ifdef __SPRT_ENFILE
SPRT_ASSERT_ERRNO(ENFILE);
#endif
#if defined(__SPRT_ENOANO) && (!SPRT_EMBOX || defined(ENOANO)) // Embox has no ENOANO
SPRT_ASSERT_ERRNO(ENOANO);
#endif
#ifdef __SPRT_ENOATTR
SPRT_ASSERT_ERRNO(ENOATTR);
#endif
#ifdef __SPRT_ENOBUFS
SPRT_ASSERT_ERRNO(ENOBUFS);
#endif
#if defined(__SPRT_ENOCSI) && (!SPRT_EMBOX || defined(ENOCSI)) // Embox has no ENOCSI
SPRT_ASSERT_ERRNO(ENOCSI);
#endif
#ifdef __SPRT_ENODATA
SPRT_ASSERT_ERRNO(ENODATA);
#endif
#ifdef __SPRT_ENODEV
SPRT_ASSERT_ERRNO(ENODEV);
#endif
#ifdef __SPRT_ENOENT
SPRT_ASSERT_ERRNO(ENOENT);
#endif
#ifdef __SPRT_ENOEXEC
SPRT_ASSERT_ERRNO(ENOEXEC);
#endif
#if defined(__SPRT_ENOKEY) && (!SPRT_EMBOX || defined(ENOKEY)) // Embox has no ENOKEY
SPRT_ASSERT_ERRNO(ENOKEY);
#endif
#ifdef __SPRT_ENOLCK
SPRT_ASSERT_ERRNO(ENOLCK);
#endif
#ifdef __SPRT_ENOLINK
SPRT_ASSERT_ERRNO(ENOLINK);
#endif
#if defined(__SPRT_ENOMEDIUM) && (!SPRT_EMBOX || defined(ENOMEDIUM)) // Embox has no ENOMEDIUM
SPRT_ASSERT_ERRNO(ENOMEDIUM);
#endif
#ifdef __SPRT_ENOMEM
SPRT_ASSERT_ERRNO(ENOMEM);
#endif
#ifdef __SPRT_ENOMSG
SPRT_ASSERT_ERRNO(ENOMSG);
#endif
#if defined(__SPRT_ENONET) && (!SPRT_EMBOX || defined(ENONET)) // Embox has no ENONET
SPRT_ASSERT_ERRNO(ENONET);
#endif
#if defined(__SPRT_ENOPKG) && (!SPRT_EMBOX || defined(ENOPKG)) // Embox has no ENOPKG
SPRT_ASSERT_ERRNO(ENOPKG);
#endif
#ifdef __SPRT_ENOPOLICY
SPRT_ASSERT_ERRNO(ENOPOLICY);
#endif
#ifdef __SPRT_ENOPROTOOPT
SPRT_ASSERT_ERRNO(ENOPROTOOPT);
#endif
#ifdef __SPRT_ENOSPC
SPRT_ASSERT_ERRNO(ENOSPC);
#endif
#ifdef __SPRT_ENOSR
SPRT_ASSERT_ERRNO(ENOSR);
#endif
#ifdef __SPRT_ENOSTR
SPRT_ASSERT_ERRNO(ENOSTR);
#endif
#ifdef __SPRT_ENOSYS
SPRT_ASSERT_ERRNO(ENOSYS);
#endif
#ifdef __SPRT_ENOTBLK
SPRT_ASSERT_ERRNO(ENOTBLK);
#endif
#ifdef __SPRT_ENOTCAPABLE
SPRT_ASSERT_ERRNO(ENOTCAPABLE);
#endif
#ifdef __SPRT_ENOTCONN
SPRT_ASSERT_ERRNO(ENOTCONN);
#endif
#ifdef __SPRT_ENOTDIR
SPRT_ASSERT_ERRNO(ENOTDIR);
#endif
#ifdef __SPRT_ENOTEMPTY
SPRT_ASSERT_ERRNO(ENOTEMPTY);
#endif
#if defined(__SPRT_ENOTNAM) && (!SPRT_EMBOX || defined(ENOTNAM)) // Embox has no ENOTNAM
SPRT_ASSERT_ERRNO(ENOTNAM);
#endif
#ifdef __SPRT_ENOTRECOVERABLE
SPRT_ASSERT_ERRNO(ENOTRECOVERABLE);
#endif
#ifdef __SPRT_ENOTSOCK
SPRT_ASSERT_ERRNO(ENOTSOCK);
#endif
#ifdef __SPRT_ENOTSUP
SPRT_ASSERT_ERRNO(ENOTSUP);
#endif
#ifdef __SPRT_ENOTTY
SPRT_ASSERT_ERRNO(ENOTTY);
#endif
#if defined(__SPRT_ENOTUNIQ) && (!SPRT_EMBOX || defined(ENOTUNIQ)) // Embox has no ENOTUNIQ
SPRT_ASSERT_ERRNO(ENOTUNIQ);
#endif
#ifdef __SPRT_ENXIO
SPRT_ASSERT_ERRNO(ENXIO);
#endif
#ifdef __SPRT_EOPNOTSUPP
SPRT_ASSERT_ERRNO(EOPNOTSUPP);
#endif
#ifdef __SPRT_EOVERFLOW
SPRT_ASSERT_ERRNO(EOVERFLOW);
#endif
#ifdef __SPRT_EOWNERDEAD
SPRT_ASSERT_ERRNO(EOWNERDEAD);
#endif
#ifdef __SPRT_EPERM
SPRT_ASSERT_ERRNO(EPERM);
#endif
#ifdef __SPRT_EPFNOSUPPORT
SPRT_ASSERT_ERRNO(EPFNOSUPPORT);
#endif
#ifdef __SPRT_EPIPE
SPRT_ASSERT_ERRNO(EPIPE);
#endif
#ifdef __SPRT_EPROCLIM
SPRT_ASSERT_ERRNO(EPROCLIM);
#endif
#ifdef __SPRT_EPROCUNAVAIL
SPRT_ASSERT_ERRNO(EPROCUNAVAIL);
#endif
#ifdef __SPRT_EPROGMISMATCH
SPRT_ASSERT_ERRNO(EPROGMISMATCH);
#endif
#ifdef __SPRT_EPROGUNAVAIL
SPRT_ASSERT_ERRNO(EPROGUNAVAIL);
#endif
#ifdef __SPRT_EPROTO
SPRT_ASSERT_ERRNO(EPROTO);
#endif
#ifdef __SPRT_EPROTONOSUPPORT
SPRT_ASSERT_ERRNO(EPROTONOSUPPORT);
#endif
#ifdef __SPRT_EPROTOTYPE
SPRT_ASSERT_ERRNO(EPROTOTYPE);
#endif
#ifdef __SPRT_EPWROFF
SPRT_ASSERT_ERRNO(EPWROFF);
#endif
#ifdef __SPRT_EQFULL
SPRT_ASSERT_ERRNO(EQFULL);
#endif
#ifdef __SPRT_ERANGE
SPRT_ASSERT_ERRNO(ERANGE);
#endif
#if defined(__SPRT_EREMCHG) && (!SPRT_EMBOX || defined(EREMCHG)) // Embox has no EREMCHG
SPRT_ASSERT_ERRNO(EREMCHG);
#endif
#if defined(__SPRT_EREMOTE) && (!SPRT_EMBOX || defined(EREMOTE)) // Embox has no EREMOTE
SPRT_ASSERT_ERRNO(EREMOTE);
#endif
#if defined(__SPRT_EREMOTEIO) && (!SPRT_EMBOX || defined(EREMOTEIO)) // Embox has no EREMOTEIO
SPRT_ASSERT_ERRNO(EREMOTEIO);
#endif
#if defined(__SPRT_ERESTART) && (!SPRT_EMBOX || defined(ERESTART)) // Embox has no ERESTART
SPRT_ASSERT_ERRNO(ERESTART);
#endif
#if defined(__SPRT_ERFKILL) && (!SPRT_EMBOX || defined(ERFKILL)) // Embox has no ERFKILL
SPRT_ASSERT_ERRNO(ERFKILL);
#endif
#ifdef __SPRT_EROFS
SPRT_ASSERT_ERRNO(EROFS);
#endif
#ifdef __SPRT_ERPCMISMATCH
SPRT_ASSERT_ERRNO(ERPCMISMATCH);
#endif
#ifdef __SPRT_ESHLIBVERS
SPRT_ASSERT_ERRNO(ESHLIBVERS);
#endif
#ifdef __SPRT_ESHUTDOWN
SPRT_ASSERT_ERRNO(ESHUTDOWN);
#endif
#ifdef __SPRT_ESOCKTNOSUPPORT
SPRT_ASSERT_ERRNO(ESOCKTNOSUPPORT);
#endif
#ifdef __SPRT_ESPIPE
SPRT_ASSERT_ERRNO(ESPIPE);
#endif
#ifdef __SPRT_ESRCH
SPRT_ASSERT_ERRNO(ESRCH);
#endif
#if defined(__SPRT_ESRMNT) && (!SPRT_EMBOX || defined(ESRMNT)) // Embox has no ESRMNT
SPRT_ASSERT_ERRNO(ESRMNT);
#endif
#if defined(__SPRT_ESTALE) && (!SPRT_EMBOX || defined(ESTALE)) // Embox has no ESTALE
SPRT_ASSERT_ERRNO(ESTALE);
#endif
#if defined(__SPRT_ESTRPIPE) && (!SPRT_EMBOX || defined(ESTRPIPE)) // Embox has no ESTRPIPE
SPRT_ASSERT_ERRNO(ESTRPIPE);
#endif
#ifdef __SPRT_ETIME
SPRT_ASSERT_ERRNO(ETIME);
#endif
#ifdef __SPRT_ETIMEDOUT
SPRT_ASSERT_ERRNO(ETIMEDOUT);
#endif
#ifdef __SPRT_ETOOMANYREFS
SPRT_ASSERT_ERRNO(ETOOMANYREFS);
#endif
#ifdef __SPRT_ETXTBSY
SPRT_ASSERT_ERRNO(ETXTBSY);
#endif
#if defined(__SPRT_EUCLEAN) && (!SPRT_EMBOX || defined(EUCLEAN)) // Embox has no EUCLEAN
SPRT_ASSERT_ERRNO(EUCLEAN);
#endif
#if defined(__SPRT_EUNATCH) && (!SPRT_EMBOX || defined(EUNATCH)) // Embox has no EUNATCH
SPRT_ASSERT_ERRNO(EUNATCH);
#endif
#if defined(__SPRT_EUSERS) && (!SPRT_EMBOX || defined(EUSERS)) // Embox has no EUSERS
SPRT_ASSERT_ERRNO(EUSERS);
#endif
#ifdef __SPRT_EWOULDBLOCK
SPRT_ASSERT_ERRNO(EWOULDBLOCK);
#endif
#ifdef __SPRT_EXDEV
SPRT_ASSERT_ERRNO(EXDEV);
#endif
#if defined(__SPRT_EXFULL) && (!SPRT_EMBOX || defined(EXFULL)) // Embox has no EXFULL
SPRT_ASSERT_ERRNO(EXFULL);
#endif

#undef SPRT_ASSERT_ERRNO

namespace sprt {

__SPRT_C_FUNC __SPRT_FALLBACK_ATTR(const) int *__SPRT_ID(__errno_location)(void) {
#if SPRT_LINUX
	return ::__errno_location();
#elif SPRT_ANDROID
	return ::__errno();
#elif SPRT_APPLE
	return __error();
#elif SPRT_EMBOX
	// Embox keeps errno in the task resource block; there is no __errno().
	return ::task_self_resource_errno();
#elif SPRT_NUTTX
	return ::__errno();
#else
	return ::__errno_location();
#endif
}

} // namespace sprt

#endif
