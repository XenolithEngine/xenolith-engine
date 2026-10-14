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

# The third-party libraries of the engine for Tau user modules on libsprt.so, built by the
# common recipes into SP_INSTALL_PREFIX (the target `libs` of the Makefile). The recipes run from
# this directory one at a time: each uses a work directory named after its library here.

.DEFAULT_GOAL := all
.NOTPARALLEL:

LIB_SRC_DIR ?= $(abspath ../src)

LIBS := \
	libz.a \
	libbz2.a \
	libbrotlienc.a \
	libjpeg.a \
	libpng16.a \
	libgif.a \
	libwebp.a \
	libfreetype.a \
	libharfbuzz.a \
	libSheenBidi.a \
	libssl.a \
	libgost_engine.a

FORWARD_VARS := \
	TAU_USER=1 \
	SP_SYSNAME=TAU_USER \
	SP_ARCH=$(SP_ARCH) \
	SP_ARCH_TARGET_CLANG=$(SP_ARCH_TARGET_CLANG) \
	SP_TAU_ARCH_FLAGS="$(SP_TAU_ARCH_FLAGS)" \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_INSTALL_PREFIX)/toolchain-libs.cmake \
	SP_TOOLCHAIN_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TARGET=$(SP_ARCH_TARGET_CLANG) \
	SP_RUNTIME_ROOT=$(SP_RUNTIME_ROOT) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="-O3" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

L := $(SP_INSTALL_PREFIX)/usr/lib

all: $(addprefix $(L)/,$(LIBS))

$(L)/libz.a: ../common/zlib.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libbz2.a: ../common/bzip2.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libbrotlienc.a: ../common/brotli.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libjpeg.a: ../common/libjpeg-turbo.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libpng16.a: ../common/libpng.mk $(L)/libz.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libgif.a: ../common/gif.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libwebp.a: ../common/libwebp.mk $(L)/libpng16.a $(L)/libjpeg.a $(L)/libgif.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libfreetype.a: ../common/freetype.mk $(L)/libbrotlienc.a $(L)/libpng16.a $(L)/libbz2.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libharfbuzz.a: ../common/harfbuzz.mk $(L)/libfreetype.a
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libSheenBidi.a: ../common/sheenbidi.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libssl.a: openssl.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(L)/libgost_engine.a: ../common/gost.mk $(L)/libssl.a
	$(MAKE) -f $< $(FORWARD_VARS)

.PHONY: all
