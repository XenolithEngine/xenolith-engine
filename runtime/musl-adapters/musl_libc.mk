# Copyright (c) 2026 Xenolith Team <admin@senolith.studio>
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

#
# Adapters for musl libc functions
#

MODULE_RUNTIME_MUSL_LIBC_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_MUSL_LIBC_DEPENDS_ON := runtime_malloc runtime_core
MODULE_RUNTIME_MUSL_LIBC_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/musl-adapters
# wasm32 is not a musl-supported arch, so its arch/ headers live in the adapter
# (musl-adapters/arch/wasm32) rather than in the untouched musl source tree; this
# dir is searched before musl-libc/arch/$(TARGET_ARCH) (which is empty for wasm).
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/musl-adapters/include \
	$(RUNTIME_MODULE_DIR)/musl-adapters/arch/$(TARGET_ARCH) \
	$(RUNTIME_MODULE_DIR)/musl-libc/arch/$(TARGET_ARCH) \
	$(RUNTIME_MODULE_DIR)/musl-libc/arch/generic \
	$(RUNTIME_MODULE_DIR)/musl-libc/src/internal \
	$(RUNTIME_MODULE_DIR)/musl-libc/src/include \
	$(RUNTIME_MODULE_DIR)/musl-libc/include \
	$(RUNTIME_MODULE_DIR)/include \

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_FLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-Wno-pointer-to-int-cast \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int \
	-Werror=pointer-sign \
	-Werror=pointer-arith \
	-Werror=int-conversion \
	-Werror=incompatible-pointer-types \
	-Werror=ignored-qualifiers \
	-Waddress \
	-Warray-bounds \
	-Wchar-subscripts \
	-Wduplicate-decl-specifier \
	-Winit-self \
	-Wreturn-type \
	-Wsequence-point \
	-Wstrict-aliasing \
	-Wunused-function \
	-Wunused-label \
	-Wunused-variable \
	-Wno-bitwise-op-parentheses \
	-Wno-shift-op-parentheses \
	-Wno-unused-but-set-variable

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS := \
	$(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_FLAGS) \
	-nostdinc \
	-ffreestanding \
	-fbuiltin \
	-fexcess-precision=standard \
	-frounding-math \
	-fno-strict-aliasing \
	-fomit-frame-pointer \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_CFLAGS := $(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS) \
	-std=c99 -pipe
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_musl_libc, MODULE_RUNTIME_MUSL_LIBC)
