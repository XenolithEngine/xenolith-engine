# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

DEBUG ?= 0

CONFIGURE_MAKEFILE := $(lastword $(MAKEFILE_LIST))

include $(dir $(CONFIGURE_MAKEFILE))utils/init-shell.mk
include $(dir $(CONFIGURE_MAKEFILE))utils/names.mk

ifdef DARWIN
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
SP_MACOS_SDK ?= $(shell xcrun --show-sdk-path)
SP_IOS_SDK ?= $(shell xcrun --sdk iphoneos --show-sdk-path)
SP_IOSSIM_SDK ?= $(shell xcrun --sdk iphonesimulator --show-sdk-path)
else
SP_MACOS_SDK ?= $(abspath $(LIB_SRC_DIR))/MacOSX.sdk
SP_IOS_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneOS.sdk
SP_IOSSIM_SDK ?= $(abspath $(LIB_SRC_DIR))/iPhoneSimulator.sdk
endif
ifneq (,$(findstring +open,$(SP_INSTALL_PREFIX)))
SP_MACOS_SDK := $(SP_INSTALL_PREFIX)
endif
endif

export PKG_CONFIG_PATH=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig

#
# Эти флаги будут использоваться при сборке без CMake.
#
SP_CFLAGS := $(SP_OPT) $(SP_USER_CFLAGS) --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS := $(SP_OPT) $(SP_USER_CXXFLAGS) --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CPPFLAGS := --target=$(SP_TARGET) -isystem $(SP_INSTALL_PREFIX)/usr/include $(SP_USER_CPPFLAGS)
SP_LDFLAGS := --target=$(SP_TARGET) -L$(SP_INSTALL_PREFIX)/usr/lib $(SP_USER_LDFLAGS)

# Если используется SP_TOOLCHAIN_FILE, значит, мы используем разделёные HOST и TARGET файлы, и
# -resource-dir нужно явно определить внутри TARGET
ifdef SP_TOOLCHAIN_FILE
SP_CFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_CXXFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_CPPFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
SP_LDFLAGS += -resource-dir $(SP_INSTALL_PREFIX)/lib/clang
endif

ifdef SP_TOOLCHAIN_PREFIX
SP_CFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CFLAGS)
SP_CXXFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CXXFLAGS)
SP_CPPFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_CPPFLAGS)
SP_LDFLAGS := --sysroot=$(SP_TOOLCHAIN_PREFIX) $(SP_LDFLAGS)
endif # SP_TOOLCHAIN_PREFIX


ifdef WINDOWS
SP_WINDOWS_INCLIUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include \
	-isystem $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows \
	-isystem $(SP_RUNTIME_ROOT)/include/sprt/wrappers/windows/casemap

SP_CFLAGS += -Xclang --dependent-lib=sprt -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
SP_CXXFLAGS += -Xclang --dependent-lib=sprt -nostdlib  $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
endif # WINDOWS


ifdef WASM
# Freestanding WebAssembly. Like the Windows target, the sprt runtime IS the
# libc — at toolchain-build time only its headers are needed (the archive links
# per-app), so point the include search at the in-tree runtime headers and force
# the wasm platform (__SPRT_WASM) + the wasm feature set the app build uses. Kept
# in sync with the app target.mk (make/os/wasm.mk + target-wasm/init-target.mk).
SP_WASM_FEATURES := -matomics -mbulk-memory -mmutable-globals -msign-ext -mnontrapping-fptoint
SP_WASM_C_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include
SP_WASM_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include

# clang does not auto-add its builtin/resource include dir for the wasm32 freestanding
# target, so its builtin headers (<stdatomic.h>, <stdarg.h>, intrinsics) do not resolve.
# Add the host clang resource include (the -resource-dir's include/, symlinked to the
# host clang's) at LOWEST priority (-idirafter) so it fills only the clang-builtin gaps
# without shadowing the sprt libc headers.
SP_WASM_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/lib/clang/include

# C++ deps (harfbuzz, ...) parse the sprt STL headers, which require C++20 (concepts,
# etc.). cmake's CXX_STANDARD does not emit a -std flag here because the toolchain
# skips compiler detection, so pin it in the flags (matches the app's GLOBAL_STDXX).
# C++ deps are compiled HOSTED (no -ffreestanding, so __STDC_HOSTED__==1), matching
# the LLVM C++ runtimes build: the sprt STL then exposes the C library under std::
# and its __STDC_HOSTED__-gated bridges activate (e.g. <cmath> -> std::isfinite,
# std::isnan), which third-party C++ (harfbuzz) relies on. C deps stay freestanding.
SP_CFLAGS += -nostdinc -ffreestanding -nostdlib $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_CXXFLAGS += -nostdinc -nostdinc++ -nostdlib -std=gnu++2a $(SP_WASM_FEATURES) $(SP_WASM_CXX_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_CPPFLAGS += -nostdinc $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
SP_LDFLAGS += -nostdlib $(SP_WASM_FEATURES)
endif # WASM


ifdef NUTTX
# NuttX hosted-POSIX on its own libc. Third-party deps cross-build against the
# imported NuttX sysroot the same way they do against glibc on LINUX: the NuttX
# libc/pthread/mm live under $(SP_INSTALL_PREFIX)/sysroot/usr/include (the
# imported export), while the third-party deps themselves install under
# $(SP_INSTALL_PREFIX)/usr/include. SP_CFLAGS above already adds
# -isystem $(SP_INSTALL_PREFIX)/usr/include (the deps); add the NuttX libc
# include too so giflib/zlib/freetype/... find <stdlib.h>, <string.h>,
# <sys/types.h>, ... from NuttX. No -nostdinc / -ffreestanding: deps are
# compiled hosted (the NuttX libc is the real libc, not sprt). The clang
# --target is the baremetal triple (LLVM has no "nuttx" OSType); -D__NuttX__ is
# driven from the FORWARD_VARS the target Makefile passes in, so it is not
# re-added here. The sprt runtime headers are NOT on the path: deps must not see
# sprt's umbrella, only NuttX libc (consistent with how LINUX/ANDROID deps build
# against the platform libc, not sprt).
SP_NUTTX_ARCH_FLAGS := -march=armv8-a
# C/C++ deps use the sprt libc C shims (include_libc) as their C Standard
# Library base, NOT the NuttX libc (sysroot/usr/include). The NuttX libc
# headers leak broad identifiers into the global scope that clash with
# third-party code: <sys/types.h> has an unnamed `enum { ERROR = -1, OK = 0 }`
# whose `OK` collides with libwebp's `WebPWorkerStatus::OK` enumerator (a hard
# C redefinition error, not a warning); <math.h> defines isinf/isnan as macros
# that break libc++'s std::isinf; and so on. The sprt include_libc C shims are
# a clean POSIX C surface the deps compile against; the NuttX libc itself is
# reached only at link time (libs in sysroot/usr/lib) and through -idirafter
# for the NuttX-internal <arch/...> and <nuttx/...> headers sprt's cross layer
# pulls (nuttx_sprt/setjmp.h -> <arch/setjmp.h>). Mirrors the WASM deps shape.
#
# -ffreestanding is critical here, same as WASM: it makes __STDC_HOSTED__=0 so
# sprt's SPRT_UMBRELLA_REQUIRED default evaluates to 0 and the sprt libc shims
# expose EXTERN prototypes (resolved against the platform libc / sprt runtime
# at link time) rather than static-inline bodies. Without it the static-inline
# umbrella declarations (e.g. strcasecmp in <string.h>) clash with the
# third-party deps' own non-static forward declarations (curl's stdcheaders.h).
SP_NUTTX_LIBC_INC := -isystem $(SP_RUNTIME_ROOT)/include_libc -isystem $(SP_RUNTIME_ROOT)/include
SP_NUTTX_SYSROOT_FALLBACK := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
# clang does not auto-add its builtin/resource include dir for the baremetal
# aarch64-none-elf target, so the NuttX <stdarg.h> shim's #include_next never
# reaches the clang-provided <stdarg.h>/<stdatomic.h>/intrinsics. Add the host
# clang resource include at LOWEST priority (-idirafter) so it fills only the
# clang-builtin gaps without shadowing the sprt libc headers. The path is the
# real host clang resource dir (lib/clang -> host/lib/clang/21 is a relative
# symlink that does not resolve to <prefix>/lib/clang/include directly).
SP_NUTTX_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/host/lib/clang/21/include
SP_CFLAGS += -ffreestanding $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
SP_CXXFLAGS += -ffreestanding $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0 -std=gnu++17
SP_CPPFLAGS += -ffreestanding $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
SP_LDFLAGS += -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib
# NuttX flat-build arm64 linker scripts have no .tbss/.tdata sections (see
# make/os/nuttx.mk); -femulated-tls keeps thread_local working in deps too.
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
# NuttX's libc/compiler headers leak broad identifiers into the global scope
# (the `OK = 0` / `ERROR = -1` enumerators in <sys/types.h>, the `CODE` / `FAR`
# / `near` / `distant` placement macros in <nuttx/compiler.h>, ...). These clash
# with third-party code (libwebp's `OK`, brotli's `CODE`), and the NuttX -Werror
# /-Wundef/-Wshadow policy in TARGET_NUTTX_ARCHCFLAGS is for NuttX's own code
# review, not for the deps built here. Downgrade the resulting diagnostics so
# the deps compile against the libc as-is, matching how LINUX/ANDROID deps build.
SP_CFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
SP_CXXFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
# NuttX flat-build arm64 linker scripts have no .tbss/.tdata sections (see
# make/os/nuttx.mk); -femulated-tls keeps thread_local working in deps too.
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
endif # NUTTX


ifdef DARWIN

# Apple's modern libc ABI in <sys/cdefs.h> (no legacy $UNIX2003 / $INODE64
# suffixes) needs no -DXNU_PLATFORM_<platform> here: the +open sysroot bakes the
# macro into <sys/cdefs.h> at assembly time (open-sysroot.mk, mirroring how
# Apple resolves those branches with unifdef when generating the SDK), and the
# real SDKs ship the header already resolved.

ifeq ($(SP_SYSNAME),Darwin)
SP_SDK_DIR := $(SP_MACOS_SDK)
SP_CFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -isysroot $(SP_MACOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mmacosx-version-min=$(SP_MACOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_MACOS_SDK)/usr/lib -F$(SP_MACOS_SDK)/System/Library/Frameworks
endif # SP_SYSNAME Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
SP_SDK_DIR := $(SP_IOSSIM_SDK)
SP_CFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -isysroot $(SP_IOSSIM_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mios-simulator-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOSSIM_SDK)/usr/lib -F$(SP_IOSSIM_SDK)/System/Library/Frameworks
else # SP_IOSSIM
SP_SDK_DIR := $(SP_IOS_SDK)
SP_CFLAGS += -mios-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS += -mios-version-min=$(SP_IOS_VER) -isysroot $(SP_IOS_SDK) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_LDFLAGS += -mios-version-min=$(SP_IOS_VER) -L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_IOS_SDK)/usr/lib -F$(SP_IOS_SDK)/System/Library/Frameworks
endif # SP_IOSSIM
endif # iOS

endif # DARWIN


#
# Autoconf helper
#

export PKG_CONFIG_PATH=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig

CONFIGURE_AUTOCONF := \
	CC=$(SP_CC) \
	CPP="$(SP_CC) -E" \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	CFLAGS="$(SP_CFLAGS)" \
	CXXFLAGS="$(SP_CXXFLAGS)" \
	CPPFLAGS="$(SP_CPPFLAGS)" \
	LDFLAGS="$(SP_LDFLAGS)" \
	PKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig" \
	--includedir=$(SP_INSTALL_PREFIX)/usr/include \
	--libdir=$(SP_INSTALL_PREFIX)/usr/lib \
	--bindir=$(MAKE_ROOT)$(LIBNAME)/prefix/bin \
	--sbindir=$(MAKE_ROOT)$(LIBNAME)/prefix/sbin \
	--datarootdir=$(MAKE_ROOT)$(LIBNAME)/prefix/share \
	--prefix=$(SP_INSTALL_PREFIX) \
	--enable-shared=no \
	--enable-static=yes

CONFIGURE_AUTOCONF += --host=$(CONFIGURE_HOST_$(SP_SYSNAME)_$(SP_ARCH))

ifdef WINDOWS
CONFIGURE_AUTOCONF += RC=$(SP_RC)
endif


#
# CMake helper
#

CONFIGURE_CMAKE_C_FLAGS_INIT := $(SP_OPT) $(SP_USER_CFLAGS)
CONFIGURE_CMAKE_CXX_FLAGS_INIT := $(SP_OPT) $(SP_USER_CXXFLAGS)
CONFIGURE_EXE_LINKER_FLAGS_INIT := $(SP_LIBS_PLATFORM) $(SP_USER_LDFLAGS)
CONFIGURE_SHARED_LINKER_FLAGS_INIT := $(SP_LIBS_PLATFORM) $(SP_USER_LDFLAGS)

ifdef WASM
CONFIGURE_CMAKE_C_FLAGS_INIT += -nostdinc -ffreestanding $(SP_WASM_FEATURES) $(SP_WASM_C_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -nostdinc -nostdinc++ -std=gnu++2a $(SP_WASM_FEATURES) $(SP_WASM_CXX_INCLUDES) $(SP_WASM_RESOURCE_INC) -D__SPRT_WASM
CONFIGURE_EXE_LINKER_FLAGS_INIT += -nostdlib -Wl,--no-entry -Wl,--export-if-defined=main \
	-L$(SP_INSTALL_PREFIX)/usr/lib -lsprt \
	$(SP_INSTALL_PREFIX)/lib/clang/lib/wasi/libclang_rt.builtins-wasm32.a
endif # WASM

ifdef LINUX
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef ANDROID
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef NUTTX
# The toolchain-libs.cmake (generated by target-nuttx/init-target.mk) sources
# CMAKE_C_FLAGS_INIT from SP_C_FLAGS (filled here), so cmake-driven deps see the
# same NuttX libc + clang-resource include paths and the same -Wno-* downgrades
# as the direct-compile (non-cmake) deps. Add the include paths and the warning
# relaxations here so CONFIGURE_CMAKE_C_FLAGS_INIT (which configure.mk passes
# through as -DSP_C_FLAGS) carries them into toolchain-libs.cmake.
CONFIGURE_CMAKE_C_FLAGS_INIT += -ffreestanding $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
# C++ deps (harfbuzz, freetype) need a C++ standard library surface for <cassert>/
# <vector>/... — the NuttX sysroot has no libc++. Use the vendored upstream libc++
# headers (libcxx/include, header-only — no libc++abi/libc++ build needed) plus
# the sprt libc++-shim wrappers (include_libc/cxx) and the sprt libc C shims
# (include_libc) as the C standard library surface libc++ #include_next's into.
# This mirrors the WASM C++ deps layout (configure.mk SP_WASM_CXX_INCLUDES): the
# order matters — libcxx/include BEFORE include_libc so libc++'s <cstdio> finds
# the C <stdio.h> in include_libc via the regular search. sprt/cxx is NOT on the
# path (it pulls sprt/wrappers/libc which conflicts with the platform libc).
# -D__NuttX__ so sprt platform detection (__sprt_def.h, reached transitively via
# libcxx/include -> __config_site) resolves to the NuttX branch. -D__SPRT_USE_STL=0
# keeps the sprt STL surface self-contained (no projection onto a real libc++).
SP_NUTTX_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include
# C++ deps use ONLY the vendored libc++ + sprt libc shims as their C/C++ header
# base — NOT the NuttX libc standard headers (sysroot/usr/include). libc++
# expects its own C Standard Library headers ahead of any platform libc (NuttX
# <math.h> defines isinf/isnan as macros that break std::isinf; NuttX <float.h>
# is not libc++'s own). So the NuttX sysroot is NOT on the CXX path via -isystem
# (which would shadow sprt's C shims); the sprt include_libc C shims (which
# libc++ #include_next's into) stand in for the C library instead. Mirrors the
# WASM C++ deps shape (sprt IS the libc there).
# The sprt nuttx_sprt/ cross headers (reached transitively via libc++'s
# <csetjmp> -> sprt include_libc/setjmp.h -> nuttx_sprt/setjmp.h -> <arch/setjmp.h>
# / <nuttx/lib/setjmp.h>) DO need the NuttX arch/nuttx trees. Expose the NuttX
# sysroot at LOWEST priority (-idirafter, after sprt + libc++) so the NuttX-
# internal <arch/...> and <nuttx/...> paths resolve, but the NuttX standard C
# headers (<math.h>, <stdio.h>, ...) never shadow the sprt/libc++ shims. If a
# header is reached only via -idirafter it is by construction not found earlier
# in the sprt/libc++ path, so NuttX only fills the arch/nuttx gaps.
SP_NUTTX_CXX_LIBC_INCLUDES := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -ffreestanding $(SP_NUTTX_RESOURCE_INC) $(SP_NUTTX_CXX_INCLUDES) $(SP_NUTTX_CXX_LIBC_INCLUDES) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0 -std=gnu++20
# cmake feature-probes (libzip's check_function_exists for the Annex K *_s
# functions, curl's recv/send, ...) need to LINK against the sprt libc to tell
# present-from-absent symbols apart when the project-include flips them to
# EXECUTABLE (see target-nuttx/nuttx-libzip-project-include.cmake). Mirror the
# WASM shape (but with ELF lld flags — wasm-ld's --no-entry/--export-if-defined
# are wasm-only): -nostdlib to keep NuttX libc/crt0 out of the probe link,
# -lsprt for the umbrella surface, the compiler-rt builtins for out-of-line
# arithmetic, --gc-sections to drop unreferenced probe helpers, and
# --unresolved-symbols=ignore-in-object-files so an absent function is silently
# left undefined instead of being a hard link error (which is what we WANT for
# detection: check_function_exists compiles+links a call to the symbol, so a
# present symbol resolves via libsprt.a and an absent one stays undefined ->
# the probe's try_compile fails -> detected ABSENT). check_type_size's main is
# kept live by the static-archive fallback path (no --export-if-defined needed
# on ELF: it is referenced by _start which lld keeps).
SP_NUTTX_PROBE_LDFLAGS := -nodefaultlibs -nostartfiles \
	-L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib \
	-lsprt -lc -lm \
	$(SP_INSTALL_PREFIX)/sysroot/usr/lib/libclang_rt.builtins-aarch64.a \
	-Wl,--no-undefined -Wl,-u,main -Wl,-e,main
CONFIGURE_EXE_LINKER_FLAGS_INIT += $(SP_NUTTX_PROBE_LDFLAGS)
CONFIGURE_SHARED_LINKER_FLAGS_INIT += $(SP_NUTTX_PROBE_LDFLAGS)
# NuttX's libc/compiler headers leak broad identifiers into the global scope
# (the `OK = 0` / `ERROR = -1` enumerators in <sys/types.h>, the `CODE` / `FAR`
# / `near` / `distant` placement macros in <nuttx/compiler.h>, ...). These clash
# with third-party code (libwebp's `OK`, brotli's `CODE`), and the NuttX -Werror
# /-Wundef/-Wshadow policy in TARGET_NUTTX_ARCHCFLAGS is for NuttX's own code
# review, not for the deps built here. Downgrade the resulting diagnostics so
# the deps compile against the libc as-is, matching how LINUX/ANDROID deps build.
CONFIGURE_CMAKE_C_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
# cmake-driven deps (SheenBidi's _Thread_local ScratchBuffer, …) must match
# the engine's -femulated-tls; NuttX arm64 linker scripts have no PT_TLS.
CONFIGURE_CMAKE_C_FLAGS_INIT += -femulated-tls
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -femulated-tls
endif # NUTTX

CONFIGURE_CMAKE :=

ifdef SP_TOOLCHAIN_FILE

CONFIGURE_CMAKE += \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(SP_TOOLCHAIN_FILE)) \
	-DCMAKE_PREFIX_PATH="${SP_INSTALL_PREFIX};${SP_INSTALL_PREFIX}/usr" \
	-DCMAKE_INSTALL_PREFIX="${SP_INSTALL_PREFIX}" \
	-DCMAKE_INSTALL_LIBDIR="${SP_INSTALL_PREFIX}/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="${SP_INSTALL_PREFIX}/usr/include" \
	-DPKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig"

else # SP_TOOLCHAIN_FILE

ifdef ANDROID
CONFIGURE_CMAKE += \
	-DCMAKE_SYSTEM_NAME=Android \
	-DCMAKE_ANDROID_ARCH=$(ANDROID_ARCH) \
	-DCMAKE_ANDROID_ARCH_ABI=$(ANDROID_ARCH_ABI) \
	-DCMAKE_ANDROID_NDK=$(NDK) \
	-DCMAKE_ANDROID_API=$(ANDROID_PLATFORM_LEVEL) \
	-DANDROID_PLATFORM_LEVEL=$(ANDROID_PLATFORM_LEVEL) \
	-DANDROID_NDK=$(NDK)
endif # ANDROID

CONFIGURE_CMAKE += \
	-DCMAKE_ASM_FLAGS_INIT="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DCMAKE_C_FLAGS_INIT="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DCMAKE_CXX_FLAGS_INIT="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DCMAKE_EXE_LINKER_FLAGS_INIT="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DCMAKE_SHARED_LINKER_FLAGS_INIT="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=Off \
	-DCMAKE_FIND_ROOT_PATH="$(SP_INSTALL_PREFIX);$(SP_INSTALL_PREFIX)/usr" \
	-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
	-DCMAKE_PREFIX_PATH="${SP_INSTALL_PREFIX};${SP_INSTALL_PREFIX}/usr" \
	-DCMAKE_INSTALL_PREFIX="${SP_INSTALL_PREFIX}" \
	-DCMAKE_INSTALL_LIBDIR="${SP_INSTALL_PREFIX}/usr/lib" \
	-DCMAKE_INSTALL_INCLUDEDIR="${SP_INSTALL_PREFIX}/usr/include" \
	-DPKG_CONFIG_PATH="$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig"
endif # SP_TOOLCHAIN_FILE


ifdef DARWIN

CONFIGURE_CMAKE_C_FLAGS_INIT += -isystem $(SP_INSTALL_PREFIX)/usr/include
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -isystem $(SP_INSTALL_PREFIX)/usr/include

CONFIGURE_CMAKE += \
	-DCMAKE_LIBTOOL="$(SP_INSTALL_PREFIX)/host/bin/llvm-libtool-darwin" \
	-DCMAKE_LIPO="$(SP_INSTALL_PREFIX)/host/bin/llvm-lipo"  \
	-DCMAKE_C_COMPILER=$(SP_CC) \
	-DCMAKE_CXX_COMPILER=$(SP_CXX) \

CONFIGURE_EXE_LINKER_FLAGS_INIT := -L$(SP_INSTALL_PREFIX)/usr/lib
CONFIGURE_SHARED_LINKER_FLAGS_INIT := -L$(SP_INSTALL_PREFIX)/usr/lib

ifeq ($(SP_SYSNAME),Darwin)
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_MACOS_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_MACOS_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_MACOS_SDK)/usr/lib \
	-F$(SP_MACOS_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_MACOS_SDK)/usr/lib \
	-F$(SP_MACOS_SDK)/System/Library/Frameworks
endif # Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_IOSSIM_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_IOSSIM_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_IOSSIM_SDK)/usr/lib \
	-F$(SP_IOSSIM_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_IOSSIM_SDK)/usr/lib \
	-F$(SP_IOSSIM_SDK)/System/Library/Frameworks
else # SP_IOSSIM
CONFIGURE_CMAKE += -DCMAKE_OSX_SYSROOT=$(SP_IOS_SDK) \
	-DCMAKE_FRAMEWORK_PATH="$(SP_IOS_SDK)/System/Library/Frameworks"
CONFIGURE_EXE_LINKER_FLAGS_INIT += \
	-L$(SP_IOS_SDK)/usr/lib \
	-F$(SP_IOS_SDK)/System/Library/Frameworks
CONFIGURE_SHARED_LINKER_FLAGS_INIT += \
	-L$(SP_IOS_SDK)/usr/lib \
	-F$(SP_IOS_SDK)/System/Library/Frameworks
endif # SP_IOSSIM
endif # iOS

endif # DARWIN


# +open header layout (Linux-target parity): the SDK-like headers — apple-oss libc,
# the baked overlay, and our libc++ (include_libc/c++/v1, built by libcxx.mk) — live
# in <sysroot>/include_libc; usr/include holds ONLY the third-party deps' own headers.
# Deps builds therefore search include_libc via -isystem, AFTER usr/include (a dep's
# own installed headers win first). C++ ordering: our c++/v1 must come FIRST (before
# usr/include and include_libc), or libc++'s <climits> -> `#include_next <limits.h>`
# would hit the C <limits.h> before libc++'s own and error. Detected by the "+open"
# marker in the install prefix (same idiom as make/os/darwin.mk). Stock SDK targets
# are unaffected. cmake deps get the include_libc -isystem from toolchain.cmake
# (emitted by init-target.mk), so only the c++/v1 prepend is mirrored there.
ifneq (,$(findstring +open,$(SP_INSTALL_PREFIX)))
SP_CXXFLAGS := -isystem $(SP_INSTALL_PREFIX)/include_libc/c++/v1 $(SP_CXXFLAGS)
CONFIGURE_CMAKE_CXX_FLAGS_INIT := -isystem $(SP_INSTALL_PREFIX)/include_libc/c++/v1 $(CONFIGURE_CMAKE_CXX_FLAGS_INIT)
SP_CFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
SP_CXXFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
SP_CPPFLAGS += -isystem $(SP_INSTALL_PREFIX)/include_libc
endif


ifdef WINDOWS
CONFIGURE_CMAKE_C_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_CMAKE_RC_FLAGS_INIT += -nostdlib $(SP_WINDOWS_INCLIUDES) -D__SPRT_WINDOWS
CONFIGURE_EXE_LINKER_FLAGS_INIT += -nostdlib -L$(SP_INSTALL_PREFIX)/lib
CONFIGURE_SHARED_LINKER_FLAGS_INIT += -nostdlib -L$(SP_INSTALL_PREFIX)/lib
endif

CONFIGURE_CMAKE += \
	-DSP_C_FLAGS="$(CONFIGURE_CMAKE_C_FLAGS_INIT)" \
	-DSP_CXX_FLAGS="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT)" \
	-DSP_EXE_LINKER_FLAGS="$(CONFIGURE_EXE_LINKER_FLAGS_INIT)" \
	-DSP_SHARED_LINKER_FLAGS="$(CONFIGURE_SHARED_LINKER_FLAGS_INIT)" \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_INSTALL_BINDIR=$(MAKE_ROOT)$(LIBNAME)/bin \
	-DCMAKE_INSTALL_DATAROOTDIR=$(SP_INSTALL_PREFIX)/usr/share \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_SYSTEM_PROCESSOR=$(CONFIGURE_PROC_$(SP_ARCH)) \
	-DCMAKE_POSITION_INDEPENDENT_CODE=On \
	-DCMAKE_VERBOSE_MAKEFILE=On

ifdef WINDOWS
CONFIGURE_CMAKE += \
	-DCMAKE_RC_COMPILER=$(SP_RC) \
	-DCMAKE_RC_FLAGS_INIT="$(CONFIGURE_CMAKE_RC_FLAGS_INIT)" \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DENABLE_EXPORTS=Off
endif

ifdef WASM
# Force every C++ dependency to the C++20 the sprt STL requires — see
# target-wasm/wasm-deps-project-include.cmake for why the toolchain file cannot do it.
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)wasm-deps-project-include.cmake
endif

ifdef NUTTX
# Force every C++ dependency to the C++20 the sprt STL requires (harfbuzz
# hard-codes CMAKE_CXX_STANDARD 11 in its CMakeLists.txt:10) and stage the
# freetype/harfbuzz build order (freetype first, no system harfbuzz) — see
# target-nuttx/nuttx-deps-project-include.cmake for the details.
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)nuttx-deps-project-include.cmake
endif

ifeq ($(DEBUG),1)
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Debug
else
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Release
endif
