# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

LIBNAME = openssl-gost-engine

include ../common/configure.mk

all:
	rm -rf $(LIBNAME)
	cp -r $(LIB_SRC_DIR)/$(LIBNAME) $(LIBNAME)
	cd $(LIBNAME); \
		cmake $(CONFIGURE_CMAKE) -DOPENSSL_USE_STATIC_LIBS=TRUE -DOPENSSL_ROOT_DIR=$(SP_INSTALL_PREFIX) $(LIB_SRC_DIR)/$(LIBNAME); \
		make gost_engine_static
	mv -f $(LIBNAME)/libgost.a $(SP_INSTALL_PREFIX)/lib/libgost.a 
	cp -f $(LIB_SRC_DIR)/$(LIBNAME)/gost-engine.h $(SP_INSTALL_PREFIX)/include
	cp -f $(LIB_SRC_DIR)/$(LIBNAME)/e_gost_err.h $(SP_INSTALL_PREFIX)/include
	rm -rf $(LIBNAME)

.PHONY: all
