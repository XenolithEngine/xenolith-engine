// unistd on Tau: the module is the process (its manifest index + 1 as the pid), no users,
// no brk, no exec.

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include "../../include/__impl_libc.h"

extern "C" unsigned __tau_module_index();
extern "C" unsigned __tau_module_cpus();

extern "C" __SPRT_ID(pid_t) getpid(void) __SPRT_NOEXCEPT { return (__SPRT_ID(pid_t))(__tau_module_index() + 1); }

extern "C" __SPRT_ID(pid_t) getppid(void) __SPRT_NOEXCEPT { return 0; }

extern "C" __SPRT_ID(uid_t) getuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(uid_t) geteuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getgid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getegid(void) __SPRT_NOEXCEPT { return 0; }

extern "C" int usleep(__SPRT_ID(time_t) useconds) __SPRT_NOEXCEPT {
	__sprt_tau_sleep_ns((unsigned long long)useconds * 1'000ull);
	return 0;
}

extern "C" int brk(void *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOMEM;
	return -1;
}

extern "C" void *sbrk(__INTPTR_TYPE__) __SPRT_NOEXCEPT {
	__sprt_errno = ENOMEM;
	return (void *)(__INTPTR_TYPE__)-1;
}

extern "C" long sysconf(int name) __SPRT_NOEXCEPT {
	switch (name) {
	case __SPRT_SC_PAGESIZE: return 4'096;
	case __SPRT_SC_NPROCESSORS_CONF:
	case __SPRT_SC_NPROCESSORS_ONLN: return (long)__tau_module_cpus(); // the module's CPU mask
	case __SPRT_SC_OPEN_MAX: return (long)sprt::MAX_FDS;
	default: return -1;
	}
}

namespace {

long __tau_pathconf(int name) {
	switch (name) {
	case __SPRT_PC_NAME_MAX: return 255;
	case __SPRT_PC_PATH_MAX: return 4'096;
	case __SPRT_PC_PIPE_BUF: return 4'096;
	default: return -1;
	}
}

} // namespace

extern "C" long pathconf(const char *, int name) __SPRT_NOEXCEPT { return __tau_pathconf(name); }

extern "C" long fpathconf(int fd, int name) __SPRT_NOEXCEPT {
	if (sprt::__libc::get()->get_fd_handle(fd) == nullptr) {
		__sprt_errno = EBADF;
		return -1;
	}
	return __tau_pathconf(name);
}

// Modules are started by root from the manifest, never by another module.
extern "C" int execve(const char *, char *const[], char *const[]) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

extern "C" char *ctermid(char *s) __SPRT_NOEXCEPT {
	static char empty[1] = {0};
	if (s) {
		s[0] = 0;
		return s;
	}
	return empty;
}

// What the libc wrappers name besides; the shared runtime keeps every wrapper (RT-A-SharedRuntime-1).
extern "C" const char *__tau_module_name();
extern "C" int faccessat(int, const char *, int, int) __SPRT_NOEXCEPT;

extern "C" unsigned int sleep(unsigned int seconds) __SPRT_NOEXCEPT {
	__sprt_tau_sleep_ns((unsigned long long)seconds * 1'000'000'000ull);
	return 0;
}

extern "C" int eaccess(const char *path, int amode) __SPRT_NOEXCEPT { return faccessat(__SPRT_AT_FDCWD, path, amode, 0); }

extern "C" int getpagesize(void) __SPRT_NOEXCEPT { return 4'096; }

extern "C" int getdtablesize(void) __SPRT_NOEXCEPT { return (int)sprt::MAX_FDS; }

extern "C" int getgroups(int, __SPRT_ID(gid_t) *) __SPRT_NOEXCEPT { return 0; }

extern "C" int getresuid(__SPRT_ID(uid_t) * r, __SPRT_ID(uid_t) * e, __SPRT_ID(uid_t) * s) __SPRT_NOEXCEPT {
	*r = *e = *s = 0;
	return 0;
}

extern "C" int getresgid(__SPRT_ID(gid_t) * r, __SPRT_ID(gid_t) * e, __SPRT_ID(gid_t) * s) __SPRT_NOEXCEPT {
	*r = *e = *s = 0;
	return 0;
}

// The node is the module, as in uname.
extern "C" int gethostname(char *name, __SPRT_ID(size_t) len) __SPRT_NOEXCEPT {
	const char *n = __tau_module_name();
	__SPRT_ID(size_t) i = 0;
	for (; n[i]; ++i) {
		if (i + 1 >= len) {
			__sprt_errno = ENAMETOOLONG;
			return -1;
		}
		name[i] = n[i];
	}
	name[i] = 0;
	return 0;
}

extern "C" char *getlogin(void) __SPRT_NOEXCEPT {
	__sprt_errno = ENXIO;
	return nullptr;
}

extern "C" int getlogin_r(char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return ENXIO; }

extern "C" char *ttyname(int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOTTY;
	return nullptr;
}

extern "C" int ttyname_r(int, char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return ENOTTY; }

extern "C" int lockf(int, int, __SPRT_ID(off_t)) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}
