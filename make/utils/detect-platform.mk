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

# Только определяем платформу, не трогаем другие функции сборки

STAPPLER_PLATFORM :=

UNAME := $(shell uname)

ifneq ($(UNAME),Darwin)
	UNAME := $(shell uname -o)
endif

ifeq ($(findstring MSYS_NT,$(UNAME)),MSYS_NT)
	UNAME := $(shell uname -o)
endif

STAPPLER_PLATFORM ?=

ifeq ($(STAPPLER_TARGET),android)
ANDROID := 1
STAPPLER_PLATFORM += ANDROID=1
else ifeq ($(STAPPLER_TARGET),xwin)
XWIN := 1
WIN32 := 1
STAPPLER_PLATFORM += XWIN=1 WIN32=1
else ifeq ($(UNAME),Darwin)
MACOS := 1
STAPPLER_ARCH ?= $(shell uname -m)
STAPPLER_PLATFORM += MACOS=1 STAPPLER_ARCH=$(STAPPLER_ARCH)
else ifeq ($(UNAME),Msys)
MSYS := 1
WIN32 := 1
STAPPLER_PLATFORM += MSYS=1 WIN32=1
else
LINUX := 1
STAPPLER_ARCH ?= $(shell uname -m)
STAPPLER_PLATFORM += LINUX=1 STAPPLER_ARCH=$(STAPPLER_ARCH)
endif

sp_detect_platform = \
	STAPPLER_TARGET=$(1) \
	$(if $(filter host,$(1)),$(STAPPLER_PLATFORM)) \
	$(if $(filter android,$(1)),ANDROID=1) \
	$(if $(filter ios,$(1)),IOS=1) \
	$(if $(filter xwin,$(1)),XWIN=1 WIN32=1) \
	$(if $(SHARED_PREFIX),SHARED_PREFIX=$(SHARED_PREFIX))
