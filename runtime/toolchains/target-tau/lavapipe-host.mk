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


# lavapipe for the build host (x86_64 Linux, the system compiler): the reference the Tau build
# (vulkan.mk) is compared with -- the same LLVM, Mesa and options (lavapipe-options.mk), the
# Tau and Embox patches of the shared sources closed by their conditions. Its users take it by
# VK_DRIVER_FILES=$(OUT)/usr/share/vulkan/icd.d/lvp_icd.x86_64.json with the system loader.
#
# The build trees stay (llvm-build, mesa-build): a rebuild after a patch is a few objects.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(THIS_FILE)))

include $(MAKE_ROOT)lavapipe-options.mk

LIB_SRC_DIR ?= $(abspath $(MAKE_ROOT)../src)
OUT ?= $(abspath $(MAKE_ROOT)intermediate)/x86_64-linux-lavapipe

LLVM_BUILD := $(OUT)/llvm-build
MESA_BUILD := $(OUT)/mesa-build
MESA_NATIVE := $(OUT)/meson-native.ini
LVP := $(OUT)/usr/lib/libvulkan_lvp.so

$(OUT)/usr/bin/llvm-config: $(MAKE_ROOT)lavapipe-options.mk
	cmake -G Ninja -S $(LIB_SRC_DIR)/llvm-project/llvm -B $(LLVM_BUILD) \
		-DCMAKE_C_COMPILER=/usr/bin/cc -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
		-DCMAKE_INSTALL_PREFIX=$(OUT)/usr \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		$(LLVM_COMMON) $(LLVM_LIBRARIES) -DLLVM_BUILD_TOOLS=ON -DLLVM_INCLUDE_TOOLS=ON
	cmake --build $(LLVM_BUILD) --parallel --target llvm-libraries llvm-config
	cmake --build $(LLVM_BUILD) --target $(LLVM_INSTALL_TARGETS) install-llvm-config

$(MESA_NATIVE): $(THIS_FILE)
	@mkdir -p $(OUT)
	@echo "[binaries]" > $@
	@echo "llvm-config = '$(OUT)/usr/bin/llvm-config'" >> $@

$(LVP): $(OUT)/usr/bin/llvm-config $(MESA_NATIVE)
	meson setup --wipe $(MESA_BUILD) $(LIB_SRC_DIR)/mesa --native-file $(MESA_NATIVE) \
		--prefix $(OUT)/usr --libdir lib $(MESA_OPTIONS) \
		|| meson setup $(MESA_BUILD) $(LIB_SRC_DIR)/mesa --native-file $(MESA_NATIVE) \
		--prefix $(OUT)/usr --libdir lib $(MESA_OPTIONS)
	ninja -C $(MESA_BUILD) $(MESA_LVP) src/gallium/targets/lavapipe/lvp_icd.x86_64.json
	mkdir -p $(OUT)/usr/lib $(OUT)/usr/share/vulkan/icd.d
	cp -f $(MESA_BUILD)/$(MESA_LVP) $@
	cp -f $(MESA_BUILD)/src/gallium/targets/lavapipe/lvp_icd.x86_64.json $(OUT)/usr/share/vulkan/icd.d/

all: $(LVP)

.PHONY: all
