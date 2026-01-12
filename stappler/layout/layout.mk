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

MODULE_STAPPLER_LAYOUT_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_LAYOUT_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_LAYOUT_PRECOMPILED_HEADERS :=
MODULE_STAPPLER_LAYOUT_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/layout
MODULE_STAPPLER_LAYOUT_SRCS_OBJS :=
MODULE_STAPPLER_LAYOUT_INCLUDES_DIRS := $(STAPPLER_MODULE_DIR)/layout
MODULE_STAPPLER_LAYOUT_INCLUDES_OBJS :=
MODULE_STAPPLER_LAYOUT_DEPENDS_ON := stappler_document

#spec

MODULE_STAPPLER_LAYOUT_SHARED_SPEC_SUMMARY := libstappler document layout engine

define MODULE_STAPPLER_LAYOUT_SHARED_SPEC_DESCRIPTION
Module libstappler-layout implements simple layout engine for structured
documents.
endef

# module name resolution
$(call define_module, stappler_layout, MODULE_STAPPLER_LAYOUT)
