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

# NuttX OpenSSL build. Mirrors target-linux/openssl.mk: NuttX is hosted-POSIX
# on its own libc, so the OpenSSL Configure target is the linux-aarch64 one
# (the build uses the linux/glibc code paths; the actual libc the link resolves
# against is NuttX's, supplied via -L$(SP_INSTALL_PREFIX)/sysroot/usr/lib at the
# final image link, not here). OpenSSL drives its own perl Configure, so there
# is no toolchain-libs.cmake involvement — the sprt libc + clang-resource
# include paths flow in through CFLAGS = SP_CFLAGS (filled by the NUTTX branch
# of common/configure.mk).

.DEFAULT_GOAL := all

LIBNAME = openssl

include ../common/configure.mk

OPENSSL_TARGET := linux-aarch64

# Sockets stay ON: `no-sock` cascades (Configure's disable table) into no-dgram,
# which drops DTLS, SCTP and QUIC. NuttX carries a BSD socket API (CONFIG_NET),
# and the sprt umbrella fills the two gaps its libc has - sendmmsg()/recvmmsg()
# and the out-of-line __cmsg_nxthdr() - in libc_wrapper/platform/nuttx/stubs.cc.
# TFO disables itself: NuttX has no TCP_FASTOPEN.
#
# OpenSSL's own Configure / make drives CC directly and reads CFLAGS from the
# environment (it does not go through cmake, so SP_C_FLAGS is not picked up
# automatically). Forward the sprt libc + clang-resource include paths and the
# warning relaxations configured by the NUTTX branch of common/configure.mk.
export CFLAGS=$(SP_CFLAGS)
export CXXFLAGS=$(SP_CXXFLAGS)
export LDFLAGS=$(SP_LDFLAGS)

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX)/usr \
	--libdir=lib \
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
	no-autoload-config \
	no-asm \
	no-apps \
	no-ui-console \
	no-afalgeng

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
	rm -rf $(SP_INSTALL_PREFIX)/bin/c_rehash
	sed -i -e 's/ -lssl/ -lssl -lpthread/g' $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig/libssl.pc 2>/dev/null || true

.PHONY: all
