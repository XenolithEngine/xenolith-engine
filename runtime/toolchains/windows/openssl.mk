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

LIBNAME = openssl

include ../common/configure.mk

OPENSSL_TARGET := VC-WIN64A

CONFIGURE := $(OPENSSL_TARGET) \
	--prefix=$(SP_INSTALL_PREFIX) \
	--release \
	no-tests \
	no-module \
	no-legacy \
	no-srtp \
	no-srp \
	no-dso \
	no-docs \
	no-filenames \
	no-shared \
	no-autoload-config \
	no-makedepend

VCINIT := C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat

undefine MAKEFLAGS

all:
	$(call rule_rm,$(LIBNAME))
	$(MKDIR) $(LIBNAME) | Out-Null
	cd $(LIBNAME); perl ..\..\src\openssl\Configure $(CONFIGURE)
	cmd.exe /c "`"$(VCINIT)`" x64 & cd /d `"$(LIBNAME)`" & nmake /f makefile"
	cmd.exe /c "`"$(VCINIT)`" x64 & cd /d `"$(LIBNAME)`" & nmake /f makefile install"
	$(call rule_rm,$(LIBNAME))
	$(call rule_mv,$(SP_INSTALL_PREFIX)/lib/libcrypto.lib,$(SP_INSTALL_PREFIX)/lib/crypto.lib)
	$(call rule_mv,$(SP_INSTALL_PREFIX)/lib/libssl.lib,$(SP_INSTALL_PREFIX)/lib/ssl.lib)

.PHONY: all
