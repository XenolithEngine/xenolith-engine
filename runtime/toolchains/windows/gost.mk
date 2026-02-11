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

LIBNAME = openssl-gost-engine

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX) \
	-D_OPENSSL_LIBDIR=$(SP_INSTALL_PREFIX) \
	-DLIB_EAY=$(SP_INSTALL_PREFIX)/lib/crypto.lib \
	-DSSL_EAY=$(SP_INSTALL_PREFIX)/lib/ssl.lib \
	-DOPENSSL_USE_STATIC_LIBS=TRUE \
	-DOPENSSL_MSVC_STATIC_RT=TRUE \
	-DOPENSSL_ENGINES_DIR=$(SP_INSTALL_PREFIX)/lib \
	-DENABLE_SHARED_LIB=OFF

all:
	$(call rule_rm,$(LIBNAME))
	$(MKDIR) $(LIBNAME) | Out-Null
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake --build . --parallel
	cd $(LIBNAME); cmake --install .
	$(call rule_cp,$(LIB_SRC_DIR)/$(LIBNAME)/gost-engine.h,$(SP_INSTALL_PREFIX)/include)
	$(call rule_cp,$(LIB_SRC_DIR)/$(LIBNAME)/e_gost_err.h,$(SP_INSTALL_PREFIX)/include)
	$(call rule_rm,$(LIBNAME))

.PHONY: all
