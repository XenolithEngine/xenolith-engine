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

LIBNAME = openssl

include ../common/configure.mk

OPENSSL_TARGET := mingw-xwin-clang-x64

export CMAKECONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/cmake
export PKGCONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig
export libdir=$(SP_INSTALL_PREFIX)/usr/lib

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	CC=$(SP_CC) \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	RC=$(SP_RC) \
	CFLAGS="$(SP_WINDOWS_INCLIUDES) -Xclang --dependent-lib=sprt" \
	no-tests \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-docs \
	no-apps \
	no-filenames \
	no-shared \
	no-autoload-config \
	no-makedepend

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install
	$(call rule_rm,$(LIBNAME))
	$(call rule_mv,$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a,$(SP_INSTALL_PREFIX)/usr/lib/crypto.lib)
	$(call rule_mv,$(SP_INSTALL_PREFIX)/usr/lib/libssl.a,$(SP_INSTALL_PREFIX)/usr/lib/ssl.lib)

.PHONY: all
