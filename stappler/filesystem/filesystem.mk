# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

MODULE_STAPPLER_FILESYSTEM_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FILESYSTEM_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FILESYSTEM_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/filesystem
MODULE_STAPPLER_FILESYSTEM_SRCS_OBJS :=
MODULE_STAPPLER_FILESYSTEM_INCLUDES_DIRS :=
MODULE_STAPPLER_FILESYSTEM_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/filesystem
MODULE_STAPPLER_FILESYSTEM_INCLUDES_INTERNAL := $(STAPPLER_MODULE_DIR)/filesystem/detail
MODULE_STAPPLER_FILESYSTEM_DEPENDS_ON := stappler_core
MODULE_STAPPLER_FILESYSTEM_GENERAL_LDFLAGS :=
MODULE_STAPPLER_FILESYSTEM_LIBS :=

ifdef ANDROID
MODULE_STAPPLER_FILESYSTEM_DEPENDS_ON += stappler_zip
endif

ifdef MACOS
MODULE_STAPPLER_FILESYSTEM_GENERAL_LDFLAGS += -framework UniformTypeIdentifiers
endif

ifdef WIN32
MODULE_STAPPLER_FILESYSTEM_LIBS +=
endif

# module name resolution
$(call define_module, stappler_filesystem, MODULE_STAPPLER_FILESYSTEM)
