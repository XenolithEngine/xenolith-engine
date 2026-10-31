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

// The floating-point environment: musl's arch independent part (src/fenv/*.c), on the
// FE_* values and fenv_t of sprt's <fenv.h> for this platform. The primitives are
// the assembly in asm/<arch>/sprt_fs_fenv_<arch>.S, or the stubs below.

#include "../include/sprt_fs_config.h"
#include <sprt/gen/c/cross/__sprt_fenv_t.h>

#define __SPRT_FS_FENV_DEF __SPRT_C_FUNC

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feclearexcept)(int);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feraiseexcept)(int);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fetestexcept)(int);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fegetround)(void);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(__fesetround)(int);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fegetenv)(__sprt_fenv_t *);
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fesetenv)(const __sprt_fenv_t *);

#if SPRT_FS_PROVIDE_FENV_PRIMITIVES && !SPRT_FS_ASM_FENV
// musl's src/fenv/fenv.c: a machine without an FP environment to control.
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feclearexcept)(int) { return 0; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feraiseexcept)(int) { return 0; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fetestexcept)(int) { return 0; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fegetround)(void) { return __SPRT_FE_TONEAREST; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(__fesetround)(int) { return 0; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fegetenv)(__sprt_fenv_t *) { return 0; }

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fesetenv)(const __sprt_fenv_t *) { return 0; }
#endif

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fegetexceptflag)(__sprt_fexcept_t *fp, int mask) {
	*fp = __sprt_fexcept_t(SPRT_FS_NAME(fetestexcept)(mask));
	return 0;
}

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fesetexceptflag)(const __sprt_fexcept_t *fp, int mask) {
	SPRT_FS_NAME(feclearexcept)(~int(*fp) & mask);
	SPRT_FS_NAME(feraiseexcept)(int(*fp) & mask);
	return 0;
}

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feholdexcept)(__sprt_fenv_t *envp) {
	SPRT_FS_NAME(fegetenv)(envp);
	SPRT_FS_NAME(feclearexcept)(__SPRT_FE_ALL_EXCEPT);
	return 0;
}

__SPRT_FS_FENV_DEF int SPRT_FS_NAME(feupdateenv)(const __sprt_fenv_t *envp) {
	int ex = SPRT_FS_NAME(fetestexcept)(__SPRT_FE_ALL_EXCEPT);
	SPRT_FS_NAME(fesetenv)(envp);
	SPRT_FS_NAME(feraiseexcept)(ex);
	return 0;
}

// The argument check is arch independent; __fesetround only sets the mode.
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(fesetround)(int r) {
	if (r != __SPRT_FE_TONEAREST
#ifdef __SPRT_FE_DOWNWARD
			&& r != __SPRT_FE_DOWNWARD
#endif
#ifdef __SPRT_FE_UPWARD
			&& r != __SPRT_FE_UPWARD
#endif
#ifdef __SPRT_FE_TOWARDZERO
			&& r != __SPRT_FE_TOWARDZERO
#endif
	) {
		return -1;
	}
	return SPRT_FS_NAME(__fesetround)(r);
}

// FLT_ROUNDS: 0 toward zero, 1 to nearest, 2 upward, 3 downward, -1 otherwise.
__SPRT_FS_FENV_DEF int SPRT_FS_NAME(__flt_rounds)(void) {
	switch (SPRT_FS_NAME(fegetround)()) {
#ifdef __SPRT_FE_TOWARDZERO
	case __SPRT_FE_TOWARDZERO: return 0;
#endif
	case __SPRT_FE_TONEAREST: return 1;
#ifdef __SPRT_FE_UPWARD
	case __SPRT_FE_UPWARD: return 2;
#endif
#ifdef __SPRT_FE_DOWNWARD
	case __SPRT_FE_DOWNWARD: return 3;
#endif
	}
	return -1;
}
