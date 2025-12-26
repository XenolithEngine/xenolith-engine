# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

ifeq ($(GLOBAL_WASM_DEBUG),1)
MODULE_STAPPLER_WASM_LIBS := -l:libvmlib-debug.a
MODULE_STAPPLER_WASM_FLAGS := -DWASM_DEBUG
else
MODULE_STAPPLER_WASM_LIBS := -l:libvmlib-release.a
MODULE_STAPPLER_WASM_FLAGS :=
endif

MODULE_STAPPLER_WASM_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_WASM_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/wasm/exports
MODULE_STAPPLER_WASM_SRCS_OBJS :=
MODULE_STAPPLER_WASM_INCLUDES_DIRS :=
MODULE_STAPPLER_WASM_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/wasm/exports
MODULE_STAPPLER_WASM_WASM_OBJS := $(STAPPLER_MODULE_DIR)/wasm/wasm/SPWasmInit.c
MODULE_STAPPLER_WASM_DEPENDS_ON := stappler_data stappler_filesystem

ifdef XWIN
MODULE_STAPPLER_WASM_LIBS += -lpathcch -lntdll
endif

# module name resolution
$(call define_module, stappler_wasm, MODULE_STAPPLER_WASM)
