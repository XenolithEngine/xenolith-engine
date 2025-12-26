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

MODULE_STAPPLER_ABI_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_ABI_LIBS :=
MODULE_STAPPLER_ABI_LIBS_SHARED :=
MODULE_STAPPLER_ABI_SRCS_DIRS := 
MODULE_STAPPLER_ABI_SRCS_OBJS := \
	$(STAPPLER_MODULE_DIR)/abi/SPAbi.scu.cpp


MODULE_STAPPLER_ABI_INCLUDES_DIRS :=
MODULE_STAPPLER_ABI_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/abi 
MODULE_STAPPLER_ABI_DEPENDS_ON := stappler_core stappler_filesystem
MODULE_STAPPLER_ABI_GENERAL_LDFLAGS :=

ifdef LINUX
MODULE_STAPPLER_ABI_LIBS += -licuuc -licudata
endif

# spec

MODULE_STAPPLER_ABI_SHARED_SPEC_SUMMARY := libstappler ABI support

define MODULE_STAPPLER_ABI_SHARED_SPEC_DESCRIPTION
Module libstappler-abi provides fixed ABI (Application Binary Interface) for some OS/platform-dependent functions

It can be effectively used to hide OS/platform interface from end-user and toolchain to exclude direct OS SDK dependencies
endef

# module name resolution
MODULE_stappler_abi := MODULE_STAPPLER_ABI
