/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ERRNO_CANONICAL_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ERRNO_CANONICAL_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/detail/ctypes.h>

/*
	Canonical errno numbering for the Status errno space.

	A Status value must mean the same thing on every platform: it travels between a Linux
	server and a Windows client, gets logged, stored and compared. errno numbering, however,
	is OS-specific - EAGAIN is 11 on Linux and 35 on Darwin, EDEADLK is 35 on Linux, 11 on
	Darwin and 36 on Windows, ENOTSUP is 95 / 45 / 129. Encoding the *native* number into a
	Status would therefore produce a different Status for the same error on each OS.

	So the errno sub-space of Status uses ONE portable numbering - the Linux/asm-generic one,
	which Android (bionic) and the wasm port already share verbatim, and which is what the
	reference platform has always produced. Conversion happens at the boundary:

		errnoToCanonical()  native errno -> canonical (used by status::errnoToStatus)
		canonicalToErrno()  canonical -> native errno (used by status::toErrno)

	On Linux/Android/wasm both are the identity. On Darwin and Windows they are tables.

	Codes that exist only on one OS (Darwin's EPWROFF, Windows' EOTHER, ...) have no portable
	counterpart; they are mapped into a reserved band (STATUS_ERRNO_PLATFORM_BAND) so they can
	never be mistaken for a canonical code of a different meaning. Going back, a canonical code
	the local platform has no name for yields 0 - the same "not an errno" answer toErrno()
	already gives for a non-errno Status.

	Two irreducible aliasings: canonical 11 covers EAGAIN and EWOULDBLOCK, canonical 95 covers
	ENOTSUP and EOPNOTSUPP (on Linux each pair IS one number). The reverse direction picks
	EAGAIN and ENOTSUP respectively.
*/

namespace sprt::status {

// Platform-specific errno codes are offset into this band. Canonical codes stay below it,
// and the whole errno sub-space is 16 bits wide (see STATUS_ERRNO_OFFSET in status.h).
constexpr int32_t STATUS_ERRNO_PLATFORM_BAND = 0x8000;

#if SPRT_APPLE

// Darwin (macOS/iOS) numbering. Values from sprt/c/cross/macos_sprt/errno.h; the trailing
// comment on a line is the native value when it differs from the canonical one.
constexpr inline int32_t errnoToCanonical(int32_t __e) {
	switch (__e) {
	case __SPRT_EPERM: return 1;
	case __SPRT_ENOENT: return 2;
	case __SPRT_ESRCH: return 3;
	case __SPRT_EINTR: return 4;
	case __SPRT_EIO: return 5;
	case __SPRT_ENXIO: return 6;
	case __SPRT_E2BIG: return 7;
	case __SPRT_ENOEXEC: return 8;
	case __SPRT_EBADF: return 9;
	case __SPRT_ECHILD: return 10;
	case __SPRT_EDEADLK: return 35; // 11
	case __SPRT_ENOMEM: return 12;
	case __SPRT_EACCES: return 13;
	case __SPRT_EFAULT: return 14;
	case __SPRT_ENOTBLK: return 15;
	case __SPRT_EBUSY: return 16;
	case __SPRT_EEXIST: return 17;
	case __SPRT_EXDEV: return 18;
	case __SPRT_ENODEV: return 19;
	case __SPRT_ENOTDIR: return 20;
	case __SPRT_EISDIR: return 21;
	case __SPRT_EINVAL: return 22;
	case __SPRT_ENFILE: return 23;
	case __SPRT_EMFILE: return 24;
	case __SPRT_ENOTTY: return 25;
	case __SPRT_ETXTBSY: return 26;
	case __SPRT_EFBIG: return 27;
	case __SPRT_ENOSPC: return 28;
	case __SPRT_ESPIPE: return 29;
	case __SPRT_EROFS: return 30;
	case __SPRT_EMLINK: return 31;
	case __SPRT_EPIPE: return 32;
	case __SPRT_EDOM: return 33;
	case __SPRT_ERANGE: return 34;
	case __SPRT_EAGAIN: return 11; // 35
	case __SPRT_EINPROGRESS: return 115; // 36
	case __SPRT_EALREADY: return 114; // 37
	case __SPRT_ENOTSOCK: return 88; // 38
	case __SPRT_EDESTADDRREQ: return 89; // 39
	case __SPRT_EMSGSIZE: return 90; // 40
	case __SPRT_EPROTOTYPE: return 91; // 41
	case __SPRT_ENOPROTOOPT: return 92; // 42
	case __SPRT_EPROTONOSUPPORT: return 93; // 43
	case __SPRT_ESOCKTNOSUPPORT: return 94; // 44
	case __SPRT_ENOTSUP: return 95; // 45
	case __SPRT_EPFNOSUPPORT: return 96; // 46
	case __SPRT_EAFNOSUPPORT: return 97; // 47
	case __SPRT_EADDRINUSE: return 98; // 48
	case __SPRT_EADDRNOTAVAIL: return 99; // 49
	case __SPRT_ENETDOWN: return 100; // 50
	case __SPRT_ENETUNREACH: return 101; // 51
	case __SPRT_ENETRESET: return 102; // 52
	case __SPRT_ECONNABORTED: return 103; // 53
	case __SPRT_ECONNRESET: return 104; // 54
	case __SPRT_ENOBUFS: return 105; // 55
	case __SPRT_EISCONN: return 106; // 56
	case __SPRT_ENOTCONN: return 107; // 57
	case __SPRT_ESHUTDOWN: return 108; // 58
	case __SPRT_ETOOMANYREFS: return 109; // 59
	case __SPRT_ETIMEDOUT: return 110; // 60
	case __SPRT_ECONNREFUSED: return 111; // 61
	case __SPRT_ELOOP: return 40; // 62
	case __SPRT_ENAMETOOLONG: return 36; // 63
	case __SPRT_EHOSTDOWN: return 112; // 64
	case __SPRT_EHOSTUNREACH: return 113; // 65
	case __SPRT_ENOTEMPTY: return 39; // 66
	case __SPRT_EUSERS: return 87; // 68
	case __SPRT_EDQUOT: return 122; // 69
	case __SPRT_ESTALE: return 116; // 70
	case __SPRT_EREMOTE: return 66; // 71
	case __SPRT_ENOLCK: return 37; // 77
	case __SPRT_ENOSYS: return 38; // 78
	case __SPRT_EOVERFLOW: return 75; // 84
	case __SPRT_ECANCELED: return 125; // 89
	case __SPRT_EIDRM: return 43; // 90
	case __SPRT_ENOMSG: return 42; // 91
	case __SPRT_EILSEQ: return 84; // 92
	case __SPRT_EBADMSG: return 74; // 94
	case __SPRT_EMULTIHOP: return 72; // 95
	case __SPRT_ENODATA: return 61; // 96
	case __SPRT_ENOLINK: return 67; // 97
	case __SPRT_ENOSR: return 63; // 98
	case __SPRT_ENOSTR: return 60; // 99
	case __SPRT_EPROTO: return 71; // 100
	case __SPRT_ETIME: return 62; // 101
	case __SPRT_EOPNOTSUPP: return 95; // 102
	case __SPRT_ENOTRECOVERABLE: return 131; // 104
	case __SPRT_EOWNERDEAD: return 130; // 105
	default: break;
	}
	// Darwin-only: EPROCLIM, EBADRPC, ERPCMISMATCH, EPROGUNAVAIL, EPROGMISMATCH, EPROCUNAVAIL,
	// EFTYPE, EAUTH, ENEEDAUTH, EPWROFF, EDEVERR, EBADEXEC, EBADARCH, ESHLIBVERS, EBADMACHO,
	// ENOATTR, ENOPOLICY, EQFULL, ENOTCAPABLE
	return (__e > 0 && __e < STATUS_ERRNO_PLATFORM_BAND) ? __e + STATUS_ERRNO_PLATFORM_BAND : 0;
}

constexpr inline int32_t canonicalToErrno(int32_t __c) {
	switch (__c) {
	case 1: return __SPRT_EPERM;
	case 2: return __SPRT_ENOENT;
	case 3: return __SPRT_ESRCH;
	case 4: return __SPRT_EINTR;
	case 5: return __SPRT_EIO;
	case 6: return __SPRT_ENXIO;
	case 7: return __SPRT_E2BIG;
	case 8: return __SPRT_ENOEXEC;
	case 9: return __SPRT_EBADF;
	case 10: return __SPRT_ECHILD;
	case 11: return __SPRT_EAGAIN;
	case 12: return __SPRT_ENOMEM;
	case 13: return __SPRT_EACCES;
	case 14: return __SPRT_EFAULT;
	case 15: return __SPRT_ENOTBLK;
	case 16: return __SPRT_EBUSY;
	case 17: return __SPRT_EEXIST;
	case 18: return __SPRT_EXDEV;
	case 19: return __SPRT_ENODEV;
	case 20: return __SPRT_ENOTDIR;
	case 21: return __SPRT_EISDIR;
	case 22: return __SPRT_EINVAL;
	case 23: return __SPRT_ENFILE;
	case 24: return __SPRT_EMFILE;
	case 25: return __SPRT_ENOTTY;
	case 26: return __SPRT_ETXTBSY;
	case 27: return __SPRT_EFBIG;
	case 28: return __SPRT_ENOSPC;
	case 29: return __SPRT_ESPIPE;
	case 30: return __SPRT_EROFS;
	case 31: return __SPRT_EMLINK;
	case 32: return __SPRT_EPIPE;
	case 33: return __SPRT_EDOM;
	case 34: return __SPRT_ERANGE;
	case 35: return __SPRT_EDEADLK;
	case 36: return __SPRT_ENAMETOOLONG;
	case 37: return __SPRT_ENOLCK;
	case 38: return __SPRT_ENOSYS;
	case 39: return __SPRT_ENOTEMPTY;
	case 40: return __SPRT_ELOOP;
	case 42: return __SPRT_ENOMSG;
	case 43: return __SPRT_EIDRM;
	case 60: return __SPRT_ENOSTR;
	case 61: return __SPRT_ENODATA;
	case 62: return __SPRT_ETIME;
	case 63: return __SPRT_ENOSR;
	case 66: return __SPRT_EREMOTE;
	case 67: return __SPRT_ENOLINK;
	case 71: return __SPRT_EPROTO;
	case 72: return __SPRT_EMULTIHOP;
	case 74: return __SPRT_EBADMSG;
	case 75: return __SPRT_EOVERFLOW;
	case 84: return __SPRT_EILSEQ;
	case 87: return __SPRT_EUSERS;
	case 88: return __SPRT_ENOTSOCK;
	case 89: return __SPRT_EDESTADDRREQ;
	case 90: return __SPRT_EMSGSIZE;
	case 91: return __SPRT_EPROTOTYPE;
	case 92: return __SPRT_ENOPROTOOPT;
	case 93: return __SPRT_EPROTONOSUPPORT;
	case 94: return __SPRT_ESOCKTNOSUPPORT;
	case 95: return __SPRT_ENOTSUP;
	case 96: return __SPRT_EPFNOSUPPORT;
	case 97: return __SPRT_EAFNOSUPPORT;
	case 98: return __SPRT_EADDRINUSE;
	case 99: return __SPRT_EADDRNOTAVAIL;
	case 100: return __SPRT_ENETDOWN;
	case 101: return __SPRT_ENETUNREACH;
	case 102: return __SPRT_ENETRESET;
	case 103: return __SPRT_ECONNABORTED;
	case 104: return __SPRT_ECONNRESET;
	case 105: return __SPRT_ENOBUFS;
	case 106: return __SPRT_EISCONN;
	case 107: return __SPRT_ENOTCONN;
	case 108: return __SPRT_ESHUTDOWN;
	case 109: return __SPRT_ETOOMANYREFS;
	case 110: return __SPRT_ETIMEDOUT;
	case 111: return __SPRT_ECONNREFUSED;
	case 112: return __SPRT_EHOSTDOWN;
	case 113: return __SPRT_EHOSTUNREACH;
	case 114: return __SPRT_EALREADY;
	case 115: return __SPRT_EINPROGRESS;
	case 116: return __SPRT_ESTALE;
	case 122: return __SPRT_EDQUOT;
	case 125: return __SPRT_ECANCELED;
	case 130: return __SPRT_EOWNERDEAD;
	case 131: return __SPRT_ENOTRECOVERABLE;
	default: break;
	}
	return (__c >= STATUS_ERRNO_PLATFORM_BAND) ? __c - STATUS_ERRNO_PLATFORM_BAND : 0;
}

#elif SPRT_WINDOWS

// Windows CRT numbering. Values from sprt/c/cross/windows_sprt/errno.h; the trailing comment
// on a line is the native value when it differs from the canonical one.
constexpr inline int32_t errnoToCanonical(int32_t __e) {
	switch (__e) {
	case EPERM: return 1;
	case ENOENT: return 2;
	case ESRCH: return 3;
	case EINTR: return 4;
	case EIO: return 5;
	case ENXIO: return 6;
	case E2BIG: return 7;
	case ENOEXEC: return 8;
	case EBADF: return 9;
	case ECHILD: return 10;
	case EAGAIN: return 11;
	case ENOMEM: return 12;
	case EACCES: return 13;
	case EFAULT: return 14;
	case EBUSY: return 16;
	case EEXIST: return 17;
	case EXDEV: return 18;
	case ENODEV: return 19;
	case ENOTDIR: return 20;
	case EISDIR: return 21;
	case EINVAL: return 22;
	case ENFILE: return 23;
	case EMFILE: return 24;
	case ENOTTY: return 25;
	case EFBIG: return 27;
	case ENOSPC: return 28;
	case ESPIPE: return 29;
	case EROFS: return 30;
	case EMLINK: return 31;
	case EPIPE: return 32;
	case EDOM: return 33;
	case ERANGE: return 34;
	case EDEADLK: return 35; // 36
	case ENAMETOOLONG: return 36; // 38
	case ENOLCK: return 37; // 39
	case ENOSYS: return 38; // 40
	case ENOTEMPTY: return 39; // 41
	case EILSEQ: return 84; // 42
	case EADDRINUSE: return 98; // 100
	case EADDRNOTAVAIL: return 99; // 101
	case EAFNOSUPPORT: return 97; // 102
	case EALREADY: return 114; // 103
	case EBADMSG: return 74; // 104
	case ECANCELED: return 125; // 105
	case ECONNABORTED: return 103; // 106
	case ECONNREFUSED: return 111; // 107
	case ECONNRESET: return 104; // 108
	case EDESTADDRREQ: return 89; // 109
	case EHOSTUNREACH: return 113; // 110
	case EIDRM: return 43; // 111
	case EINPROGRESS: return 115; // 112
	case EISCONN: return 106; // 113
	case ELOOP: return 40; // 114
	case EMSGSIZE: return 90; // 115
	case ENETDOWN: return 100; // 116
	case ENETRESET: return 102; // 117
	case ENETUNREACH: return 101; // 118
	case ENOBUFS: return 105; // 119
	case ENODATA: return 61; // 120
	case ENOLINK: return 67; // 121
	case ENOMSG: return 42; // 122
	case ENOPROTOOPT: return 92; // 123
	case ENOSR: return 63; // 124
	case ENOSTR: return 60; // 125
	case ENOTCONN: return 107; // 126
	case ENOTRECOVERABLE: return 131; // 127
	case ENOTSOCK: return 88; // 128
	case ENOTSUP: return 95; // 129
	case EOPNOTSUPP: return 95; // 130
	case EOVERFLOW: return 75; // 132
	case EOWNERDEAD: return 130; // 133
	case EPROTO: return 71; // 134
	case EPROTONOSUPPORT: return 93; // 135
	case EPROTOTYPE: return 91; // 136
	case ETIME: return 62; // 137
	case ETIMEDOUT: return 110; // 138
	case ETXTBSY: return 26; // 139
	case EWOULDBLOCK: return 11; // 140
	case ENOMEDIUM: return 123; // 256
	case ELNRNG: return 48; // 257
	default: break;
	}
	// Windows-only: EOTHER
	return (__e > 0 && __e < STATUS_ERRNO_PLATFORM_BAND) ? __e + STATUS_ERRNO_PLATFORM_BAND : 0;
}

constexpr inline int32_t canonicalToErrno(int32_t __c) {
	switch (__c) {
	case 1: return EPERM;
	case 2: return ENOENT;
	case 3: return ESRCH;
	case 4: return EINTR;
	case 5: return EIO;
	case 6: return ENXIO;
	case 7: return E2BIG;
	case 8: return ENOEXEC;
	case 9: return EBADF;
	case 10: return ECHILD;
	case 11: return EAGAIN;
	case 12: return ENOMEM;
	case 13: return EACCES;
	case 14: return EFAULT;
	case 16: return EBUSY;
	case 17: return EEXIST;
	case 18: return EXDEV;
	case 19: return ENODEV;
	case 20: return ENOTDIR;
	case 21: return EISDIR;
	case 22: return EINVAL;
	case 23: return ENFILE;
	case 24: return EMFILE;
	case 25: return ENOTTY;
	case 26: return ETXTBSY;
	case 27: return EFBIG;
	case 28: return ENOSPC;
	case 29: return ESPIPE;
	case 30: return EROFS;
	case 31: return EMLINK;
	case 32: return EPIPE;
	case 33: return EDOM;
	case 34: return ERANGE;
	case 35: return EDEADLK;
	case 36: return ENAMETOOLONG;
	case 37: return ENOLCK;
	case 38: return ENOSYS;
	case 39: return ENOTEMPTY;
	case 40: return ELOOP;
	case 42: return ENOMSG;
	case 43: return EIDRM;
	case 48: return ELNRNG;
	case 60: return ENOSTR;
	case 61: return ENODATA;
	case 62: return ETIME;
	case 63: return ENOSR;
	case 67: return ENOLINK;
	case 71: return EPROTO;
	case 74: return EBADMSG;
	case 75: return EOVERFLOW;
	case 84: return EILSEQ;
	case 88: return ENOTSOCK;
	case 89: return EDESTADDRREQ;
	case 90: return EMSGSIZE;
	case 91: return EPROTOTYPE;
	case 92: return ENOPROTOOPT;
	case 93: return EPROTONOSUPPORT;
	case 95: return ENOTSUP;
	case 97: return EAFNOSUPPORT;
	case 98: return EADDRINUSE;
	case 99: return EADDRNOTAVAIL;
	case 100: return ENETDOWN;
	case 101: return ENETUNREACH;
	case 102: return ENETRESET;
	case 103: return ECONNABORTED;
	case 104: return ECONNRESET;
	case 105: return ENOBUFS;
	case 106: return EISCONN;
	case 107: return ENOTCONN;
	case 110: return ETIMEDOUT;
	case 111: return ECONNREFUSED;
	case 113: return EHOSTUNREACH;
	case 114: return EALREADY;
	case 115: return EINPROGRESS;
	case 123: return ENOMEDIUM;
	case 125: return ECANCELED;
	case 130: return EOWNERDEAD;
	case 131: return ENOTRECOVERABLE;
	default: break;
	}
	return (__c >= STATUS_ERRNO_PLATFORM_BAND) ? __c - STATUS_ERRNO_PLATFORM_BAND : 0;
}

#else

// Linux, Android and wasm all use the asm-generic numbering, which IS the canonical one.
// The static_asserts in status.h pin the canonical constants to this platform's list.
constexpr inline int32_t errnoToCanonical(int32_t __e) { return __e; }
constexpr inline int32_t canonicalToErrno(int32_t __c) { return __c; }

#endif

// Every canonical code that has a local counterpart must survive the round trip
// canonical -> native -> canonical. (The other direction cannot hold for aliases: Windows
// EWOULDBLOCK and EAGAIN both canonicalize to 11, which maps back to EAGAIN.)
constexpr int32_t STATUS_ERRNO_CANONICAL_MAX = 256; // the canonical list ends well below this

constexpr inline bool __checkErrnoCanonicalization() {
	for (int32_t __c = 1; __c < STATUS_ERRNO_CANONICAL_MAX; ++__c) {
		auto __n = canonicalToErrno(__c);
		if (__n != 0 && errnoToCanonical(__n) != __c) {
			return false;
		}
	}
	return true;
}

static_assert(__checkErrnoCanonicalization(),
		"errno canonicalization tables are not round-trippable");

} // namespace sprt::status

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ERRNO_CANONICAL_H_
