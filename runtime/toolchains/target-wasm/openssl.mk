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

# Freestanding wasm32 OpenSSL build. Mirrors target-windows/openssl.mk: the
# OpenSSL Configure target (wasm32-sprt-clang, from
# replacements/openssl/50-wasm-sprt-clang.conf) carries the flag-independent knobs
# while the full sprt-libc compile flags flow in through CFLAGS = SP_CFLAGS (which
# the WASM branch of common/configure.mk fills with --target, -nostdinc
# -ffreestanding -nostdlib, the wasm feature set, the include_libc search paths and
# the clang -resource-dir). Unlike the deps that go through cmake/autoconf, OpenSSL
# drives its own perl Configure, so there is no toolchain-libs.cmake involvement here.

.DEFAULT_GOAL := all

LIBNAME = openssl

include ../common/configure.mk

OPENSSL_TARGET := wasm32-sprt-clang

export CMAKECONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/cmake
export PKGCONFIGDIR=$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig
export libdir=$(SP_INSTALL_PREFIX)/usr/lib

# The sprt libc headers + wasm target/feature flags are all in SP_CFLAGS; OpenSSL
# reads CFLAGS from the environment and merges it into every compile.
export CFLAGS=$(SP_CFLAGS)

# no-asm     : wasm has no perlasm backend (pure-C crypto).
# no-async   : ucontext/fibre stack-switching does not exist on wasm; force the
#              async_null backend instead of async_posix.
# no-dso/-module/-shared/-legacy : no dynamic loading or loadable providers.
# no-apps/-docs/-tests : build only libcrypto.a / libssl.a.
CONFIGURE := $(OPENSSL_TARGET) \
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
		$(LIB_SRC_DIR)/$(LIBNAME)/Configure $(CONFIGURE); \
		make -j8; \
		make install_sw
	$(call rule_rm,$(LIBNAME))

.PHONY: all
