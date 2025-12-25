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

MODULE_STAPPLER_RUNTIME_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_RUNTIME_LIBS :=
MODULE_STAPPLER_RUNTIME_FLAGS :=
MODULE_STAPPLER_RUNTIME_GENERAL_CFLAGS := -idirafter $(STAPPLER_MODULE_DIR)/runtime/include_libc
MODULE_STAPPLER_RUNTIME_GENERAL_CXXFLAGS := -idirafter $(STAPPLER_MODULE_DIR)/runtime/include_libc
MODULE_STAPPLER_RUNTIME_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/runtime/src
MODULE_STAPPLER_RUNTIME_SRCS_OBJS :=
MODULE_STAPPLER_RUNTIME_INCLUDES_DIRS :=
MODULE_STAPPLER_RUNTIME_INCLUDES_OBJS := \
	$(STAPPLER_MODULE_DIR)/runtime/include \
	$(STAPPLER_MODULE_DIR)/runtime

MODULE_STAPPLER_RUNTIME_INCLUDES_INTERNAL := \
	$(STAPPLER_MODULE_DIR)/runtime/private

ifdef TOOLCHAIN_INCLUDEDIR_LIBC
MODULE_STAPPLER_RUNTIME_PRIVATE_CXXFLAGS := -idirafter $(TOOLCHAIN_INCLUDEDIR_LIBC)
endif

MODULE_STAPPLER_RUNTIME_DEPENDS_ON :=

ifdef LINUX
MODULE_STAPPLER_RUNTIME_LIBS += -ldl -l:libbacktrace.a
endif

ifdef ANDROID
MODULE_STAPPLER_RUNTIME_LIBS += -ldl -l:libbacktrace.a
endif

ifdef MACOS
MODULE_STAPPLER_RUNTIME_GENERAL_LDFLAGS += -framework CoreFoundation  -framework Foundation
endif

ifdef WIN32
MODULE_STAPPLER_RUNTIME_LIBS += -ladvapi32 -lshlwapi -lshell32 -lole32 -luserenv
endif

#spec

MODULE_STAPPLER_RUNTIME_SHARED_SPEC_SUMMARY := libstappler platform-specific runtime

define MODULE_STAPPLER_RUNTIME_SHARED_SPEC_DESCRIPTION
libstappler platform-specific runtime
endef

# module name resolution
$(call define_module, stappler_runtime, MODULE_STAPPLER_RUNTIME)
