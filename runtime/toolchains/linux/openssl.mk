# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

OPENSSL_TARGET := linux-x86_64-clang

ifeq ($(ARCH),aarch64)
OPENSSL_TARGET := linux-aarch64
endif

ifdef SP_TOOLCHAIN_PREFIX
export CFLAGS= --sysroot=$(SP_TOOLCHAIN_PREFIX) -idirafter $(SP_TOOLCHAIN_PREFIX)/include_libc $(SP_OPT)
endif

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX) \
	CC=$(SP_CC) \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	no-tests \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-filenames \
	no-shared \
	no-autoload-config

ifeq ($(ARCH),e2k)
CONFIGURE += no-asm -mno-sse4.2
endif

ifeq ($(DEBUG),1)
CONFIGURE += -d
endif

ifeq ($(ARCH),aarch64)
all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)
	rm -rf $(SP_INSTALL_PREFIX)/bin/c_rehash
	sed -i -e 's/ -lssl/ -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/libssl.pc
else
all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)
	mv -f $(SP_INSTALL_PREFIX)/lib64/libssl.a $(SP_INSTALL_PREFIX)/lib/libssl.a 
	mv -f $(SP_INSTALL_PREFIX)/lib64/libcrypto.a $(SP_INSTALL_PREFIX)/lib/libcrypto.a 
	mv -f $(SP_INSTALL_PREFIX)/lib64/pkgconfig/libssl.pc $(SP_INSTALL_PREFIX)/lib/pkgconfig/libssl.pc
	mv -f $(SP_INSTALL_PREFIX)/lib64/pkgconfig/libcrypto.pc $(SP_INSTALL_PREFIX)/lib/pkgconfig/libcrypto.pc
	rm -rf $(SP_INSTALL_PREFIX)/lib64 $(SP_INSTALL_PREFIX)/bin/c_rehash
	sed -i -e 's/ -lssl/ -lgost -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/libssl.pc
	cp $(SP_INSTALL_PREFIX)/lib/pkgconfig/libssl.pc $(SP_INSTALL_PREFIX)/lib/pkgconfig/openssl.pc
	sed -i -e 's/ -lssl/ -lssl -lcrypto/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/openssl.pc
	sed -i -e 's/{exec_prefix}\/lib64/{exec_prefix}\/lib/g' $(SP_INSTALL_PREFIX)/lib/pkgconfig/libssl.pc
endif

.PHONY: all
