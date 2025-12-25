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

LIBNAME = curl

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_AUTOCONF) \
	--enable-optimize \
	--enable-symbol-hiding \
	--enable-http \
	--enable-ftp \
	--enable-file \
	--enable-smtp \
	--enable-proxy \
	--enable-ipv6 \
	--enable-unix-sockets \
	--enable-cookies \
	--enable-shared=no \
	--enable-static=yes \
	--enable-websockets \
	--disable-warnings \
	--disable-curldebug \
	--disable-debug \
	--disable-dependency-tracking \
	--disable-ldap \
	--disable-ldaps \
	--disable-rtsp \
	--disable-dict \
	--disable-telnet \
	--disable-tftp \
	--disable-pop3 \
	--disable-imap \
	--disable-smb \
	--disable-gopher \
	--disable-docs \
	--disable-manual \
	--disable-ntlm \
	--disable-versioned-symbols \
	--disable-verbose  \
	--disable-sspi \
	--with-zlib \
	--with-brotli \
	--with-libidn2 \
	--with-zstd \
	--without-ca-path \
	--without-libssh2 \
	--without-librtmp \
	--without-libpsl \
	--without-ngtcp2 \
	--with-ca-bundle=$(realpath ../replacements/curl/cacert.pem) \
	--without-ca-path

ifeq ($(VARIANT),mbedtls)
CONFIGURE += \
	--with-mbedtls \
	--without-openssl \
	--without-gnutls
endif

ifeq ($(VARIANT),openssl)
CONFIGURE += \
	--without-mbedtls \
	--with-openssl \
	--without-gnutls \
	--with-nghttp3 \
	--with-openssl-quic
endif

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(LIB_SRC_DIR)/$(LIBNAME)/configure $(CONFIGURE); \
		make -j8; \
		make install
	mv -f $(SP_INSTALL_PREFIX)/lib/libcurl.a $(SP_INSTALL_PREFIX)/lib/libcurl-$(VARIANT).a
	rm -rf $(LIBNAME)

.PHONY: all
