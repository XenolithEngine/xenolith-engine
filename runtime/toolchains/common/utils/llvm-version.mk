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

# The single place the LLVM version is pinned. Everything else - the source tag,
# the patch directories under replacements/llvm, the driver name (clang-NN), the
# sonames (libLLVM.so.NN.M / liblldb.so.NN.M.P / liblldb.NN.M.P.dylib) and the
# resource-dir path (lib/clang/NN/include) - is derived from it.
#
# Update policy (docs/articles/ru/libc/sprt-purpose.adoc): move to the last patch
# release of series N-1 as soon as the release of series N starts.
#
# Included by every makefile that names a version. Guarded so repeated includes
# through the recursive-make chain are free.

ifndef SP_LLVM_VERSION_INCLUDED
SP_LLVM_VERSION_INCLUDED := 1

# https://github.com/llvm/llvm-project/releases # revised: 11 aug 2026
SP_LLVM_VER   := 22
SP_LLVM_MINOR := 1
SP_LLVM_PATCH := 8

# 22.1 - the soname suffix of the LLVM shared libraries
SP_LLVM_SOVER := $(SP_LLVM_VER).$(SP_LLVM_MINOR)

# 22.1.8 - the full version; also the prefix of the replacements/llvm patch dirs
SP_LLVM_V     := $(SP_LLVM_SOVER).$(SP_LLVM_PATCH)

# llvmorg-22.1.8 - the upstream git tag
SP_LLVM_TAG   := llvmorg-$(SP_LLVM_V)

endif
