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

.DEFAULT_GOAL := all

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))

include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

TARGET_FILE := $(MAKE_ROOT)init-target.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk

TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

SP_OPT := -O3

LIBS := \
	z.lib \
	bz2.lib \
	lzma.lib \
	zstd.lib \
	brotlienc.lib \
	jpeg.lib \
	png16.lib \
	gif.lib \
	webp.lib \
	tiff.lib \
	freetype.lib \
	harfbuzz.lib \
	SheenBidi.lib \
	sqlite3.lib \
	nghttp3.lib \
	ngtcp2.lib \
	ssl.lib \
	gost_engine.lib \
	curl-openssl.lib \
	zip.lib

INCLUDES := \
	spirv/unified1/spirv.h \
	vulkan/vulkan.h \
	simde/simde-arch.h

ifndef SP_ARCH

ARCHS := x86_64

LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

SP_ARCH_COMMON := \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..)

SP_ARCH_x86_64_INFO_BASE := \
	$(SP_ARCH_COMMON) \
	SP_ARCH_WIN=x64 \
	SP_ARCH=x86_64 \
	SP_RES_ARCH=x86_64 \
	SP_ARCH_TARGET_CLANG=x86_64-pc-windows-msvc

SP_ARCH_x86_64_INFO := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc

SP_ARCH_x86_64_INSTALL := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc \
	T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc

# The "+dll" variant is the same sysroot built around sprt.dll instead of the static
# runtime archive. Static and shared cannot share a directory: usr/lib/sprt.lib is the
# runtime archive in one case and sprt.dll's import library in the other, same path and
# silently incompatible, and the sprt.lib rule does not depend on SPRT_TOOLCHAIN_SHARED,
# so flipping the flag over an up-to-date sysroot rebuilds nothing and leaves the other
# mode's artifact in place. Only the directories carry the suffix; the compiler triple
# (SP_TARGET, SP_ARCH_TARGET_CLANG) stays the same, exactly as for apple's "+open".
SP_ARCH_x86_64_DLL_INFO := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SP_TARGET=x86_64-pc-windows-msvc \
	SPRT_TOOLCHAIN_SHARED=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc+dll

SP_ARCH_x86_64_DLL_INSTALL := \
	$(SP_ARCH_x86_64_INFO_BASE) \
	SPRT_TOOLCHAIN_SHARED=1 \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc+dll \
	T_TARGET=$(TOOLCHAIN_TARGETS)/x86_64-pc-windows-msvc+dll

SP_ARCH_aarch64_INFO_BASE := \
	$(SP_ARCH_COMMON) \
	SP_ARCH_WIN=arm64 \
	SP_ARCH=aarch64 \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-pc-windows-msvc

SP_ARCH_aarch64_INFO := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	SP_TARGET=aarch64-pc-windows-msvc \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc

SP_ARCH_aarch64_INSTALL := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc \
	T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-pc-windows-msvc

SP_ARCH_aarch64_DLL_INFO := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	SP_TARGET=aarch64-pc-windows-msvc \
	SPRT_TOOLCHAIN_SHARED=1 \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc+dll

SP_ARCH_aarch64_DLL_INSTALL := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	SPRT_TOOLCHAIN_SHARED=1 \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc+dll \
	T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-pc-windows-msvc+dll

all: $(ARCHS)

clean:

# The two leading passes exist for one dependency that runs against the grain of the rest:
# linking sprt.dll needs clang_rt.builtins, which this makefile builds, while compiler-rt in
# turn needs what init-target.mk lays down first (the toolchain file, the `host` symlink, the
# Win32 import libraries). On a rebuild the artifacts are already there and both passes are
# no-ops, which is why this only ever breaks from an empty intermediate directory.
x86_64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_INFO) toolchain
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO) compiler-rt
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_x86_64_INSTALL)

aarch64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO) toolchain
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_aarch64_INFO) compiler-rt
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_aarch64_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_aarch64_INSTALL)

# The shared sysroot carries the runtime layer only - the toolchain file, the Win32 import
# libraries, compiler-rt, simde and sprt.dll/sprt.lib - which is the whole of what its one
# consumer, host-windows-msvc/cross, reads out of it: stage0 builds its zlib and libxml2
# straight from src/, and xlmake pulls stappler_makefile/filesystem/runtime, none of which
# names a third-party library. Building the dependency set here would also mean building it
# against an import library, where link-time feature probes (check_function_exists and its
# autotools equivalents) cannot see the libc surface by plain name and answer wrong.
#
# So the fourth pass of the arch recipes above - the one that runs this makefile's own `all`
# - is the one thing missing here. The first three keep their order for the same reason as
# above: linking sprt.dll pulls in clang_rt.builtins, and compiler-rt needs what
# init-target.mk lays down first.
x86_64+dll:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc+dll/usr/include $(TOOLCHAIN_OUTPUT_DIR)/x86_64-pc-windows-msvc+dll/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_DLL_INFO) toolchain
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_x86_64_DLL_INFO) compiler-rt
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_x86_64_DLL_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_x86_64_DLL_INSTALL)

aarch64+dll:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc+dll/usr/include $(TOOLCHAIN_OUTPUT_DIR)/aarch64-pc-windows-msvc+dll/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_DLL_INFO) toolchain
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_aarch64_DLL_INFO) compiler-rt
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_DLL_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_aarch64_DLL_INSTALL)

.PHONY: all x86_64 aarch64 x86_64+dll aarch64+dll

else # SP_ARCH

OUT_SYSROOT ?= $(realpath ../$(SP_TARGET))

SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)

SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)
SP_RC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-rc)

LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	WINDOWS=1 \
	SP_SYSNAME=Windows \
	SP_ARCH=$(SP_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	SP_TOOLCHAIN_PREFIX=$(TOOLCHAIN_OUTPUT_DIR) \
	SP_TARGET=$(SP_TARGET) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_RC=$(SP_RC) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)


ARCH_LIBS = \
	$(addprefix $(SP_INSTALL_PREFIX)/usr/lib/,$(LIBS))

ARCH_INCLUDES  = $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(INCLUDES))

ARCH_BINS  = $(addprefix $(SP_INSTALL_PREFIX)/bin/,$(BINS))

all: $(ARCH_LIBS) $(ARCH_INCLUDES) $(ARCH_BINS) $(SP_INSTALL_PREFIX)/share/licenses \
	$(SP_INSTALL_PREFIX)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib

$(SP_INSTALL_PREFIX)/usr/lib/z.lib:  ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/bz2.lib: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/lzma.lib:  ../common/xz.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/zstd.lib:  ../common/zstd.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/brotlienc.lib:  ../common/brotli.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/jpeg.lib:  ../common/libjpeg-turbo.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

# libpng's find_package(ZLIB) is a hard error, so z.lib has to be declared here the way the
# other dependent libraries below declare theirs. Serial builds got away without it - LIBS
# happens to list z.lib first - but under -j the configure step loses the race.
$(SP_INSTALL_PREFIX)/usr/lib/png16.lib:  ../common/libpng.mk $(SP_INSTALL_PREFIX)/usr/lib/z.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/gif.lib:  ../common/gif.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/webp.lib:  ../common/libwebp.mk $(SP_INSTALL_PREFIX)/usr/lib/png16.lib $(SP_INSTALL_PREFIX)/usr/lib/jpeg.lib $(SP_INSTALL_PREFIX)/usr/lib/gif.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/tiff.lib:  ../common/tiff.mk $(SP_INSTALL_PREFIX)/usr/lib/webp.lib $(SP_INSTALL_PREFIX)/usr/lib/lzma.lib $(SP_INSTALL_PREFIX)/usr/lib/zstd.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/freetype.lib:  ../common/freetype.mk $(SP_INSTALL_PREFIX)/usr/lib/brotlienc.lib $(SP_INSTALL_PREFIX)/usr/lib/png16.lib $(SP_INSTALL_PREFIX)/usr/lib/bz2.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/harfbuzz.lib:  ../common/harfbuzz.mk $(SP_INSTALL_PREFIX)/usr/lib/freetype.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/SheenBidi.lib: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/sqlite3.lib: ../common/sqlite.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/nghttp3.lib: ../common/nghttp3.mk
	$(MAKE) -f  $< $(FORWARD_VARS)

# ngtcp2 QUIC transport with the OpenSSL (ossl) crypto backend: ngtcp2.lib +
# ngtcp2_crypto_ossl.lib, consumed by curl for HTTP/3 (USE_NGTCP2).
$(SP_INSTALL_PREFIX)/usr/lib/ngtcp2.lib: ../common/ngtcp2.mk $(SP_INSTALL_PREFIX)/usr/lib/ssl.lib
	$(MAKE) -f  $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/ssl.lib: $(MAKE_ROOT)openssl.mk
	$(MAKE) -f $(MAKE_ROOT)openssl.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/gost_engine.lib: ../common/gost.mk $(SP_INSTALL_PREFIX)/usr/lib/ssl.lib
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/idn2.h: $(MAKE_ROOT)../../include/sprt/compat/idn2.h
	$(MKDIR) $(SP_INSTALL_PREFIX)/usr/include
	cp -f $< $@

$(SP_INSTALL_PREFIX)/usr/lib/curl-openssl.lib: ../common/curl.mk $(SP_INSTALL_PREFIX)/usr/lib/ssl.lib $(SP_INSTALL_PREFIX)/usr/lib/ngtcp2.lib $(SP_INSTALL_PREFIX)/usr/lib/nghttp3.lib $(SP_INSTALL_PREFIX)/usr/include/idn2.h
	$(MAKE) -f $< $(FORWARD_VARS) VARIANT=openssl

$(SP_INSTALL_PREFIX)/usr/lib/curl-mbedtls.lib: $(MAKE_ROOT)curl.mk $(SP_INSTALL_PREFIX)/usr/lib/mbedtls.lib $(SP_INSTALL_PREFIX)/usr/include/idn2.h
	$(MAKE) -f $(MAKE_ROOT)curl.mk $(FORWARD_VARS) VARIANT=mbedtls

$(SP_INSTALL_PREFIX)/usr/lib/zip.lib: ../common/libzip.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/lib/iwasm-release.lib $(SP_INSTALL_PREFIX)/usr/lib/iwasm-debug.lib: $(MAKE_ROOT)wamr.mk $(SP_INSTALL_PREFIX)/usr/lib/z.lib
	$(MAKE) -f $(MAKE_ROOT)wamr.mk $(FORWARD_VARS)

$(SP_INSTALL_PREFIX)/usr/include/vulkan/vulkan.h: ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr//include/vulkan/vulkan.h)

$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h: ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr/include/spirv/unified1/spirv.h)

$(SP_INSTALL_PREFIX)/usr/include/simde/simde-arch.h: ../common/simde.mk
	$(MAKE) -f $< $(FORWARD_VARS)
	$(call rule_touch,$(SP_INSTALL_PREFIX)/usr//include/simde/simde-arch.h)

# cp cannot create the intermediate share/ - nothing else in a clean build makes it, so the
# copy fails with "No such file or directory" on the destination.
$(SP_INSTALL_PREFIX)/share/licenses:
	$(call rule_rm,$(SP_INSTALL_PREFIX)/share/licenses)
	$(call rule_mkdir,$(SP_INSTALL_PREFIX)/share)
	$(call rule_cpr,../licenses,$(SP_INSTALL_PREFIX)/share/licenses)

# compiler-rt (builtins, profile, orc) is cross-built inside the target with the
# current host clang against the MSVC SDK/CRT, instead of being copied from host.
$(SP_INSTALL_PREFIX)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib: $(MAKE_ROOT)compiler_rt.mk
	$(MAKE) -f $< $(FORWARD_VARS)

# Named entry point for the ordering pass above, which cannot spell the path itself: the
# arch recipes run before SP_ARCH is set and SP_INSTALL_PREFIX exists.
compiler-rt: $(SP_INSTALL_PREFIX)/lib/clang/lib/windows/clang_rt.builtins-$(SP_ARCH).lib

.PHONY: all compiler-rt

endif
