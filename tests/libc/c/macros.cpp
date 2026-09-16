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

#include <signal.h>
#include <fenv.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stddef.h>

namespace sprt::test {

// These ISO-required macros/types were previously unreachable through the
// freestanding headers. The test mostly verifies they *compile* (a missing one
// would fail to build on the affected target); the printed values are restricted
// to facts that are identical on both targets (impl-defined numeric values such
// as the FE_* bit assignments or WCHAR_MAX width are deliberately not printed).
void performMacrosTest() {
	// <signal.h>: SIG_DFL/SIG_IGN/SIG_ERR usable as dispositions; sig_atomic_t.
	void (*disp)(int) = SIG_DFL;
	disp = SIG_IGN;
	bool sigErrOk = (SIG_ERR != disp); // SIG_IGN != SIG_ERR
	volatile sig_atomic_t sa = 1;
	printf("signal: SIG_ERR!=SIG_IGN=%d sig_atomic_t=%d\n", sigErrOk ? 1 : 0, (int)sa);

	// <fenv.h>: the five exception flags combine into a nonzero mask within
	// FE_ALL_EXCEPT; FE_DFL_ENV is a valid (non-null) environment pointer.
	int feMask = FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW;
	printf("fenv: flags_in_all=%d dfl_env_set=%d\n",
			(feMask & FE_ALL_EXCEPT) == feMask ? 1 : 0, FE_DFL_ENV != nullptr ? 1 : 0);

	// <math.h>: MATH_ERRNO/MATH_ERREXCEPT are fixed by the standard (1/2);
	// math_errhandling is some subset of them.
	printf("math: MATH_ERRNO=%d MATH_ERREXCEPT=%d errhandling_subset=%d\n", MATH_ERRNO,
			MATH_ERREXCEPT, (math_errhandling & (MATH_ERRNO | MATH_ERREXCEPT)) == math_errhandling ? 1 : 0);

	// <wchar.h>/<wctype.h>: WEOF, WCHAR_MIN/MAX present and sane.
	printf("wide: WEOF_nonzero=%d WCHAR_MAX_pos=%d WCHAR_MIN_nonpos=%d\n", WEOF != 0 ? 1 : 0,
			WCHAR_MAX > 0 ? 1 : 0, WCHAR_MIN <= 0 ? 1 : 0);

	// <stdio.h>: FOPEN_MAX/TMP_MAX meet the ISO minimums; fpos_t is a usable type.
	fpos_t pos;
	(void)pos;
	printf("stdio: FOPEN_MAX_min=%d TMP_MAX_min=%d\n", FOPEN_MAX >= 8 ? 1 : 0, TMP_MAX >= 25 ? 1 : 0);

	// NULL reachable through these headers.
	printf("NULL_ok=%d\n", NULL == (void *)0 ? 1 : 0);
}

} // namespace sprt::test
