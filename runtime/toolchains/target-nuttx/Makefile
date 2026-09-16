# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including limitation the rights
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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# NuttX target. Hosted POSIX on NuttX's own libc — sysroot is IMPORTED from a
# `make export` package produced by a NuttX source tree (or built on the spot
# from NUTTX_DIR+NUTTX_CONFIG), then augmented with our LLVM runtimes built
# against that libc. No libc_impl, no musl: libc/pthread/mm/... come from NuttX.
#
# Triple layout (see docs/platforms/nuttx.adoc §Triple naming):
#   <arch>-nuttx-none-elf      bare-metal ARMv8/RISC-V boards (RPi4, VisionFive 2, ...)
#   arm-nuttx-none-eabi        Cortex-M / -R / 32-bit -A boards
#   x86_64-nuttx-sim-linux-gnu the NuttX sim target (a Linux process)
# LLVM does not know an "OSType nuttx", so the OS field stays "none"/"linux" and
# NuttX rides in the vendor field; the clang --target uses the baremetal triple.
#
# Per-board profiles arrive as a +<board> suffix (e.g. aarch64-nuttx-none-elf+rpi4)
# mirroring the +dll/+sprt suffix scheme; the suffix selects a specific export
# package via NUTTX_BOARD / NUTTX_CONFIG.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

IMPORT_FILE := $(MAKE_ROOT)import-export.mk
TARGET_FILE := $(MAKE_ROOT)init-target.mk
COMPILER_RT_FILE := $(MAKE_ROOT)compiler_rt.mk
LIBCXX_FILE := $(MAKE_ROOT)libcxx.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk
TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

# Source tree for header-only deps that init-target.mk builds into the sysroot
# (simde for the geom SIMD headers). Defaults to runtime/toolchains/src/, the
# same place `make download` populates; override LIB_SRC_DIR when running
# out-of-tree (CI, our VM tests where engine is mounted ro).
LIB_SRC_DIR ?= $(abspath $(MAKE_ROOT)../src)

SP_OPT := -O2

# A NuttX export package is REQUIRED input. We accept either a prebuilt tarball
# (preferred — reproducible, what `make export` in a NuttX tree produces) or a
# NuttX source tree + board:config that we build the export from on the spot.
ifndef SP_ARCH
# --- outer pass: validate inputs, describe targets, drive sub-passes ---

NUTTX_EXPORT ?=
NUTTX_DIR ?=
NUTTX_CONFIG ?=
NUTTX_BOARD ?=

ifeq ($(NUTTX_EXPORT),)
ifeq ($(NUTTX_DIR),)
$(error NuttX target requires NUTTX_EXPORT=<pkg.tar.gz> or NUTTX_DIR=<nuttx-tree> [NUTTX_CONFIG=<board>:<config>])
endif
ifeq ($(NUTTX_CONFIG),)
$(error NuttX target with NUTTX_DIR also requires NUTTX_CONFIG=<board>:<config> (e.g. raspberrypi-4b:fb))
endif
endif

# Per-arch parameters. The STAPPLER_TARGET name carries the nuttx vendor; the
# clang --target strips it (LLVM has no "nuttx" OSLiteral). SP_ARCH_TARGET_CLANG
# is what we pass to --target=; SP_TARGET is the on-disk directory name.
SP_ARCH_aarch64_INFO_BASE := \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..) \
	SP_ARCH=aarch64 \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-none-elf

SP_ARCH_aarch64_INFO := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	SP_TARGET=aarch64-nuttx-none-elf \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf

SP_ARCH_aarch64_INSTALL := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf \
	T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-nuttx-none-elf

# Future arches (M1 ships aarch64 only; the rest are added by copying this block
# and swapping the clang triple / STAPPLER_TARGET name):
#   riscv64-nuttx-none-elf   -> SP_ARCH_TARGET_CLANG=riscv64-none-elf
#   arm-nuttx-none-eabi      -> SP_ARCH_TARGET_CLANG=arm-none-eabi
#   x86_64-nuttx-sim-linux-gnu -> SP_ARCH_TARGET_CLANG=x86_64-unknown-linux-gnu

ARCHES := aarch64

all: $(addprefix target-,$(ARCHES))

clean:
	$(call rule_rm,$(TOOLCHAIN_OUTPUT_DIR))

# Build one NuttX target:
#   1. import-export.mk  — unpack / build the nuttx-export package, lay out headers
#                           and libraries into the sysroot, extract arch flags
#   2. init-target.mk    — generate toolchain.cmake + the app-facing target.mk,
#                           link the shared host toolchain into this sysroot
#   3. compiler_rt.mk    — build libclang_rt.builtins + sme_stub against NuttX libc
#   4. inner pass        — cross-build the third-party dependency libs (gif/png/
#                           jpeg/zlib/freetype/harfbuzz/...) against the NuttX libc,
#                           same set target-linux/target-wasm carry
#   5. install-target.mk — copy intermediate/ into targets/<triple>/
target-aarch64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf
	$(MAKE) -f $(IMPORT_FILE) \
		$(SP_ARCH_aarch64_INFO) \
		NUTTX_EXPORT="$(NUTTX_EXPORT)" NUTTX_DIR="$(NUTTX_DIR)" \
		NUTTX_CONFIG="$(NUTTX_CONFIG)" NUTTX_BOARD="$(NUTTX_BOARD)"
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO) \
		LIB_SRC_DIR="$(LIB_SRC_DIR)"
	# M2: build libclang_rt.builtins-aarch64.a + sme_stub against the NuttX libc.
	$(MAKE) -f $(COMPILER_RT_FILE) \
		$(SP_ARCH_aarch64_INFO_BASE) \
		TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf \
		LIB_SRC_DIR="$(LIB_SRC_DIR)"
	$(MAKE) -f $(LIBCXX_FILE) \
		$(SP_ARCH_aarch64_INFO_BASE) \
		TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf \
		LIB_SRC_DIR="$(LIB_SRC_DIR)"
	# Cross-build the third-party dependency libs against the NuttX libc (the
	# sysroot laid out by import-export + init-target). Mirrors the inner pass
	# target-linux/target-wasm carry; see the SP_ARCH branch below for the rules.
	# Run from a writable scratch dir (libbuild/) under TOOLCHAIN_OUTPUT_DIR so
	# the per-lib build subdirs (zlib/, giflib/, ...) land there, not in this
	# (possibly read-only) source tree. The parent scratch dir carries a `common`
	# symlink so both `../common/gif.mk` (in the rule prerequisites below) and
	# `include ../common/configure.mk` (inside the common recipes) resolve the
	# same way they do for target-linux/target-wasm running in-tree.
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild
	@ln -sfn $(abspath $(MAKE_ROOT)../common) $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/common
	@ln -sfn $(abspath $(MAKE_ROOT)nuttx-deps-project-include.cmake) $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild/nuttx-deps-project-include.cmake
	@ln -sfn $(abspath $(MAKE_ROOT)nuttx-curl-project-include.cmake) $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild/nuttx-curl-project-include.cmake
	@ln -sfn $(abspath $(MAKE_ROOT)openssl.mk) $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild/openssl.mk
	@ln -sfn $(abspath $(MAKE_ROOT)gen-probe-stubs.sh) $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild/gen-probe-stubs.sh
	# MAKE_ROOT points at the writable scratch (libbuild) so configure.mk's
	# CMAKE_INSTALL_BINDIR = $(MAKE_ROOT)$(LIBNAME)/bin and similar per-lib
	# staging paths land there, not under this (possibly read-only) source tree.
	$(MAKE) -f $(MAKE_FILE) --directory=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild \
		$(SP_ARCH_aarch64_INFO_BASE) \
		SP_TARGET=aarch64-nuttx-none-elf \
		MAKE_ROOT=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf/libbuild/ \
		TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf \
		LIB_SRC_DIR="$(LIB_SRC_DIR)"
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_aarch64_INSTALL)

.PHONY: all clean target-aarch64

else # SP_ARCH
# --- inner pass: cross-build the third-party dependency libs against NuttX libc ---
# The sysroot (NuttX libc + headers + compiler-rt builtins) is already laid out
# under $(TOOLCHAIN_OUTPUT_DIR) by the outer pass. Deps build hosted against the
# NuttX libc via common/configure.mk's NUTTX branch (no -nostdinc, no sprt headers
# on the path — same as LINUX/ANDROID deps build against their platform libc).
SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)
SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)
# toolchain-libs.cmake (not toolchain.cmake): the deps must pick up their
# CMAKE_C_FLAGS_INIT from the per-dep SP_C_FLAGS (filled by configure.mk's
# NUTTX branch, with -Wno-error/-Wno-shadow), not the NuttX -Werror-baked
# toolchain.cmake used by compiler_rt.mk. Mirrors target-wasm.
SP_TOOLCHAIN_FILE ?= $(abspath $(TOOLCHAIN_OUTPUT_DIR)/toolchain-libs.cmake)
LIB_SRC_DIR ?= $(abspath $(MAKE_ROOT)../src)

# libsprt.a — in-tree sprt runtime archive, same shape as target-wasm.
# Deps that drive cmake feature-probes (curl recv/send, ...)
# link those probes against it so missing symbols are hard errors rather than
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY false-positives. Hosted-POSIX:
# wrapper + core + runtime only — no libc_impl / malloc / musl (NuttX libc).
SPRT_LIB := $(SP_INSTALL_PREFIX)/usr/lib/libsprt.a

# libprobe-stubs.a — weak placeholders for everything the staged archives
# reference but none of them defines (board code, linker-script symbols, the
# engine's libc_impl). Only the cmake feature probes of the deps that link them
# (curl) ever pull it in; see gen-probe-stubs.sh for the derivation and
# common/configure.mk's SP_NUTTX_PROBE_LDFLAGS for the link line it completes.
#
# Its content is a function of every other staged archive, which is not
# expressible as a prerequisite list, so it is a .PHONY step re-run before each
# dep that needs it — an nm sweep, cheap next to the dep build behind it. Those
# deps take it as an ORDER-ONLY prerequisite (`| probe-stubs`): a phony normal
# prerequisite is always out of date, which would rebuild curl on
# every incremental toolchain make.
PROBE_STUBS_LIB := $(SP_INSTALL_PREFIX)/usr/lib/libprobe-stubs.a

.PHONY: probe-stubs
probe-stubs: $(SPRT_LIB)
	$(MAKE_ROOT)gen-probe-stubs.sh \
		$(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-nm) \
		"$(SP_CC) --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang" \
		$(SP_AR) \
		$(PROBE_STUBS_LIB) \
		$(SP_INSTALL_PREFIX)/sysroot/usr/lib $(SPRT_LIB) \
		-- $(SP_INSTALL_PREFIX)/usr/lib

# Third-party dependency libs. NuttX ships its own libc/pthread/mm in the export,
# so the set mirrors target-linux/target-wasm (media/text/data + crypto + net).
# Vulkan / WAMR / libbacktrace are excluded: no GPU driver in the flat NuttX image
# (the soft rasterizer is the renderer), no wasm runtime, and NuttX provides its
# own backtrace path. libicuuc is deferred: ICU's autoconf cross-build needs the
# ability to link a probe executable, which the NuttX flat build does not provide
# (crt0/special linker script). The sprt runtime's unicode backend is stubbed in
# runtime/libc_wrapper/platform/nuttx/unicode.cc for the M6 milestone; the full
# ICU port is tracked as a separate task.
# The crypto/net stack (openssl/gost/mbedtls/nghttp3/curl) is staged to
# land alongside a NuttX openssl.mk (perl Configure target) — added incrementally
# as the engine surface that needs it comes online.
# libzstd builds unchanged (plain cmake, no host probes) and is in: curl's zstd
# content-encoding wants it. liblzma/libtiff are
# still staged behind xz's tuklib_cpucores/physmem, which needs a NuttX case;
# libtiff chains in once xz lands, because it pulls both.
LIBS := \
	libz.a \
	libbz2.a \
	libzstd.a \
	libbrotlienc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libfreetype.a \
	libharfbuzz.a \
	libSheenBidi.a \
	libsqlite3.a \
	libssl.a \
	libgost_engine.a \
	libnghttp3.a \
	libngtcp2.a \
	libcurl-openssl.a

FORWARD_VARS := \
	NUTTX=1 \
	SP_SYSNAME=NuttX \
	SP_ARCH=$(SP_ARCH) \
	SP_RES_ARCH=$(SP_RES_ARCH) \
	SP_ARCH_TARGET_CLANG=$(SP_ARCH_TARGET_CLANG) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_TOOLCHAIN_FILE) \
	SP_TARGET=$(SP_ARCH_TARGET_CLANG) \
	SP_RUNTIME_ROOT=$(SP_RUNTIME_ROOT) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

all: $(SPRT_LIB) $(ARCH_LIBS)

# Cross-build libsprt.a from the in-tree runtime against the generated
# host.mk/target.mk (mirrors target-wasm). Static-archive builds only archive
# the modules they are handed — spell the hosted libc closure, no window/GUI.
$(SPRT_LIB): $(SP_INSTALL_PREFIX)/sysroot/usr/lib/libclang_rt.builtins-$(SP_ARCH).a
	$(call rule_rm,$(abspath build/xlsprt))
	$(MAKE) -C $(SP_RUNTIME_ROOT) \
		STAPPLER_HOST_FILE=$(SP_INSTALL_PREFIX)/host/host.mk \
		STAPPLER_TARGET_FILE=$(SP_INSTALL_PREFIX)/target.mk \
		STAPPLER_TARGET=$(SP_TARGET) \
		LOCAL_MODULES="runtime_libc_wrapper runtime_core runtime" \
		LOCAL_OUTDIR=$(abspath build/xlsprt) \
		RELEASE=1
	@mkdir -p $(dir $@)
	cp -f $(abspath build/xlsprt)/$(SP_TARGET)/release/cc/libsprt.a $@
	$(call rule_rm,$(abspath build/xlsprt))

$(SP_INSTALL_PREFIX)/usr/lib/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a: ../common/zstd.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# liblzma/libzstd are staged behind xz's tuklib_cpucores/physmem NuttX case
# (see LIBS comment); the rules + entries return once unblocked.
$(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a: ../common/brotli.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a: ../common/libjpeg-turbo.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libpng16.a: ../common/libpng.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgif.a: ../common/gif.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwebp.a: ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a $(SP_INSTALL_PREFIX)/usr/lib/libgif.a
	$(MAKE) -f $< $(FORWARD_VARS)

# libtiff returns alongside liblzma/libzstd/libbrotli (its compression backends).

# freetype/harfbuzz build order mirrors target-apple/target-linux: freetype
# builds first (no system harfbuzz on NuttX — nuttx-deps-project-include.cmake
# forces FT_REQUIRE_HARFBUZZ OFF so freetype.mk's hard-coded TRUE does not fail
# find_package(HARFBUZZ)), then harfbuzz builds against that freetype. harfbuzz
# hard-codes `set(CMAKE_CXX_STANDARD 11)` in its CMakeLists.txt:10; the sprt STL
# surface requires C++20 concepts, so the override is driven through
# CMAKE_PROJECT_INCLUDE (NUTTX branch in common/configure.mk ->
# nuttx-deps-project-include.cmake).
$(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a: ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libharfbuzz.a: ../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libSheenBidi.a: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libsqlite3.a: ../common/sqlite.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# OpenSSL drives its own perl Configure (no cmake/autoconf), so it uses the
# target-local openssl.mk (NuttX aarch64 = linux-aarch64 Configure target).
# The single rule builds both libcrypto.a and libssl.a.
$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a:  ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# ngtcp2 QUIC transport on the OpenSSL (ossl) crypto backend: libngtcp2.a +
# libngtcp2_crypto_ossl.a, consumed by curl for HTTP/3. Requires an OpenSSL with
# sockets/QUIC (openssl.mk drops no-sock) and a NuttX export with CONFIG_NET.
$(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a:  ../common/ngtcp2.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

# curl with the OpenSSL crypto backend. Built against the sprt libc shims
# (-ffreestanding, same as the rest of the deps); the engine uses memory-based
# HTTP, and on NuttX the socket layer is driven by the app/sprt umbrella.
$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk \
		$(SPRT_LIB) \
		$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a \
		| probe-stubs
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

.PHONY: all

endif # SP_ARCH
