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

# The musl implementations the module replaces, renamed to __lfs_musl_* and
# __lfs_musl_asm_*, built with musl's own headers and flags (as
# runtime/musl-adapters/musl_libc.mk) so they can be linked beside the host libc.

LFS_TEST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
LFS_RUNTIME_DIR := $(abspath $(LFS_TEST_DIR)/../../runtime)

MODULE_LFS_REF_MUSL_DEFINED_IN := $(LFS_TEST_DIR)/lfs-modules.mk
MODULE_LFS_REF_MUSL_PRIVATE_STANDALONE := 1
MODULE_LFS_REF_MUSL_DEPENDS_ON :=
MODULE_LFS_REF_MUSL_SRCS_DIRS :=
MODULE_LFS_REF_MUSL_SRCS_OBJS := \
	$(LFS_TEST_DIR)/musl/lfs_musl_c.c \
	$(LFS_TEST_DIR)/musl/lfs_musl_asm.S
MODULE_LFS_REF_MUSL_PRIVATE_INCLUDES := \
	$(LFS_RUNTIME_DIR)/musl-adapters/include \
	$(LFS_RUNTIME_DIR)/musl-libc/arch/$(TARGET_ARCH) \
	$(LFS_RUNTIME_DIR)/musl-libc/arch/generic \
	$(LFS_RUNTIME_DIR)/musl-libc/src/internal \
	$(LFS_RUNTIME_DIR)/musl-libc/src/include \
	$(LFS_RUNTIME_DIR)/musl-libc/include \
	$(LFS_RUNTIME_DIR)/include

MODULE_LFS_REF_MUSL_PRIVATE_CFLAGS := \
	-std=c99 \
	-nostdinc \
	-ffreestanding \
	-fno-builtin \
	-fno-strict-aliasing \
	-Wno-unused-function \
	-Wno-bitwise-op-parentheses \
	-Wno-shift-op-parentheses

$(call define_module, lfs_ref_musl, MODULE_LFS_REF_MUSL)

# The module's assembly under the test's prefixes, at each level. Built with the module's
# own assembler flags, so the test measures what the module ships.
MODULE_LFS_IMPL_ASM_DEFINED_IN := $(LFS_TEST_DIR)/lfs-modules.mk
MODULE_LFS_IMPL_ASM_PRIVATE_STANDALONE := 1
MODULE_LFS_IMPL_ASM_DEPENDS_ON :=
MODULE_LFS_IMPL_ASM_SRCS_DIRS :=
MODULE_LFS_IMPL_ASM_SRCS_OBJS := \
	$(LFS_TEST_DIR)/asm/lfs_impl_asm.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_gpr.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_sse2.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_avx2.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_sve.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_zbb.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_rvv.S \
	$(LFS_TEST_DIR)/asm/lfs_impl_fenv.S
MODULE_LFS_IMPL_ASM_PRIVATE_INCLUDES := $(LFS_RUNTIME_DIR)/include
MODULE_LFS_IMPL_ASM_PRIVATE_SFLAGS = $(MODULE_RUNTIME_LIBC_FREESTANDING_PRIVATE_SFLAGS)

$(call define_module, lfs_impl_asm, MODULE_LFS_IMPL_ASM)
