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

#include "lfs.h"

#include <dlfcn.h>
#include <fenv.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// The floating-point environment of the module, held to the hardware: flags raised by
// real operations are what fetestexcept reports, flags and modes it sets are what
// the host libc reads back, and a rounding mode changes the result of a division.

extern "C" {
int __lfs_fe_feclearexcept(int);
int __lfs_fe_feraiseexcept(int);
int __lfs_fe_fetestexcept(int);
int __lfs_fe_fegetround(void);
int __lfs_fe_fesetround(int);
int __lfs_fe_fegetenv(fenv_t *);
int __lfs_fe_fesetenv(const fenv_t *);
int __lfs_fe_feholdexcept(fenv_t *);
int __lfs_fe_feupdateenv(const fenv_t *);
int __lfs_fe_fegetexceptflag(fexcept_t *, int);
int __lfs_fe_fesetexceptflag(const fexcept_t *, int);
int __lfs_fe___flt_rounds(void);
}

namespace lfs {

namespace {

struct FenvImpl {
	const char *name;
	bool exact; // raising a flag raises that flag only
	const fenv_t *dfl;
	int (*clear)(int);
	int (*raise)(int);
	int (*test)(int);
	int (*getround)(void);
	int (*setround)(int);
	int (*getenv)(fenv_t *);
	int (*setenv)(const fenv_t *);
	int (*hold)(fenv_t *);
	int (*update)(const fenv_t *);
	int (*getflag)(fexcept_t *, int);
	int (*setflag)(const fexcept_t *, int);
	int (*fltRounds)(void);
};

// The host libc, for the cross-checks; looked up by name past the sprt wrappers.
struct HostFenv {
	int (*clear)(int);
	int (*raise)(int);
	int (*test)(int);
	int (*getround)(void);
	int (*setround)(int);
};

template <typename T>
void hostSym(T &fn, const char *name) {
	fn = reinterpret_cast<T>(dlsym(RTLD_DEFAULT, name));
}

HostFenv makeHostFenv() {
	HostFenv ret;
	hostSym(ret.clear, "feclearexcept");
	hostSym(ret.raise, "feraiseexcept");
	hostSym(ret.test, "fetestexcept");
	hostSym(ret.getround, "fegetround");
	hostSym(ret.setround, "fesetround");
	return ret;
}

FenvImpl makeFsFenv() {
	return FenvImpl{"fs", true, reinterpret_cast<const fenv_t *>(-1L), __lfs_fe_feclearexcept,
		__lfs_fe_feraiseexcept, __lfs_fe_fetestexcept, __lfs_fe_fegetround, __lfs_fe_fesetround,
		__lfs_fe_fegetenv, __lfs_fe_fesetenv, __lfs_fe_feholdexcept, __lfs_fe_feupdateenv,
		__lfs_fe_fegetexceptflag, __lfs_fe_fesetexceptflag, __lfs_fe___flt_rounds};
}

FenvImpl makeHostImpl() {
	FenvImpl ret{"host", false, FE_DFL_ENV};
	hostSym(ret.clear, "feclearexcept");
	hostSym(ret.raise, "feraiseexcept");
	hostSym(ret.test, "fetestexcept");
	hostSym(ret.getround, "fegetround");
	hostSym(ret.setround, "fesetround");
	hostSym(ret.getenv, "fegetenv");
	hostSym(ret.setenv, "fesetenv");
	hostSym(ret.hold, "feholdexcept");
	hostSym(ret.update, "feupdateenv");
	hostSym(ret.getflag, "fegetexceptflag");
	hostSym(ret.setflag, "fesetexceptflag");
	return ret;
}

struct FenvCtx {
	const FenvImpl *impl;
	uint64_t checks = 0;
	uint64_t failures = 0;

	void expect(bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4))) {
		++checks;
		if (!ok) {
			if (failures < 10) {
				printf("FAIL: %s fenv: ", impl->name);
				va_list ap;
				va_start(ap, fmt);
				vprintf(fmt, ap);
				va_end(ap);
				printf("\n");
			}
			++failures;
		}
	}
};

const int s_excepts[] = {FE_INVALID, FE_DIVBYZERO, FE_OVERFLOW, FE_UNDERFLOW, FE_INEXACT};
const char *s_exceptNames[] = {"INVALID", "DIVBYZERO", "OVERFLOW", "UNDERFLOW", "INEXACT"};

const int s_modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
const int s_fltRounds[] = {1, 3, 2, 0};

volatile double s_one = 1.0, s_three = 3.0, s_zero = 0.0, s_big = 1e308, s_tiny = 1e-308;
volatile double s_sink;

// Raises exactly one exception by arithmetic (OVERFLOW and UNDERFLOW also raise INEXACT).
void raiseByArithmetic(int i) {
	switch (i) {
	case 0: s_sink = s_zero / s_zero; break;
	case 1: s_sink = s_one / s_zero; break;
	case 2: s_sink = s_big * s_big; break;
	case 3: s_sink = s_tiny * s_tiny; break;
	case 4: s_sink = s_one / s_three; break;
	}
}

void testFlags(FenvCtx &ctx, const HostFenv &host) {
	auto &f = *ctx.impl;
	for (int set = 0; set < 32; ++set) {
		int ex = 0;
		for (int i = 0; i < 5; ++i) {
			if (set & (1 << i)) {
				ex |= s_excepts[i];
			}
		}
		ctx.expect(f.clear(FE_ALL_EXCEPT) == 0 && f.test(FE_ALL_EXCEPT) == 0,
				"set %d: flags remain after feclearexcept", set);
		ctx.expect(f.raise(ex) == 0, "set %d: feraiseexcept failed", set);
		auto got = f.test(FE_ALL_EXCEPT);
		ctx.expect(f.exact ? got == ex : (got & ex) == ex, "set %d: raised %#x, reads %#x", set,
				ex, got);
		ctx.expect(f.test(ex) == (got & ex), "set %d: fetestexcept(mask) %#x", set, f.test(ex));
		if (host.test && f.exact) {
			ctx.expect(host.test(FE_ALL_EXCEPT) == ex, "set %d: the host reads %#x", set,
					host.test(FE_ALL_EXCEPT));
		}
		f.clear(ex);
		ctx.expect(f.test(FE_ALL_EXCEPT) == (got & ~ex), "set %d: feclearexcept(%#x) left %#x",
				set, ex, f.test(FE_ALL_EXCEPT));
	}
	for (int i = 0; i < 5; ++i) {
		f.clear(FE_ALL_EXCEPT);
		raiseByArithmetic(i);
		ctx.expect(f.test(s_excepts[i]) == s_excepts[i], "%s by arithmetic is not seen",
				s_exceptNames[i]);
		if (host.raise) {
			f.clear(FE_ALL_EXCEPT);
			host.raise(s_excepts[i]);
			ctx.expect(f.test(s_excepts[i]) == s_excepts[i], "%s raised by the host is not seen",
					s_exceptNames[i]);
		}
	}
	f.clear(FE_ALL_EXCEPT);
}

void testRounding(FenvCtx &ctx, const HostFenv &host) {
	auto &f = *ctx.impl;
	double q[4], nq[4];
	for (int m = 0; m < 4; ++m) {
		ctx.expect(f.setround(s_modes[m]) == 0, "fesetround(%#x) failed", s_modes[m]);
		ctx.expect(f.getround() == s_modes[m], "mode %#x reads %#x", s_modes[m], f.getround());
		if (host.getround) {
			ctx.expect(host.getround() == s_modes[m], "mode %#x: the host reads %#x", s_modes[m],
					host.getround());
		}
		if (f.fltRounds) {
			ctx.expect(f.fltRounds() == s_fltRounds[m], "mode %#x: __flt_rounds %d", s_modes[m],
					f.fltRounds());
		}
		q[m] = s_one / s_three;
		nq[m] = -s_one / s_three;
	}
	f.setround(FE_TONEAREST);
	// 1/3 is not representable: the four modes round it the four ways
	ctx.expect(q[2] > q[1], "upward %a is not above downward %a", q[2], q[1]);
	ctx.expect(q[0] == q[1] || q[0] == q[2], "to nearest %a is neither", q[0]);
	ctx.expect(q[3] == q[1], "toward zero %a is not downward %a for a positive", q[3], q[1]);
	ctx.expect(nq[3] == nq[2] && nq[2] == -q[1] && nq[1] == -q[2],
			"the negative quotient rounds wrong");

	if (host.setround) {
		host.setround(FE_UPWARD);
		ctx.expect(f.getround() == FE_UPWARD, "the host's FE_UPWARD reads %#x", f.getround());
		host.setround(FE_TONEAREST);
	}
	const int bad[] = {0x12345, -1, FE_UPWARD | 0x10000000};
	for (auto r : bad) {
		ctx.expect(f.setround(r) != 0 && f.getround() == FE_TONEAREST, "fesetround(%#x) accepted",
				r);
	}
}

void testEnv(FenvCtx &ctx) {
	auto &f = *ctx.impl;
	fenv_t env;
	f.setround(FE_UPWARD);
	f.clear(FE_ALL_EXCEPT);
	f.raise(FE_OVERFLOW);
	ctx.expect(f.getenv(&env) == 0, "fegetenv failed");
	f.setround(FE_TONEAREST);
	f.clear(FE_ALL_EXCEPT);
	ctx.expect(f.setenv(&env) == 0, "fesetenv failed");
	ctx.expect(f.getround() == FE_UPWARD, "fesetenv: mode %#x", f.getround());
	ctx.expect((f.test(FE_ALL_EXCEPT) & FE_OVERFLOW) && (!f.exact || f.test(FE_ALL_EXCEPT) == FE_OVERFLOW),
			"fesetenv: flags %#x", f.test(FE_ALL_EXCEPT));

	ctx.expect(f.setenv(f.dfl) == 0, "fesetenv(FE_DFL_ENV) failed");
	ctx.expect(f.getround() == FE_TONEAREST && f.test(FE_ALL_EXCEPT) == 0,
			"FE_DFL_ENV: mode %#x, flags %#x", f.getround(), f.test(FE_ALL_EXCEPT));

	// feholdexcept keeps the flags and the mode aside; feupdateenv puts them back and
	// raises what was raised in between
	f.setround(FE_DOWNWARD);
	f.raise(FE_INEXACT);
	ctx.expect(f.hold(&env) == 0, "feholdexcept failed");
	ctx.expect(f.test(FE_ALL_EXCEPT) == 0, "feholdexcept left flags %#x", f.test(FE_ALL_EXCEPT));
	f.setround(FE_TONEAREST);
	f.raise(FE_DIVBYZERO);
	ctx.expect(f.update(&env) == 0, "feupdateenv failed");
	ctx.expect(f.getround() == FE_DOWNWARD, "feupdateenv: mode %#x", f.getround());
	auto flags = f.test(FE_ALL_EXCEPT);
	ctx.expect((flags & (FE_INEXACT | FE_DIVBYZERO)) == (FE_INEXACT | FE_DIVBYZERO)
					&& (!f.exact || flags == (FE_INEXACT | FE_DIVBYZERO)),
			"feupdateenv: flags %#x", flags);

	fexcept_t fx;
	f.setenv(f.dfl);
	f.raise(FE_INVALID | FE_OVERFLOW);
	ctx.expect(f.getflag(&fx, FE_ALL_EXCEPT) == 0, "fegetexceptflag failed");
	f.clear(FE_ALL_EXCEPT);
	ctx.expect(f.setflag(&fx, FE_OVERFLOW | FE_INEXACT) == 0, "fesetexceptflag failed");
	flags = f.test(FE_ALL_EXCEPT);
	ctx.expect(flags == FE_OVERFLOW || (!f.exact && (flags & (FE_OVERFLOW | FE_INVALID)) == FE_OVERFLOW),
			"fesetexceptflag: flags %#x", flags);
	f.setenv(f.dfl);
}

} // namespace

bool runFenvConformance(const Options &opts, uint64_t &checks, uint64_t &failures) {
	auto host = makeHostFenv();
	FenvImpl impls[] = {makeFsFenv(), makeHostImpl()};
	for (auto &im : impls) {
		if (opts.impl ? strcmp(opts.impl, im.name) != 0 : (!opts.full && !im.exact)) {
			continue;
		}
		if (!im.clear || !im.getenv) {
			continue;
		}
		FenvCtx ctx{&im};
		testFlags(ctx, host);
		testRounding(ctx, host);
		testEnv(ctx);
		printf("%-9s %-8s %10llu checks, %llu failures\n", im.name, "fenv",
				(unsigned long long)ctx.checks, (unsigned long long)ctx.failures);
		checks += ctx.checks;
		failures += ctx.failures;
	}
	return true;
}

} // namespace lfs
