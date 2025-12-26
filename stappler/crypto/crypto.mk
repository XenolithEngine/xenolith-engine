# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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


#
# GnuTLS backend
#
# GnuTLS backend available only on Linux for OS distribution
#

ifdef LINUX

MODULE_STAPPLER_CRYPTO_GNUTLS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_CRYPTO_GNUTLS_LIBS := -lgnutls -lnettle -lgmp -lhogweed
MODULE_STAPPLER_CRYPTO_GNUTLS_LIBS_SHARED := -lgnutls -lnettle -lgmp -lhogweed
MODULE_STAPPLER_CRYPTO_GNUTLS_FLAGS :=
MODULE_STAPPLER_CRYPTO_GNUTLS_SRCS_DIRS :=
MODULE_STAPPLER_CRYPTO_GNUTLS_SRCS_OBJS :=
MODULE_STAPPLER_CRYPTO_GNUTLS_INCLUDES_DIRS :=
MODULE_STAPPLER_CRYPTO_GNUTLS_INCLUDES_OBJS :=
MODULE_STAPPLER_CRYPTO_GNUTLS_DEPENDS_ON :=

# module name resolution
MODULE_stappler_crypto_gnutls := MODULE_STAPPLER_CRYPTO_GNUTLS

endif # LINUX


#
# OpenSSL backend
#

MODULE_STAPPLER_CRYPTO_OPENSSL_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_CRYPTO_OPENSSL_LIBS := -l:libgost.a -l:libssl.a -l:libcrypto.a
MODULE_STAPPLER_CRYPTO_OPENSSL_LIBS_SHARED := -lssl -lcrypto
MODULE_STAPPLER_CRYPTO_OPENSSL_FLAGS :=
MODULE_STAPPLER_CRYPTO_OPENSSL_SRCS_DIRS :=
MODULE_STAPPLER_CRYPTO_OPENSSL_SRCS_OBJS :=
MODULE_STAPPLER_CRYPTO_OPENSSL_INCLUDES_DIRS :=
MODULE_STAPPLER_CRYPTO_OPENSSL_INCLUDES_OBJS :=
MODULE_STAPPLER_CRYPTO_OPENSSL_DEPENDS_ON :=

ifdef LINUX
MODULE_STAPPLER_CRYPTO_OPENSSL_LIBS += -l:libz.a -lpthread
endif

ifdef ANDROID
MODULE_STAPPLER_CRYPTO_OPENSSL_LIBS += -lz
endif

ifdef WIN32
MODULE_STAPPLER_CRYPTO_OPENSSL_LIBS += -lz -lcrypt32 -ladvapi32 -luser32
endif

# module name resolution
MODULE_stappler_crypto_openssl := MODULE_STAPPLER_CRYPTO_OPENSSL


#
# MbedTLS backend
#

MODULE_STAPPLER_CRYPTO_MBEDTLS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_CRYPTO_MBEDTLS_LIBS := -l:libmbedtls.a -l:libmbedcrypto.a -l:libmbedx509.a
MODULE_STAPPLER_CRYPTO_MBEDTLS_FLAGS :=
MODULE_STAPPLER_CRYPTO_MBEDTLS_SRCS_DIRS :=
MODULE_STAPPLER_CRYPTO_MBEDTLS_SRCS_OBJS :=
MODULE_STAPPLER_CRYPTO_MBEDTLS_INCLUDES_DIRS :=
MODULE_STAPPLER_CRYPTO_MBEDTLS_INCLUDES_OBJS :=
MODULE_STAPPLER_CRYPTO_MBEDTLS_DEPENDS_ON :=

ifdef WIN32
MODULE_STAPPLER_CRYPTO_MBEDTLS_LIBS += -lcrypt32 -ladvapi32
endif

# module name resolution
MODULE_stappler_crypto_mbedtls := MODULE_STAPPLER_CRYPTO_MBEDTLS


MODULE_STAPPLER_CRYPTO_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_CRYPTO_LIBS :=
MODULE_STAPPLER_CRYPTO_FLAGS :=
MODULE_STAPPLER_CRYPTO_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/crypto
MODULE_STAPPLER_CRYPTO_SRCS_OBJS := 
MODULE_STAPPLER_CRYPTO_INCLUDES_DIRS :=
MODULE_STAPPLER_CRYPTO_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/crypto
MODULE_STAPPLER_CRYPTO_DEPENDS_ON := stappler_crypto_$(STAPPLER_CRYPTO_DEFAULT) stappler_data

MODULE_STAPPLER_CRYPTO_SHARED_CONSUME := \
	stappler_crypto_$(STAPPLER_CRYPTO_DEFAULT)

# spec

MODULE_STAPPLER_CRYPTO_SHARED_SPEC_SUMMARY := libstappler cryptographic functions

define MODULE_STAPPLER_CRYPTO_SHARED_SPEC_DESCRIPTION
Module libstappler-crypto provides interface for:
- Private/public keys
- Symmetric block cyphers
- JWT with signatures and encrypted data
- GOST cryptography
endef

# module name resolution
$(call define_module, stappler_crypto, MODULE_STAPPLER_CRYPTO)
