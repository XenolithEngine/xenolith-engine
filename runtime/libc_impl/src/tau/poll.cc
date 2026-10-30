// poll over the streams, sockets and GPU descriptors of a Tau module (sprt_tau.h, gpufd.h):
// readiness is asked of each, and the wait is on the module's I/O epoch, so nothing is held while
// sleeping. Files of the namespace are always ready. There are no pipes.

#include "tau_fs.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_poll.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static_assert(__SPRT_POLLIN == __SPRT_TAU_POLLIN && __SPRT_POLLOUT == __SPRT_TAU_POLLOUT
				&& __SPRT_POLLERR == __SPRT_TAU_POLLERR && __SPRT_POLLHUP == __SPRT_TAU_POLLHUP,
		"poll bits of sprt_tau.h are the libc's");

static int __tau_poll(struct __SPRT_POLLFD_NAME *fds, __SPRT_ID(nfds_t) n, unsigned long long timeoutNs) {
	auto libc = __libc::get();
	unsigned long long deadline = timeoutNs == __SPRT_TAU_TIMEOUT_INFINITE ? 0 : __sprt_tau_clock_ns() + timeoutNs;
	for (;;) {
		unsigned epoch = __sprt_tau_io_epoch();
		int ready = 0;
		for (__SPRT_ID(nfds_t) i = 0; i < n; ++i) {
			fds[i].revents = 0;
			if (fds[i].fd < 0) {
				continue;
			}
			auto slot = libc->get_fd_slot(fds[i].fd);
			if (!slot || !slot->handle) {
				fds[i].revents = __SPRT_POLLNVAL;
			} else if (slot->ops == &libc->fdFileOps) {
				fds[i].revents = short(__sprt_tau_stream_ready(static_cast<__sprt_tau_stream *>(slot->handle),
						unsigned(fds[i].events) | __SPRT_TAU_POLLERR | __SPRT_TAU_POLLHUP));
			} else if (slot->ops == __tau_fs_ops()) {
				fds[i].revents = short(fds[i].events & (__SPRT_POLLIN | __SPRT_POLLOUT));
			} else if (slot->ops == __tau_sock_ops()) {
				fds[i].revents = short(__sprt_tau_sock_ready(*static_cast<__sprt_tau_sock **>(slot->handle),
						unsigned(fds[i].events) | __SPRT_TAU_POLLERR | __SPRT_TAU_POLLHUP));
			} else {
				int r = __tau_gpufd_ready(slot, fds[i].events);
				fds[i].revents = short(r < 0 ? 0 : r);
			}
			ready += fds[i].revents ? 1 : 0;
		}
		if (ready) {
			return ready;
		}
		unsigned long long now = __sprt_tau_clock_ns();
		if (timeoutNs == 0 || (deadline && now >= deadline)) {
			return 0;
		}
		__sprt_tau_io_wait(epoch, deadline ? deadline - now : __SPRT_TAU_TIMEOUT_INFINITE);
	}
}

} // namespace sprt

__SPRT_C_FUNC int poll(struct __SPRT_POLLFD_NAME *fds, __SPRT_ID(nfds_t) n, int timeout) __SPRT_NOEXCEPT {
	if (!fds && n) {
		__sprt_errno = EFAULT;
		return -1;
	}
	return sprt::__tau_poll(fds, n,
			timeout < 0 ? __SPRT_TAU_TIMEOUT_INFINITE : (unsigned long long)timeout * 1'000'000ull);
}

__SPRT_C_FUNC int ppoll(struct __SPRT_POLLFD_NAME *fds, __SPRT_ID(nfds_t) n, const struct __SPRT_TIMESPEC_NAME *ts,
		const __SPRT_ID(sigset_t) *) __SPRT_NOEXCEPT {
	if (!fds && n) {
		__sprt_errno = EFAULT;
		return -1;
	}
	return sprt::__tau_poll(fds, n,
			ts ? (unsigned long long)ts->tv_sec * 1'000'000'000ull + (unsigned long long)ts->tv_nsec
			   : __SPRT_TAU_TIMEOUT_INFINITE);
}

__SPRT_C_FUNC int pipe2(int *, int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int pipe(int *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}
