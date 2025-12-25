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

LIBNAME = glslang

include ../common/configure.mk

ifdef SP_NATIVE
SPIRV_EXTRA_LINKER_FLAGS :=
else
SPIRV_EXTRA_LINKER_FLAGS := -lc++ -lc++abi
endif

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DCMAKE_INSTALL_BINDIR=$(SP_INSTALL_PREFIX)/bin \
	-DGLSLANG_TESTS=Off \
	-DGLSLANG_ENABLE_INSTALL=Off \
	-DENABLE_HLSL=Off \
	-DENABLE_OPT=On \
	-DALLOW_EXTERNAL_SPIRV_TOOLS=On \
	-DGLSLANG_ENABLE_INSTALL=On \
	-DCMAKE_C_FLAGS_INIT="$(CONFIGURE_CMAKE_C_FLAGS_INIT) -ffunction-sections -fdata-sections" \
	-DCMAKE_CXX_FLAGS_INIT="$(CONFIGURE_CMAKE_CXX_FLAGS_INIT) -ffunction-sections -fdata-sections" \
	-DCMAKE_EXE_LINKER_FLAGS="$(SPIRV_EXTRA_LINKER_FLAGS) -Wl,--gc-sections"

all:
	@rm -rf $(LIBNAME)
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); \
		cmake $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE); \
		cmake --build . --parallel; \
		cmake --install .
	cd $(SP_INSTALL_PREFIX)/bin; ln -s -f glslang glslangValidator
	rm -rf $(LIBNAME)
