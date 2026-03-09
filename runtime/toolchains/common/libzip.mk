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

VARIANT ?= mbedtls

LIBNAME = libzip

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_SHARED_LIBS=OFF \
	-DENABLE_BZIP2=ON \
	-DENABLE_LZMA=ON \
	-DENABLE_ZSTD=ON \
	-DBUILD_TOOLS=OFF \
	-DBUILD_REGRESS=OFF \
	-DBUILD_OSSFUZZ=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOC=OFF \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX)/usr \
	-DCMAKE_INSTALL_LIBDIR=$(SP_INSTALL_PREFIX)/usr/lib \
	-DCMAKE_INSTALL_INCLUDEDIR=$(SP_INSTALL_PREFIX)/usr/include \

ifdef WINDOWS

CONFIGURE += \
	-DENABLE_WINDOWS_CRYPTO=ON \
	-DENABLE_MBEDTLS=OFF \
	-DENABLE_OPENSSL=OFF

else # WINDOWS

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	-DMbedTLS_LIBRARY="$(SP_INSTALL_PREFIX)/usr/lib/libmbedcrypto.a" -DMbedTLS_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include \
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
	-DOPENSSL_ROOT=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOpenSSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a -DOpenSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include
endif

ifeq ($(VARIANT),gnutls)
CONFIGURE += \
	-DENABLE_MBEDTLS=OFF \
	-DENABLE_GNUTLS=ON \
	-DENABLE_COMMONCRYPTO=OFF \
	-DENABLE_OPENSSL=OFF
endif

endif # WINDOWS

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
	$(if $(LINUX),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip-$(VARIANT))))
	$(if $(LINUX),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip)))
	$(if $(ANDROID),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip-$(VARIANT))))
	$(if $(ANDROID),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,zip)))

.PHONY: all
