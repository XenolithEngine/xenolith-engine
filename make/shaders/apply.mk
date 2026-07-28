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
	@$$(call rule_mkdir,$$(dir $$@))
	$$(GLOBAL_QUIET_GLSLC)
	$$(call sp_compile_glsl_header,$(2),$(1),$(LOCAL_SHADERS_RULES))
$(2):.TARGET_NAME := [$(notdir $(GLSLC))] $(notdir $(1))
endef

$(foreach FILE,$(BUILD_SHADERS_SRCS_FILES) $(TOOLKIT_SHADERS_SRCS_FILES),\
	$(eval $(call BUILD_SHADERS_compile_single_rule,$(FILE),\
		$(addsuffix .h,$(addprefix $(BUILD_SHADERS_OUTDIR)/,$(notdir $(FILE))))\
	)))

# Dual-entry MaterialFrag: dynamic index + *_static literal [0] for V3DV /
# devices without shaderSampledImageArrayDynamicIndexing (XLVkPipeline picks *_static).
XL_MATERIAL_FRAG_SRC := $(firstword $(filter %/xl_2d_material.frag,$(TOOLKIT_SHADERS_SRCS_FILES) $(BUILD_SHADERS_SRCS_FILES)))
XL_MATERIAL_STATIC_SRC := $(firstword $(filter %/xl_2d_material_static.frag,$(TOOLKIT_SHADERS_SRCS_FILES) $(BUILD_SHADERS_SRCS_FILES)))

ifneq ($(and $(XL_MATERIAL_FRAG_SRC),$(XL_MATERIAL_STATIC_SRC)),)
XL_MATERIAL_FRAG_H := $(BUILD_SHADERS_OUTDIR)/xl_2d_material.frag.h
XL_MATERIAL_FRAG_SPV := $(BUILD_SHADERS_OUTDIR)/xl_2d_material.frag.dyn.spv
XL_MATERIAL_STATIC_SPV := $(BUILD_SHADERS_OUTDIR)/xl_2d_material.frag_static.spv
XL_MATERIAL_LINKED_SPV := $(BUILD_SHADERS_OUTDIR)/xl_2d_material.frag.linked.spv

$(XL_MATERIAL_FRAG_SPV) : $(XL_MATERIAL_FRAG_SRC) $(LOCAL_MAKEFILE)
	@$(call rule_mkdir,$(dir $@))
	$(GLOBAL_QUIET_GLSLC)
	$(VERBOSE_GUARD) $(GLSLC) $(BUILD_SHADERS_FLAGS) $(LOCAL_SHADERS_RULES) -V \
		--target-env vulkan1.1 --target-env vulkan1.0 -o $@ $< -e xl_2d_material.frag --sep main
$(XL_MATERIAL_FRAG_SPV):.TARGET_NAME := [$(notdir $(GLSLC))] xl_2d_material.frag.dyn.spv

$(XL_MATERIAL_STATIC_SPV) : $(XL_MATERIAL_STATIC_SRC) $(LOCAL_MAKEFILE)
	@$(call rule_mkdir,$(dir $@))
	$(GLOBAL_QUIET_GLSLC)
	$(VERBOSE_GUARD) $(GLSLC) $(BUILD_SHADERS_FLAGS) $(LOCAL_SHADERS_RULES) -V \
		--target-env vulkan1.1 --target-env vulkan1.0 -o $@ $< -e xl_2d_material.frag_static --sep main
$(XL_MATERIAL_STATIC_SPV):.TARGET_NAME := [$(notdir $(GLSLC))] xl_2d_material.frag_static.spv

$(XL_MATERIAL_LINKED_SPV) : $(XL_MATERIAL_FRAG_SPV) $(XL_MATERIAL_STATIC_SPV)
	@$(call rule_mkdir,$(dir $@))
	$(VERBOSE_GUARD) $(SPIRV_LINK) --target-env vulkan1.1 --create-library -o $@ $^
$(XL_MATERIAL_LINKED_SPV):.TARGET_NAME := [spirv-link] xl_2d_material.frag.linked.spv

# Overrides the single-entry header rule from BUILD_SHADERS_compile_single_rule.
$(XL_MATERIAL_FRAG_H) : $(XL_MATERIAL_LINKED_SPV) $(LOCAL_MAKEFILE)
	@$(call rule_mkdir,$(dir $@))
	$(GLOBAL_QUIET_GLSLC)
	$(VERBOSE_GUARD) python3 -c 'import pathlib,struct,sys; p=pathlib.Path(sys.argv[1]); data=p.read_bytes(); \
words=struct.unpack("<"+str(len(data)//4)+"I", data); out=pathlib.Path(sys.argv[2]); \
lines=["\t// linked MaterialFrag (dynamic + _static)","\t #pragma once","const uint32_t xl_2d_material_frag[] = {"]; \
lines += ["    0x%08x,"%w for w in words]; lines.append("};"); out.write_text("\n".join(lines)+"\n")' \
		$(XL_MATERIAL_LINKED_SPV) $@
$(XL_MATERIAL_FRAG_H):.TARGET_NAME := [spirv-link] xl_2d_material.frag.h
endif
