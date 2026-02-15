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

LIBNAME = curl

SP_USER_CFLAGS := -DNGHTTP3_STATICLIB
SP_USER_CXXFLAGS := -DNGHTTP3_STATICLIB

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_CURL_EXE=OFF \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_STATIC_LIBS=ON \
	-DUSE_NGHTTP2=OFF \
	-DUSE_WIN32_IDN=ON \
	-DCURL_USE_LIBPSL=OFF \
	-DBUILD_LIBCURL_DOCS=OFF \
	-DBUILD_MISC_DOCS=OFF \
	-DENABLE_CURL_MANUAL=OFF \
	-DCURL_CA_BUNDLE="$(realpath ../replacements/curl/cacert.pem)"

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="mbedtls" -DCURL_USE_MBEDTLS=ON
endif

ifeq ($(VARIANT),openssl)
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="openssl" -DCURL_USE_OPENSSL=ON \
	-DLIB_EAY=$(SP_INSTALL_PREFIX)/lib/crypto.lib \
	-DSSL_EAY=$(SP_INSTALL_PREFIX)/lib/ssl.lib \
	-DUSE_OPENSSL_QUIC=ON
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(MKDIR) $(LIBNAME) | Out-Null
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
	$(call rule_cp,$(SP_INSTALL_PREFIX)/lib/libcurl.lib,$(SP_INSTALL_PREFIX)/lib/curl-$(VARIANT).lib)

.PHONY: all
