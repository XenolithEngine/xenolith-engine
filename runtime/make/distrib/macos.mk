# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

MACOS_PROJECT_DIR := $(addsuffix proj.macos,$(dir $(realpath $(LOCAL_MAKEFILE))))

MACOS_CONSUMED_MODULES := $(sort $(foreach module,$(LOCAL_MODULES),$($(MODULE_$(module))_SHARED_CONSUME)))

MACOS_CONSUMED_LDFLAGS := $(foreach module,$(MACOS_CONSUMED_MODULES),$($(MODULE_$(module))_GENERAL_LDFLAGS))

MACOS_GENERAL_CFLAGS := \
	$(OSTYPE_GENERAL_CFLAGS) \
	-DSTAPPLER -DSTAPPLER_SHARED \
	$(TOOLKIT_GENERAL_CFLAGS) \
	$(LOCAL_CFLAGS)

MACOS_GENERAL_CXXFLAGS := \
	$(OSTYPE_GENERAL_CXXFLAGS) \
	-DSTAPPLER -DSTAPPLER_SHARED \
	$(TOOLKIT_GENERAL_CXXFLAGS) \
	$(LOCAL_CXXFLAGS)

MACOS_EXEC_CFLAGS := \
	$(GLOBAL_EXEC_CFLAGS) \
	$(TOOLKIT_EXEC_CFLAGS)

MACOS_EXEC_CXXFLAGS := \
	$(GLOBAL_EXEC_CXXFLAGS) \
	$(TOOLKIT_EXEC_CXXFLAGS)

MACOS_LIB_CFLAGS := \
	$(GLOBAL_LIB_CFLAGS) \
	$(TOOLKIT_LIB_CFLAGS)

MACOS_LIB_CXXFLAGS := \
	$(GLOBAL_LIB_CXXFLAGS) \
	$(TOOLKIT_LIB_CXXFLAGS)

MACOS_GENERAL_LDFLAGS := \
	$(GLOBAL_GENERAL_LDFLAGS) \
	$(TOOLKIT_GENERAL_LDFLAGS) \
	$(LOCAL_LDFLAGS) \
	$(MACOS_CONSUMED_LDFLAGS)

MACOS_EXEC_LDFLAGS := \
	$(GLOBAL_EXEC_LDFLAGS) \
	$(TOOLKIT_EXEC_LDFLAGS)

MACOS_LIB_LDFLAGS := \
	$(GLOBAL_LIB_LDFLAGS) \
	$(TOOLKIT_LIB_LDFLAGS) \
	$(if $(filter-out $(LOCAL_BUILD_SHARED),1),$(OSTYPE_STANDALONE_LDFLAGS))

MACOS_HEADER_SEARCH_PATHS := \
	$(realpath $(TOOLKIT_INCLUDES)) \
	$(realpath $(BUILD_INCLUDES)) \
	$(BUILD_SHADERS_OUTDIR)


MACOS_LIBS := $(call sp_toolkit_transform_lib_ldflag, $(TOOLKIT_LIBS))

mac-shaders: $(BUILD_SHADERS_EMBEDDED) $(TOOLKIT_SHADERS_EMBEDDED)

.PHONY: mac-shaders
