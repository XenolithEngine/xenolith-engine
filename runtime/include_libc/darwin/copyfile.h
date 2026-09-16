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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_DARWIN_COPYFILE_H_
#define CORE_RUNTIME_INCLUDE_LIBC_DARWIN_COPYFILE_H_

/*
	Darwin-only <copyfile.h>: the libSystem file-copy API (copyfile(3)).

	Not part of POSIX and not part of the SPRT libc, so it lives in the Darwin-only
	overlay (include_libc/darwin, added to the include path for the Darwin/iOS targets
	only) rather than in include_libc proper.

	Why it exists at all: SPRT does not ship the Xcode/MacOSX SDK headers -- the target
	include chain is SPRT's own libc -- but it DOES link -lSystem, and the platform
	exports the copyfile entry points (_copyfile, _fcopyfile, _copyfile_state_alloc,
	_copyfile_state_free in usr/lib/libSystem.tbd). Declaring them here lets Apple-targeted
	code use the native, kernel-assisted copy path (which knows about clonefile, sparse
	files and metadata) instead of a byte-by-byte fallback. libc++'s filesystem is the
	first consumer: src/filesystem/operations.cpp selects its copy_file backend with
	`#elif defined(__APPLE__) || __has_include(<copyfile.h>)` and then calls
	fcopyfile(read_fd, write_fd, state, COPYFILE_DATA) -- with no <copyfile.h> on the path
	that is a hard "file not found", and this header is what makes the vendored source
	compile unmodified.

	The declarations mirror MacOSX.sdk/usr/include/copyfile.h (SPRT's fds are the platform's
	fds -- the Darwin libc layer forwards to libSystem -- so they are directly usable).
	Only the stable, documented core is exposed: the state object, the flag words and the
	four entry points. The callback/progress surface (copyfile_callback_t,
	COPYFILE_STATE_STATUS_CB, the recursive COPYFILE_RECURSIVE walker) is deliberately
	left out until something needs it.

	Public surface:
	  types:      copyfile_state_t, copyfile_flags_t
	  what-to-copy flags:  COPYFILE_ACL, COPYFILE_STAT, COPYFILE_XATTR, COPYFILE_DATA
	                       + the composites COPYFILE_SECURITY, COPYFILE_METADATA, COPYFILE_ALL
	  behaviour flags:     COPYFILE_EXCL, COPYFILE_NOFOLLOW_SRC, COPYFILE_NOFOLLOW_DST,
	                       COPYFILE_NOFOLLOW, COPYFILE_MOVE, COPYFILE_UNLINK, COPYFILE_CLONE,
	                       COPYFILE_CLONE_FORCE, COPYFILE_DATA_SPARSE
	  state:      copyfile_state_alloc, copyfile_state_free, copyfile_state_get,
	              copyfile_state_set (+ the COPYFILE_STATE_* selectors)
	  copy:       copyfile (by path), fcopyfile (by descriptor)
*/

#include <sprt/c/bits/__sprt_def.h>
#include <stdint.h>

__SPRT_BEGIN_DECL

typedef struct _copyfile_state *copyfile_state_t;
typedef uint32_t copyfile_flags_t;

// What to copy.
#define COPYFILE_ACL (1 << 0)
#define COPYFILE_STAT (1 << 1)
#define COPYFILE_XATTR (1 << 2)
#define COPYFILE_DATA (1 << 3)

#define COPYFILE_SECURITY (COPYFILE_STAT | COPYFILE_ACL)
#define COPYFILE_METADATA (COPYFILE_SECURITY | COPYFILE_XATTR)
#define COPYFILE_ALL (COPYFILE_METADATA | COPYFILE_DATA)

// How to copy.
#define COPYFILE_NOFOLLOW_SRC (1 << 18)
#define COPYFILE_NOFOLLOW_DST (1 << 19)
#define COPYFILE_NOFOLLOW (COPYFILE_NOFOLLOW_SRC | COPYFILE_NOFOLLOW_DST)
#define COPYFILE_MOVE (1 << 20)
#define COPYFILE_UNLINK (1 << 21)
#define COPYFILE_EXCL (1 << 17)
#define COPYFILE_CLONE_FORCE (1 << 24)
#define COPYFILE_CLONE (1 << 25)
#define COPYFILE_DATA_SPARSE (1 << 27)

// copyfile_state_get / copyfile_state_set selectors.
#define COPYFILE_STATE_SRC_FD 1
#define COPYFILE_STATE_SRC_FILENAME 2
#define COPYFILE_STATE_DST_FD 3
#define COPYFILE_STATE_DST_FILENAME 4
#define COPYFILE_STATE_QUARANTINE 5
#define COPYFILE_STATE_COPIED 8
#define COPYFILE_STATE_XATTRNAME 9
#define COPYFILE_STATE_WAS_CLONED 10
#define COPYFILE_STATE_SRC_BSIZE 11
#define COPYFILE_STATE_DST_BSIZE 12
#define COPYFILE_STATE_BSIZE 13

copyfile_state_t copyfile_state_alloc(void);
int copyfile_state_free(copyfile_state_t __state);
int copyfile_state_get(copyfile_state_t __state, uint32_t __flag, void *__dst);
int copyfile_state_set(copyfile_state_t __state, uint32_t __flag, const void *__src);

int copyfile(const char *__from, const char *__to, copyfile_state_t __state,
		copyfile_flags_t __flags);
int fcopyfile(int __from_fd, int __to_fd, copyfile_state_t __state, copyfile_flags_t __flags);

__SPRT_END_DECL

#endif /* CORE_RUNTIME_INCLUDE_LIBC_DARWIN_COPYFILE_H_ */
