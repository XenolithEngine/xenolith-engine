// Embox EL0 pipes and poll: pipe2(59) and ppoll(73).
//
// Both are new here rather than moved: before M2 this libc had no pipe and no
// poll of any kind, so there is nothing being replaced and no stub to delete.
//
// WHAT THE TWO fd NUMBERS ARE. The libc's descriptors are its own -- create_fd()
// hands out the lowest free slot -- and are NOT the kernel's. Everything in this
// file therefore translates in both directions, and nothing passes a libc number
// to a syscall. That is the same rule the rest of this backend follows; it
// matters more here because poll takes an ARRAY of them, and a single untranslated
// entry would not fail, it would watch somebody else's descriptor.
//
// WHAT THE KERNEL DOES THAT THIS CANNOT SEE. Embox numbers POLLOUT and POLLPRI
// the other way round from Linux, and the dispatcher translates both directions
// through a generated table. So the constants used here are the ABI's, which are
// Linux's, and they mean what they say.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_poll.h>

#include "../../../core/include/__el0_syscall.h"

namespace sprt {

void *__el0_handle(int kfd);        // libc_file_ops.cc
int __el0_kfd(const __fd_slot *fp); // libc_file_ops.cc

// The kernel refuses more than this in one call, because Embox's descriptor
// table has that many slots and nothing can usefully watch more.
static constexpr unsigned long EL0_POLL_MAX = 64;

} // namespace sprt

__SPRT_C_FUNC int pipe2(int fds[2], int flags) __SPRT_NOEXCEPT {
	using namespace sprt;

	if (!fds) {
		__sprt_errno = EFAULT;
		return -1;
	}

	int kfds[2] = {-1, -1};
	if (__el0_ret(__el0_pipe2(kfds, flags)) < 0) {
		return -1;
	}

	auto libc = __libc::get();
	uint32_t slotFlags = (uint32_t)(flags & (__SPRT_O_CLOEXEC | __SPRT_O_NONBLOCK));

	int r = libc->create_fd(__el0_handle(kfds[0]), &libc->fdFileOps,
			slotFlags | __SPRT_O_RDONLY, 0);
	if (r < 0) {
		__el0_close(kfds[0]);
		__el0_close(kfds[1]);
		__sprt_errno = EMFILE;
		return -1;
	}

	int w = libc->create_fd(__el0_handle(kfds[1]), &libc->fdFileOps,
			slotFlags | __SPRT_O_WRONLY, 0);
	if (w < 0) {
		// The read end already has a slot; releasing it through its own ops is
		// what closes the kernel descriptor with it.
		auto slot = libc->get_fd_slot(r);
		slot->ops->fo_close(slot);
		libc->release_fd(r);
		__el0_close(kfds[1]);
		__sprt_errno = EMFILE;
		return -1;
	}

	fds[0] = r;
	fds[1] = w;
	return 0;
}

__SPRT_C_FUNC int pipe(int fds[2]) __SPRT_NOEXCEPT { return pipe2(fds, 0); }

__SPRT_C_FUNC int ppoll(struct __SPRT_POLLFD_NAME *fds, __SPRT_ID(nfds_t) nfds,
		const struct __SPRT_TIMESPEC_NAME *timeout,
		const __SPRT_ID(sigset_t) * sigmask) __SPRT_NOEXCEPT {
	using namespace sprt;

	if (nfds > EL0_POLL_MAX) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (sigmask) {
		// There are no signals on this system, so the mask swap ppoll exists
		// for cannot happen. The kernel refuses it too; refusing here as well
		// saves the trap and says the same thing.
		__sprt_errno = EINVAL;
		return -1;
	}
	if (nfds && !fds) {
		__sprt_errno = EFAULT;
		return -1;
	}

	struct __SPRT_POLLFD_NAME wire[EL0_POLL_MAX];
	auto libc = __libc::get();

	for (__SPRT_ID(nfds_t) i = 0; i < nfds; ++i) {
		fds[i].revents = 0;
		wire[i].events = fds[i].events;
		wire[i].revents = 0;

		if (fds[i].fd < 0) {
			wire[i].fd = -1; // ignored on both sides, as POSIX says
			continue;
		}

		auto slot = libc->get_fd_slot(fds[i].fd);
		if (!slot || !slot->handle) {
			// A libc descriptor the kernel has never heard of. Answering here
			// is the only way: handing the kernel -1 would have it ignored, and
			// the caller would wait on a descriptor that does not exist.
			wire[i].fd = -1;
			fds[i].revents = __SPRT_POLLNVAL;
			continue;
		}
		wire[i].fd = __el0_kfd(slot);
	}

	auto ret = (int)__el0_ret(__el0_ppoll(wire, (unsigned long)nfds, timeout, nullptr));
	if (ret < 0) {
		return -1;
	}

	// Count again rather than trusting the kernel's number: the entries this
	// side flagged POLLNVAL were never shown to it.
	int ready = 0;
	for (__SPRT_ID(nfds_t) i = 0; i < nfds; ++i) {
		if (wire[i].fd >= 0) {
			fds[i].revents = wire[i].revents;
		}
		if (fds[i].revents) {
			++ready;
		}
	}
	return ready;
}

__SPRT_C_FUNC int poll(struct __SPRT_POLLFD_NAME *fds, __SPRT_ID(nfds_t) nfds,
		int timeout) __SPRT_NOEXCEPT {
	struct __SPRT_TIMESPEC_NAME ts;

	if (timeout < 0) {
		return ppoll(fds, nfds, nullptr, nullptr);
	}
	ts.tv_sec = (__SPRT_ID(time_t))(timeout / 1'000);
	ts.tv_nsec = (long)(timeout % 1'000) * 1'000'000;
	return ppoll(fds, nfds, &ts, nullptr);
}
