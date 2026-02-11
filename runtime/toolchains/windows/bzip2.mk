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

LIBNAME = bzip2

include ../common/configure.mk

prepare:
	$(call rule_rm,$(LIBNAME))
	$(MKDIR) $(LIBNAME) | Out-Null

OBJS = \
	blocksort.o  \
	huffman.o    \
	crctable.o   \
	randtable.o  \
	compress.o   \
	decompress.o \
	bzlib.o

$(LIBNAME)/%.o: $(LIB_SRC_DIR)/$(LIBNAME)/%.c | prepare
	$(SP_CC) $(SP_CFLAGS) -D_FILE_OFFSET_BITS=64 -c $< -o $@

$(LIBNAME)/bz2.lib: $(addprefix $(LIBNAME)/,$(OBJS))
	$(SP_AR) rcs $@ $(addprefix $(LIBNAME)/,$(OBJS))

all: $(LIBNAME)/bz2.lib
	$(call rule_cp,$(LIB_SRC_DIR)/$(LIBNAME)/bzlib.h,$(SP_INSTALL_PREFIX)/include)
	$(call rule_cp,$(LIBNAME)/bz2.lib,$(SP_INSTALL_PREFIX)/lib/bz2.lib)
	$(call rule_rm,$(LIBNAME))

.PHONY: all prepare
