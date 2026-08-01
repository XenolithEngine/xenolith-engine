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

MODULE_RUNTIME_WINDOW_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_WINDOW_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_WINDOW_LIBS :=
MODULE_RUNTIME_WINDOW_FLAGS :=
MODULE_RUNTIME_WINDOW_GENERAL_CFLAGS :=
MODULE_RUNTIME_WINDOW_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_WINDOW_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/window
MODULE_RUNTIME_WINDOW_SRCS_OBJS :=
MODULE_RUNTIME_WINDOW_INCLUDES_DIRS :=
MODULE_RUNTIME_WINDOW_INCLUDES_OBJS :=
MODULE_RUNTIME_WINDOW_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

MODULE_RUNTIME_WINDOW_PRIVATE_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS)

MODULE_RUNTIME_WINDOW_PRIVATE_CXXFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc++ -Wno-unused-command-line-argument

# libdrm installs xf86drm.h/xf86drmMode.h into usr/include (already on the path via
# the runtime module), but their <drm.h>/<drm_mode.h> live in usr/include/libdrm.
# Direct-to-display (KMS) support compiles out where the sysroot has no libdrm --
# see SPRT_HAS_LIBDRM in linux/SPRTWinLinux.h.
ifeq ($(TARGET_SYSTEM),Linux)
MODULE_RUNTIME_WINDOW_PRIVATE_INCLUDES += \
	$(TARGET_INCLUDE_DIR)/libdrm
endif # ($(TARGET_SYSTEM),Linux)

# If target toolchain have include_libc dir, use it
ifdef TARGET_INCLUDE_DIR_LIBC
MODULE_RUNTIME_WINDOW_PRIVATE_CFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
MODULE_RUNTIME_WINDOW_PRIVATE_CXXFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
endif # TARGET_INCLUDE_DIR_LIBC

ifneq ($(filter Darwin iOS,$(TARGET_SYSTEM)),)
# Change include ordering by duplicating HOST flags before SDK's flags
MODULE_RUNTIME_WINDOW_PRIVATE_CFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_RUNTIME_WINDOW_PRIVATE_CXXFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
endif # ($(TARGET_SYSTEM),Darwin/iOS)

ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_WINDOW_PRIVATE_COMMON_FLAGS := \
	-ffreestanding \
	-fbuiltin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_WINDOW_PRIVATE_INCLUDES += \
	$(TARGET_INCLUDE_DIR) \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_WINDOW_PRIVATE_CFLAGS += $(MODULE_RUNTIME_WINDOW_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_WINDOW_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_WINDOW_PRIVATE_COMMON_FLAGS)
endif # ($(TARGET_SYSTEM),Windows)

ifeq ($(TARGET_SYSTEM),WASM)
# wasm is an internal-libc target like Windows: expose include_libc as a regular include
# (NOT -idirafter — the freestanding stdint.h does #include_next, which needs the clang
# resource-dir stdint to sit after include_libc in the search order) and build freestanding.
MODULE_RUNTIME_WINDOW_PRIVATE_INCLUDES += \
	$(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_WINDOW_PRIVATE_CFLAGS += -ffreestanding -fbuiltin
MODULE_RUNTIME_WINDOW_PRIVATE_CXXFLAGS += -ffreestanding -fbuiltin
endif # ($(TARGET_SYSTEM),WASM)

$(call define_module, runtime_window, MODULE_RUNTIME_WINDOW)
