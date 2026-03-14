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
	-DCURL_USE_LIBSSH2=OFF \
	-DCURL_USE_LIBPSL=OFF \
	-DBUILD_LIBCURL_DOCS=OFF \
	-DBUILD_MISC_DOCS=OFF \
	-DENABLE_CURL_MANUAL=OFF \
	-DENABLE_UNICODE=On \
	-DCURL_DISABLE_LDAP=On \
	-DCURL_STATIC_CRT=On \
	-DCURL_CA_BUNDLE="$(realpath ../replacements/curl/cacert.pem)"

ifdef DARWIN
CONFIGURE += \
	-DSYSTEMCONFIGURATION_FRAMEWORK="SystemConfiguration" \
	-DCOREFOUNDATION_FRAMEWORK="CoreFoundation" \
	-DCORESERVICES_FRAMEWORK="CoreServices" \
	-DUSE_APPLE_IDN=On \
	-DUSE_LIBIDN2=Off
endif

ifdef ANDROID
CONFIGURE += -DLIBIDN2_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a
endif

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="mbedtls" -DCURL_USE_MBEDTLS=ON
endif

ifeq ($(VARIANT),openssl)
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="openssl" -DCURL_USE_OPENSSL=ON \
	-DLIB_EAY=$(SP_INSTALL_PREFIX)/usr/lib/crypto.lib \
	-DSSL_EAY=$(SP_INSTALL_PREFIX)/usr/lib/ssl.lib \
	-DUSE_OPENSSL_QUIC=ON
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
	$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl-$(VARIANT)))
	$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl))

.PHONY: all
