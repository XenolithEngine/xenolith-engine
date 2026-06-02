# Copyright (c) 2026 Xenolith Team <admin@senolith.studio>
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

#
# Libc umbrella wrapper, that uses libc_impl or platform libc
#

MODULE_RUNTIME_LIBC_WRAPPER_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBC_WRAPPER_LIBS :=
MODULE_RUNTIME_LIBC_WRAPPER_FLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_GENERAL_CFLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/libc_wrapper
MODULE_RUNTIME_LIBC_WRAPPER_SRCS_OBJS :=
MODULE_RUNTIME_LIBC_WRAPPER_INCLUDES_DIRS :=
MODULE_RUNTIME_LIBC_WRAPPER_INCLUDES_OBJS :=
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS)

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc++ -Wno-unused-command-line-argument


# If target toolchain have include_libc dir, use it
ifdef TARGET_INCLUDE_DIR_LIBC
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
endif # TARGET_INCLUDE_DIR_LIBC


ifeq ($(TARGET_SYSTEM),Darwin)
# Change include ordering by duplicating HOST flags before SDK's flags
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
endif # ($(TARGET_SYSTEM),Darwin)


ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS := \
	-ffreestanding \
	-fbuiltin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES += \
	$(TARGET_INCLUDE_DIR) \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
endif # ($(TARGET_SYSTEM),Windows)

$(call define_module, runtime_libc_wrapper, MODULE_RUNTIME_LIBC_WRAPPER)
