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
# Runtime core functions, that can be used to build runtime_libc_impl
#

MODULE_RUNTIME_CORE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_CORE_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_CORE_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/core
MODULE_RUNTIME_CORE_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

MODULE_RUNTIME_CORE_PRIVATE_CFLAGS := $(MODULE_RUNTIME_COMMON_CFLAGS)
MODULE_RUNTIME_CORE_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_COMMON_CFLAGS)

ifdef TARGET_INCLUDE_DIR_LIBC
MODULE_RUNTIME_CORE_PRIVATE_CFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
MODULE_RUNTIME_CORE_PRIVATE_CXXFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
endif

ifeq ($(TARGET_SYSTEM),Darwin)
# Change include ordering by duplicating HOST flags before SDK's flags
MODULE_RUNTIME_CORE_PRIVATE_CFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_RUNTIME_CORE_PRIVATE_CXXFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
endif

$(call define_module, runtime_core, MODULE_RUNTIME_CORE)
