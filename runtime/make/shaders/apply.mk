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

TOOLKIT_SHADERS_SRCS_FILES := $(call sp_toolkit_shaders_files,$(TOOLKIT_SHADERS_DIR))
TOOLKIT_SHADERS_EMBEDDED := $(call sp_toolkit_shaders_objs,$(TOOLKIT_SHADERS_SRCS_FILES))

BUILD_SHADERS_SRCS_FILES := $(call sp_toolkit_shaders_files,$(LOCAL_SHADERS_DIR))
BUILD_SHADERS_EMBEDDED := $(call sp_toolkit_shaders_objs,$(BUILD_SHADERS_SRCS_FILES))

BUILD_SHADERS_TARGET_INCLUDE_ALL := $(addprefix -I,$(BUILD_SHADERS_OUTDIR))

BUILD_SHADERS_INCLUDE = $(addprefix -I,$(realpath $(LOCAL_SHADERS_INCLUDE) $(TOOLKIT_SHADERS_INCLUDE)))
BUILD_SHADERS_FLAGS := $(BUILD_SHADERS_INCLUDE) -DSP_GLSL=1

ifdef OSTYPE_IS_MACOS
BUILD_SHADERS_FLAGS += -DSP_MVK=1
endif

define BUILD_SHADERS_compile_single_rule
$(2) : $(1) $$(LOCAL_MAKEFILE)
	$$(call sp_compile_glsl_header,$(2),$(1),$(LOCAL_SHADERS_RULES))
endef

$(foreach FILE,$(BUILD_SHADERS_SRCS_FILES) $(TOOLKIT_SHADERS_SRCS_FILES),\
	$(eval $(call BUILD_SHADERS_compile_single_rule,$(FILE),\
		$(addsuffix .h,$(addprefix $(BUILD_SHADERS_OUTDIR)/,$(notdir $(FILE))))\
	)))
