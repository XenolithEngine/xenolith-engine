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

OPENSSL_TARGET := darwin64-$(SP_ARCH)

ifdef SP_TOOLCHAIN_PREFIX
export CFLAGS=$(SP_CFLAGS)
endif

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	CC=$(SP_CC) \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	LIBTOOL=$(SP_LIBTOOL) \
	RANLIB=$(SP_RANLIB) \
	no-apps \
	no-tests \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-filenames \
	no-shared \
	no-autoload-config

ifeq ($(SP_ARCH),e2k)
CONFIGURE += no-asm -mno-sse4.2
endif

ifeq ($(DEBUG),1)
CONFIGURE += -d
endif

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	rm -rf $(LIBNAME)

.PHONY: all
