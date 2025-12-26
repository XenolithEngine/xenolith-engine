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
MODULE_XENOLITH_BACKEND_VK_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_VK_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_VK_INCLUDES_DIRS := 
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_DEPENDS_ON := xenolith_core

ifdef VULKAN_SDK_PREFIX
MODULE_XENOLITH_CORE_INCLUDES_OBJS += $(call sp_os_path,$(VULKAN_SDK_PREFIX)/include)
endif

#spec

MODULE_XENOLITH_BACKEND_VK_SHARED_SPEC_SUMMARY := Xenolith on Vulkan API

define MODULE_XENOLITH_BACKEND_VK_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-vk implements graphic engine with backend on Vulkan API.
This module only implements basic functions without platform-dependent parts.
endef

# module name resolution
$(call define_module, xenolith_backend_vk, MODULE_XENOLITH_BACKEND_VK)

ifdef VULKAN_SDK
ifdef WIN32
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS += $(call sp_os_path,$(VULKAN_SDK)/Include)
else
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS += $(call sp_os_path,$(VULKAN_SDK)/include)
endif
endif


ifdef WIN32
MODULE_XENOLITH_BACKEND_VK_LIBS := -lvulkan-1
MODULE_XENOLITH_BACKEND_VK_GENERAL_LDFLAGS := -L$(call sp_os_path,$(VULKAN_SDK)/Lib)
endif


ifdef MACOS

MODULE_XENOLITH_BACKEND_VK_GENERAL_LDFLAGS := -framework Metal

$(info VULKAN_SDK_PREFIX $(VULKAN_SDK_PREFIX) $(realpath $(VULKAN_SDK_PREFIX)/lib/libvulkan.1.dylib))
VULKAN_LOADER_PATH = $(realpath $(VULKAN_SDK_PREFIX)/lib/libvulkan.1.dylib)
VULKAN_MOLTENVK_PATH = $(realpath $(VULKAN_SDK_PREFIX)/lib/libMoltenVK.dylib)
VULKAN_MOLTENVK_ICD_PATH = $(realpath $(VULKAN_SDK_PREFIX)/share/vulkan/icd.d/MoltenVK_icd.json)
VULKAN_LAYERS_PATH = $(realpath $(VULKAN_SDK_PREFIX)/share/vulkan/explicit_layer.d)
VULKAN_LIBDIR = $(dir $(BUILD_EXECUTABLE))vulkan

$(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json: $(VULKAN_MOLTENVK_ICD_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/icd.d
	sed 's/..\/..\/..\/lib\/libMoltenVK/..\/lib\/libMoltenVK/g' $(VULKAN_MOLTENVK_ICD_PATH) >  $(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json

$(VULKAN_LIBDIR)/explicit_layer.d/%.json: $(VULKAN_LAYERS_PATH)/%.json
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/explicit_layer.d
	sed 's/..\/..\/..\/lib\/libVkLayer/..\/lib\/libVkLayer/g' $(VULKAN_LAYERS_PATH)/$*.json > $(VULKAN_LIBDIR)/explicit_layer.d/$*.json

$(VULKAN_LIBDIR)/lib/%.dylib: $(VULKAN_SDK_PREFIX)/lib/%.dylib
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/lib
	cp $(VULKAN_SDK_PREFIX)/lib/$*.dylib $(VULKAN_LIBDIR)/lib/$*.dylib

$(BUILD_EXECUTABLE): \
	$(VULKAN_LIBDIR)/lib/libvulkan.1.dylib \
	$(VULKAN_LIBDIR)/lib/libMoltenVK.dylib \
	$(subst $(VULKAN_LAYERS_PATH),$(VULKAN_LIBDIR)/explicit_layer.d,$(wildcard $(VULKAN_LAYERS_PATH)/*.json)) \
	$(subst $(VULKAN_SDK_PREFIX),$(VULKAN_LIBDIR),$(wildcard $(VULKAN_SDK_PREFIX)/lib/libVkLayer_*.dylib))

$(BUILD_SHARED_LIBRARY): \
	$(VULKAN_LIBDIR)/lib/libvulkan.1.dylib \
	$(VULKAN_LIBDIR)/lib/libMoltenVK.dylib \
	$(subst $(VULKAN_LAYERS_PATH),$(VULKAN_LIBDIR)/explicit_layer.d,$(wildcard $(VULKAN_LAYERS_PATH)/*.json)) \
	$(subst $(VULKAN_SDK_PREFIX),$(VULKAN_LIBDIR),$(wildcard $(VULKAN_SDK_PREFIX)/lib/libVkLayer_*.dylib))

$(BUILD_STATIC_LIBRARY): \
	$(VULKAN_LIBDIR)/lib/libvulkan.1.dylib \
	$(VULKAN_LIBDIR)/lib/libMoltenVK.dylib \
	$(subst $(VULKAN_LAYERS_PATH),$(VULKAN_LIBDIR)/explicit_layer.d,$(wildcard $(VULKAN_LAYERS_PATH)/*.json)) \
	$(subst $(VULKAN_SDK_PREFIX),$(VULKAN_LIBDIR),$(wildcard $(VULKAN_SDK_PREFIX)/lib/libVkLayer_*.dylib))

endif
