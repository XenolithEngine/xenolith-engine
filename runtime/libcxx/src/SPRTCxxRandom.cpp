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

// Out-of-line translation unit for the vendored libc++ <random> port. It compiles the
// verbatim upstream libcxx/random.cpp, which implements the std::random_device backend
// (the engines and distributions are header-only). Upstream selects the backend from a
// _LIBCPP_USING_* macro that its own platform configuration would set; the freestanding
// port config does not, so pick the getentropy backend here — sprt's libc supplies
// getentropy (and <sys/random.h>), and unlike the /dev/urandom path it needs no ioctl
// (sprt has no __sprt_ioctl syscall wrapper for this target). Other backends stay off.
//
// Hosted-style libc++ code (pulls the full STL layer); the libcxx module compiles it
// WITHOUT __SPRT_BUILD (see libcxx.mk).

#define _LIBCPP_USING_GETENTROPY 1

// random.cpp's random_device reports open/read failures via std::__throw_system_error,
// a libc++ internal sprt does not provide. Supply it before the vendored body. Under the
// port's -fno-exceptions build it aborts (mirroring libc++'s no-exceptions path); the
// exceptions branch is kept for faithfulness should the port ever enable them.
#include <__config>
#include <__verbose_abort>
#if _LIBCPP_HAS_EXCEPTIONS
#  include <system_error>
#endif

// sprt's libc exposes getrandom() but not POSIX getentropy(), which the vendored
// random_device getentropy backend calls. Provide the thin standard wrapper (getentropy
// returns 0 on success / -1 on error; getrandom returns the byte count / -1).
#include <sys/random.h>
static inline int getentropy(void *__b, size_t __n) {
	return getrandom(__b, __n, 0u) == static_cast<ssize_t>(__n) ? 0 : -1;
}

// __throw_system_error(int, const char*) is declared _LIBCPP_EXPORTED_FROM_ABI and
// defined out-of-line by the vendored system_error.cpp (built as SPRTCxxSystemError.cpp.o,
// always linked alongside this TU in every build config). An earlier local `inline`
// definition here for random's calls collided with that canonical one: tolerated on ELF
// (inline => weak, folded with the strong def) but a hard duplicate-symbol on COFF, where
// a comdat inline cannot coexist with a regular external definition. Reference the
// canonical symbol instead.
#include "libcxx/random.cpp"
