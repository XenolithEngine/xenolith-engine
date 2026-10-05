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
