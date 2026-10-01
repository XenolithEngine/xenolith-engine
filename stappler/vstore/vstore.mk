# Copyright (c) 2026 Stappler Team <admin@stappler.org>
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

MODULE_STAPPLER_VSTORE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_VSTORE_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_VSTORE_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/vstore
MODULE_STAPPLER_VSTORE_SRCS_OBJS :=
MODULE_STAPPLER_VSTORE_INCLUDES_DIRS :=
MODULE_STAPPLER_VSTORE_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/vstore
MODULE_STAPPLER_VSTORE_DEPENDS_ON := stappler_core stappler_data

# Codecs are consumed, not depended on: a journal codec whose module is missing is refused by
# Journal::init(), and the default codec follows what the build has.
MODULE_STAPPLER_VSTORE_SHARED_CONSUME := \
	stappler_zstd_lib \
	stappler_lzma_lib

MODULE_STAPPLER_VSTORE_SHARED_SPEC_SUMMARY := libstappler versioned arena storage

define MODULE_STAPPLER_VSTORE_SHARED_SPEC_DESCRIPTION
Module libstappler-vstore implements a relocatable arena with versioning
- 32-bit addressed arena in 64 KiB chunks, byte-copyable images
- write barrier with dirty tracking and a debug shadow validator
- undo journal with commit, rollback and keyframes
- page codecs: lz4, zstd, lzma
endef

$(call define_module, stappler_vstore, MODULE_STAPPLER_VSTORE)
