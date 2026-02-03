# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

# Выбираем компиляторы, предпочитаем gcc/g++ если не заданы
# Устанавливаем флаг, если компилируем с clang

ifeq ($(UNAME),Darwin)

GLOBAL_CXX ?= clang++
GLOBAL_CC ?= clang

else ifeq ($(UNAME),Windows)

GLOBAL_CXX ?= clang++
GLOBAL_CC ?= clang

else

ifeq ($(GLOBAL_COMPILER_IS_CLANG),1)
GLOBAL_CXX ?= clang++
GLOBAL_CC ?= clang
endif

ifeq ($(STAPPLER_ARCH),e2k)
ifneq ($(STAPPLER_ARCH),$(STAPPLER_TARGET_ARCH))
GLOBAL_CXX ?= e2k-linux-l++
GLOBAL_CC ?= e2k-linux-lcc
endif
endif

ifeq ($(STAPPLER_ARCH),aarch64)
ifneq ($(STAPPLER_ARCH),$(STAPPLER_TARGET_ARCH))
GLOBAL_CXX ?= aarch64-linux-gnu-g++
GLOBAL_CC ?= aarch64-linux-gnu-gcc
endif
endif

endif

ifndef GLOBAL_CXX
	ifdef MINGW
		GLOBAL_CXX := $(MINGW)-g++
	else
		GLOBAL_CXX := g++
	endif # ifdef MINGW
else
	ifneq (,$(findstring clang,$(GLOBAL_CXX)))
		GLOBAL_COMPILER_IS_CLANG := 1
	endif
endif # ifndef GLOBAL_CXX

ifndef GLOBAL_CC
	ifdef MINGW
		GLOBAL_CC := $(MINGW)-gcc
	else
		GLOBAL_CC := gcc
	endif # ifdef MINGW
else
	ifneq (,$(findstring clang,$(GLOBAL_CC)))
		GLOBAL_COMPILER_IS_CLANG := 1
	endif
endif # ifndef GLOBAL_CC
