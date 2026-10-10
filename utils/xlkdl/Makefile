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

# xlkdl: KDL documents from the command line, and C headers generated from ABI descriptions.
#
#     make -C utils/xlkdl
#     xlkdl dump <file.kdl>
#     xlkdl fmt <file.kdl>
#     xlkdl abi --map <map.kdl> --out <dir> [--check] [--inline] [<header>...]

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := xlkdl

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

LOCAL_PRIVATE_INCLUDE_PCH := SPCommon.h

LOCAL_SRCS_DIRS := src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk

# stappler_filesystem and stappler_brotli_lib are shared-only dependencies of stappler_data, and a
# static build has to name them.
LOCAL_MODULES ?= \
	runtime \
	stappler_core \
	stappler_filesystem \
	stappler_data \
	stappler_brotli_lib \
	stappler_kdl

APPCONFIG_APP_NAME := XlKdl
APPCONFIG_BUNDLE_NAME := org.stappler.XlKdl

# Next to the executable: a tool reads and writes the paths it is given.
APPCONFIG_APP_PATH_COMMON := 0

include $(STAPPLER_BUILD_ROOT)/universal.mk
