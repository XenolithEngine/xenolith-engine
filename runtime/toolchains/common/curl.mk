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

SP_USER_CFLAGS := -DNGHTTP3_STATICLIB -DNGTCP2_STATICLIB
SP_USER_CXXFLAGS := -DNGHTTP3_STATICLIB -DNGTCP2_STATICLIB

ifdef WINDOWS
SP_USER_CFLAGS += -DSIZEOF_CURL_OFF_T=8 -Wno-incompatible-pointer-types-discards-qualifiers -Wno-cast-function-type-strict
SP_USER_CXXFLAGS += -DSIZEOF_CURL_OFF_T=8 -Wno-incompatible-pointer-types-discards-qualifiers -Wno-cast-function-type-strict
endif

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_CURL_EXE=OFF \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_STATIC_LIBS=ON \
	-DUSE_NGHTTP2=OFF \
	-DCURL_USE_LIBSSH2=OFF \
	-DCURL_USE_LIBPSL=OFF \
	-DBUILD_LIBCURL_DOCS=OFF \
	-DBUILD_MISC_DOCS=OFF \
	-DENABLE_CURL_MANUAL=OFF \
	-DENABLE_UNICODE=On \
	-DCURL_DISABLE_LDAP=On \
	-DCURL_STATIC_CRT=On \
	-DCURL_CA_BUNDLE="$(realpath ../replacements/curl/cacert.pem)"

# IDN comes from the runtime's own UTS-46 engine, which exports the libidn2 C ABI
# (runtime/src/idn/SPRuntimeIdn2Api.cpp) on every target. That replaces what used to
# be three different answers: USE_WIN32_IDN on Windows (IDNA2003), USE_APPLE_IDN on
# Darwin, and no IDN at all on wasm.
#
# There is no libidn2.a to point cmake at - the symbols resolve at the final link of
# the application - so LIBIDN2_LIBRARY names an archive that is present but
# irrelevant, purely to satisfy find_package. The header is the real dependency, and
# each target Makefile installs it into the sysroot.
CONFIGURE += \
	-DUSE_LIBIDN2=ON \
	-DLIBIDN2_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libnghttp3.a \
	-DLIBIDN2_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include

ifdef DARWIN
CONFIGURE += \
	-DSYSTEMCONFIGURATION_FRAMEWORK="SystemConfiguration" \
	-DCOREFOUNDATION_FRAMEWORK="CoreFoundation" \
	-DCORESERVICES_FRAMEWORK="CoreServices" \
	-DUSE_APPLE_IDN=Off
endif

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="mbedtls" -DCURL_USE_MBEDTLS=ON
endif

ifeq ($(VARIANT),openssl)
# HTTP/3 via ngtcp2 (QUIC) + nghttp3 (framing) on the OpenSSL crypto backend. NOTE: curl
# 8.20 has NO USE_OPENSSL_QUIC option - it is a no-op and does NOT turn on the HTTP3
# feature (curl_add_if("HTTP3" USE_NGTCP2 OR USE_QUICHE)). USE_NGTCP2 does: with OpenSSL
# 3.5+ curl calls find_package(NGTCP2 COMPONENTS ossl) -> libngtcp2 + libngtcp2_crypto_ossl
# (needs ngtcp2 >= 1.12.0) and pulls nghttp3 automatically. The libs are static, so tell
# FindNGTCP2 to resolve the *_static/.a via pkg-config (NGTCP2_STATICLIB comes from
# SP_USER_CFLAGS above; the LIB_EAY/SSL_EAY hints were dead in 8.20).
CONFIGURE += \
	-DCURL_DEFAULT_SSL_BACKEND="openssl" -DCURL_USE_OPENSSL=ON \
	-DUSE_NGTCP2=ON \
	-DNGTCP2_USE_STATIC_LIBS=ON
endif

ifdef WASM
CONFIGURE += \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_CRYPTO_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a \
	-DOPENSSL_SSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
	-DOPENSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DCURL_DISABLE_NETRC=ON \
	-DCURL_USE_LIBPSL=OFF \
	-DENABLE_THREADED_RESOLVER=OFF
endif

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
	$(if $(WINDOWS),,$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl-$(VARIANT))))
	$(if $(WINDOWS),,$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl)))
	$(if $(WINDOWS),$(call rule_cp,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,libcurl),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,curl-$(VARIANT))))
	$(if $(WINDOWS),$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,libcurl)))

.PHONY: all
