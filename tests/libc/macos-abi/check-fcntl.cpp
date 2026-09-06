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
// cross/macos_sprt/<arch>_sprt/fcntl.h <-> Darwin <fcntl.h> parity.
//
// open()/fcntl()/openat() flags reach libSystem unmodified. The table lives
// under the per-arch directory even though x86_64 and aarch64 currently agree,
// so this TU is compiled once per architecture and pins whichever one the
// target selects.
//
// Darwin's O_* differ sharply from Linux: O_CREAT is 0x0200 (Linux 0100),
// O_DIRECTORY 0x100000, O_CLOEXEC 0x1000000, and the F_* command space is
// almost entirely Apple's own.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <fcntl.h>
#include <sys/fcntl.h>

#define SPRT_ABI_HEADER <sprt/c/cross/__sprt_fcntl.h>
#include "abi_check.h"

// === open() access modes and flags ===
SPRT_CONST(O_NONBLOCK);
SPRT_CONST(O_APPEND);
SPRT_CONST(O_SYNC);
SPRT_CONST(O_SHLOCK);
SPRT_CONST(O_EXLOCK);
SPRT_CONST(O_ASYNC);
SPRT_CONST(O_FSYNC);
SPRT_CONST(O_NOFOLLOW);
SPRT_CONST(O_CREAT);
SPRT_CONST(O_TRUNC);
SPRT_CONST(O_EXCL);
SPRT_CONST(O_EVTONLY);
SPRT_CONST(O_NOCTTY);
SPRT_CONST(O_DIRECTORY);
SPRT_CONST(O_SYMLINK);
SPRT_CONST(O_DSYNC);
SPRT_CONST(O_CLOEXEC);
SPRT_CONST(O_EXEC);
SPRT_CONST(O_SEARCH);
SPRT_CONST(O_NDELAY);
SPRT_CONST(O_ACCMODE);
SPRT_CONST(O_RDONLY);
SPRT_CONST(O_WRONLY);
SPRT_CONST(O_RDWR);

// === fcntl() commands and lock types ===
SPRT_CONST(F_DUPFD);
SPRT_CONST(F_GETFD);
SPRT_CONST(F_SETFD);
SPRT_CONST(F_GETFL);
SPRT_CONST(F_SETFL);
SPRT_CONST(F_GETOWN);
SPRT_CONST(F_SETOWN);
SPRT_CONST(F_GETLK);
SPRT_CONST(F_SETLK);
SPRT_CONST(F_SETLKW);
SPRT_CONST(F_DUPFD_CLOEXEC);
SPRT_CONST(F_OFD_SETLK);
SPRT_CONST(F_OFD_SETLKW);
SPRT_CONST(F_OFD_GETLK);
SPRT_CONST(F_RDLCK);
SPRT_CONST(F_UNLCK);
SPRT_CONST(F_WRLCK);

// === FD_CLOEXEC and the *at() resolution flags ===
SPRT_CONST(FD_CLOEXEC);
SPRT_CONST(AT_FDCWD);
SPRT_CONST(AT_EACCESS);
SPRT_CONST(AT_SYMLINK_NOFOLLOW);
SPRT_CONST(AT_SYMLINK_FOLLOW);
SPRT_CONST(AT_REMOVEDIR);
SPRT_CONST(AT_REALDEV);
SPRT_CONST(AT_FDONLY);
SPRT_CONST(AT_SYMLINK_NOFOLLOW_ANY);

// === deliberate omissions ==================================================
//
// Three groups of names in the table have nothing in the pinned SDK to be
// compared against. None is silently dropped: each is either an sprt-wide
// extension or a value from a newer Darwin, and where a real invariant exists
// it is asserted instead of the (impossible) value parity.

// (a) F_WAIT / F_FLOCK / F_POSIX / F_PROV / F_WAKE1_SAFE / F_ABORT /
//     F_OFD_LOCK / F_TRANSFER / F_CONFINED are the kernel's internal flock()
//     flags: `#ifdef KERNEL` in xnu's <sys/fcntl.h> and stripped from the
//     published SDK entirely. They are not userspace fcntl() commands, so there
//     is no ABI contract to pin. (The +open sysroot, being verbatim xnu, is
//     visibly wider than the SDK here -- same situation as MSG_USEUPCALL.)

// (b) O_INHERITABLE is an sprt-wide portability name defined on every target
//     (Linux and Windows carry it too); on Darwin it is 0, a no-op.
static_assert(__SPRT_O_INHERITABLE == 0, "O_INHERITABLE must stay a no-op on Darwin");

// (c) O_RESOLVE_BENEATH / O_UNIQUE / AT_RESOLVE_BENEATH / AT_NODELETEBUSY /
//     AT_UNIQUE are macOS 15 additions. The SDK this harness validates against
//     is 14.5, so their values cannot be confirmed here -- the same limitation
//     that puts os_unfair_lock_lock_with_flags in tbd-exceptions.txt. What can
//     still be checked, and is what actually matters for correctness, is that
//     they do not collide with an O_*/AT_* bit 14.5 *does* define: a collision
//     would silently turn one flag into another at the syscall boundary.
static_assert((__SPRT_O_RESOLVE_BENEATH | __SPRT_O_UNIQUE)
				& (O_ACCMODE | O_NONBLOCK | O_APPEND | O_SHLOCK | O_EXLOCK | O_ASYNC
						| O_SYNC | O_NOFOLLOW | O_CREAT | O_TRUNC | O_EXCL | O_EVTONLY
						| O_NOCTTY | O_DIRECTORY | O_SYMLINK | O_DSYNC | O_CLOEXEC)
				? 0 : 1,
		"a macOS 15 O_* bit collides with an O_* flag macOS 14.5 already defines");
static_assert((__SPRT_AT_RESOLVE_BENEATH | __SPRT_AT_NODELETEBUSY | __SPRT_AT_UNIQUE)
				& (AT_EACCESS | AT_SYMLINK_NOFOLLOW | AT_SYMLINK_FOLLOW | AT_REMOVEDIR
						| AT_REALDEV | AT_FDONLY | AT_SYMLINK_NOFOLLOW_ANY)
				? 0 : 1,
		"a macOS 15 AT_* bit collides with an AT_* flag macOS 14.5 already defines");
