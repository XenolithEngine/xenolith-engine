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

LIBNAME = spirv-tools

include ../common/configure.mk

TARGET_LDFLAGS := $(SPIRV_EXTRA_LINKER_FLAGS)
TARGET_CFLAGS := $(CONFIGURE_CMAKE_C_FLAGS_INIT)

ifdef LINUX
TARGET_LDFLAGS +=  -Wl,--gc-sections -lc++ -lc++abi
TARGET_CFLAGS += -ffunction-sections -fdata-sections
endif

ifdef ANDROID
TARGET_LDFLAGS +=  -Wl,--gc-sections
TARGET_CFLAGS += -ffunction-sections -fdata-sections
endif

ifdef DARWIN
ifeq ($(SP_SYSNAME),Darwin)
TARGET_LDFLAGS += --sysroot=$(SP_MACOS_SDK) -lc++
endif # Darwin

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
TARGET_LDFLAGS += --sysroot=$(SP_IOSSIM_SDK) -lc++
else # SP_IOSSIM
TARGET_LDFLAGS += --sysroot=$(SP_IOS_SDK) -lc++
endif # SP_IOSSIM
endif # iOS

endif # DARWIN

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DSPIRV-Headers_SOURCE_DIR=$(LIB_SRC_DIR)/spirv-headers \
	-DCMAKE_INSTALL_BINDIR=$(SP_INSTALL_PREFIX)/bin \
	-DCMAKE_C_FLAGS_INIT="$(TARGET_CFLAGS)" \
	-DCMAKE_CXX_FLAGS_INIT="$(TARGET_CFLAGS)" \
	-DCMAKE_EXE_LINKER_FLAGS="$(TARGET_LDFLAGS)" \
	-DCMAKE_SHARED_LINKER_FLAGS="$(TARGET_LDFLAGS)" \
	-DSPIRV_WERROR=Off \
	-DSPIRV_TOOLS_BUILD_STATIC=On \
	-DSPIRV_SKIP_EXECUTABLES=On \
	-DBUILD_SHARED_LIBS=Off

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE);
	cd $(LIBNAME); cmake --build .
	cd $(LIBNAME); cmake --install .
	$(call rule_rm,$(LIBNAME))
