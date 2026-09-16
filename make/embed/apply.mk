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

# Emits one codegen rule per embedded bundle. See make/embed/compiler.mk for the variables.

# Project-level bundles are relative to the project, module-level ones to the engine root
BUILD_EMBED_LOCAL_PLAIN := $(call sp_embed_resolve,$(LOCAL_EMBED_DIRS),$(LOCAL_ROOT))
BUILD_EMBED_LOCAL_PACKED := $(call sp_embed_resolve,$(LOCAL_EMBED_COMPRESSED_DIRS),$(LOCAL_ROOT))

BUILD_EMBED_TOOLKIT_PLAIN := $(call sp_embed_resolve,$(TOOLKIT_EMBED_DIRS),$(GLOBAL_ROOT))
BUILD_EMBED_TOOLKIT_PACKED := $(call sp_embed_resolve,$(TOOLKIT_EMBED_COMPRESSED_DIRS),$(GLOBAL_ROOT))

BUILD_EMBED_PACKED_DIRS := $(BUILD_EMBED_LOCAL_PACKED) $(BUILD_EMBED_TOOLKIT_PACKED)

# Sources are split the same way ordinary sources are, so that a library build keeps a module's
# bundle with the module and the project's bundle with the project
BUILD_EMBED_LOCAL_SRCS := $(call sp_embed_sources,$(BUILD_EMBED_LOCAL_PLAIN) $(BUILD_EMBED_LOCAL_PACKED))
BUILD_EMBED_TOOLKIT_SRCS := $(call sp_embed_sources,$(BUILD_EMBED_TOOLKIT_PLAIN) $(BUILD_EMBED_TOOLKIT_PACKED))

# Decompression goes through the stappler_data seam (SPFilesystemEmbedded.cc); without that module
# a compressed bundle would build and then fail to open at runtime, so refuse it here instead.
ifneq ($(strip $(BUILD_EMBED_PACKED_DIRS)),)
ifneq ($(filter stappler_data,$(GLOBAL_MODULES)),stappler_data)
$(error Compressed embedded bundles require the stappler_data module: $(BUILD_EMBED_PACKED_DIRS))
endif
endif

$(call print_verbose,(embed/apply.mk) Embedded bundles: $(BUILD_EMBED_LOCAL_PLAIN) $(BUILD_EMBED_TOOLKIT_PLAIN))
$(call print_verbose,(embed/apply.mk) Embedded bundles (compressed): $(BUILD_EMBED_PACKED_DIRS))

# $(1) - bundle directory, $(2) - compression flag
define BUILD_EMBED_single_rule
$(eval $(call BUILD_embed_source,$(call sp_embed_source,$(1)),$(call sp_embed_name,$(1)),$(1),$(2),\
	$(call sp_embed_deps,$(1))))
endef

$(foreach DIR,$(BUILD_EMBED_LOCAL_PLAIN) $(BUILD_EMBED_TOOLKIT_PLAIN),\
	$(call BUILD_EMBED_single_rule,$(DIR),0))

$(foreach DIR,$(BUILD_EMBED_PACKED_DIRS),\
	$(call BUILD_EMBED_single_rule,$(DIR),1))
