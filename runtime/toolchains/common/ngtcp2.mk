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

LIBNAME = ngtcp2

include ../common/configure.mk

ifdef WASM
NGTCP2_OPENSSL := \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_CRYPTO_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a \
	-DOPENSSL_SSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
	-DOPENSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include
else
NGTCP2_OPENSSL :=
endif

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_SHARED_LIBS=OFF \
	-DENABLE_SHARED_LIB=OFF \
	-DENABLE_STATIC_LIB=ON \
	-DENABLE_LIB_ONLY=ON \
	-DENABLE_OPENSSL=ON \
	$(NGTCP2_OPENSSL) \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX)/usr \

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE);
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))

.PHONY: all
