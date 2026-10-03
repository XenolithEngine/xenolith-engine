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

# What a value is, how a record of values is laid out in an arena, and the registry of record types.
# Nothing here knows an entity store, a scene, a file envelope or a sentence of text; diagnostics are
# numbers (SPFlowValueDiag.h) that the layers above turn into words.

MODULE_STAPPLER_FLOW_VALUE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FLOW_VALUE_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_FLOW_VALUE_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/flow/value
MODULE_STAPPLER_FLOW_VALUE_SRCS_OBJS :=
MODULE_STAPPLER_FLOW_VALUE_INCLUDES_DIRS :=
MODULE_STAPPLER_FLOW_VALUE_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/flow/value
MODULE_STAPPLER_FLOW_VALUE_DEPENDS_ON := stappler_core stappler_data stappler_vstore

MODULE_STAPPLER_FLOW_VALUE_SHARED_SPEC_SUMMARY := libstappler typed values and record schemas over the versioned arena

define MODULE_STAPPLER_FLOW_VALUE_SHARED_SPEC_DESCRIPTION
Module libstappler-flow-value implements typed values laid out in a stappler_vstore arena
- value types, element chains, casts and their codec
- record schemas: field layout, access, init/copy/destroy
- a registry of record, enum and alias types
- numeric diagnostics that a consumer turns into its own words
endef

$(call define_module, stappler_flow_value, MODULE_STAPPLER_FLOW_VALUE)
