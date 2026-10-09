// Files of a Tau module through libtau (sprt_tau.h): descriptors of open files and
// directories, paths from a descriptor or the working directory, and stat.

#ifndef RUNTIME_LIBC_IMPL_SRC_TAU_TAU_FS_H_
#define RUNTIME_LIBC_IMPL_SRC_TAU_TAU_FS_H_

#include "../../include/__impl_libc.h"

#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static constexpr size_t TAU_PATH_MAX = 4'096;

// The handle of an fd of a file or directory.
struct __tau_fd {
	__sprt_tau_file *file;
	off64_t pos;
	unsigned flags; // __SPRT_TAU_FS_*
};

const __fd_ops *__tau_fs_ops();
// The fds of sockets (socket.cc); the handle's first field is its __sprt_tau_sock.
const __fd_ops *__tau_sock_ops();

// -1 with errno for a negative result of libtau, the result otherwise.
long __tau_ret(long r);

bool __tau_path_ok(const char *path);

// Where `path` starts: an open directory, or the namespace root with a relative path made
// absolute from the working directory into `buf`. False with errno set.
bool __tau_at(int dirfd, const char *path, char *buf, __sprt_tau_file **dir, const char **out);

unsigned __tau_open_flags(int oflags);
void __tau_to_stat(const __sprt_tau_fs_stat &in, struct __SPRT_STAT_NAME *out);

// An fd of `ops` for `path`; -1 with errno set.
int __tau_open_fd(int dirfd, const char *path, int oflags, mode_t mode, const __fd_ops *ops);

} // namespace sprt

#endif // RUNTIME_LIBC_IMPL_SRC_TAU_TAU_FS_H_
