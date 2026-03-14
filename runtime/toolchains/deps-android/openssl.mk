# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

.DEFAULT_GOAL := all

LIBNAME = openssl

include ../common/configure.mk

ifdef ANDROID
ifndef NDK
OPENSSL_CC := clang
OPENSSL_CXX := clang++
OPENSSL_AR := llvm-ar
else
OPENSSL_CC := $(SP_CC)
OPENSSL_CXX := $(SP_CXX)
OPENSSL_AR := $(SP_AR)
endif
else
OPENSSL_CC := $(SP_CC)
OPENSSL_CXX := $(SP_CXX)
OPENSSL_AR := $(SP_AR)
endif

CONFIGURE := android-$(ANDROID_ARCH) \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	CC=$(OPENSSL_CC) \
	CXX=$(OPENSSL_CXX) \
	AR=$(OPENSSL_AR) \
	no-apps \
	no-tests \
	no-shared \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-filenames \
	no-autoload-config \
	$(SP_OPT)

ifeq ($(ARCH),armeabi-v7a)
CONFIGURE += no-asm
endif

NDK_ROOT := $(NDK)
NDK_PATH := $(NDKPATH)

ifdef ANDROID
ifndef NDK
NDK_ROOT := $(SP_TOOLCHAIN_PREFIX)
NDK_PATH := $(SP_TOOLCHAIN_PREFIX)/bin:$(SP_TOOLCHAIN_PREFIX)/host/bin
endif
endif

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		export ANDROID_NDK_ROOT=$(NDK_ROOT); \
		export PATH=$(NDK_PATH):$$PATH; \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)
	sed -i -e 's/ -lssl/ -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc

.PHONY: all
