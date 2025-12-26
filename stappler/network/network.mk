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

MODULE_STAPPLER_NETWORK_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_NETWORK_LIBS := -l:libcurl-$(STAPPLER_CRYPTO_DEFAULT).a -l:libnghttp3.a -l:libzstd.a
MODULE_STAPPLER_NETWORK_LIBS_SHARED := -lcurl
MODULE_STAPPLER_NETWORK_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/network
MODULE_STAPPLER_NETWORK_SRCS_OBJS :=
MODULE_STAPPLER_NETWORK_INCLUDES_DIRS :=
MODULE_STAPPLER_NETWORK_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/network
MODULE_STAPPLER_NETWORK_DEPENDS_ON := stappler_crypto stappler_filesystem stappler_brotli_lib stappler_data
MODULE_STAPPLER_NETWORK_GENERAL_LDFLAGS :=

ifdef LINUX
MODULE_STAPPLER_NETWORK_LIBS += -l:libidn2.a
endif

ifdef MACOS
MODULE_STAPPLER_NETWORK_GENERAL_LDFLAGS += -framework SystemConfiguration -framework CoreFoundation -framework Security -liconv
endif

ifdef WIN32
MODULE_STAPPLER_NETWORK_GENERAL_LDFLAGS += -lnormaliz -loldnames -lbcrypt
endif

#spec

MODULE_STAPPLER_NETWORK_SHARED_SPEC_SUMMARY := libstappler network interface

define MODULE_STAPPLER_NETWORK_SHARED_SPEC_DESCRIPTION
Module libstappler-network is an interface for network queries, based on cURL
endef

# module name resolution
$(call define_module, stappler_network, MODULE_STAPPLER_NETWORK)
