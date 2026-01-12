# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

MODULE_STAPPLER_DOCUMENT_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_DOCUMENT_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_DOCUMENT_PRECOMPILED_HEADERS :=
MODULE_STAPPLER_DOCUMENT_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/document
MODULE_STAPPLER_DOCUMENT_SRCS_OBJS :=
MODULE_STAPPLER_DOCUMENT_INCLUDES_DIRS :=
MODULE_STAPPLER_DOCUMENT_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/document
MODULE_STAPPLER_DOCUMENT_DEPENDS_ON := \
	stappler_filesystem \
	stappler_vg \
	stappler_font \
	stappler_zip

#spec

MODULE_STAPPLER_DOCUMENT_SHARED_SPEC_SUMMARY := libstappler extension for structured documents

define MODULE_DSTAPPLER_DOCUMENT_SHARED_SPEC_DESCRIPTION
Module libstappler-document provides primitives to build
structured document (like HTML, Markdown, ODT) renderer.
endef

# module name resolution
$(call define_module, stappler_document, MODULE_STAPPLER_DOCUMENT)
