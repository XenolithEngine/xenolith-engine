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

MODULE_XENOLITH_BACKEND_WEBGPU_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_BACKEND_WEBGPU_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_BACKEND_WEBGPU_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_WEBGPU_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/webgpu
MODULE_XENOLITH_BACKEND_WEBGPU_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_WEBGPU_INCLUDES_DIRS :=
MODULE_XENOLITH_BACKEND_WEBGPU_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/webgpu
MODULE_XENOLITH_BACKEND_WEBGPU_DEPENDS_ON := xenolith_core
# On wasm the browser provides WebGPU through navigator.gpu (the wgpu* functions are host
# imports declared in the vendored webgpu.h); no native wgpu-native library is linked.
ifneq ($(TARGET_SYSTEM),WASM)
MODULE_XENOLITH_BACKEND_WEBGPU_LIBS := -lwgpu_native
endif

# wgpu-native is a shared library, installed next to the application
ifeq ($(TARGET_SYSTEM),Linux)
MODULE_XENOLITH_BACKEND_WEBGPU_GENERAL_LDFLAGS := -Wl,-rpath,'$$ORIGIN'
endif

#spec

MODULE_XENOLITH_BACKEND_WEBGPU_SHARED_SPEC_SUMMARY := Xenolith on WebGPU API

define MODULE_XENOLITH_BACKEND_WEBGPU_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-webgpu implements graphic engine with backend on WebGPU API
(via wgpu-native implementation).
endef

# module name resolution
$(call define_module, xenolith_backend_webgpu, MODULE_XENOLITH_BACKEND_WEBGPU)

# Install libwgpu_native.so next to the application executable, so the
# rpath=$ORIGIN entry resolves it without a system-wide installation
ifeq ($(TARGET_SYSTEM),Linux)
ifneq ($(filter xenolith_backend_webgpu,$(LOCAL_MODULES)),)

BUILD_WGPU_LIBRARY_PATH := $(abspath $(dir $(BUILD_EXECUTABLE)))/libwgpu_native.so

$(BUILD_WGPU_LIBRARY_PATH): $(TARGET_LIB_DIR)/libwgpu_native.so
	@$(call rule_mkdir,$(dir $@))
	$(call rule_cp,$<,$@)

$(BUILD_EXECUTABLE): $(BUILD_WGPU_LIBRARY_PATH)

$(BUILD_SHARED_LIBRARY): $(BUILD_WGPU_LIBRARY_PATH)

endif
endif
