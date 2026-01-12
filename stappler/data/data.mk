# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

## General data::Value module
MODULE_STAPPLER_DATA_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_DATA_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_DATA_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/data
MODULE_STAPPLER_DATA_SRCS_OBJS :=
MODULE_STAPPLER_DATA_INCLUDES_DIRS :=
MODULE_STAPPLER_DATA_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/data 
MODULE_STAPPLER_DATA_DEPENDS_ON := stappler_core
MODULE_STAPPLER_DATA_SHARED_DEPENDS_ON := stappler_filesystem

MODULE_STAPPLER_DATA_SHARED_CONSUME := \
	stappler_brotli_lib

# spec

MODULE_STAPPLER_DATA_SHARED_SPEC_SUMMARY := libstappler JSON/CBOR support

define MODULE_STAPPLER_DATA_SHARED_SPEC_DESCRIPTION
Module libstappler-data adds:
- weakly-typed container data::Valye
- encoding/decoding for JSON and CBOR
- command line arguments processing
- URL arguments processing
- compression/decompression with brotli or lz4
endef

# module name resolution
$(call define_module, stappler_data, MODULE_STAPPLER_DATA)

## Brotli encoding extra
MODULE_STAPPLER_BROTLI_LIB_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_BROTLI_LIB_LIBS := -l:libbrotlidec.a -l:libbrotlienc.a -l:libbrotlicommon.a
MODULE_STAPPLER_BROTLI_LIB_LIBS_SHARED := -lbrotlidec -lbrotlienc -lbrotlicommon
MODULE_STAPPLER_BROTLI_LIB_SRCS_DIRS :=
MODULE_STAPPLER_BROTLI_LIB_SRCS_OBJS :=
MODULE_STAPPLER_BROTLI_LIB_INCLUDES_DIRS :=
MODULE_STAPPLER_BROTLI_LIB_INCLUDES_OBJS :=
MODULE_STAPPLER_BROTLI_LIB_DEPENDS_ON :=

# module name resolution
$(call define_module, stappler_brotli_lib, MODULE_STAPPLER_BROTLI_LIB)
