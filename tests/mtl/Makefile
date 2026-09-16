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

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

# Рабочая директория
LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

# Директория для результатов сборки
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# Название исполняемого файла
LOCAL_EXECUTABLE := mtltest

# Пути к наборам модулей
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

# Используемые модули
LOCAL_MODULES := \
	stappler_data \
	stappler_bitmap \
	xenolith_backend_mtl \
	xenolith_renderer_basic2d_mtl \
	xenolith_font

LOCAL_SRCS_DIRS :=
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

# Подключения системы сборки
include $(STAPPLER_BUILD_ROOT)/universal.mk
