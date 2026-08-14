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

ifdef EMBOX
# curl.h #include <sys/select.h> only for __NuttX__ (and a handful of other
# OSes), not __EMBOX__. Without it FD_SETSIZE/FD_SET are undeclared in
# cshutdn.c via lib/select.h's FDSET_SOCK.
SP_USER_CFLAGS += -include sys/select.h
endif

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
CONFIGURE += \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_CRYPTO_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a \
	-DOPENSSL_SSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
	-DOPENSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DCURL_DISABLE_NETRC=ON \
	-DUSE_LIBIDN2=OFF \
	-DCURL_USE_LIBPSL=OFF \
	-DENABLE_THREADED_RESOLVER=OFF
endif

ifneq ($(or $(NUTTX),$(EMBOX)),)
# NuttX has no FindThreads-friendly try_compile (toolchain-libs.cmake sets
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY so the link-test executable never
# builds), so find_package(Threads) fails -> ENABLE_THREADED_RESOLVER (curl's
# default-on option) hits its "Threaded resolver requires POSIX Threads" guard.
# Disable it explicitly; NuttX's libc has pthreads at the final image link, not
# at dep-build feature-probe time. Also pin the OpenSSL find module to the
# just-built archives (FindOpenSSL does not see them via CMAKE_FIND_ROOT_PATH
# alone) and skip the optional deps the sysroot does not carry yet (libidn2/psl
# are deferred behind the libuidna/NuttX-network port). HTTP/3 via ngtcp2 is
# disabled (the engine does TLS over the OpenSSL backend, not QUIC, on NuttX).
CONFIGURE += \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX)/usr \
	-DOPENSSL_CRYPTO_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libcrypto.a \
	-DOPENSSL_SSL_LIBRARY=$(SP_INSTALL_PREFIX)/usr/lib/libssl.a \
	-DOPENSSL_INCLUDE_DIR=$(SP_INSTALL_PREFIX)/usr/include \
	-DCURL_DISABLE_NETRC=ON \
	-DUSE_LIBIDN2=OFF \
	-DCURL_USE_LIBPSL=OFF \
	-DENABLE_THREADED_RESOLVER=OFF \
	-DCMAKE_USE_PTHREADS_INIT=ON \
	-DHAVE_THREADS_POSIX=ON \
	-DUSE_NGTCP2=OFF \
	-DCURL_CA_BUNDLE=none \
	-DCURL_CA_PATH=none
# recv/send detection: curl's check_function_exists probes for recv/send fail
# under the toolchain's STATIC_LIBRARY default (the probe never links). Pre-fill
# the POSIX recv/send signature results as cmake cache variables so curl's
# configure writes HAVE_RECV/HAVE_SEND into curl_config.h; the recv/send symbols
# resolve against the NuttX libc at the final image link.
CONFIGURE += \
	-DHAVE_RECV=1 \
	-DRECV_TYPE_ARG1=int -DRECV_TYPE_ARG2="void *" -DRECV_TYPE_ARG3=size_t -DRECV_TYPE_ARG4=int -DRECV_TYPE_RETV=ssize_t \
	-DHAVE_SEND=1 \
	-DSEND_TYPE_ARG1=int -DSEND_TYPE_ARG2="const void *" -DSEND_TYPE_ARG3=size_t -DSEND_TYPE_ARG4=int -DSEND_TYPE_RETV=ssize_t \
	-DHAVE_SELECT=1 -DHAVE_POLL=1 -DHAVE_POLL_FINE=1 \
	-DHAVE_SYS_SELECT_H=1 -DHAVE_POLL_H=1 \
	-DHAVE_SOCKET=1 \
	-DHAVE_FCNTL_H=1 -DHAVE_FCNTL=1 \
	-DHAVE_FCNTL_O_NONBLOCK=1 \
	-DHAVE_NONBLOCKINGSOCKET=1
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
