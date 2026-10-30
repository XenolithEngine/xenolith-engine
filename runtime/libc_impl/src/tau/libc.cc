// The descriptors a Tau module starts with: 0, 1 and 2 are streams of tau.console@1,
// attached when first used (sprt_tau.h).

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/cxx/mutex>

namespace sprt {

void __init_default_fds(__libc *libc) {
	unique_lock lock(libc->fdMutex);
	for (int fd = 0; fd < 3; ++fd) {
		__sprt_tau_stream *s = nullptr;
		if (__sprt_tau_console_open(fd, &s) != 0) {
			continue;
		}
		libc->fdDispatch->bits.set(fd);
		auto &slot = libc->fdPages[0]->fds[fd];
		slot.ops = &libc->fdFileOps; // filled by load_file_fd_ops() next
		slot.handle = s;
		slot.flags = (fd == __libc::STDIN_FD) ? __SPRT_O_RDONLY : __SPRT_O_WRONLY;
		slot.mode = 0;
	}
	lock.unlock();

	// No users on Tau: a module's authority is its capabilities.
	libc->euid = 0;
	libc->egid = 0;
	libc->isAppContainer = false;
}

} // namespace sprt
