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

$(call print_verbose,(apply-toolchain.mk) Build toolckain configuration...)

#
# System-specific definitions
#

ifeq ($(TARGET_SYSTEM),Linux)
	include $(BUILD_ROOT)/os/linux.mk
else ifeq ($(TARGET_SYSTEM),Darwin)
	include $(BUILD_ROOT)/os/darwin.mk
else ifeq ($(TARGET_SYSTEM),MacOS)
	include $(BUILD_ROOT)/os/darwin.mk
else ifeq ($(TARGET_SYSTEM),iOS)
	include $(BUILD_ROOT)/os/darwin.mk
else ifeq ($(TARGET_SYSTEM),Windows)
	include $(BUILD_ROOT)/os/windows.mk
else ifeq ($(TARGET_SYSTEM),Android)
	include $(BUILD_ROOT)/os/linux.mk
else ifeq ($(TARGET_SYSTEM),Android-NDK)
	include $(BUILD_ROOT)/os/android-ndk.mk
else ifeq ($(TARGET_SYSTEM),WASM)
	include $(BUILD_ROOT)/os/wasm.mk
else ifeq ($(TARGET_SYSTEM),NuttX)
	include $(BUILD_ROOT)/os/nuttx.mk
else ifeq ($(TARGET_SYSTEM),Embox)
	include $(BUILD_ROOT)/os/embox.mk
else
$(error Unknown TARGET_SYSTEM: $(TARGET_SYSTEM))
endif

GLOBAL_GENERAL_CFLAGS := $(OSTYPE_GENERAL_CFLAGS)
GLOBAL_GENERAL_SFLAGS := $(OSTYPE_GENERAL_SFLAGS)
GLOBAL_GENERAL_CXXFLAGS := $(OSTYPE_GENERAL_CXXFLAGS)
GLOBAL_GENERAL_LDFLAGS := $(OSTYPE_GENERAL_LDFLAGS)

GLOBAL_EXEC_CFLAGS := $(OSTYPE_EXEC_CFLAGS)
GLOBAL_EXEC_CXXFLAGS := $(OSTYPE_EXEC_CXXFLAGS)
GLOBAL_EXEC_LDFLAGS := $(OSTYPE_EXEC_LDFLAGS)

GLOBAL_LIB_CFLAGS := $(OSTYPE_LIB_CFLAGS)
GLOBAL_LIB_CXXFLAGS := $(OSTYPE_LIB_CXXFLAGS)
GLOBAL_LIB_LDFLAGS := $(OSTYPE_LIB_LDFLAGS)

#
# Host half
#

ifdef HOST_AR
GLOBAL_AR = $(HOST_AR) rcs
endif

ifdef HOST_CC
GLOBAL_CC = $(HOST_CC)
endif

ifdef HOST_CXX
GLOBAL_CXX = $(HOST_CXX)
endif

ifdef HOST_GLSLANG
GLSLC = $(HOST_GLSLANG)
endif

ifdef HOST_SPIRV_LINK
SPIRV_LINK = $(HOST_SPIRV_LINK)
endif

ifdef HOST_GENERAL_CFLAGS
GLOBAL_GENERAL_CFLAGS += $(HOST_GENERAL_CFLAGS)
endif

ifdef HOST_GENERAL_CXXFLAGS
GLOBAL_GENERAL_CXXFLAGS += $(HOST_GENERAL_CXXFLAGS)
endif

ifdef HOST_EXEC_CFLAGS
GLOBAL_EXEC_CFLAGS += $(HOST_EXEC_CFLAGS)
endif

ifdef HOST_EXEC_CXXFLAGS
GLOBAL_EXEC_CXXFLAGS += $(HOST_EXEC_CXXFLAGS)
endif

ifdef HOST_LIB_CFLAGS
GLOBAL_LIB_CFLAGS += $(HOST_LIB_CFLAGS)
endif

ifdef HOST_LIB_CXXFLAGS
GLOBAL_LIB_CXXFLAGS += $(HOST_LIB_CXXFLAGS)
endif

ifdef HOST_GENERAL_LDFLAGS
GLOBAL_GENERAL_LDFLAGS += $(HOST_GENERAL_LDFLAGS)
endif

ifdef HOST_LIB_LDFLAGS
GLOBAL_LIB_LDFLAGS += $(HOST_LIB_LDFLAGS)
endif

ifdef HOST_EXEC_LDFLAGS
GLOBAL_EXEC_LDFLAGS += $(HOST_EXEC_LDFLAGS)
endif


#
# Target half
#

ifdef TARGET_NAME
ifneq ($(TARGET_NAME),)
GLOBAL_GENERAL_CFLAGS += --target=$(TARGET_NAME)
GLOBAL_GENERAL_SFLAGS += --target=$(TARGET_NAME)
GLOBAL_GENERAL_CXXFLAGS += --target=$(TARGET_NAME)
GLOBAL_GENERAL_LDFLAGS += --target=$(TARGET_NAME)
endif
endif

ifdef TARGET_SYSROOT
ifneq ($(TARGET_SYSROOT),)
GLOBAL_GENERAL_CFLAGS += --sysroot=$(TARGET_SYSROOT)
GLOBAL_GENERAL_CXXFLAGS += --sysroot=$(TARGET_SYSROOT)
GLOBAL_GENERAL_LDFLAGS += --sysroot=$(TARGET_SYSROOT)
endif
endif

ifdef TARGET_GENERAL_CFLAGS
GLOBAL_GENERAL_CFLAGS += $(TARGET_GENERAL_CFLAGS)
endif

ifdef TARGET_GENERAL_SFLAGS
GLOBAL_GENERAL_SFLAGS += $(TARGET_GENERAL_SFLAGS)
endif

ifdef TARGET_GENERAL_CXXFLAGS
GLOBAL_GENERAL_CXXFLAGS += $(TARGET_GENERAL_CXXFLAGS)
endif

ifdef TARGET_EXEC_CFLAGS
GLOBAL_EXEC_CFLAGS += $(TARGET_EXEC_CFLAGS)
endif

ifdef TARGET_EXEC_CXXFLAGS
GLOBAL_EXEC_CXXFLAGS += $(TARGET_EXEC_CXXFLAGS)
endif

ifdef TARGET_LIB_CFLAGS
GLOBAL_LIB_CFLAGS += $(TARGET_LIB_CFLAGS)
endif

ifdef TARGET_LIB_CXXFLAGS
GLOBAL_LIB_CXXFLAGS += $(TARGET_LIB_CXXFLAGS)
endif

ifdef TARGET_GENERAL_LDFLAGS
GLOBAL_GENERAL_LDFLAGS += $(TARGET_GENERAL_LDFLAGS)
endif

ifdef TARGET_LIB_LDFLAGS
GLOBAL_LIB_LDFLAGS += $(TARGET_LIB_LDFLAGS)
endif

ifdef TARGET_EXEC_LDFLAGS
GLOBAL_EXEC_LDFLAGS += $(TARGET_EXEC_LDFLAGS)
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

$(call print_verbose,(apply-toolchain.mk) GLSLC: $(GLSLC))
$(call print_verbose,(apply-toolchain.mk) SPIRV_LINK: $(SPIRV_LINK))

# Find runtime for toolchain
ifeq ($(patsubst %$+sprt,,$(STAPPLER_TARGET)),)
$(call print_verbose,(apply-toolchain.mk) $(STAPPLER_TARGET) uses integrated stappler runtime)
include $(STAPPLER_TARGET_DIR)/runtime.mk
else
$(call print_verbose,(apply-toolchain.mk) $(STAPPLER_TARGET) requires internal runtime)
include $(GLOBAL_ROOT)/runtime/runtime.mk
endif

$(call print_verbose,(apply-toolchain.mk) Toolchain configured!)
