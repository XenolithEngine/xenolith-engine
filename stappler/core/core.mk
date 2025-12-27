# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

MODULE_STAPPLER_CORE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_CORE_PRECOMPILED_HEADERS := $(STAPPLER_MODULE_DIR)/core/SPCommon.h
MODULE_STAPPLER_CORE_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/core
MODULE_STAPPLER_CORE_SRCS_OBJS := $(STAPPLER_MODULE_DIR)/thirdparty/libbacktrace/libbacktrace.scu.c
MODULE_STAPPLER_CORE_INCLUDES_DIRS := \
	$(STAPPLER_MODULE_DIR)/core/io \
	$(STAPPLER_MODULE_DIR)/core/string \
	$(STAPPLER_MODULE_DIR)/core/utils
MODULE_STAPPLER_CORE_INCLUDES_OBJS := \
	$(STAPPLER_MODULE_DIR)/core \
	$(STAPPLER_MODULE_DIR)/core/memory \
	$(STAPPLER_MODULE_DIR)/thirdparty
MODULE_STAPPLER_CORE_DEPENDS_ON := runtime
MODULE_STAPPLER_CORE_LIBS :=
MODULE_STAPPLER_CORE_LIBS_SHARED := -ldl
MODULE_STAPPLER_CORE_GENERAL_LDFLAGS :=
MODULE_STAPPLER_CORE_CONFIG_VALUES := \
	STAPPLER_VERSION_API=$(STAPPLER_VERSION_API) \
	STAPPLER_VERSION_REV=$(STAPPLER_VERSION_REV) \
	STAPPLER_VERSION_BUILD=$(STAPPLER_VERSION_BUILD) \

MODULE_STAPPLER_CORE_INCLUDES_INTERNAL := \
	$(STAPPLER_MODULE_DIR)/core/SPPlatformUnistd.h \
	$(STAPPLER_MODULE_DIR)/thirdparty/libbacktrace

MODULE_STAPPLER_CORE_SHARED_CONSUME := \
	stappler_filesystem \
	stappler_threads

ifdef TOOLCHAIN_SYSROOT
MODULE_STAPPLER_CORE_LIBS += -l:libicuuc.a -l:libicudata.a
endif

ifdef LINUX
MODULE_STAPPLER_CORE_LIBS += -ldl
endif

ifdef MACOS
MODULE_STAPPLER_CORE_GENERAL_LDFLAGS += -framework CoreFoundation  -framework Foundation
endif

ifdef WIN32
MODULE_STAPPLER_CORE_LIBS += -ladvapi32 -lshlwapi -lshell32 -lole32 -luserenv
endif

# spec

MODULE_STAPPLER_CORE_SHARED_SPEC_SUMMARY := libstappler basic functions

define MODULE_STAPPLER_CORE_SHARED_SPEC_DESCRIPTION
Module libstappler-core provides basic libstappler framework functions:
- Memory manegement (memory pools or standart allocations)
- String manupulation
- Filesystem functions
- Threading functions
- Url/Email validators
- SAX HTML parser
- Time utilities
endef

# module name resolution
$(call define_module, stappler_core, MODULE_STAPPLER_CORE)

APR_LIB_PATH ?= /usr/local/lib/apache

MODULE_STAPPLER_APR_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_APR_PRECOMPILED_HEADERS :=
MODULE_STAPPLER_APR_SRCS_DIRS :=
MODULE_STAPPLER_APR_SRCS_OBJS :=
MODULE_STAPPLER_APR_INCLUDES_DIRS :=
MODULE_STAPPLER_APR_INCLUDES_OBJS :=
MODULE_STAPPLER_APR_DEPENDS_ON :=
MODULE_STAPPLER_APR_LIBS :=

ifneq ($(LOCAL_BUILD_SHARED),2)
MODULE_STAPPLER_APR_LIBS += -L$(APR_LIB_PATH) -lapr-1
endif

# module name resolution
$(call define_module, stappler_apr, MODULE_STAPPLER_APR)
