# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Generate the per-arch NuttX sysroot glue: a toolchain.cmake for building the
# LLVM runtimes against NuttX's libc, the app-facing target.mk descriptor, and
# the symlink to the shared host toolchain (clang/lld/llvm-ar).
#
# This is the NuttX-hosted counterpart of target-wasm/init-target.mk: instead
# of freestanding + sprt libc, we point clang at the NuttX headers/libs that
# import-export.mk just laid out under sysroot/usr. NuttX defines __NuttX__
# at build time, which our runtime detects (runtime/include/sprt/c/bits/__sprt_def.h).

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

OUT := $(TOOLCHAIN_OUTPUT_DIR)
SYSROOT := $(OUT)/sysroot
ARCH_FLAGS_FILE := $(OUT)/nuttx-arch-flags.mk
CONFIG_FILE := $(OUT)/nuttx-config.mk

# Pull the per-board flags extracted by import-export.mk.
-include $(ARCH_FLAGS_FILE)
-include $(CONFIG_FILE)

# Clang --target triple (no "nuttx" OS — LLVM does not know it). The arch flags
# (-mcpu/-march/-mfpu/-mabi for the specific board) ride on top of this triple.
NUTTX_TARGET := $(SP_ARCH_TARGET_CLANG)

# Build a single combined CFLAGS string for the toolchain file:
#   --target=<baremetal triple>  +  NuttX's per-board -mcpu/-march/... flags
#   -isystem <sysroot>/usr/include  so the NuttX libc/pthread headers resolve
#   -D__NuttX__                    the runtime's platform-detector predicate
NUTTX_CFLAGS := --target=$(NUTTX_TARGET) \
	$(NUTTX_ARCHCPUFLAGS) $(NUTTX_ARCHCFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__NuttX__

NUTTX_CXXFLAGS := --target=$(NUTTX_TARGET) \
	$(NUTTX_ARCHCPUFLAGS) $(NUTTX_ARCHCXXFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__NuttX__

# Linker flags for runtime try-compiles: point at the NuttX libs + startup
# objects and bring in libc/libm/libpthread so feature probes link cleanly.
NUTTX_LDFLAGS := --target=$(NUTTX_TARGET) \
	-L$(SYSROOT)/usr/lib \
	$(NUTTX_LDELFFLAGS) \
	-lc -lm

# --- toolchain.cmake ------------------------------------------------------
# CMAKE_SYSTEM_NAME Generic (baremetal): LLVM sets LLVM_ON_UNIX=0, which pairs
# with the COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL flags the runtimes need.
# NuttX's libc is a hosted libc, but it is reached via -isystem + -lc, not via
# CMAKE_SYSTEM_NAME=Linux (which would assume glibc + dynamic linker machinery
# NuttX does not have).
$(OUT)/toolchain.cmake: $(lastword $(MAKEFILE_LIST)) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo '# NuttX hosted-POSIX target. Generic (baremetal) drives LLVM_ON_UNIX=0,' > $@
	@echo '# which pairs with COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL in the runtimes.' >> $@
	@echo '# The libc is NuttX own libc, supplied via -isystem + -lc, not via the system.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo '# Static archives only on NuttX (flat build); keep the shared-lib suffix' >> $@
	@echo '# distinct so the never-built shared runtime targets do not collide.' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(NUTTX_TARGET)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo '# No standalone executable until the NuttX image link; feature probes build archives.' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(NUTTX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(NUTTX_CXXFLAGS)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(NUTTX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(NUTTX_LDFLAGS)")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(NUTTX_LDFLAGS)")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR}/sysroot;$${CMAKE_CURRENT_LIST_DIR}/sysroot/usr")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(OUT)/host
	# Symlink to the prebuilt host toolchain shipped in the engine repo
	# (<engine-root>/toolchains/hosts/$(HOST_ID)). SP_RUNTIME_ROOT is the engine
	# root passed in by the outer target-nuttx Makefile; the prebuilt host lives
	# next to runtime/, not under runtime/toolchains/hosts/ (which is a build
	# artifact dir created by `make host`).
	cd $(OUT); ln -fs $(SP_RUNTIME_ROOT)/../toolchains/hosts/$(HOST_ID) host
	# resource-dir symlink to the host clang resource dir: <stdarg.h>, <stddef.h>,
	# <arm_neon.h>, etc. live there. Before M2 there are no builtins; M2 will
	# drop libclang_rt.builtins.a under lib/clang/lib/<arch>/ (or replace the
	# symlink with a real directory carrying builtins + the host include/).
	rm -rf $(OUT)/lib
	mkdir -p $(OUT)/lib
	ln -fs ../../host/lib/clang/21 $(OUT)/lib/clang

# --- app-facing target.mk -------------------------------------------------
# The descriptor the rest of the make-system consumes (TARGET_SYSROOT /
# TARGET_SYSTEM=NuttX triggers make/os/nuttx.mk via apply-toolchain.mk).
# -resource-dir resolves the compiler-rt builtins once M2 ships them; before
# that it points at the host's (only builtins are looked up there).
$(OUT)/target.mk: $(lastword $(MAKEFILE_LIST)) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := NuttX' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(NUTTX_TARGET)' >> $@
	@echo '# NuttX per-board arch flags (extracted from the export Make.defs):' >> $@
	@echo 'TARGET_NUTTX_ARCHCPUFLAGS := $(NUTTX_ARCHCPUFLAGS)' >> $@
	@echo 'TARGET_NUTTX_ARCHCFLAGS := $(NUTTX_ARCHCFLAGS)' >> $@
	@echo 'TARGET_NUTTX_ARCHCXXFLAGS := $(NUTTX_ARCHCXXFLAGS)' >> $@
	@echo 'TARGET_NUTTX_CROSSDEV := $(NUTTX_CROSSDEV)' >> $@
	@echo '# The sprt runtime does not use NuttX'\''s -Werror/-Wundef (they are' >> $@
	@echo '# for NuttX'\''s own code review policy); strip them. Same for' >> $@
	@echo '# -fno-exceptions/-fno-rtti on the C++ side, then re-enable what the' >> $@
	@echo '# runtime needs.' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_NUTTX_ARCHCPUFLAGS) $(filter-out -Werror -Wundef -Weverything,$(NUTTX_ARCHCFLAGS)) -D__NuttX__' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_NUTTX_ARCHCPUFLAGS) $(filter-out -Werror -Wundef -Weverything -fno-exceptions -fno-rtti,$(NUTTX_ARCHCXXFLAGS)) -frtti -fexceptions -D__NuttX__' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := --target=$(NUTTX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang -L$$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo '# NuttX libc lives under sysroot/usr/include; expose it via TARGET_INCLUDE_DIR_LIBC' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/usr/lib' >> $@

# simde (SIMD-everywhere) is a header-only dependency the geom SIMD headers
# pull (<simde/x86/sse.h>, <simde/arm/neon.h>). Build+install it through its
# own cmake into the sysroot's usr/include/simde (header-only, so no target
# compilation — CMAKE_*_COMPILER_WORKS skips the probe), exactly like the wasm
# target does. NB: install goes into $(OUT)/sysroot/usr/include (where the
# NuttX libc headers live), not $(OUT)/usr/include — install-target.mk copies
# from sysroot/, and app code finds simde via TARGET_INCLUDE_DIR_LIBC.
SIMDE_CONFIGURE := \
	-DCMAKE_INSTALL_PREFIX="$(OUT)/sysroot" \
	-DCMAKE_INSTALL_LIBDIR="$(OUT)/sysroot/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="$(OUT)/sysroot/usr/include" \
	-DCMAKE_C_COMPILER_WORKS=1 \
	-DCMAKE_CXX_COMPILER_WORKS=1

$(OUT)/sysroot/usr/include/simde/simde-arch.h: ../common/simde.mk
	@mkdir -p $(OUT)/_simde-build
	cd $(OUT)/_simde-build; cmake -G "Ninja" $(LIB_SRC_DIR)/simde $(SIMDE_CONFIGURE)
	cd $(OUT)/_simde-build; cmake --build .
	cd $(OUT)/_simde-build; cmake --install .
	rm -rf $(OUT)/_simde-build
	$(call rule_touch,$(OUT)/sysroot/usr/include/simde/simde-arch.h)

all: $(OUT)/toolchain.cmake $(OUT)/target.mk \
	$(OUT)/sysroot/usr/include/simde/simde-arch.h \
	$(OUT)/sysroot/usr/include/setjmp.h \
	$(OUT)/sysroot/usr/include/fenv.h \
	$(OUT)/sysroot/usr/include/complex.h \
	$(OUT)/sysroot/usr/include/features.h \
	$(OUT)/sysroot/usr/include/stdarg.h \
	$(OUT)/sysroot/usr/include/uchar.h \
	$(OUT)/sysroot/usr/include/wchar_extras.h

# NuttX libc is non-conforming here: <setjmp.h> lives at nuttx/lib/setjmp.h and
# arch/setjmp.h, NOT at the POSIX-mandated <setjmp.h>. The sprt runtime (and a
# lot of vendored C/C++ code) does #include <setjmp.h>, so drop a shim into the
# sysroot that pulls the NuttX definition. sigjmp_buf / sigsetjmp come from
# nuttx/lib/setjmp.h; jmp_buf / setjmp buffer layout comes from arch/setjmp.h.
$(OUT)/sysroot/usr/include/setjmp.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: NuttX libc has no top-level <setjmp.h>.' \
		'   Forward to the NuttX-internal locations (arch + nuttx/lib). */' \
		'#ifndef __NUTTX_SETJMP_SHIM_H' \
		'#define __NUTTX_SETJMP_SHIM_H' \
		'#include <arch/setjmp.h>' \
		'#include <nuttx/lib/setjmp.h>' \
		'#endif' \
		> $@

# NuttX libc has no <fenv.h> (no floating-point environment control on the
# targets we ship). Drop a stub declaring the types and the no-op entry points
# the C++ runtime (<cfenv>, libc++'s fenv_t wrapper) needs to compile. Values
# match the aarch64 FPCR/FPSR layout the sprt runtime expects (see
# cross/linux_sprt/aarch64_sprt/fenv.h) so the static_asserts in fenv.cc line up.
$(OUT)/sysroot/usr/include/fenv.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: NuttX libc has no <fenv.h>. Stub the API. */' \
		'#ifndef __NUTTX_FENV_SHIM_H' \
		'#define __NUTTX_FENV_SHIM_H' \
		'' \
		'#ifdef __cplusplus' \
		'extern "C" {' \
		'#endif' \
		'' \
		'typedef struct { unsigned int __fpcr; unsigned int __fpsr; } fenv_t;' \
		'typedef unsigned int fexcept_t;' \
		'' \
		'int feclearexcept(int);' \
		'int fegetexceptflag(fexcept_t *, int);' \
		'int feraiseexcept(int);' \
		'int fesetexceptflag(const fexcept_t *, int);' \
		'int fetestexcept(int);' \
		'int fegetround(void);' \
		'int fesetround(int);' \
		'int fegetenv(fenv_t *);' \
		'int feholdexcept(fenv_t *);' \
		'int fesetenv(const fenv_t *);' \
		'int feupdateenv(const fenv_t *);' \
		'' \
		'#ifdef __cplusplus' \
		'}' \
		'#endif' \
		'' \
		'#define FE_INVALID    1' \
		'#define FE_DIVBYZERO  2' \
		'#define FE_OVERFLOW   4' \
		'#define FE_UNDERFLOW  8' \
		'#define FE_INEXACT   16' \
		'#define FE_ALL_EXCEPT (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)' \
		'#define FE_TONEAREST  0' \
		'#define FE_DOWNWARD   0x800000' \
		'#define FE_UPWARD     0x400000' \
		'#define FE_TOWARDZERO 0xc00000' \
		'#define FE_DFL_ENV ((const fenv_t *) -1)' \
		'' \
		'#endif' \
		> $@

# NuttX libc ships no top-level <complex.h> — the C99 complex API lives
# nowhere in its tree (NuttX is a deeply-embedded libc). Drop a shim declaring
# the prototypes the sprt runtime (libc_wrapper/c/common/SPRuntimeCComplex.cpp)
# re-exports under __sprt_-prefixed names. Declarations only — sprt provides the
# implementations via its own complex table.
$(OUT)/sysroot/usr/include/complex.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: NuttX libc has no <complex.h>. */' \
		'#ifndef __NUTTX_COMPLEX_SHIM_H' \
		'#define __NUTTX_COMPLEX_SHIM_H' \
		'#include <math.h>' \
		'#ifdef __cplusplus' \
		'extern "C" {' \
		'#endif' \
		'#define complex _Complex' \
		'/* C99 complex prototypes (declarations only — sprt supplies the' \
		'   implementations). */' \
		'double cabs(double _Complex);' \
		'float cabsf(float _Complex);' \
		'long double cabsl(long double _Complex);' \
		'double carg(double _Complex);' \
		'float cargf(float _Complex);' \
		'long double cargl(long double _Complex);' \
		'double cimag(double _Complex);' \
		'float cimagf(float _Complex);' \
		'long double cimagl(long double _Complex);' \
		'double creal(double _Complex);' \
		'float crealf(float _Complex);' \
		'long double creall(long double _Complex);' \
		'double _Complex cacos(double _Complex);' \
		'float _Complex cacosf(float _Complex);' \
		'long double _Complex cacosl(long double _Complex);' \
		'double _Complex cacosh(double _Complex);' \
		'float _Complex cacoshf(float _Complex);' \
		'long double _Complex cacoshl(long double _Complex);' \
		'double _Complex casin(double _Complex);' \
		'float _Complex casinf(float _Complex);' \
		'long double _Complex casinl(long double _Complex);' \
		'double _Complex casinh(double _Complex);' \
		'float _Complex casinhf(float _Complex);' \
		'long double _Complex casinhl(long double _Complex);' \
		'double _Complex catan(double _Complex);' \
		'float _Complex catanf(float _Complex);' \
		'long double _Complex catanl(long double _Complex);' \
		'double _Complex catanh(double _Complex);' \
		'float _Complex catanhf(float _Complex);' \
		'long double _Complex catanhl(long double _Complex);' \
		'double _Complex ccos(double _Complex);' \
		'float _Complex ccosf(float _Complex);' \
		'long double _Complex ccosl(long double _Complex);' \
		'double _Complex ccosh(double _Complex);' \
		'float _Complex ccoshf(float _Complex);' \
		'long double _Complex ccoshl(long double _Complex);' \
		'double _Complex cexp(double _Complex);' \
		'float _Complex cexpf(float _Complex);' \
		'long double _Complex cexpl(long double _Complex);' \
		'double _Complex clog(double _Complex);' \
		'float _Complex clogf(float _Complex);' \
		'long double _Complex clogl(long double _Complex);' \
		'double _Complex cproj(double _Complex);' \
		'float _Complex cprojf(float _Complex);' \
		'long double _Complex cprojl(long double _Complex);' \
		'double _Complex cpow(double _Complex, double _Complex);' \
		'float _Complex cpowf(float _Complex, float _Complex);' \
		'long double _Complex cpowl(long double _Complex, long double _Complex);' \
		'double _Complex csqrt(double _Complex);' \
		'float _Complex csqrtf(float _Complex);' \
		'long double _Complex csqrtl(long double _Complex);' \
		'double _Complex csin(double _Complex);' \
		'float _Complex csinf(float _Complex);' \
		'long double _Complex csinl(long double _Complex);' \
		'double _Complex csinh(double _Complex);' \
		'float _Complex csinhf(float _Complex);' \
		'long double _Complex csinhl(long double _Complex);' \
		'double _Complex ctan(double _Complex);' \
		'float _Complex ctanf(float _Complex);' \
		'long double _Complex ctanl(long double _Complex);' \
		'double _Complex ctanh(double _Complex);' \
		'float _Complex ctanhf(float _Complex);' \
		'long double _Complex ctanhl(long double _Complex);' \
		'double _Complex conj(double _Complex);' \
		'float _Complex conjf(float _Complex);' \
		'long double _Complex conjl(long double _Complex);' \
		'#ifdef __cplusplus' \
		'}' \
		'#endif' \
		'#endif' \
		> $@

# NuttX has no <features.h>; glibc code includes it for feature-test macros.
# Empty stub satisfies the include.
$(OUT)/sysroot/usr/include/features.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: NuttX libc has no <features.h>. */' \
		'#ifndef __NUTTX_FEATURES_SHIM_H' \
		'#define __NUTTX_FEATURES_SHIM_H' \
		'#endif' \
		> $@

# NuttX has no top-level <stdarg.h>; clang provides it via the resource dir
# (lib/clang/21/include/stdarg.h). Drop a forwarding shim for TUs that reach
# <stdarg.h> through paths that do not include the resource dir.
$(OUT)/sysroot/usr/include/stdarg.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: forward to clang resource dir stdarg.h. */' \
		'#ifndef __NUTTX_STDARG_SHIM_H' \
		'#define __NUTTX_STDARG_SHIM_H' \
		'#include_next <stdarg.h>' \
		'#endif' \
		> $@

# NuttX has no <uchar.h>; drop a minimal C11 char16_t/char32_t/mbrtowc shim.
$(OUT)/sysroot/usr/include/uchar.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: NuttX libc has no <uchar.h>. */' \
		'#ifndef __NUTTX_UCHAR_SHIM_H' \
		'#define __NUTTX_UCHAR_SHIM_H' \
		'#include <stddef.h>' \
		'#include <wchar.h>' \
		'#ifndef __cplusplus' \
		'/* C11 mode: char16_t/char32_t are typedefs. C++ has them as builtins. */' \
		'typedef unsigned short char16_t;' \
		'typedef unsigned int char32_t;' \
		'#endif' \
		'size_t mbrtoc16(char16_t *, const char *, size_t, mbstate_t *);' \
		'size_t c16rtomb(char *, char16_t, mbstate_t *);' \
		'size_t mbrtoc32(char32_t *, const char *, size_t, mbstate_t *);' \
		'size_t c32rtomb(char *, char32_t, mbstate_t *);' \
		'#endif' \
		> $@

# NuttX <wchar.h> ships the C99 minimum: it lacks the POSIX _unlocked variants
# (getwc_unlocked, fgetws_unlocked, ...) and the BSD/GNU extensions sprt's
# umbrella re-exports (wcsdup, wcsnlen, wcpcpy, wcpncpy, wcscasecmp, ...).
# Drop a side-header declaring them with the glibc/BSD signatures so the
# umbrella prototypes resolve. They are defined as weak/ENOSYS stubs by sprt's
# libc_wrapper on NuttX (the umbrella __sprt_ spells dispatch through that).
$(OUT)/sysroot/usr/include/wchar_extras.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated: NuttX wchar.h POSIX/BSD extensions. */' \
		'#ifndef __NUTTX_WCHAR_EXTRAS_H' \
		'#define __NUTTX_WCHAR_EXTRAS_H' \
		'#include <wchar.h>' \
		'#include <stddef.h>' \
		'#ifdef __cplusplus' \
		'extern "C" {' \
		'#endif' \
		'wint_t getwc_unlocked(FILE *);' \
		'wint_t getwchar_unlocked(void);' \
		'wint_t fgetwc_unlocked(FILE *);' \
		'wchar_t *fgetws_unlocked(wchar_t *__restrict, int, FILE *__restrict);' \
		'wchar_t *wcsdup(const wchar_t *);' \
		'size_t wcsnlen(const wchar_t *, size_t);' \
		'wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict);' \
		'wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);' \
		'int wcscasecmp(const wchar_t *, const wchar_t *);' \
		'int wcsncasecmp(const wchar_t *, const wchar_t *, size_t);' \
		'int wcscoll_l(const wchar_t *, const wchar_t *, locale_t);' \
		'size_t wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, size_t, locale_t);' \
		'#ifdef __cplusplus' \
		'}' \
		'#endif' \
		'#endif' \
		> $@

.PHONY: all
