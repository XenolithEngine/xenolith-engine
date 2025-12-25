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

VARIANT ?= mbedtls

LIBNAME = libzip

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DENABLE_BZIP2=OFF \
	-DENABLE_LZMA=OFF \
	-DENABLE_ZSTD=OFF \
	-DBUILD_TOOLS=OFF \
	-DBUILD_REGRESS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOC=OFF \
	-DZLIB_LIBRARY="-lz"

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	-DMbedTLS_LIBRARY=$(SP_INSTALL_PREFIX)/lib/libmbedcrypto.a -DMbedTLS_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/include \
	-DENABLE_GNUTLS=OFF \
	-DENABLE_COMMONCRYPTO=OFF \
	-DENABLE_OPENSSL=OFF
endif

ifeq ($(VARIANT),openssl)
CONFIGURE += \
	-DENABLE_MBEDTLS=OFF \
	-DENABLE_GNUTLS=OFF \
	-DENABLE_COMMONCRYPTO=OFF \
	-DENABLE_OPENSSL=ON \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX) \
	-DOpenSSL_LIBRARY=$(SP_INSTALL_PREFIX)/lib/libcrypto.a -DOpenSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/include
endif

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(PRE_CONFIGURE) cmake $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	mv -f $(SP_INSTALL_PREFIX)/lib/libzip.a $(SP_INSTALL_PREFIX)/lib/libzip-$(VARIANT).a
	rm -rf $(LIBNAME)
