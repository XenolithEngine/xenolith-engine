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

OSTYPE_IS_MACOS := 1

OSTYPE_SDK_PATH := $(shell xcrun --sdk $(TARGET_SDK_NAME) --show-sdk-path 2> /dev/null)

$(info OSTYPE_SDK_PATH $(OSTYPE_SDK_PATH))

ifeq ($(OSTYPE_SDK_PATH),)
OSTYPE_SDK_PATH := $(TARGET_SDK_FALLBACK)
endif

$(info OSTYPE_SDK_PATH $(OSTYPE_SDK_PATH))

OSTYPE_EXEC_SUFFIX :=
OSTYPE_DSO_SUFFIX := .dylib
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := MACOS

OSTYPE_GENERAL_CFLAGS := -Wall -fvisibility=hidden
OSTYPE_LIB_CFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CFLAGS :=

# -Wno-overloaded-virtual: complains about 'hides overloaded virtual function', that is normal for Stappler/Xenolith
OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-elaborated-enum-base \
	-frtti -fvisibility=hidden -fvisibility-inlines-hidden
OSTYPE_LIB_CXXFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CXXFLAGS :=

OSTYPE_GENERAL_LDFLAGS := -Xlinker -all_load
OSTYPE_EXEC_LDFLAGS := 
OSTYPE_LIB_LDFLAGS := -rdynamic -Wl,--exclude-libs,ALL

ifdef BUILD_SHARED

OSTYPE_LIB_LDFLAGS += -Wl,-z,defs

endif # BUILD_SHARED

OSTYPE_LIBS_REALPATH := 1
BUILD_OBJC := 1
DARWIN := 1
