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

// New std re-export headers <cfenv> and <csignal>. Both just lift the C
// <fenv.h> / <signal.h> surface into namespace std (the FE_* / SIG* macros stay
// global). Signal *numbers* and errno differ between glibc and the freestanding
// libc_impl, so everything printed here is either a symbolic name or a boolean
// so the sprt Linux and x86_64-pc-windows-msvc runs diff identically.

#include <stdio.h>

#include <cfenv>
#include <csignal>
#include <type_traits>

namespace sprt::test {

namespace {

const char *roundName(int m) {
	if (m == FE_TONEAREST) { return "TONEAREST"; }
	if (m == FE_DOWNWARD) { return "DOWNWARD"; }
	if (m == FE_UPWARD) { return "UPWARD"; }
	if (m == FE_TOWARDZERO) { return "TOWARDZERO"; }
	return "OTHER";
}

// std::signal handler used by the delivery round-trip below.
volatile std::sig_atomic_t g_signalHits = 0;
void onSignal(int) { g_signalHits += 1; }

} // namespace

void performFenvSignalTest() {
	// ---- <cfenv> -----------------------------------------------------------
	// std:: names resolve to the C functions/types.
	static_assert(std::is_same_v<std::fenv_t, ::fenv_t>);

	// Clear all exception flags, then confirm none are set.
	std::feclearexcept(FE_ALL_EXCEPT);
	printf("fenv: cleared=%d\n", (int) (std::fetestexcept(FE_ALL_EXCEPT) == 0));

	// Raise a couple of flags explicitly and test them back.
	std::feraiseexcept(FE_INVALID);
	printf("fenv: invalid_set=%d divbyzero_unset=%d\n",
			(int) (std::fetestexcept(FE_INVALID) != 0),
			(int) (std::fetestexcept(FE_DIVBYZERO) == 0));
	std::feclearexcept(FE_ALL_EXCEPT);

	// Rounding-mode round-trip. The default mode is FE_TONEAREST.
	printf("fenv: default_round=%s\n", roundName(std::fegetround()));
	int rc = std::fesetround(FE_DOWNWARD);
	printf("fenv: set_downward_ok=%d now=%s\n", (int) (rc == 0),
			roundName(std::fegetround()));
	std::fesetround(FE_UPWARD);
	printf("fenv: now=%s\n", roundName(std::fegetround()));
	std::fesetround(FE_TONEAREST); // restore

	// ---- <csignal> ---------------------------------------------------------
	// sig_atomic_t is an integral type; SIG_DFL / SIG_IGN / SIG_ERR are distinct.
	static_assert(std::is_integral_v<std::sig_atomic_t>);
	printf("signal: dfl_ne_ign=%d dfl_ne_err=%d\n", (int) (SIG_DFL != SIG_IGN),
			(int) (SIG_DFL != SIG_ERR));

	// Handler install round-trip: installing a handler returns the previous
	// disposition (SIG_DFL initially); reinstalling returns our handler.
	auto prev = std::signal(SIGABRT, &onSignal);
	printf("signal: install_prev_dfl=%d\n", (int) (prev != SIG_ERR && prev == SIG_DFL));

	// Deliver the signal to ourselves and confirm the handler ran.
	g_signalHits = 0;
	int rr = std::raise(SIGABRT);
	printf("signal: raise_ok=%d hits=%d\n", (int) (rr == 0), (int) g_signalHits);

	// Restore default disposition.
	std::signal(SIGABRT, SIG_DFL);
}

} // namespace sprt::test
