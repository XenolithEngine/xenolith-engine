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

MODULE_STAPPLER_GIT_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_GIT_PRIVATE_INCLUDE_PCH := SPCommon.h
# zlib: required now to satisfy libcurl's content-encoding (inflate), and later to
# inflate git pack/loose objects directly.
MODULE_STAPPLER_GIT_LIBS :=
MODULE_STAPPLER_GIT_LIBS_SHARED := -lz
MODULE_STAPPLER_GIT_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/git
MODULE_STAPPLER_GIT_SRCS_OBJS :=
MODULE_STAPPLER_GIT_INCLUDES_DIRS :=
MODULE_STAPPLER_GIT_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/git
MODULE_STAPPLER_GIT_DEPENDS_ON := stappler_core stappler_network stappler_crypto stappler_filesystem stappler_data
MODULE_STAPPLER_GIT_GENERAL_LDFLAGS :=

ifdef LINUX
MODULE_STAPPLER_GIT_LIBS += -l:libz.a
endif

ifeq ($(TARGET_SYSTEM),Darwin)
MODULE_STAPPLER_GIT_GENERAL_LDFLAGS += -lz
endif

ifdef ANDROID
MODULE_STAPPLER_GIT_LIBS += -l:libz.a
endif

ifdef WIN32
MODULE_STAPPLER_GIT_LIBS += -lz
endif

ifeq ($(TARGET_SYSTEM),WASM)
MODULE_STAPPLER_GIT_LIBS += -l:libz.a
endif

#spec

MODULE_STAPPLER_GIT_SHARED_SPEC_SUMMARY := libstappler git remote interface

define MODULE_STAPPLER_GIT_SHARED_SPEC_DESCRIPTION
Module libstappler-git implements a from-scratch client for the Git Smart HTTP
protocol (protocol v2) on top of libstappler network and crypto, allowing a
remote repository to be inspected and cloned without external git tooling.
endef

# module name resolution
$(call define_module, stappler_git, MODULE_STAPPLER_GIT)
