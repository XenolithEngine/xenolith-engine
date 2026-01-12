# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

MODULE_XENOLITH_RESOURCES_NETWORK_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RESOURCES_NETWORK_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_RESOURCES_NETWORK_LIBS :=
MODULE_XENOLITH_RESOURCES_NETWORK_LIBS_SHARED := -lcurl
MODULE_XENOLITH_RESOURCES_NETWORK_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/resources/network
MODULE_XENOLITH_RESOURCES_NETWORK_SRCS_OBJS :=
MODULE_XENOLITH_RESOURCES_NETWORK_INCLUDES_DIRS :=
MODULE_XENOLITH_RESOURCES_NETWORK_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/resources/network
MODULE_XENOLITH_RESOURCES_NETWORK_DEPENDS_ON := xenolith_application stappler_network

#spec

MODULE_XENOLITH_RESOURCES_NETWORK_SHARED_SPEC_SUMMARY := Xenolith network interface

define MODULE_XENOLITH_RESOURCES_NETWORK_SHARED_SPEC_DESCRIPTION
Module libxenolith-resources-network implements dedicated thread with multiplexed
network interface, that can be bound with application loop.
endef

# module name resolution
$(call define_module, xenolith_resources_network, MODULE_XENOLITH_RESOURCES_NETWORK)
