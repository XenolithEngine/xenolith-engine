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

# UI-agnostic installer core: dirs/Layout, target-triple detection, manifest/catalogue,
# transport (HTTPS + FTP), install state. A library consumed by both the CLI and the GUI
# (mirrors the Rust `xenolith-installer-core` crate).
INSTALLER_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

MODULE_INSTALLER_CORE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_INSTALLER_CORE_SRCS_DIRS := $(INSTALLER_CORE_DIR)src
MODULE_INSTALLER_CORE_INCLUDES_OBJS := $(INSTALLER_CORE_DIR)src
MODULE_INSTALLER_CORE_DEPENDS_ON := stappler_core stappler_filesystem stappler_data stappler_git

MODULE_INSTALLER_CORE_SHARED_SPEC_SUMMARY := Xenolith installer UI-agnostic core (CLI + GUI shared)

# module name resolution
$(call define_module, installer_core, MODULE_INSTALLER_CORE)
