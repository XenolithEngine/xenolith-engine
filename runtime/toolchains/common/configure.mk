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
include $(dir $(CONFIGURE_MAKEFILE))utils/llvm-version.mk

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
# The clang triple, which is NOT always the target's on-disk name. They coincide
# for every target whose name happens to be a valid triple, and diverge as soon
# as one carries a +suffix: `aarch64-embox-none-elf+user` makes clang read
# `elf+user` as a version and reject it. SP_ARCH_TARGET_CLANG is the triple where
# the two differ; fall back to SP_TARGET so nothing else has to change.
SP_TARGET_TRIPLE := $(if $(SP_ARCH_TARGET_CLANG),$(SP_ARCH_TARGET_CLANG),$(SP_TARGET))

SP_CFLAGS := $(SP_OPT) $(SP_USER_CFLAGS) --target=$(SP_TARGET_TRIPLE) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CXXFLAGS := $(SP_OPT) $(SP_USER_CXXFLAGS) --target=$(SP_TARGET_TRIPLE) -isystem $(SP_INSTALL_PREFIX)/usr/include
SP_CPPFLAGS := --target=$(SP_TARGET_TRIPLE) -isystem $(SP_INSTALL_PREFIX)/usr/include $(SP_USER_CPPFLAGS)
SP_LDFLAGS := --target=$(SP_TARGET_TRIPLE) -L$(SP_INSTALL_PREFIX)/usr/lib $(SP_USER_LDFLAGS)

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
ifdef EMBOX_USER
# Embox user mode (EL0). Freestanding, exactly like WASM above and for the same
# reason: the sprt runtime IS the libc, and at toolchain-build time only its
# headers are needed. Kept in sync with the app flags (make/os/embox-user.mk +
# target-embox-user/init-target.mk) -- a dependency compiled against different
# headers than the application is the failure this mirroring prevents.
#
# -march=armv8-a with no -mtune: one binary runs on the A53 of QEMU virt and the
# A72 of a Pi 4 (decision D3), so nothing board-specific may enter here.
SP_EMBOX_USER_ARCH_FLAGS := -march=armv8-a
SP_EMBOX_USER_C_INCLUDES := -isystem $(SP_RUNTIME_ROOT)/include_libc -isystem $(SP_RUNTIME_ROOT)/include
SP_EMBOX_USER_CXX_INCLUDES := -isystem $(SP_RUNTIME_ROOT)/include_libc/cxx -isystem $(SP_RUNTIME_ROOT)/libcxx/include -isystem $(SP_RUNTIME_ROOT)/include_libc -isystem $(SP_RUNTIME_ROOT)/include
# clang adds no resource include dir for a -nostdinc freestanding target, so
# <stdatomic.h>/<stdarg.h> and the intrinsics do not resolve. -idirafter puts it
# at the lowest priority: it fills the clang-builtin gaps without shadowing an
# sprt libc header.
SP_EMBOX_USER_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/lib/clang/include
# C deps stay freestanding; C++ deps compile HOSTED (no -ffreestanding, so
# __STDC_HOSTED__ is 1) because the sprt STL only exposes the C library under
# std:: in that mode, and third-party C++ (harfbuzz) relies on it. Same split as
# WASM.
SP_CFLAGS += -nostdinc -ffreestanding -nostdlib $(SP_EMBOX_USER_ARCH_FLAGS) $(SP_EMBOX_USER_C_INCLUDES) $(SP_EMBOX_USER_RESOURCE_INC) -D__EMBOX_USER__
SP_CXXFLAGS += -nostdinc -nostdinc++ -nostdlib -std=gnu++2a $(SP_EMBOX_USER_ARCH_FLAGS) $(SP_EMBOX_USER_CXX_INCLUDES) $(SP_EMBOX_USER_RESOURCE_INC) -D__EMBOX_USER__
SP_CPPFLAGS += -nostdinc $(SP_EMBOX_USER_ARCH_FLAGS) $(SP_EMBOX_USER_C_INCLUDES) $(SP_EMBOX_USER_RESOURCE_INC) -D__EMBOX_USER__
SP_LDFLAGS += -nostdlib $(SP_EMBOX_USER_ARCH_FLAGS)
endif # EMBOX_USER


ifdef NUTTX
SP_NUTTX_ARCH_FLAGS := -march=armv8-a
SP_NUTTX_LIBC_INC := -isystem $(SP_RUNTIME_ROOT)/include_libc -isystem $(SP_RUNTIME_ROOT)/include
SP_NUTTX_SYSROOT_FALLBACK := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
SP_NUTTX_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/host/lib/clang/$(SP_LLVM_VER)/include
SP_CFLAGS += $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
SP_CXXFLAGS += $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0 -std=gnu++17
SP_CPPFLAGS += $(SP_NUTTX_ARCH_FLAGS) $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
SP_LDFLAGS += -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
SP_CFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
SP_CXXFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
endif # NUTTX

ifdef EMBOX
SP_EMBOX_ARCH_FLAGS := -march=armv8-a
SP_EMBOX_LIBC_INC := -isystem $(SP_RUNTIME_ROOT)/include_libc -isystem $(SP_RUNTIME_ROOT)/include
SP_EMBOX_SYSROOT_FALLBACK := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
SP_EMBOX_RESOURCE_INC := -idirafter $(SP_INSTALL_PREFIX)/host/lib/clang/$(SP_LLVM_VER)/include
SP_CFLAGS += $(SP_EMBOX_ARCH_FLAGS) $(SP_EMBOX_LIBC_INC) $(SP_EMBOX_SYSROOT_FALLBACK) $(SP_EMBOX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__EMBOX__ -D__SPRT_USE_STL=0
SP_CXXFLAGS += $(SP_EMBOX_ARCH_FLAGS) $(SP_EMBOX_LIBC_INC) $(SP_EMBOX_SYSROOT_FALLBACK) $(SP_EMBOX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__EMBOX__ -D__SPRT_USE_STL=0 -std=gnu++17
SP_CPPFLAGS += $(SP_EMBOX_ARCH_FLAGS) $(SP_EMBOX_LIBC_INC) $(SP_EMBOX_SYSROOT_FALLBACK) $(SP_EMBOX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__EMBOX__ -D__SPRT_USE_STL=0
SP_LDFLAGS += -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
SP_CFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
SP_CXXFLAGS += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
SP_CFLAGS += -femulated-tls
SP_CXXFLAGS += -femulated-tls
endif # EMBOX


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
ifdef EMBOX_USER
# NO arch flags here, matching the NuttX and Embox branches. cmake gives its own
# compile lines the architecture through CMAKE_C_COMPILER_TARGET, but a raw
# ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} invocation inside a cmake -P script (libpng's
# genchk.cmake is one) gets the flags WITHOUT the target -- and then -march=armv8-a
# reaches a compiler defaulting to the host triple, which rejects it as an unknown
# CPU. The arch flags stay in SP_CFLAGS, where the triple always travels with them.
# --target= goes IN the flags, not only in CMAKE_C_COMPILER_TARGET. cmake adds the
# latter to its own compile lines, but a raw ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS}
# invocation inside a cmake -P script (libpng's genchk.cmake) sees the flags alone
# -- and then -D__EMBOX_USER__ arrives at a compiler defaulting to the host arch,
# which sends the sprt headers looking for embox_user_sprt/x86_64_sprt/. The flags
# have to be self-contained for that case.
#
# Arch flags stay OUT, matching the NuttX and Embox branches: the triple already
# implies the armv8-a baseline, and a stray -march on a host-targeted invocation
# is what this comment's first version was written about.
CONFIGURE_CMAKE_C_FLAGS_INIT += --target=$(SP_TARGET_TRIPLE) -nostdinc -ffreestanding $(SP_EMBOX_USER_C_INCLUDES) $(SP_EMBOX_USER_RESOURCE_INC) -D__EMBOX_USER__
CONFIGURE_CMAKE_CXX_FLAGS_INIT += --target=$(SP_TARGET_TRIPLE) -nostdinc -nostdinc++ -std=gnu++2a $(SP_EMBOX_USER_CXX_INCLUDES) $(SP_EMBOX_USER_RESOURCE_INC) -D__EMBOX_USER__
# Feature probes link against the live libsprt.a with NO --allow-undefined, so an
# absent function fails the link and is correctly detected as ABSENT. That is what
# replaces a hand-maintained list of -DHAVE_*=OFF -- and it matters more here than
# anywhere: this target's syscall table is a fifth of Linux's, so the probes find
# a great deal absent, and each one they get wrong is a call that traps on a board.
# The probe has to link what an APPLICATION links, or it fails for reasons that
# have nothing to do with the function it is asking about -- curl's QUIC probe
# reported "QUICTLS API support is missing" when what was actually missing was
# _Unwind_Resume. libc++abi and libunwind belong on the line for the same reason
# the builtins do: -nostdlib means nothing is implicit.
#
# The unwinder's .eh_frame scan bounds are defsym'd to zero rather than taken
# from share/app-aarch64.lds. An application gets the real values from that
# script; a probe cannot, because the script reads ADDR(.eh_frame_hdr) and a
# probe TU with no unwind data has no such section -- which lld rejects outright.
# Zeroing them is safe HERE and only here: a feature probe has to link, never to
# run, and these four are unwinder internals no probe is ever testing.
CONFIGURE_EXE_LINKER_FLAGS_INIT += -nostdlib \
	-L$(SP_INSTALL_PREFIX)/usr/lib -lsprt \
	$(SP_INSTALL_PREFIX)/usr/lib/libc++abi.a \
	$(SP_INSTALL_PREFIX)/usr/lib/libunwind.a \
	$(SP_INSTALL_PREFIX)/lib/clang/lib/embox_user/libclang_rt.builtins-aarch64.a \
	-Wl,--defsym=__eh_frame_start=0 -Wl,--defsym=__eh_frame_end=0 \
	-Wl,--defsym=__eh_frame_hdr_start=0 -Wl,--defsym=__eh_frame_hdr_end=0
endif # EMBOX_USER

ifdef LINUX
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef ANDROID
CONFIGURE_EXE_LINKER_FLAGS_INIT += -Wl,--gc-sections
endif

ifdef NUTTX
CONFIGURE_CMAKE_C_FLAGS_INIT += $(SP_NUTTX_LIBC_INC) $(SP_NUTTX_SYSROOT_FALLBACK) $(SP_NUTTX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0
SP_NUTTX_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include
SP_NUTTX_CXX_LIBC_INCLUDES := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
CONFIGURE_CMAKE_CXX_FLAGS_INIT += $(SP_NUTTX_RESOURCE_INC) $(SP_NUTTX_CXX_INCLUDES) $(SP_NUTTX_CXX_LIBC_INCLUDES) -D_LDBL_EQ_DBL -D__NuttX__ -D__SPRT_USE_STL=0 -std=gnu++20
SP_NUTTX_PROBE_LDFLAGS := -nodefaultlibs -nostartfiles \
	-L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib \
	-Wl,--start-group \
	-lsprt -lc -lm -lmm -lnet -lfs -lsched -larch -ldrivers -lboards -lboard \
	-lbinfmt -lopenamp -lxx -lsme_stub -lc++abi -lunwind -lprobe-stubs \
	-Wl,--end-group \
	$(SP_INSTALL_PREFIX)/sysroot/usr/lib/libclang_rt.builtins-aarch64.a \
	-Wl,--no-dependent-libraries -Wl,--no-undefined -Wl,-u,main -Wl,-e,main
CONFIGURE_EXE_LINKER_FLAGS_INIT += $(SP_NUTTX_PROBE_LDFLAGS)
CONFIGURE_SHARED_LINKER_FLAGS_INIT += $(SP_NUTTX_PROBE_LDFLAGS)
CONFIGURE_CMAKE_C_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
CONFIGURE_CMAKE_C_FLAGS_INIT += -femulated-tls
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -femulated-tls
endif # NUTTX

ifdef EMBOX
CONFIGURE_CMAKE_C_FLAGS_INIT += $(SP_EMBOX_LIBC_INC) $(SP_EMBOX_SYSROOT_FALLBACK) $(SP_EMBOX_RESOURCE_INC) -D_LDBL_EQ_DBL -D__EMBOX__ -D__SPRT_USE_STL=0
SP_EMBOX_CXX_INCLUDES := \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include
SP_EMBOX_CXX_LIBC_INCLUDES := -idirafter $(SP_INSTALL_PREFIX)/sysroot/usr/include
CONFIGURE_CMAKE_CXX_FLAGS_INIT += $(SP_EMBOX_RESOURCE_INC) $(SP_EMBOX_CXX_INCLUDES) $(SP_EMBOX_CXX_LIBC_INCLUDES) -D_LDBL_EQ_DBL -D__EMBOX__ -D__SPRT_USE_STL=0 -std=gnu++20
SP_EMBOX_PROBE_LDFLAGS := -nodefaultlibs -nostartfiles \
	-L$(SP_INSTALL_PREFIX)/usr/lib -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib \
	-Wl,--start-group \
	-lsprt -lc -lm -lc++abi -lunwind -lsme_stub -lprobe-stubs \
	-Wl,--end-group \
	$(SP_INSTALL_PREFIX)/sysroot/usr/lib/libclang_rt.builtins-aarch64.a \
	-Wl,--no-dependent-libraries -Wl,--no-undefined -Wl,-u,main -Wl,-e,main
CONFIGURE_EXE_LINKER_FLAGS_INIT += $(SP_EMBOX_PROBE_LDFLAGS)
CONFIGURE_SHARED_LINKER_FLAGS_INIT += $(SP_EMBOX_PROBE_LDFLAGS)
CONFIGURE_CMAKE_C_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -Wno-error -Wno-shadow -Wno-macro-redefined -Wno-undef
CONFIGURE_CMAKE_C_FLAGS_INIT += -femulated-tls
CONFIGURE_CMAKE_CXX_FLAGS_INIT += -femulated-tls
endif # EMBOX

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
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)nuttx-deps-project-include.cmake
endif

ifdef EMBOX
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)embox-deps-project-include.cmake
endif
ifdef EMBOX_USER
CONFIGURE_CMAKE += -DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)embox-user-deps-project-include.cmake
endif

ifeq ($(DEBUG),1)
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Debug
else
CONFIGURE_CMAKE += -DCMAKE_BUILD_TYPE=Release
endif
