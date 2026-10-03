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

# The graph kernel: values and schemas, the build, the interpreter over both stores, the standard
# operations and the generator, in the kernel's own environment (no scene).
#
#     flowtest                     every section, reduced
#     flowtest --full              the gate
#     flowtest flow-run            one or several
#     flowtest --emit=tests/flow/gen flow-codegen    rewrite the committed unit, then rebuild
#
# Exit code is the number of failed checks (clamped to 254).

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := flowtest

LOCAL_PRIVATE_INCLUDE_PCH := SPCommon.h

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk

# stappler_filesystem and stappler_brotli_lib are shared-only dependencies of stappler_data, and a
# static build has to name them.
LOCAL_MODULES ?= \
	runtime \
	stappler_core \
	stappler_filesystem \
	stappler_data \
	stappler_brotli_lib \
	stappler_vstore \
	stappler_flow_value \
	stappler_flow \
	stappler_flow_ops \
	stappler_flow_codegen

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

# gen/ is the unit the generator wrote for the kernel's own environment, committed and proved
# current by `flow-codegen`.
LOCAL_SRCS_DIRS := kernel gen
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS := $(LOCAL_ROOT)

LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := FlowTest
APPCONFIG_BUNDLE_NAME := org.stappler.FlowTest
APPCONFIG_APP_PATH_COMMON := 3

include $(STAPPLER_BUILD_ROOT)/universal.mk
