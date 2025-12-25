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

BUILD_SHADERS_OUTDIR := $(BUILD_OUTDIR)/$(notdir $(GLSLC))

sp_compile_glsl = $(GLOBAL_QUIET_GLSLC) $(GLOBAL_MKDIR) $(dir $@); $(GLSLC) \
	$(BUILD_SHADERS_FLAGS) $(3) -V --target-env vulkan1.1 --target-env vulkan1.0 -o $(1) $(2) -e $(notdir $(basename $(1))) --sep main 

sp_compile_glsl_header = $(GLOBAL_QUIET_GLSLC) $(GLOBAL_MKDIR) $(dir $@); $(GLSLC) \
	$(BUILD_SHADERS_FLAGS) $(3) -V --target-env vulkan1.1 --target-env vulkan1.0 \
	--vn $(subst .,_,$(notdir $(basename $(1)))) -o $(1) $(2) -e $(notdir $(basename $(1))) --sep main 

sp_link_spirv = $(GLOBAL_QUIET_SPIRV_LINK) $(GLOBAL_MKDIR) $(dir $@); $(SPIRV_LINK) --target-env vulkan1.0 \
	-o $@ $(addprefix $(BUILD_SHADERS_OUTDIR)/compiled,$(wildcard $(subst $(BUILD_SHADERS_OUTDIR)/linked,,$@)/*))

sp_embed_spirv = $(GLOBAL_QUIET_SPIRV_EMBED) $(GLOBAL_MKDIR) $(dir $@); \
	cd $(dir $<); xxd -i $(notdir $<) $(abspath $@).tmp; echo '///@ SP_EXCLUDE' | cat - $(abspath $@).tmp > $(abspath $@); rm $(abspath $@).tmp

# $(1) - dirs where to find shaders
sp_toolkit_shaders_files = $(realpath $(foreach dir,$(1),$(wildcard $(dir)/*.*)))

# $(1) - dirs where to find shaders
sp_toolkit_shaders_objs = $(addsuffix .h,\
	$(addprefix $(BUILD_SHADERS_OUTDIR)/,$(notdir $(1))))
