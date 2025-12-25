# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

LIBNAME = libuidna

include ../common/configure.mk

CONFIGURE := OUTDIR=$(abspath $(LIBNAME)) LIBNAME=libidn2 PREFIX=$(SP_INSTALL_PREFIX) \
	CC=$(SP_CC) CXX=$(SP_CXX) CFLAGS= CFLAGS_OPTIMIZE="$(SP_OPT)" AR=$(SP_AR) WITHOUT_IDN=1

all:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		$(MAKE) -C $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE) install-static install-pc
	rm -rf $(LIBNAME)
