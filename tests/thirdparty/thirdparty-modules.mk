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

# Catalog of "raw third-party library" modules: no sources, no engine code -
# only the link line needed to use a shipped library through its own headers.
# The headers themselves need no -I: $(TARGET_SYSROOT)/usr/include is already on
# the global include path for every target.
#
# Names are prefixed with thirdparty_ so they cannot collide with the engine
# module namespace (stappler_*, xenolith_*, runtime_*).

THIRDPARTY_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

#
# OpenSSL: libssl + libcrypto (+ the GOST engine that the toolchain builds
# against them, so that engine-loading code links too)
#

MODULE_THIRDPARTY_OPENSSL_DEFINED_IN := $(THIRDPARTY_MODULE_DIR)/thirdparty-modules.mk
MODULE_THIRDPARTY_OPENSSL_LIBS := -l:libssl.a -l:libcrypto.a
MODULE_THIRDPARTY_OPENSSL_LIBS_SHARED := -lssl -lcrypto
MODULE_THIRDPARTY_OPENSSL_SRCS_DIRS :=
MODULE_THIRDPARTY_OPENSSL_SRCS_OBJS :=
MODULE_THIRDPARTY_OPENSSL_INCLUDES_DIRS :=
MODULE_THIRDPARTY_OPENSSL_INCLUDES_OBJS :=
MODULE_THIRDPARTY_OPENSSL_DEPENDS_ON :=

# libcrypto's zlib support and the pthread-based locking
ifeq ($(TARGET_SYSTEM),Linux)
MODULE_THIRDPARTY_OPENSSL_LIBS += -l:libz.a -lpthread
endif

ifdef WIN32
MODULE_THIRDPARTY_OPENSSL_LIBS += -l:libz.a
endif

$(call define_module, thirdparty_openssl, MODULE_THIRDPARTY_OPENSSL)

#
# cURL, OpenSSL flavour (libcurl-openssl.a), with everything it was configured
# with: HTTP/3 (ngtcp2 + nghttp3), content encodings (zlib/brotli/zstd) and IDN.
#

MODULE_THIRDPARTY_CURL_DEFINED_IN := $(THIRDPARTY_MODULE_DIR)/thirdparty-modules.mk
MODULE_THIRDPARTY_CURL_LIBS := -l:libcurl-openssl.a \
	-l:libngtcp2_crypto_ossl.a -l:libngtcp2.a -l:libnghttp3.a \
	-l:libz.a -l:libzstd.a -l:libbrotlidec.a -l:libbrotlicommon.a
MODULE_THIRDPARTY_CURL_LIBS_SHARED := -lcurl
MODULE_THIRDPARTY_CURL_SRCS_DIRS :=
MODULE_THIRDPARTY_CURL_SRCS_OBJS :=
MODULE_THIRDPARTY_CURL_INCLUDES_DIRS :=
MODULE_THIRDPARTY_CURL_INCLUDES_OBJS :=
MODULE_THIRDPARTY_CURL_DEPENDS_ON := thirdparty_openssl

# libcurl is a static library here, so its headers must not declare the symbols
# dllimport; ngtcp2/nghttp3 need the same treatment (they are built with
# -DNGHTTP3_STATICLIB -DNGTCP2_STATICLIB, see runtime/toolchains/common/curl.mk).
MODULE_THIRDPARTY_CURL_FLAGS := -DCURL_STATICLIB -DNGHTTP3_STATICLIB -DNGTCP2_STATICLIB

# IDN: the runtime implements the libidn2 C ABI itself (runtime/src/idn), on every
# target, so nothing is linked here. Linking the sysroot libidn2.a as well would be
# a duplicate definition of idn2_lookup_u8 and friends - it only ever worked because
# archive members are pulled in lazily.

$(call define_module, thirdparty_curl, MODULE_THIRDPARTY_CURL)
