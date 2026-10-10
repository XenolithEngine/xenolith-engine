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


# KDL v2 documents: a parser into a DOM over a memory interface, and a canonical emitter.

MODULE_STAPPLER_KDL_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_KDL_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_KDL_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/kdl
MODULE_STAPPLER_KDL_SRCS_OBJS :=
MODULE_STAPPLER_KDL_INCLUDES_DIRS :=
MODULE_STAPPLER_KDL_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/kdl
MODULE_STAPPLER_KDL_DEPENDS_ON := stappler_core

MODULE_STAPPLER_KDL_SHARED_SPEC_SUMMARY := libstappler KDL document parser and emitter

define MODULE_STAPPLER_KDL_SHARED_SPEC_DESCRIPTION
Module libstappler-kdl reads and writes KDL v2 documents
- an event parser over UTF-8 text with line and column in every error
- a DOM of nodes, entries and children over a memory interface
- a canonical emitter in the form of the KDL test suite
endef

$(call define_module, stappler_kdl, MODULE_STAPPLER_KDL)
