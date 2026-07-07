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

// WebAssembly libc bring-up: wire fd 0/1/2 to the console backend and set the
// effective ids. There is no OS identity in a sandbox, so euid/egid are 0.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/cxx/mutex>

namespace sprt {

// Defined in libc_file_ops.cc: the static console node for fd 0/1/2. The file
// ops recognise it and route reads/writes to the fd_read/fd_write host imports.
void *__wasm_console_handle(int fd);

void __init_default_fds(__libc *libc) {
	unique_lock lock(libc->fdMutex);
	for (int fd = 0; fd < 3; ++fd) {
		libc->fdDispatch->bits.set(fd);
		auto &slot = libc->fdPages[0]->fds[fd];
		slot.ops = &libc->fdFileOps; // table filled by load_file_fd_ops() next
		slot.handle = __wasm_console_handle(fd);
		slot.flags = (fd == __libc::STDIN_FD) ? __SPRT_O_RDONLY : __SPRT_O_WRONLY;
		slot.mode = 0;
	}
	lock.unlock();

	libc->euid = 0;
	libc->egid = 0;
	libc->isAppContainer = false;
}

} // namespace sprt
