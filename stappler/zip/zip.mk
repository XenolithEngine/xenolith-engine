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

MODULE_STAPPLER_ZIP_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_ZIP_PRIVATE_INCLUDE_PCH := SPCommon.h
# zlib is the only external dependency left: inflate/deflate for method 8 and
# crc32 for entry integrity. Everything else - the catalog, the name decoding,
# the reader and the writer - is in this directory.
MODULE_STAPPLER_ZIP_LIBS :=
MODULE_STAPPLER_ZIP_LIBS_SHARED := -lz
MODULE_STAPPLER_ZIP_FLAGS :=
MODULE_STAPPLER_ZIP_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/zip
MODULE_STAPPLER_ZIP_SRCS_OBJS :=
MODULE_STAPPLER_ZIP_INCLUDES_DIRS :=
MODULE_STAPPLER_ZIP_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/zip
# No stappler_crypto: that dependency existed to pick the libzip build variant
# (libzip-$(STAPPLER_CRYPTO_DEFAULT).a, for its AES support). CoderSource comes
# from stappler_core's SPCoreCrypto.h, and crc32 comes from zlib.
MODULE_STAPPLER_ZIP_DEPENDS_ON :=
MODULE_STAPPLER_ZIP_GENERAL_LDFLAGS :=

ifdef LINUX
MODULE_STAPPLER_ZIP_LIBS += -l:libz.a
endif

ifeq ($(TARGET_SYSTEM),Darwin)
MODULE_STAPPLER_ZIP_GENERAL_LDFLAGS += -lz
endif

ifdef ANDROID
MODULE_STAPPLER_ZIP_LIBS += -l:libz.a
endif

ifdef WIN32
MODULE_STAPPLER_ZIP_LIBS += -lz
endif

ifeq ($(TARGET_SYSTEM),WASM)
MODULE_STAPPLER_ZIP_LIBS += -l:libz.a
endif

ifdef NUTTX
MODULE_STAPPLER_ZIP_LIBS += -l:libz.a
endif

ifdef EMBOX
MODULE_STAPPLER_ZIP_LIBS += -l:libz.a
endif

#spec

MODULE_STAPPLER_ZIP_SHARED_SPEC_SUMMARY := libstappler ZIP archive interface

define MODULE_STAPPLER_ZIP_SHARED_SPEC_DESCRIPTION
Module libstappler-zip implements interface to access files within ZIP archive
endef

# module name resolution
$(call define_module, stappler_zip, MODULE_STAPPLER_ZIP)
