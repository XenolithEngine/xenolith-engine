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

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

TARGET_FILE := $(dir $(THIS_FILE))init-target.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk

TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)

TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

MKDIR = mkdir -p

SP_MACOS_VER := 14.5
SP_IOS_VER := 17.4

# The Vulkan validation layer is a loadable MODULE (.dylib) on macOS, but on iOS
# upstream forces it to a SHARED .framework bundle. Track the produced artifact
# (a real file usable as a make marker) per platform; SP_SYSNAME is only set in
# the per-arch (else) branch, which is the only place this list is consumed.
ifeq ($(SP_SYSNAME),iOS)
SP_VVL_LIB := VkLayer_khronos_validation.framework/VkLayer_khronos_validation
else
SP_VVL_LIB := libVkLayer_khronos_validation.dylib
endif

INCLUDES := \
	simde/simde-arch.h \
	spirv/unified1/spirv.h \
	spirv-tools/libspirv.hpp \
	vulkan/vulkan.h

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
	libnghttp3.a \
	libngtcp2.a \
	libssl.a \
	libgost_engine.a \
	libmbedtls.a \
	libcurl-openssl.a \
	libcurl-mbedtls.a \
	libzip.a \
	libiwasm-release.a \
	libiwasm-debug.a \
	libbacktrace.a \
	libxml2.a \
	libvulkan.dylib \
	libMoltenVK.dylib \
	libVulkanLayerSettings.a \
	$(SP_VVL_LIB)

ifndef SP_ARCH

ARCHS := \
	x86_64-apple-macosx \
	aarch64-apple-macosx \
	aarch64-apple-ios \
	x86_64-apple-ios-simulator \
	aarch64-apple-ios-simulator

all: $(ARCHS)

SP_ARCH_COMMON := \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..)

SP_x86_64_MACOSX_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=x86_64-apple-macosx \
	SP_ARCH=x86_64 \
	SP_APPLE_ARCH=x86_64-macos \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx

SP_aarch64_MACOSX_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=aarch64-apple-macosx \
	SP_ARCH=arm64 \
	SP_APPLE_ARCH=arm64-macos \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx

# "+open" now builds a STANDALONE sysroot from scratch in its OWN intermediate
# (intermediate/<arch>-apple-macosx+open) — it is a primary target, no longer an
# overlay staged from the stock <arch>-apple-macosx build. Same compiler triple
# (SP_TARGET); only the intermediate dir differs. init-target lays the toolchain
# there and open-sysroot.mk assembles usr/include from apple-oss + the baked overlay.
SP_x86_64_MACOSX_OPEN_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=x86_64-apple-macosx \
	SP_ARCH=x86_64 \
	SP_APPLE_ARCH=x86_64-macos \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx+open

SP_aarch64_MACOSX_OPEN_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=Darwin \
	SP_OSVER=$(SP_MACOS_VER) \
	SP_TARGET=aarch64-apple-macosx \
	SP_ARCH=arm64 \
	SP_APPLE_ARCH=arm64-macos \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx+open

SP_aarch64_IOS_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_IOS_VER) \
	SP_TARGET=aarch64-apple-ios \
	SP_ARCH=arm64 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios

SP_x86_64_IOSSIM_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_IOS_VER) \
	SP_TARGET=x86_64-apple-ios-simulator \
	SP_ARCH=x86_64 \
	SP_IOSSIM=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator

SP_aarch64_IOSSIM_INFO := \
	$(SP_ARCH_COMMON) \
	SP_SYSNAME=iOS \
	SP_OSVER=$(SP_IOS_VER) \
	SP_TARGET=aarch64-apple-ios-simulator \
	SP_ARCH=arm64 \
	SP_IOSSIM=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator

x86_64-apple-macosx:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_MACOSX_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-apple-macosx

aarch64-apple-macosx:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_MACOSX_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-apple-macosx

x86_64-apple-macosx+sprt:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_MACOSX_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_x86_64_MACOSX_INFO) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-apple-macosx+sprt

aarch64-apple-macosx+sprt:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_MACOSX_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_MACOSX_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_aarch64_MACOSX_INFO) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-apple-macosx+sprt

# The "+open" variant is a STANDALONE Xcode-SDK-free sysroot, built from scratch
# in its OWN intermediate (intermediate/<arch>-apple-macosx+open):
#   1. init-target bootstrap — toolchain.cmake + target.mk (fresh, both SDK-free:
#                     -isysroot = the sysroot itself, -DXNU_PLATFORM_MacOSX) + host/
#                     lib-clang symlinks + the CSU checkout (NO crt1 yet);
#   2. open-sysroot — assemble usr/include from apple-oss + the baked overlay
#                     (tbds, framework headers, libm) directly in the intermediate;
#   3. init-target crt1 — compile crt1.o against the now-populated +open sysroot
#                     (SDK-free; runs AFTER open-sysroot so usr/include exists);
#   4. libcxx       — cross-built libc++/libc++abi/libunwind (see libcxx.mk); MUST
#                     precede the C++ deps so usr/include/c++/v1 exists for them;
#   5. all          — the full third-party dependency set, same per-lib rules as the
#                     stock targets. Includes lib/clang/lib/darwin/libclang_rt.osx.a
#                     (compiler-rt builtins + sanitizers via libc++.mk): configure.mk
#                     redirects SP_MACOS_SDK to the sysroot for +open prefixes, so
#                     that build is SDK-free too;
#   6. install      — export the final sysroot into targets/<arch>-apple-macosx+open
#                     via install-target.mk: include_libc + verbatim usr/lib (tbds,
#                     dylib chains, crt1.o) + frameworks + usr/local/include link +
#                     the +sprt packaging (prebuilt libsprt.dylib/.tbd + runtime.mk).
# The working sysroot IS the intermediate dir; the install step only exports it.
x86_64-apple-macosx+open:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx+open/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx+open/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_MACOSX_OPEN_INFO) bootstrap
	$(MAKE) -f $(MAKE_ROOT)open-sysroot.mk $(SP_x86_64_MACOSX_OPEN_INFO) \
		T_TARGET=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx+open
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_MACOSX_OPEN_INFO) crt1
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_MACOSX_OPEN_INFO) libcxx
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_MACOSX_OPEN_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_x86_64_MACOSX_OPEN_INFO) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-macosx+open \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-apple-macosx+open

aarch64-apple-macosx+open:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx+open/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx+open/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_MACOSX_OPEN_INFO) bootstrap
	$(MAKE) -f $(MAKE_ROOT)open-sysroot.mk $(SP_aarch64_MACOSX_OPEN_INFO) \
		T_TARGET=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx+open
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_MACOSX_OPEN_INFO) crt1
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_MACOSX_OPEN_INFO) libcxx
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_MACOSX_OPEN_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_aarch64_MACOSX_OPEN_INFO) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-macosx+open \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-apple-macosx+open

aarch64-apple-ios:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_IOS_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_IOS_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-apple-ios

x86_64-apple-ios-simulator:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_x86_64_IOSSIM_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_x86_64_IOSSIM_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-apple-ios-simulator \
		T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-apple-ios-simulator

aarch64-apple-ios-simulator:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_aarch64_IOSSIM_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_aarch64_IOSSIM_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) \
		T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-apple-ios-simulator \
		T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-apple-ios-simulator

.PHONY: all $(ARCHS)
.DEFAULT_GOAL := all

else

OUT_SYSROOT ?= $(TOOLCHAIN_OUTPUT_DIR)

SP_TOOLCHAIN_PREFIX ?= $(OUT_SYSROOT)
SP_INSTALL_PREFIX ?= $(OUT_SYSROOT)
SP_TOOLCHAIN_FILE ?= $(realpath $(OUT_SYSROOT)/toolchain.cmake)

SP_CC := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/cc)
SP_CXX := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/c++)
SP_AR := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/ar)
SP_LIBTOOL := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/llvm-libtool-darwin)
SP_RANLIB := $(abspath $(SP_TOOLCHAIN_PREFIX)/host/bin/llvm-ranlib)

SP_OPT := -O3 -fPIC -DPIC

LIB_SRC_DIR := $(realpath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	DARWIN=1 \
	SP_SYSNAME=$(SP_SYSNAME) \
	SP_ARCH=$(SP_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_TOOLCHAIN_FILE) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_LIBTOOL=$(SP_LIBTOOL) \
	SP_RANLIB=$(SP_RANLIB) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT) \
	SP_MACOS_VER=$(SP_MACOS_VER) \
	SP_IOS_VER=$(SP_IOS_VER)

ifdef SP_TOOLCHAIN_PREFIX
FORWARD_VARS += SP_TOOLCHAIN_PREFIX=$(SP_TOOLCHAIN_PREFIX)
endif

ARCH_LIBS = \
	$(SP_INSTALL_PREFIX)/lib/clang/lib/darwin/libclang_rt.osx.a \
	$(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(SP_INSTALL_PREFIX)/share/licenses

$(SP_INSTALL_PREFIX)/lib/clang/lib/darwin/libclang_rt.osx.a: $(MAKE_ROOT)libc++.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/liblzma.a: ../common/xz.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libzstd.a:  ../common/zstd.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a: ../common/brotli.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a: ../common/libjpeg-turbo.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libpng16.a: ../common/libpng.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgif.a: ../common/gif.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libwebp.a: ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libjpeg.a $(SP_INSTALL_PREFIX)/usr/lib/libgif.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libtiff.a: ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/libwebp.a $(SP_INSTALL_PREFIX)/usr/lib/liblzma.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a: ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/libbrotlienc.a $(SP_INSTALL_PREFIX)/usr/lib/libpng16.a $(SP_INSTALL_PREFIX)/usr/lib/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libharfbuzz.a:../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libSheenBidi.a: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libsqlite3.a: ../common/sqlite.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a: ../common/mbedtls.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a:  ../common/nghttp3.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# ngtcp2 QUIC transport with the OpenSSL (ossl) crypto backend: libngtcp2.a +
# libngtcp2_crypto_ossl.a, consumed by curl for HTTP/3 (USE_NGTCP2).
$(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a:  ../common/ngtcp2.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libssl.a: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libgost_engine.a:  ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-openssl.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libssl.a $(SP_INSTALL_PREFIX)/usr/lib/libngtcp2.a $(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/libcurl-mbedtls.a: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/libmbedtls.a
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/libzip.a: ../common/libzip.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/usr/lib/libiwasm-debug.a:  ../common/wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libbacktrace.a: $(MAKE_ROOT)backtrace.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libxml2.a: ../common/libxml2.mk $(SP_INSTALL_PREFIX)/usr/lib/libz.a $(SP_INSTALL_PREFIX)/usr/lib/libzstd.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libvulkan.dylib: $(MAKE_ROOT)vulkan-loader.mk $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libVulkanLayerSettings.a: $(MAKE_ROOT)vulkan-utility.mk $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h \
		$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/$(SP_VVL_LIB): $(MAKE_ROOT)vulkan-validation.mk $(SP_INSTALL_PREFIX)/usr/lib/libVulkanLayerSettings.a \
		$(SP_INSTALL_PREFIX)/usr/include/spirv-tools/libspirv.hpp
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/libMoltenVK.dylib: $(MAKE_ROOT)moltenvk.mk $(SP_INSTALL_PREFIX)/usr/lib/libvulkan.dylib
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h

$(SP_INSTALL_PREFIX)/usr/include/spirv-tools/libspirv.hpp: ../common/spirv-tools.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/spirv-tools/libspirv.hpp

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	touch $(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h

$(SP_INSTALL_PREFIX)/share/licenses:
	rm -rf $(SP_INSTALL_PREFIX)/share/licenses

# libc++ / libc++abi / libunwind cross-built from src/llvm-project-17 into the target
# sysroot (SDK-like: separate abi dylib, /usr/lib install-names, tbds via llvm-readtapi).
# Invoked as a standalone goal by the +open target rule BEFORE `all` so that
# usr/include/c++/v1 exists for the C++ deps. Keyed on the final artifact (libc++.tbd,
# written last by libcxx.mk) so re-running the +open pipeline does not redo the whole
# LLVM runtimes build every time.
libcxx: $(SP_INSTALL_PREFIX)/usr/lib/libc++.tbd

$(SP_INSTALL_PREFIX)/usr/lib/libc++.tbd: $(MAKE_ROOT)libcxx.mk
	$(MAKE) -f $(MAKE_ROOT)libcxx.mk $(FORWARD_VARS)

.PHONY: all libcxx

endif
