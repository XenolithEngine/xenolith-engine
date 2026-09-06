
// Embox EL0 uname backend.
//
// uname(160) is real, and the answer is the kernel's own -- unlike the wasm
// backend, which has no host to ask and reports a fixed identity.
//
// The wire form is six fixed 65-byte fields (struct xl_utsname, ABI doc section
// 4.2), because Embox's own struct utsname is six `const char *` and pointers
// into kernel memory are not something EL0 can be handed. sprt's
// struct utsname has its own field width, so the fields are copied rather than
// the struct.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_utsname.h>
#include <sprt/c/__sprt_errno.h>

#include "../include/__el0_syscall.h"

namespace sprt {

static constexpr unsigned EL0_UTSNAME_LEN = 65;

struct __el0_utsname {
	char sysname[EL0_UTSNAME_LEN];
	char nodename[EL0_UTSNAME_LEN];
	char release[EL0_UTSNAME_LEN];
	char version[EL0_UTSNAME_LEN];
	char machine[EL0_UTSNAME_LEN];
	char domainname[EL0_UTSNAME_LEN];
};

static_assert(sizeof(__el0_utsname) == 6 * 65, "struct utsname is 6x65 bytes on the wire");

// Copy one NUL-terminated field, truncating to whichever side is smaller. The
// kernel guarantees termination within its 65 bytes; sprt's field may be shorter
// or longer, so neither length can be assumed.
static void __el0_uname_copy(char *dst, const char *src) {
	unsigned n = 0;
	while (src[n] && n < __SPRT_SYS_NAMELEN - 1 && n < EL0_UTSNAME_LEN - 1) {
		dst[n] = src[n];
		++n;
	}
	dst[n] = 0;
}

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	if (!buf) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__el0_utsname wire;
	if (__el0_ret(__el0_uname(&wire)) < 0) {
		return -1;
	}
	__builtin_memset(buf, 0, sizeof(struct __SPRT_UTSNAME_NAME));
	__el0_uname_copy(buf->sysname, wire.sysname);
	__el0_uname_copy(buf->nodename, wire.nodename);
	__el0_uname_copy(buf->release, wire.release);
	__el0_uname_copy(buf->version, wire.version);
	__el0_uname_copy(buf->machine, wire.machine);
	return 0;
}

} // namespace sprt
