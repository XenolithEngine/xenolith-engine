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

// dma-buf and sync_file descriptors of a Tau module (xenolith-tau RT-A-DmaBuf-1), made by the
// runtime over the GPU of libtau (sprt_tau.h). A dma-buf holds a buffer of a BO: dup and close
// count it, mmap gives its bytes in the heap's mapping, lseek(SEEK_END) its size. A sync_file
// holds fence points of the session: poll() says POLLIN once every point is reached, POLLERR
// with it when the session or a context failed. Calls answer -1 with errno.

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_GPUFD_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_GPUFD_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

__SPRT_BEGIN_DECL

// A dma-buf descriptor taking the reference `buf`; O_CLOEXEC is the only flag.
SPRT_API int __sprt_dmabuf_fd(struct __sprt_tau_gpu_buf *buf, int flags);
// The buffer of a dma-buf descriptor, still the descriptor's; NULL with EBADF or EINVAL.
SPRT_API struct __sprt_tau_gpu_buf *__sprt_dmabuf_buf(int fd);

// A sync_file of the points; none is one already signaled.
SPRT_API int __sprt_syncfile_fd(const struct __sprt_tau_gpu_point *points, unsigned count);
// The points of a sync_file, up to `max` of them into `out`: their count.
SPRT_API int __sprt_syncfile_points(int fd, struct __sprt_tau_gpu_point *out, unsigned max);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_TAU_SPRT_GPUFD_H_
