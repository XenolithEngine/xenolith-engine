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

#ifndef XENOLITH_REMOTE_XLREMOTESHMEMBOX_H_
#define XENOLITH_REMOTE_XLREMOTESHMEMBOX_H_

/* The contract between the `shm:` transport and the Embox kernel. Plain C: the kernel side is a C
 * module in the Embox tree, the engine side is XLRemoteShmProviderEmbox.cc.
 *
 * Blocks. A block is the ShmBlockHeader layout from XLRemoteShmBlock.h: a 128-byte header and four
 * rings, formatted by the client, validated by the server. The kernel does not interpret it beyond
 * the two fields below, which it writes when a task dies.
 *
 * The window server runs in EL1 and reaches the kernel through xl_shm_kernel_ops, which the kernel
 * module registers at init with xl_remote_shm_set_kernel_ops. Clients run in EL0 and use /dev/wm:
 *
 *   1. open("/dev/wm", O_RDWR)
 *   2. ioctl(fd, XL_WM_IOC_CONNECT, &req)  -- req.name and req.size in; the kernel allocates
 *      req.size bytes of zeroed pages (it may round up and writes the final size back) and fills
 *      req.server_task; ENOENT when no listener has that name
 *   3. mmap(NULL, req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)
 *   4. format the block
 *   5. ioctl(fd, XL_WM_IOC_POST, 0)         -- the kernel queues the block for the listener and
 *      increments its doorbell (then wakes it)
 *
 * The block stays alive while either side still maps it: the client until munmap and close, the
 * server until xl_shm_kernel_ops.release.
 *
 * Death. When a task holding one side ends, the kernel sets that side's bit in
 * header.closed (bit 0 server, bit 1 client, atomic OR at XL_SHM_HEADER_CLOSED_OFFSET), increments
 * the other side's doorbell (4-byte words at XL_SHM_HEADER_DOORBELL_OFFSET, index 0 server, 1
 * client) and wakes it. Nothing on the data path makes a system call.
 *
 * Kernel prerequisites: mmap of a device descriptor currently takes the physical range from
 * FBIOGET_FSCREENINFO (docs/EMBOX-SYSCALL-ABI.md in xenolith-os); /dev/wm needs the generic path.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XL_SHM_KERNEL_ABI_VERSION 1

#define XL_SHM_HEADER_CLOSED_OFFSET 48
#define XL_SHM_HEADER_DOORBELL_OFFSET 52

/* A block a client posted, as the server receives it. */
struct xl_shm_accept {
	void *block; /* kernel address of the whole block */
	size_t size;
	void *conn; /* the kernel's handle, passed back to release */
	int64_t peer_task; /* the client task */
};

struct xl_shm_kernel_ops {
	uint32_t abi_version; /* XL_SHM_KERNEL_ABI_VERSION */

	/* Bind `name`; blocks larger than max_block_size are refused at CONNECT. NULL on failure. */
	void *(*listen)(const char *name, size_t max_block_size);
	void (*close_listener)(void *listener);

	/* The word a client's POST increments; stable for the listener's lifetime. */
	uint32_t *(*listener_doorbell)(void *listener);

	/* Take one posted block: 0 when `out` is filled, -EAGAIN when nothing is pending. */
	int (*accept)(void *listener, struct xl_shm_accept *out);

	/* The server no longer maps the block. */
	void (*release)(void *conn);
};

/* Called by the kernel module; NULL withdraws the provider. A table with another abi_version is
 * refused with a log message. */
// clang-format off
__attribute__((visibility("default")))
void xl_remote_shm_set_kernel_ops(const struct xl_shm_kernel_ops *ops);
// clang-format on

struct xl_wm_connect {
	char name[64]; /* listener name, NUL-terminated */
	uint64_t size; /* in: requested block size; out: the size actually allocated */
	int64_t server_task; /* out */
};

#define XL_WM_CONNECT_SIZE 80

/* Linux-style encoding: _IOWR('W', 1, struct xl_wm_connect) and _IO('W', 2). Plain C: no digit
 * separators here. */
// clang-format off
#define XL_WM_IOC_CONNECT 0xC0505701u
#define XL_WM_IOC_POST 0x00005702u
// clang-format on

#ifdef __cplusplus
}
#endif

#endif /* XENOLITH_REMOTE_XLREMOTESHMEMBOX_H_ */
