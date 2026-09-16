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

MODULE_XENOLITH_BACKEND_VK_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_BACKEND_VK_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_BACKEND_VK_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_VK_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_VK_INCLUDES_DIRS := 
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_DEPENDS_ON := xenolith_core

#spec

MODULE_XENOLITH_BACKEND_VK_SHARED_SPEC_SUMMARY := Xenolith on Vulkan API

define MODULE_XENOLITH_BACKEND_VK_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-vk implements graphic engine with backend on Vulkan API.
This module only implements basic functions without platform-dependent parts.
endef

# module name resolution
$(call define_module, xenolith_backend_vk, MODULE_XENOLITH_BACKEND_VK)


# Bundle the Vulkan loader + MoltenVK driver and its ICD manifest into the .app on
# Apple platforms. macOS uses the nested .app/Contents/{Frameworks,Resources} layout;
# iOS uses the flat .app/{Frameworks,vulkan} layout. The ICD manifest's library_path
# is relative to the manifest itself, so it differs between the two layouts.
ifneq ($(filter Darwin iOS,$(TARGET_SYSTEM)),)

ifeq ($(TARGET_SYSTEM),iOS)
# Flat iOS bundle: resources live at the .app root.
BUILD_BUNDLE_RESOURCES := $(abspath $(dir $(BUILD_EXECUTABLE)))
BUILD_BUNDLE_FRAMEWORKS := $(abspath $(dir $(BUILD_EXECUTABLE)))/Frameworks
BUILD_MOLTENVK_ICD_RELPATH := ../../Frameworks/libMoltenVK.dylib
else
# Nested macOS bundle: resources live under .app/Contents.
BUILD_BUNDLE_RESOURCES := $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources
BUILD_BUNDLE_FRAMEWORKS := $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Frameworks
BUILD_MOLTENVK_ICD_RELPATH := ../../../Frameworks/libMoltenVK.dylib
endif

BUILD_MOLTENVK_ICD_PATH := $(BUILD_BUNDLE_RESOURCES)/vulkan/icd.d/MoltenVK_icd.json
BUILD_VULKAN_LOADER_PATH := $(BUILD_BUNDLE_FRAMEWORKS)/libvulkan.dylib
BUILD_VULKAN_MOLTENVK_PATH := $(BUILD_BUNDLE_FRAMEWORKS)/libMoltenVK.dylib

$(BUILD_MOLTENVK_ICD_PATH): $(BUILD_VULKAN_MOLTENVK_PATH)
	@$(call rule_mkdir,$(dir $@))
	@echo '{"file_format_version":"1.0.0","ICD":{"library_path":"$(BUILD_MOLTENVK_ICD_RELPATH)","api_version":"1.4.0","is_portability_driver":true}}' > $@

$(BUILD_BUNDLE_FRAMEWORKS)/%.dylib: $(TARGET_LIB_DIR)/%.dylib
	@$(call rule_mkdir,$(dir $@))
	$(call rule_cp,$<,$@)

$(BUILD_EXECUTABLE): \
	$(BUILD_VULKAN_LOADER_PATH) \
	$(BUILD_VULKAN_MOLTENVK_PATH) \
	$(BUILD_MOLTENVK_ICD_PATH)

$(BUILD_SHARED_LIBRARY): \
	$(BUILD_VULKAN_LOADER_PATH) \
	$(BUILD_VULKAN_MOLTENVK_PATH) \
	$(BUILD_MOLTENVK_ICD_PATH)

$(BUILD_STATIC_LIBRARY): \
	$(BUILD_VULKAN_LOADER_PATH) \
	$(BUILD_VULKAN_MOLTENVK_PATH) \
	$(BUILD_MOLTENVK_ICD_PATH)

endif
