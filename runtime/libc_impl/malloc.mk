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
# mimalloc for builtin libc
#

MODULE_RUNTIME_MALLOC_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_MALLOC_PRIVATE_STANDALONE := 1

ifeq ($(TARGET_SYSTEM),WASM)

# wasm has no mmap/VirtualAlloc, so mimalloc's OS primitive layer does not apply.
# Use the runtime's own simple native allocator built directly on memory.grow
# (libc_impl/wasm_malloc/wasm_malloc.c). It exposes the same public C entry
# points the mimalloc SCU did.
MODULE_RUNTIME_MALLOC_SRCS_OBJS := \
	$(RUNTIME_MODULE_DIR)/libc_impl/wasm_malloc/wasm_malloc.c
MODULE_RUNTIME_MALLOC_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_MALLOC_PRIVATE_COMMON_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc \
	-ffreestanding \
	-fbuiltin

else # ($(TARGET_SYSTEM),WASM)

MODULE_RUNTIME_MALLOC_SRCS_OBJS := \
	$(RUNTIME_MODULE_DIR)/libc_impl/mimalloc/mimalloc.scu.c
MODULE_RUNTIME_MALLOC_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include/sprt/wrappers/windows \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/libc_impl/mimalloc/include

MODULE_RUNTIME_MALLOC_PRIVATE_COMMON_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc \
	-ffreestanding \
	-fbuiltin \
	 -funwind-tables -fasynchronous-unwind-tables \
	-DMALLOC_NO_PRIVATE_NAMESPACE

endif # ($(TARGET_SYSTEM),WASM)

MODULE_RUNTIME_MALLOC_PRIVATE_CFLAGS := $(MODULE_RUNTIME_MALLOC_PRIVATE_COMMON_CFLAGS)
MODULE_RUNTIME_MALLOC_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_MALLOC_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_malloc, MODULE_RUNTIME_MALLOC)
