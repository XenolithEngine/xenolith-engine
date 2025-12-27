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

$(call print_verbose,(apply-toolchain.mk) Start)

ifdef TOOLCHAIN_AR
GLOBAL_AR = $(TOOLCHAIN_AR)
endif

ifdef TOOLCHAIN_CC
GLOBAL_CC = $(TOOLCHAIN_CC)
endif

ifdef TOOLCHAIN_CXX
GLOBAL_CXX = $(TOOLCHAIN_CXX)
endif

ifdef TOOLCHAIN_GLSLANG
GLSLC = $(TOOLCHAIN_GLSLANG)
endif

ifdef TOOLCHAIN_SPIRV_LINK
SPIRV_LINK = $(TOOLCHAIN_SPIRV_LINK)
endif

ifdef TOOLCHAIN_TARGET
GLOBAL_GENERAL_CFLAGS += --target=$(TOOLCHAIN_TARGET)
GLOBAL_GENERAL_CXXFLAGS += --target=$(TOOLCHAIN_TARGET)
endif

ifdef TOOLCHAIN_SYSROOT
GLOBAL_GENERAL_CFLAGS += --sysroot=$(TOOLCHAIN_SYSROOT)
GLOBAL_GENERAL_CXXFLAGS += --sysroot=$(TOOLCHAIN_SYSROOT)
endif

ifdef TOOLCHAIN_GENERAL_CFLAGS
GLOBAL_GENERAL_CFLAGS += $(TOOLCHAIN_GENERAL_CFLAGS)
endif

ifdef TOOLCHAIN_GENERAL_CXXFLAGS
GLOBAL_GENERAL_CXXFLAGS += $(TOOLCHAIN_GENERAL_CXXFLAGS)
endif

ifdef TOOLCHAIN_EXEC_CFLAGS
GLOBAL_EXEC_CFLAGS := $(TOOLCHAIN_EXEC_CFLAGS)
endif

ifdef TOOLCHAIN_EXEC_CXXFLAGS
GLOBAL_EXEC_CXXFLAGS := $(TOOLCHAIN_EXEC_CXXFLAGS)
endif

ifdef TOOLCHAIN_LIB_CFLAGS
GLOBAL_LIB_CFLAGS := $(TOOLCHAIN_LIB_CFLAGS)
endif

ifdef TOOLCHAIN_LIB_CXXFLAGS
GLOBAL_LIB_CXXFLAGS := $(TOOLCHAIN_LIB_CXXFLAGS)
endif

ifdef TOOLCHAIN_GENERAL_LDFLAGS
GLOBAL_GENERAL_LDFLAGS := $(TOOLCHAIN_GENERAL_LDFLAGS)
endif

ifdef TOOLCHAIN_LIB_LDFLAGS
GLOBAL_LIB_LDFLAGS := $(TOOLCHAIN_LIB_LDFLAGS)
endif

ifdef TOOLCHAIN_EXEC_LDFLAGS
GLOBAL_EXEC_LDFLAGS := $(TOOLCHAIN_EXEC_LDFLAGS)
endif

ifneq (,$(findstring clang,$(GLOBAL_CXX)))
	GLOBAL_COMPILER_IS_CLANG := 1
endif

$(call print_verbose,(apply-toolchain.mk) GLOBAL_AR: $(GLOBAL_AR))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_CC: $(GLOBAL_CC))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_CXX: $(GLOBAL_CXX))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_GENERAL_CFLAGS: $(GLOBAL_GENERAL_CFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_GENERAL_CXXFLAGS: $(GLOBAL_GENERAL_CXXFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_EXEC_CFLAGS: $(GLOBAL_EXEC_CFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_EXEC_CXXFLAGS: $(GLOBAL_EXEC_CXXFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_LIB_CFLAGS: $(GLOBAL_LIB_CFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_LIB_CXXFLAGS: $(GLOBAL_LIB_CXXFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_GENERAL_LDFLAGS: $(GLOBAL_GENERAL_LDFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_LIB_LDFLAGS: $(GLOBAL_LIB_LDFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_EXEC_LDFLAGS: $(GLOBAL_EXEC_LDFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_EXEC_LDFLAGS: $(GLOBAL_EXEC_LDFLAGS))
$(call print_verbose,(apply-toolchain.mk) GLOBAL_COMPILER_IS_CLANG: $(GLOBAL_COMPILER_IS_CLANG))

$(call print_verbose,(apply-toolchain.mk) GLSLC: $(GLSLC))
$(call print_verbose,(apply-toolchain.mk) SPIRV_LINK: $(SPIRV_LINK))

# Find runtime for toolchain
-include $(TOOLCHAIN_SYSROOT)/share/stappler/runtime.mk

$(call print_verbose,(apply-toolchain.mk) End)
