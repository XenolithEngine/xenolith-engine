# Catalog for the Xenolith installer's own modules — the UI-agnostic core library shared by
# the CLI and the GUI (mirrors the Rust `xenolith-installer-core` crate split).
INSTALLER_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

TOOLKIT_MODULE_LIST += $(INSTALLER_MODULE_DIR)/core/core.mk
