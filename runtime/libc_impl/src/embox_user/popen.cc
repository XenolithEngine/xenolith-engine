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
// Embox EL0 popen/system backend.
//
// Not "not yet": decision D5 fixes the application as a single static ET_EXEC
// with no loader and no second process, so there is nothing for fork/exec to do
// and no shell to hand a command line to. These fail with ENOSYS permanently.

#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"
#include "fcntl.h"
#include "unistd.h"

namespace sprt {

__SPRT_C_FUNC FILE *popen(const char *cmd, const char *mode) __SPRT_NOEXCEPT {
	errno = ENOSYS;
	return 0;
}

__SPRT_C_FUNC int pclose(FILE *f) __SPRT_NOEXCEPT {
	errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int system(const char *cmd) __SPRT_NOEXCEPT {
	errno = ENOSYS;
	return -1;
}

} // namespace sprt
