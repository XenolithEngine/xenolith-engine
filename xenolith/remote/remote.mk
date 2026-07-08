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

MODULE_XENOLITH_REMOTE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_REMOTE_PRECOMPILED_HEADERS := $(XENOLITH_MODULE_DIR)/core/XLCommon.h
MODULE_XENOLITH_REMOTE_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_REMOTE_LIBS :=
MODULE_XENOLITH_REMOTE_LIBS_SHARED :=
MODULE_XENOLITH_REMOTE_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/remote
MODULE_XENOLITH_REMOTE_SRCS_OBJS :=
MODULE_XENOLITH_REMOTE_INCLUDES_DIRS :=
MODULE_XENOLITH_REMOTE_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/remote
MODULE_XENOLITH_REMOTE_DEPENDS_ON := xenolith_core stappler_crypto stappler_data

ifeq ($(TARGET_SYSTEM),WASM)
# Stub OpenSSL/QUIC headers: the native SSL transport compiles but is unused on wasm.
MODULE_XENOLITH_REMOTE_PRIVATE_INCLUDES += $(XENOLITH_MODULE_DIR)/remote/wasm-stub
endif

#spec

MODULE_XENOLITH_REMOTE_SHARED_SPEC_SUMMARY := Xenolith remote connection transport (QUIC)

define MODULE_XENOLITH_REMOTE_SHARED_SPEC_DESCRIPTION
Module libxenolith-remote implements the server-side connection listener and client
transport (OpenSSL QUIC) for the remote rendering protocol (the X11-style client/server split).
endef

# module name resolution
$(call define_module, xenolith_remote, MODULE_XENOLITH_REMOTE)
