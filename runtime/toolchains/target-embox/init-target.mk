# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Generate the per-arch Embox sysroot glue: a toolchain.cmake for building the
# LLVM runtimes against Embox's libc, the app-facing target.mk descriptor, and
# the symlink to the shared host toolchain (clang/lld/llvm-ar).
#
# This is the Embox-hosted counterpart of target-wasm/init-target.mk: instead
# of freestanding + sprt libc, we point clang at the Embox headers/libs that
# import-export.mk just laid out under sysroot/usr. Embox defines __EMBOX__
# at build time, which our runtime detects (runtime/include/sprt/c/bits/__sprt_def.h).

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk
include $(dir $(THIS_FILE))../common/utils/llvm-version.mk

OUT := $(TOOLCHAIN_OUTPUT_DIR)
SYSROOT := $(OUT)/sysroot
ARCH_FLAGS_FILE := $(OUT)/embox-arch-flags.mk
CONFIG_FILE := $(OUT)/embox-config.mk

# Pull the per-board flags extracted by import-export.mk.
-include $(ARCH_FLAGS_FILE)
-include $(CONFIG_FILE)

# Clang --target triple (no "embox" OS — LLVM does not know it). The arch flags
# (-mcpu/-march/-mfpu/-mabi for the specific board) ride on top of this triple.
EMBOX_TARGET := $(SP_ARCH_TARGET_CLANG)

# Build a single combined CFLAGS string for the toolchain file:
#   --target=<baremetal triple>  +  Embox's per-board -mcpu/-march/... flags
#   -isystem <sysroot>/usr/include  so the Embox libc/pthread headers resolve
#   -D__EMBOX__                    the runtime's platform-detector predicate
EMBOX_CFLAGS := --target=$(EMBOX_TARGET) \
	$(EMBOX_ARCHCPUFLAGS) $(EMBOX_ARCHCFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__EMBOX__

EMBOX_CXXFLAGS := --target=$(EMBOX_TARGET) \
	$(EMBOX_ARCHCPUFLAGS) $(EMBOX_ARCHCXXFLAGS) \
	-isystem $(SYSROOT)/usr/include \
	-D__EMBOX__

# Linker flags for runtime try-compiles: point at the Embox libs + startup
# objects and bring in libc/libm/libpthread so feature probes link cleanly.
EMBOX_LDFLAGS := --target=$(EMBOX_TARGET) \
	-L$(SYSROOT)/usr/lib \
	$(EMBOX_LDELFFLAGS) \
	-lc -lm

# --- toolchain.cmake ------------------------------------------------------
# CMAKE_SYSTEM_NAME Generic (baremetal): LLVM sets LLVM_ON_UNIX=0, which pairs
# with the COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL flags the runtimes need.
# Embox's libc is a hosted libc, but it is reached via -isystem + -lc, not via
# CMAKE_SYSTEM_NAME=Linux (which would assume glibc + dynamic linker machinery
# Embox does not have).
$(OUT)/toolchain.cmake: $(THIS_FILE) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo '# Embox hosted-POSIX target. Generic (baremetal) drives LLVM_ON_UNIX=0,' > $@
	@echo '# which pairs with COMPILER_RT_BAREMETAL_BUILD / *_BAREMETAL in the runtimes.' >> $@
	@echo '# The libc is Embox own libc, supplied via -isystem + -lc, not via the system.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo '# Static archives only on Embox (flat build); keep the shared-lib suffix' >> $@
	@echo '# distinct so the never-built shared runtime targets do not collide.' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo '# No standalone executable until the Embox image link; feature probes build archives.' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(EMBOX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(EMBOX_CXXFLAGS)")' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(EMBOX_CFLAGS)")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(EMBOX_LDFLAGS)")' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(EMBOX_LDFLAGS)")' >> $@
	@echo '# CMAKE_FIND_ROOT_PATH covers BOTH the Embox libc sysroot (sysroot/usr) AND' >> $@
	@echo '# the install prefix (usr) where the cross-built third-party deps (libz,' >> $@
	@echo '# libpng, libgif, ...) land. Without the install prefix on the path,' >> $@
	@echo '# find_package(ZLIB) / find_library(zlib) in dep cmakes (libpng, freetype,' >> $@
	@echo '# harfbuzz, ...) only see sysroot/usr and miss the just-built deps.' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr;$${CMAKE_CURRENT_LIST_DIR}/sysroot;$${CMAKE_CURRENT_LIST_DIR}/sysroot/usr")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@
	rm -f $(OUT)/host
	# Symlink to the prebuilt host toolchain shipped in the engine repo
	# (<engine-root>/toolchains/hosts/$(HOST_ID)). SP_RUNTIME_ROOT is the engine
	# root passed in by the outer target-embox Makefile; the prebuilt host lives
	# next to runtime/, not under runtime/toolchains/hosts/ (which is a build
	# artifact dir created by `make host`).
	cd $(OUT); ln -fs $(SP_RUNTIME_ROOT)/../toolchains/hosts/$(HOST_ID) host
	# resource-dir symlink to the host clang resource dir: <stdarg.h>, <stddef.h>,
	# <arm_neon.h>, etc. live there. Before M2 there are no builtins; M2 will
	# drop libclang_rt.builtins.a under lib/clang/lib/<arch>/ (or replace the
	# symlink with a real directory carrying builtins + the host include/).
	rm -rf $(OUT)/lib
	mkdir -p $(OUT)/lib
	ln -fs ../../host/lib/clang/$(SP_LLVM_VER) $(OUT)/lib/clang

# --- toolchain-libs.cmake -------------------------------------------------
# Separate toolchain file for cross-building the third-party dependency libs
# (target-embox/Makefile inner pass -> common/{zlib,gif,brotli,...}.mk). Mirrors
# target-wasm/init-target.mk's split: toolchain.cmake (used by compiler_rt.mk)
# bakes the Embox ARCHCFLAGS into CMAKE_C_FLAGS_INIT, including the -Werror/
# -Wshadow/-Wundef that are Embox's own code-review policy. The third-party deps
# must NOT inherit those (Embox libc headers leak identifiers like `OK`/
# `ERROR`/`CODE` that clash with dep code), so this file rebuilds the same
# Generic-baremetal shape but sources CMAKE_C_FLAGS_INIT from the per-dep
# SP_C_FLAGS that common/configure.mk's EMBOX branch fills (and which appends
# -Wno-error/-Wno-shadow). Also keeps the wider CMAKE_FIND_ROOT_PATH that
# covers both the Embox libc sysroot and the cross-built deps under usr/.
$(OUT)/toolchain-libs.cmake: $(THIS_FILE)
	@echo 'Build $@'
	@echo '# Embox third-party-deps toolchain. Same baremetal Generic shape as' > $@
	@echo '# toolchain.cmake, but CMAKE_C_FLAGS_INIT sources from the per-dep' >> $@
	@echo '# SP_C_FLAGS (filled by common/configure.mk EMBOX branch) instead of' >> $@
	@echo '# the Embox ARCHCFLAGS, so the deps do not inherit Embox -Werror policy.' >> $@
	@echo 'set(CMAKE_SYSTEM_NAME Generic)' >> $@
	@echo 'set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")' >> $@
	@echo 'set(CMAKE_SHARED_LIBRARY_PREFIX "lib")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang++")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER "$${CMAKE_CURRENT_LIST_DIR}/host/bin/clang")' >> $@
	@echo 'set(CMAKE_AR "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ar")' >> $@
	@echo 'set(CMAKE_RANLIB "$${CMAKE_CURRENT_LIST_DIR}/host/bin/llvm-ranlib")' >> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET "$(EMBOX_TARGET)")' >> $@
	@echo 'set(CMAKE_C_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_CXX_COMPILER_WORKS ON)' >> $@
	@echo 'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)' >> $@
	@echo '# --target must sit in CMAKE_C_FLAGS, not only CMAKE_C_COMPILER_TARGET:' >> $@
	@echo '# libpng genout.cmake (and similar execute_process preprocessors) invoke' >> $@
	@echo '# clang -E with CMAKE_C_FLAGS alone. Without --target the Xenolith clang' >> $@
	@echo '# defaults to x86_64-unknown-linux-gnu and sprt looks up' >> $@
	@echo '# embox_sprt/x86_64_sprt/config.h (does not exist). Mirrors target-wasm.' >> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$${SP_C_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(EMBOX_TARGET)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$${SP_CXX_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(EMBOX_TARGET)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$${SP_C_FLAGS} -resource-dir $${CMAKE_CURRENT_LIST_DIR}/lib/clang --target=$(EMBOX_TARGET)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$${SP_EXE_LINKER_FLAGS} --target=$(EMBOX_TARGET)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$${SP_SHARED_LINKER_FLAGS} --target=$(EMBOX_TARGET)" CACHE STRING "" FORCE)' >> $@
	@echo 'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH Off)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr;$${CMAKE_CURRENT_LIST_DIR}/sysroot;$${CMAKE_CURRENT_LIST_DIR}/sysroot/usr")' >> $@
	@echo 'set(PKG_CONFIG_PATH "$${CMAKE_CURRENT_LIST_DIR}/usr/lib/pkgconfig")' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> $@
	@echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> $@
	@echo 'set(CMAKE_PREFIX_PATH "$${CMAKE_CURRENT_LIST_DIR};$${CMAKE_CURRENT_LIST_DIR}/usr")' >> $@
	@echo 'set(CMAKE_INSTALL_PREFIX "$${CMAKE_CURRENT_LIST_DIR}")' >> $@
	@echo 'set(CMAKE_INSTALL_LIBDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/lib")' >> $@
	@echo 'set(CMAKE_INSTALL_INCLUDEDIR "$${CMAKE_CURRENT_LIST_DIR}/usr/include")' >> $@
	@echo 'set(CMAKE_POSITION_INDEPENDENT_CODE OFF)' >> $@

# --- app-facing target.mk -------------------------------------------------
# The descriptor the rest of the make-system consumes (TARGET_SYSROOT /
# TARGET_SYSTEM=Embox triggers make/os/embox.mk via apply-toolchain.mk).
# -resource-dir resolves the compiler-rt builtins once M2 ships them; before
# that it points at the host's (only builtins are looked up there).
$(OUT)/target.mk: $(THIS_FILE) $(ARCH_FLAGS_FILE) $(CONFIG_FILE)
	@echo 'Build $@'
	@echo 'TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'TARGET_SYSTEM := Embox' >> $@
	@echo 'TARGET_ARCH := $(SP_ARCH)' >> $@
	@echo 'TARGET_NAME := $(EMBOX_TARGET)' >> $@
	@echo '# Embox per-board arch flags (extracted from the export Make.defs):' >> $@
	@echo 'TARGET_EMBOX_ARCHCPUFLAGS := $(EMBOX_ARCHCPUFLAGS)' >> $@
	@echo 'TARGET_EMBOX_ARCHCFLAGS := $(EMBOX_ARCHCFLAGS)' >> $@
	@echo 'TARGET_EMBOX_ARCHCXXFLAGS := $(EMBOX_ARCHCXXFLAGS)' >> $@
	@echo 'TARGET_EMBOX_CROSSDEV := $(EMBOX_CROSSDEV)' >> $@
	@echo '# The sprt runtime does not use Embox'\''s -Werror/-Wundef (they are' >> $@
	@echo '# for Embox'\''s own code review policy); strip them. Same for' >> $@
	@echo '# -fno-exceptions/-fno-rtti on the C++ side, then re-enable what the' >> $@
	@echo '# runtime needs.' >> $@
	@echo 'TARGET_GENERAL_CFLAGS := --target=$(EMBOX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_EMBOX_ARCHCPUFLAGS) $(filter-out -Werror -Wundef -Weverything,$(EMBOX_ARCHCFLAGS)) -D__EMBOX__' >> $@
	@echo '# App C++ flags: -nostdinc++ drops clang'\''s bundled libc++ so the engine' >> $@
	@echo '# consumes sprt'\''s STL surface (include_libc/cxx + libcxx/include, added' >> $@
	@echo '# per-module in runtime.mk — same order as Linux). Do NOT force' >> $@
	@echo '# -D__SPRT_USE_STL=0: hosted app TUs default to USE_STL=1 like Linux;' >> $@
	@echo '# runtime TUs keep USE_STL=0 via __SPRT_BUILD / SPRT_BUILD_RUNTIME.' >> $@
	@echo '# Embox libc stays at the lowest search priority via embox.mk -idirafter.' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS := --target=$(EMBOX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang $$(TARGET_EMBOX_ARCHCPUFLAGS) -nostdinc++ $(filter-out -Werror -Wundef -Weverything -fno-exceptions -fno-rtti,$(EMBOX_ARCHCXXFLAGS)) -frtti -fexceptions -D__EMBOX__' >> $@
	@echo '# Embox libc lives under sysroot/usr during assemble and usr/ after' >> $@
	@echo '# install-target.mk merges the two. Pick whichever exists.' >> $@
	@echo 'ifeq ($$(wildcard $$(TARGET_SYSROOT)/sysroot/usr/include),)' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/usr/include' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'else' >> $@
	@echo 'TARGET_INCLUDE_DIR_LIBC := $$(TARGET_SYSROOT)/sysroot/usr/include' >> $@
	@echo 'TARGET_LIB_DIR_LIBC := $$(TARGET_SYSROOT)/sysroot/usr/lib' >> $@
	@echo 'endif' >> $@
	@echo 'TARGET_INCLUDE_DIR := $$(TARGET_INCLUDE_DIR_LIBC)' >> $@
	@echo 'TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib' >> $@
	@echo 'TARGET_GENERAL_LDFLAGS := --target=$(EMBOX_TARGET) -resource-dir $$(TARGET_SYSROOT)/lib/clang -L$$(TARGET_SYSROOT)/usr/lib -L$$(TARGET_LIB_DIR_LIBC)' >> $@
	@echo 'TARGET_EXEC_CFLAGS :=' >> $@
	@echo 'TARGET_EXEC_CXXFLAGS :=' >> $@
	@echo 'TARGET_EXEC_LDFLAGS :=' >> $@
	@echo 'TARGET_LIB_CFLAGS :=' >> $@
	@echo 'TARGET_LIB_CXXFLAGS :=' >> $@
	@echo 'TARGET_LIB_LDFLAGS :=' >> $@
	@echo 'TARGET_GENERAL_CFLAGS += -idirafter $$(TARGET_INCLUDE_DIR_LIBC)' >> $@
	@echo 'TARGET_GENERAL_CXXFLAGS += -idirafter $$(TARGET_INCLUDE_DIR_LIBC)' >> $@

# simde (SIMD-everywhere) is a header-only dependency the geom SIMD headers
# pull (<simde/x86/sse.h>, <simde/arm/neon.h>). Build+install it through its
# own cmake into the sysroot's usr/include/simde (header-only, so no target
# compilation — CMAKE_*_COMPILER_WORKS skips the probe), exactly like the wasm
# target does. NB: install goes into $(OUT)/sysroot/usr/include (where the
# Embox libc headers live), not $(OUT)/usr/include — install-target.mk copies
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

all: $(OUT)/toolchain.cmake $(OUT)/toolchain-libs.cmake $(OUT)/target.mk \
	$(OUT)/sysroot/usr/include/simde/simde-arch.h \
	$(OUT)/sysroot/usr/include/setjmp.h \
	$(OUT)/sysroot/usr/include/fenv.h \
	$(OUT)/sysroot/usr/include/complex.h \
	$(OUT)/sysroot/usr/include/features.h \
	$(OUT)/sysroot/usr/include/stdarg.h \
	embox-stdatomic-shim \
	embox-lib-aliases \
	$(OUT)/sysroot/usr/include/uchar.h \
	$(OUT)/sysroot/usr/include/wchar_extras.h \
	$(OUT)/sysroot/usr/include/glob.h

# Embox libc is non-conforming here: <setjmp.h> lives at embox/lib/setjmp.h and
# arch/setjmp.h, NOT at the POSIX-mandated <setjmp.h>. The sprt runtime (and a
# lot of vendored C/C++ code) does #include <setjmp.h>, so drop a shim into the
# sysroot that pulls the Embox definition. sigjmp_buf / sigsetjmp come from
# embox/lib/setjmp.h; jmp_buf / setjmp buffer layout comes from arch/setjmp.h.
$(OUT)/sysroot/usr/include/setjmp.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: Embox libc has no top-level <setjmp.h>.' \
		'   Forward to the Embox-internal locations (arch + embox/lib). */' \
		'#ifndef __EMBOX_SETJMP_SHIM_H' \
		'#define __EMBOX_SETJMP_SHIM_H' \
		'#include <asm/setjmp.h>' \
		'typedef __jmp_buf jmp_buf;' \
		'#endif' \
		> $@

# Embox libc has no <fenv.h> (no floating-point environment control on the
# targets we ship). Drop a stub declaring the types and the no-op entry points
# the C++ runtime (<cfenv>, libc++'s fenv_t wrapper) needs to compile. Values
# match the aarch64 FPCR/FPSR layout the sprt runtime expects (see
# cross/linux_sprt/aarch64_sprt/fenv.h) so the static_asserts in fenv.cc line up.
$(OUT)/sysroot/usr/include/fenv.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: Embox libc has no <fenv.h>. Stub the API. */' \
		'#ifndef __EMBOX_FENV_SHIM_H' \
		'#define __EMBOX_FENV_SHIM_H' \
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

# Embox libc ships no top-level <complex.h> — the C99 complex API lives
# nowhere in its tree (Embox is a deeply-embedded libc). Drop a shim declaring
# the prototypes the sprt runtime (libc_wrapper/c/common/SPRuntimeCComplex.cpp)
# re-exports under __sprt_-prefixed names. Declarations only — sprt provides the
# implementations via its own complex table.
$(OUT)/sysroot/usr/include/complex.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: Embox libc has no <complex.h>. */' \
		'#ifndef __EMBOX_COMPLEX_SHIM_H' \
		'#define __EMBOX_COMPLEX_SHIM_H' \
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

# Embox has no <features.h>; glibc code includes it for feature-test macros.
# Empty stub satisfies the include.
$(OUT)/sysroot/usr/include/features.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: Embox libc has no <features.h>. */' \
		'#ifndef __EMBOX_FEATURES_SHIM_H' \
		'#define __EMBOX_FEATURES_SHIM_H' \
		'#endif' \
		> $@

# Embox has no top-level <stdarg.h>; clang provides it via the resource dir
# (lib/clang/$(SP_LLVM_VER)/include/stdarg.h). Drop a forwarding shim for TUs that reach
# <stdarg.h> through paths that do not include the resource dir.
$(OUT)/sysroot/usr/include/stdarg.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: forward to clang resource dir stdarg.h. */' \
		'#ifndef __EMBOX_STDARG_SHIM_H' \
		'#define __EMBOX_STDARG_SHIM_H' \
		'#include_next <stdarg.h>' \
		'#endif' \
		> $@

# Embox's <stdatomic.h> is a 2-typedef stub (atomic_uintptr_t /
# atomic_uint_least32_t only). It sits on the -idirafter sysroot path and
# shadows clang's complete C11 header, so sheenbidi (USE_C11_ATOMICS) fails
# with unknown type name 'atomic_flag'. The file already exists after
# import-sysroot, so a normal make target would be considered up to date and
# never replace the stub. Phony-overwrite after every import; include_next
# reaches host/lib/clang/$(SP_LLVM_VER)/include/stdatomic.h.
.PHONY: embox-stdatomic-shim embox-lib-aliases
embox-stdatomic-shim:
	@mkdir -p $(OUT)/sysroot/usr/include
	printf '%s\n' \
		'/* Auto-generated shim: Embox <stdatomic.h> is a 2-typedef stub.' \
		'   Forward to clang resource dir stdatomic.h (C11 atomics). */' \
		'#ifndef __EMBOX_STDATOMIC_SHIM_H' \
		'#define __EMBOX_STDATOMIC_SHIM_H' \
		'#include_next <stdatomic.h>' \
		'#endif' \
		> $(OUT)/sysroot/usr/include/stdatomic.h

# cmake feature-probes and many deps pass -lm / -lpthread. Embox has a single
# archive (libc.a -> embox.a); alias libm/libpthread to it so ld.lld can
# resolve those flags. Re-run after every import (import may not refresh
# when the stamp is already current).
embox-lib-aliases:
	@mkdir -p $(OUT)/sysroot/usr/lib
	@if [ -e $(OUT)/sysroot/usr/lib/libc.a ]; then \
		ln -sfn libc.a $(OUT)/sysroot/usr/lib/libm.a; \
		ln -sfn libc.a $(OUT)/sysroot/usr/lib/libpthread.a; \
	fi

# Embox has no <uchar.h>; drop a minimal C11 char16_t/char32_t/mbrtowc shim.
$(OUT)/sysroot/usr/include/uchar.h:
	@if [ -f $@ ]; then echo keeping imported $@; exit 0; fi
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated shim: Embox libc has no <uchar.h>. */' \
		'#ifndef __EMBOX_UCHAR_SHIM_H' \
		'#define __EMBOX_UCHAR_SHIM_H' \
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

# Embox has no <glob.h>. musl glob.c (SPRuntimeCGlobMusl.c) still includes it;
# the C unit defines glob_t / GLOB_* first, so this header is only a guard.
$(OUT)/sysroot/usr/include/glob.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated: Embox libc has no <glob.h>. */' \
		'#ifndef _GLOB_H' \
		'#define _GLOB_H' \
		'#endif' \
		'#ifndef _GLOB_H_' \
		'#define _GLOB_H_' \
		'#endif' \
		> $@

# Embox <wchar.h> ships the C99 minimum: it lacks the POSIX _unlocked variants
# (getwc_unlocked, fgetws_unlocked, ...) and the BSD/GNU extensions sprt's
# umbrella re-exports (wcsdup, wcsnlen, wcpcpy, wcpncpy, wcscasecmp, ...).
# Drop a side-header declaring them with the glibc/BSD signatures so the
# umbrella prototypes resolve. They are defined as weak/ENOSYS stubs by sprt's
# libc_wrapper on Embox (the umbrella __sprt_ spells dispatch through that).
$(OUT)/sysroot/usr/include/wchar_extras.h:
	@mkdir -p $(dir $@)
	printf '%s\n' \
		'/* Auto-generated: Embox wchar.h POSIX/BSD extensions. */' \
		'#ifndef __EMBOX_WCHAR_EXTRAS_H' \
		'#define __EMBOX_WCHAR_EXTRAS_H' \
		'#include <wchar.h>' \
		'#include <stddef.h>' \
		'#include <locale.h>' \
		'#ifdef __cplusplus' \
		'extern "C" {' \
		'#endif' \
		'wint_t getwc_unlocked(FILE *);' \
		'wint_t getwchar_unlocked(void);' \
		'wint_t fgetwc_unlocked(FILE *);' \
		'wchar_t *fgetws_unlocked(wchar_t *__restrict, int, FILE *__restrict);' \
		'wint_t fputwc_unlocked(wchar_t, FILE *);' \
		'wint_t putwc_unlocked(wchar_t, FILE *);' \
		'wint_t putwchar_unlocked(wchar_t);' \
		'int fputws_unlocked(const wchar_t *__restrict, FILE *__restrict);' \
		'wchar_t *wcsdup(const wchar_t *);' \
		'size_t wcsnlen(const wchar_t *, size_t);' \
		'wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict);' \
		'wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);' \
		'int wcscasecmp(const wchar_t *, const wchar_t *);' \
		'int wcsncasecmp(const wchar_t *, const wchar_t *, size_t);' \
		'int wcscoll_l(const wchar_t *, const wchar_t *, locale_t);' \
		'size_t wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, size_t, locale_t);' \
		'wint_t getwchar(void);' \
		'wint_t fgetwc(FILE *stream);' \
		'wchar_t *fgetws(wchar_t *ws, int n, FILE *stream);' \
		'wint_t fputwc(wchar_t wc, FILE *stream);' \
		'int fputws(const wchar_t *wc, FILE *stream);' \
		'int fwide(FILE *stream, int wc);' \
		'int mbsinit(const mbstate_t *st);' \
		'size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps);' \
		'wint_t putwchar(wchar_t wc);' \
		'wchar_t *wcscat(wchar_t *ws1, const wchar_t *ws2);' \
		'wchar_t *wcschr(const wchar_t *ws, wchar_t wc);' \
		'wchar_t *wcscpy(wchar_t *ws1, const wchar_t *ws2);' \
		'size_t wcscspn(const wchar_t *ws1, const wchar_t *ws2);' \
		'wchar_t *wcsncat(wchar_t *ws1, const wchar_t *ws2, size_t n);' \
		'int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);' \
		'wchar_t *wcspbrk(const wchar_t *ws1, const wchar_t *ws2);' \
		'wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc);' \
		'size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps);' \
		'size_t wcsspn(const wchar_t *ws1, const wchar_t *ws2);' \
		'wchar_t *wcsstr(const wchar_t *ws1, const wchar_t *ws2);' \
		'wchar_t *wcstok(wchar_t *ws1, const wchar_t *ws2, wchar_t **ptr);' \
		'size_t mbrlen(const char *s, size_t n, mbstate_t *ps);' \
		'#ifdef __cplusplus' \
		'}' \
		'#endif' \
		'#endif' \
		> $@

.PHONY: all
