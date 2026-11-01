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

#
# Freestanding memory and string primitives (memcpy, strlen, ...) for the targets
# without a platform libc. No dependencies: the same sources are compiled into the
# Tau kernel and bare modules directly, with their own flags.
#

MODULE_RUNTIME_LIBC_FREESTANDING_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBC_FREESTANDING_DEPENDS_ON :=
MODULE_RUNTIME_LIBC_FREESTANDING_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/libc_freestanding/src \
	$(RUNTIME_MODULE_DIR)/libc_freestanding/asm/$(TARGET_ARCH)
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/libc_freestanding/include \
	$(RUNTIME_MODULE_DIR)/include

# -fno-builtin: the byte loops here must not be recognized back into calls to
# the functions they implement.
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_COMMON_FLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc \
	-ffreestanding \
	-fno-builtin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_SFLAGS :=

# Intel Skylake to Comet Lake (the JCC erratum fix) lose time on a branch that crosses
# or ends at a 32-byte boundary; the assembler pads the code so none does.
ifeq ($(TARGET_ARCH),x86_64)
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_SFLAGS += -mbranches-within-32B-boundaries
endif
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_CFLAGS := $(MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_COMMON_FLAGS)

$(call define_module, runtime_libc_freestanding, MODULE_RUNTIME_LIBC_FREESTANDING)
