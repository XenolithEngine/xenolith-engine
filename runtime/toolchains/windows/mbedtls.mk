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

LIBNAME = mbedtls

include ../common/configure.mk

# For 3.6.5, we should manually apply path and wait for a fix on
# https://github.com/Mbed-TLS/mbedtls/issues/9896
#
#if (CMAKE_C_COMPILER_ID MATCHES "Clang")
#  if (CMAKE_C_COMPILER_FRONTEND_VARIANT MATCHES "GNU")
#    set(COMPILER_ID "Clang")
#  else()
#    set(COMPILER_ID ${CMAKE_C_COMPILER_FRONTEND_VARIANT})
#  endif()
#else()
#  set(COMPILER_ID ${CMAKE_C_COMPILER_ID})
#endif()


CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DENABLE_PROGRAMS=Off \
	-DENABLE_TESTING=Off \
	-DMBEDTLS_FATAL_WARNINGS=Off

all:
	$(call rule_rm,$(LIBNAME))
	$(MKDIR) $(LIBNAME) | Out-Null
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Release --target install --parallel
	$(call rule_rm,$(LIBNAME))
