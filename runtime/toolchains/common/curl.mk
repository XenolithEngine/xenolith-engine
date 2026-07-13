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
# Freestanding wasm: point find_package(OpenSSL) at the sprt-built static libs
# (unix libcrypto.a/libssl.a names, not the crypto.lib/ssl.lib the openssl block
# above assumes) - both curl and ngtcp2 need this to locate the wasm OpenSSL. HTTP/3
# comes from the ngtcp2/nghttp3 stack enabled in the openssl block above (USE_NGTCP2
# implies USE_NGHTTP3). Drop the pieces the wasm sysroot does not ship (IDN2, PSL) or
# that cannot work in the browser sandbox (no raw sockets - the socket API resolves to
# the libc no-op stubs at link time).
CONFIGURE += \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_CRYPTO_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a \
	-DOPENSSL_SSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
	-DOPENSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DUSE_LIBIDN2=OFF \
	-DCURL_USE_LIBPSL=OFF \
	-DENABLE_THREADED_RESOLVER=OFF
# No -DSIZEOF_* pins here anymore either: curl runs its check_type_size probes inside the
# same EXECUTABLE region as the function checks, and configure.mk's --export-if-defined=main
# keeps the probe's main - and the info_size marker it references - alive through
# gc-sections, so the sizes are recovered from the freestanding wasm exe (ILP32 with 64-bit
# ssize_t/off_t/time_t/curl_off_t) instead of coming back empty.
# No -DHAVE_*=OFF list here anymore: curl forces its check_function_exists probes to
# EXECUTABLE, and configure.mk now links those probes against the sprt libc archive with
# NO --allow-undefined, so functions the libc genuinely lacks (fnmatch - curl has its own
# curl_fnmatch; the rlimit fd-limit tuning; the passwd/getpass/if_nametoindex bits) are
# detected as absent automatically and curl falls back to its portable paths.
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
