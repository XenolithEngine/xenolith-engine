# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

# Cross-build the LLVM runtimes (compiler-rt builtins, libunwind, libc++abi,
# libc++) for the freestanding wasm32 target using the host clang. Mirrors the
# two-phase target-windows/target-linux flow: the outer pass (no SP_ARCH) sets up
# the per-arch variables and drives init-target -> build -> install; the inner
# pass (SP_ARCH set) builds against the generated wasm toolchain.cmake.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk
TARGET_FILE := $(MAKE_ROOT)init-target.mk
COMPILER_RT_FILE := $(MAKE_ROOT)compiler_rt.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk
TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

SP_OPT := -O3

ifndef SP_ARCH
# --- outer pass: describe the wasm32 target and drive the sub-passes ---
ARCHS := wasm32
LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

SP_ARCH_wasm32_INFO_BASE := \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..) \
	SP_ARCH=wasm32 \
	SP_RES_ARCH=wasm32 \
	SP_ARCH_TARGET_CLANG=wasm32-unknown-unknown

SP_ARCH_wasm32_INFO := \
	$(SP_ARCH_wasm32_INFO_BASE) \
	SP_TARGET=wasm32-unknown-unknown \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown

SP_ARCH_wasm32_INSTALL := \
	$(SP_ARCH_wasm32_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown \
	T_TARGET=$(TOOLCHAIN_TARGETS)/wasm32-unknown-unknown

all: $(ARCHS)

clean:
	$(call rule_rm,$(TOOLCHAIN_OUTPUT_DIR))

wasm32:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown/usr/include $(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_wasm32_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_wasm32_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_wasm32_INSTALL)

.PHONY: all clean wasm32

else # SP_ARCH
# --- inner pass: build the LLVM runtimes + the third-party dependency libs ---
SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)
SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)
LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

# Third-party dependency libs (media / text / data + crypto + networking). Vulkan
# and WAMR are intentionally excluded from the wasm target, so the mbedtls/iwasm and
# vulkan/spirv/glslang recipes the other targets carry are omitted here. OpenSSL
# (libcrypto.a/libssl.a) + libzip + curl (HTTP/3 via OpenSSL QUIC + nghttp3) ARE
# built for wasm; the socket layer is the no-op-stub sprt libc (see libc_impl).
LIBS := \
	libz.a \
	libbz2.a \
	liblzma.a \
	libzstd.a \
	libbrotlienc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libtiff.a \
	libfreetype.a \
	libharfbuzz.a \
	libSheenBidi.a \
	libsqlite3.a \
	libssl.a \
	libgost_engine.a \
	libzip-openssl.a \
	libnghttp3.a \
	libcurl-openssl.a

# Deps are cross-built through common/configure.mk + the SP_C_FLAGS-driven
# toolchain-libs.cmake. The LLVM runtimes keep the flag-baked toolchain.cmake
# (compiler_rt.mk drives cmake directly and passes no SP_C_FLAGS), so its rule
# overrides SP_TOOLCHAIN_FILE below.
FORWARD_VARS := \
	WASM=1 \
	SP_SYSNAME=WASM \
	SP_ARCH=$(SP_ARCH) \
	SP_RES_ARCH=$(SP_RES_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain-libs.cmake \
	SP_TOOLCHAIN_PREFIX=$(TOOLCHAIN_OUTPUT_DIR) \
	SP_TARGET=$(SP_TARGET) \
	SP_RUNTIME_ROOT=$(SP_RUNTIME_ROOT) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

ARCH_LIBS = $(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

RUNTIME_LIB := $(SP_INSTALL_PREFIX)/lib/clang/lib/wasi/libclang_rt.builtins-wasm32.a

all: $(RUNTIME_LIB) $(ARCH_LIBS)

# LLVM runtimes: compiler-rt builtins + libunwind + libc++abi, on the flag-baked
# toolchain.cmake (see COMPILER_RT_FILE header for why it is a separate file).
$(RUNTIME_LIB): $(COMPILER_RT_FILE)
	$(MAKE) -f $< $(FORWARD_VARS) SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake

$(SP_INSTALL_PREFIX)/usr/lib/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/liblzma.a: ../common/xz.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a: ../common/zstd.mk
	$(MAKE) -f $< $(FORWARD_VARS)

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

$(SP_INSTALL_PREFIX)/usr/lib/libtiff.a: ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/libwebp.a $(SP_INSTALL_PREFIX)/usr/lib/liblzma.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a: ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libharfbuzz.a: ../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libSheenBidi.a: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libsqlite3.a: ../common/sqlite.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# OpenSSL drives its own perl Configure (no cmake/autoconf), so it uses the
# target-local openssl.mk, not a common/ recipe. The single rule builds both
# libcrypto.a and libssl.a.
$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

# libzip with the OpenSSL crypto backend (AES). cmake build; depends on the
# compression backends (zlib/bzip2/lzma/zstd, ENABLE_*=ON) and libcrypto.a. There
# is no GOST engine on wasm, so unlike target-linux the openssl variant needs only
# libssl.a. Output is renamed to libzip-openssl.a (WASM branch of common/libzip.mk).
$(SP_INSTALL_PREFIX)/usr/lib/libzip-openssl.a: ../common/libzip.mk \
		$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libz.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a \
		$(SP_INSTALL_PREFIX)/usr/lib/liblzma.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

# HTTP/3 framing library (cmake, lib-only). No crypto/compression deps of its own;
# curl consumes it for HTTP/3 on top of OpenSSL's built-in QUIC.
$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a: ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# curl with the OpenSSL TLS backend + HTTP/3 (USE_OPENSSL_QUIC + nghttp3). No IDN2
# on wasm (disabled in curl.mk's WASM branch). Sockets/resolver come from the sprt
# libc no-op stubs. Output renamed to libcurl-openssl.a (non-WINDOWS branch of curl.mk).
$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk \
		$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a \
		$(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

.PHONY: all

endif # SP_ARCH
