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

# OpenSSL of Tau user modules: its own perl Configure with the target of openssl-tau.conf, the
# flags of common/configure.mk (TAU_USER) through CFLAGS.

.DEFAULT_GOAL := all

LIBNAME = openssl

include ../common/configure.mk

export CMAKECONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/cmake
export PKGCONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig
export libdir=$(SP_INSTALL_PREFIX)/usr/lib
export CFLAGS=$(SP_CFLAGS)

# no-async: no ucontext in the runtime; no-dso, no-module, no-shared: one module, no loader.
CONFIGURE := --config=$(MAKE_ROOT)openssl-tau.conf tau-user-sprt-clang \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	--libdir=lib \
	CC=$(SP_CC) \
	CXX=$(SP_CXX) \
	AR=$(SP_AR) \
	no-asm \
	no-async \
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
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE) && \
		$(MAKE) && \
		$(MAKE) install_sw
	$(call rule_rm,$(LIBNAME))

.PHONY: all
