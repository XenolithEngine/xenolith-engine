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

OSTYPE_IS_WIN32 := 1

OSTYPE_EXEC_SUFFIX := .exe
OSTYPE_DSO_SUFFIX := .dll
OSTYPE_LIB_SUFFIX := .lib
OSTYPE_LIB_PREFIX :=

OSTYPE_CONFIG_FLAGS := WIN32

OSTYPE_CFLAGS := -Wall -D_MT -Wno-vla-cxx-extension -Wno-microsoft-include -Wno-unused-command-line-argument

ifeq ($(RELEASE),1)
OSTYPE_CFLAGS +=
OSTYPE_LDFLAGS :=
else
OSTYPE_CFLAGS += -g -gcodeview
OSTYPE_LDFLAGS := -g
endif

OSTYPE_GENERAL_CFLAGS := $(OSTYPE_CFLAGS)
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

OSTYPE_GENERAL_CXXFLAGS :=  $(OSTYPE_CFLAGS) -Wno-overloaded-virtual -frtti
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld -Xlinker -nodefaultlib
OSTYPE_EXEC_LDFLAGS :=
OSTYPE_LIB_LDFLAGS :=

WIN32 := 1
WINDOWS := 1
