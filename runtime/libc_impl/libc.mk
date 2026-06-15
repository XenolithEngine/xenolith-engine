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
# Standalone libc implementation, based on musl-libc with platform-specific extensions
#

MODULE_RUNTIME_LIBC_IMPL_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBC_IMPL_DEPENDS_ON := runtime_malloc runtime_musl_libc runtime_core
MODULE_RUNTIME_LIBC_IMPL_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/libc_impl/src \
	$(RUNTIME_MODULE_DIR)/libc_impl/asm/$(TARGET_SYSTEM)/$(TARGET_ARCH)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc \
	-ffreestanding \
	-fbuiltin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_LIBC_IMPL_PRIVATE_SFLAGS := $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_CFLAGS := $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS) 
	-std=c99 -pipe
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_libc_impl, MODULE_RUNTIME_LIBC_IMPL)
