
// Embox EL0 libc bring-up: wire fd 0/1/2 and set the effective ids.
//
// The kernel gives every task the console on descriptors 0/1/2 already open, so
// unlike wasm -- which has to invent a console node -- this only has to record
// that the libc's first three slots refer to the kernel's first three.
//
// That correspondence is an assumption about the loader, and it is the ONLY
// place in the backend where a libc fd number is assumed equal to a kernel one.
// When the ELF loader lands (K7) and starts handing over a descriptor table, it
// is this function that has to read it.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/cxx/mutex>

namespace sprt {

// Defined in libc_file_ops.cc (same TU).
void *__el0_handle(int kfd);

void __init_default_fds(__libc *libc) {
	unique_lock lock(libc->fdMutex);
	for (int fd = 0; fd < 3; ++fd) {
		libc->fdDispatch->bits.set(fd);
		auto &slot = libc->fdPages[0]->fds[fd];
		slot.ops = &libc->fdFileOps; // table filled by load_file_fd_ops() next
		slot.handle = __el0_handle(fd);
		slot.flags = (fd == __libc::STDIN_FD) ? __SPRT_O_RDONLY : __SPRT_O_WRONLY;
		slot.mode = 0;
	}
	lock.unlock();

	// Embox has no user model at all -- no uid/gid on a task, nothing for
	// setuid to change. Reporting root is the honest answer for a system where
	// everything runs with the same (total) authority; reporting anything else
	// would suggest a check that does not exist.
	libc->euid = 0;
	libc->egid = 0;
	libc->isAppContainer = false;
}

} // namespace sprt
