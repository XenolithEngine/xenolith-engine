# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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


BUILD_EXECUTABLE_DSYM :=
BUILD_EXECUTABLE_DSYM_GOAL :=


ifdef BUILD_EXECUTABLE

ifneq ($(RELEASE),1)
BUILD_EXECUTABLE_DSYM := $(BUILD_EXECUTABLE).dSYM

# Цель должна указывать на конкретный файл, чтобы сравнить даты создания
BUILD_EXECUTABLE_DSYM_GOAL := $(BUILD_EXECUTABLE_DSYM)/Contents/Resources/DWARF/$(notdir $(BUILD_EXECUTABLE))

$(BUILD_EXECUTABLE_DSYM_GOAL) : $(BUILD_EXECUTABLE)
	dsymutil $(BUILD_EXECUTABLE) -o $(BUILD_EXECUTABLE_DSYM)

all: $(BUILD_EXECUTABLE_DSYM_GOAL)
endif # ($(RELEASE),1)


ifdef OSTYPE_IS_IOS
# iOS .app bundles are flat: Info.plist sits at the bundle root, beside the executable.
BUILD_INFO_PLIST := $(abspath $(dir $(BUILD_EXECUTABLE))Info.plist)
ifeq ($(TARGET_SDK_NAME),iphonesimulator)
BUILD_INFO_PLIST_PLATFORM := iPhoneSimulator
else
BUILD_INFO_PLIST_PLATFORM := iPhoneOS
endif
else
BUILD_INFO_PLIST := $(abspath $(dir $(BUILD_EXECUTABLE))../Info.plist)
endif

ifeq ($(LOCAL_MACOS_BUNDLE),1)
all: $(BUILD_INFO_PLIST)
endif

ifdef OSTYPE_IS_IOS
# iOS needs an iOS-flavoured plist (MinimumOSVersion / CFBundleSupportedPlatforms /
# UIDeviceFamily) rather than the macOS LSMinimumSystemVersion / NSHighResolutionCapable.
$(BUILD_INFO_PLIST): $(BUILD_EXECUTABLE)
	@$(call rule_mkdir,$(dir $(BUILD_INFO_PLIST)))
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > $@
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $@
	@echo '<plist version="1.0">' >> $@
	@echo '<dict>' >> $@
	@echo '	<key>CFBundleExecutable</key>' >> $@
	@echo '	<string>$(LOCAL_EXECUTABLE)</string>' >> $@
	@echo '	<key>CFBundleIdentifier</key>' >> $@
	@echo '	<string>$(APPCONFIG_BUNDLE_NAME)</string>' >> $@
	@echo '	<key>CFBundlePackageType</key>' >> $@
	@echo '	<string>APPL</string>' >> $@
	@echo '	<key>MinimumOSVersion</key>' >> $@
	@echo '	<string>$(TARGET_OSVER)</string>' >> $@
	@echo '	<key>CFBundleSupportedPlatforms</key>' >> $@
	@echo '	<array>' >> $@
	@echo '		<string>$(BUILD_INFO_PLIST_PLATFORM)</string>' >> $@
	@echo '	</array>' >> $@
	@echo '	<key>UIDeviceFamily</key>' >> $@
	@echo '	<array>' >> $@
	@echo '		<integer>1</integer>' >> $@
	@echo '		<integer>2</integer>' >> $@
	@echo '	</array>' >> $@
	@echo '</dict>' >> $@
	@echo '</plist>' >> $@
else ifeq ($(GLOBAL_SHELL),powershell)
$(BUILD_INFO_PLIST): $(BUILD_EXECUTABLE)
	@$(call rule_mkdir,$(dir $(BUILD_INFO_PLIST)))
	@"<?xml version=$"1.0$" encoding=$"UTF-8$"?>`n", \
	"<!DOCTYPE plist PUBLIC $"-//Apple//DTD PLIST 1.0//EN$" $"http://www.apple.com/DTDs/PropertyList-1.0.dtd$">`n", \
	"<plist version=$"1.0$">`n", \
	"<dict>`n", \
	"	<key>CFBundleExecutable</key>`n", \
	"	<string>$(LOCAL_EXECUTABLE)</string>`n", \
	"	<key>CFBundleIdentifier</key>`n", \
	"	<string>$(APPCONFIG_BUNDLE_NAME)</string>`n", \
	"	<key>LSMinimumSystemVersion</key>`n", \
	"	<string>$(TARGET_OSVER)</string>`n", \
	"	<key>NSHighResolutionCapable</key>`n", \
	"	<true/>`n", \
	"</dict>`n", \
	"</plist>`n", \
		| Set-Content -NoNewline -Encoding utf8 -Path "$@"
else
# Two complete recipes: mid-recipe `ifdef` truncates xlmake, and a shell `if` cannot
# run under the wasm host (no /bin/sh). Icon keys are a parse-time choice.
ifndef LOCAL_MACOS_ICON
$(BUILD_INFO_PLIST): $(BUILD_EXECUTABLE)
	@$(call rule_mkdir,$(dir $(BUILD_INFO_PLIST)))
	@$(WRITE_START) '<?xml version=$"1.0$" encoding=$"UTF-8$"?>' $(WRITE_END)
	@$(APPEND_START) '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' $(APPEND_END)
	@$(APPEND_START) '<plist version="1.0">' $(APPEND_END)
	@$(APPEND_START) '<dict>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleExecutable</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(LOCAL_EXECUTABLE)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleName</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(LOCAL_EXECUTABLE)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleDisplayName</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(APPCONFIG_APP_NAME)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleIdentifier</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(APPCONFIG_BUNDLE_NAME)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundlePackageType</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>APPL</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>LSMinimumSystemVersion</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(TARGET_OSVER)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>NSHighResolutionCapable</key>' $(APPEND_END)
	@$(APPEND_START) '	<true/>' $(APPEND_END)
	@$(APPEND_START) '</dict>' $(APPEND_END)
	@$(APPEND_START) '</plist>' $(APPEND_END)
else
$(BUILD_INFO_PLIST): $(BUILD_EXECUTABLE)
	@$(call rule_mkdir,$(dir $(BUILD_INFO_PLIST)))
	@$(WRITE_START) '<?xml version=$"1.0$" encoding=$"UTF-8$"?>' $(WRITE_END)
	@$(APPEND_START) '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' $(APPEND_END)
	@$(APPEND_START) '<plist version="1.0">' $(APPEND_END)
	@$(APPEND_START) '<dict>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleExecutable</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(LOCAL_EXECUTABLE)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleName</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(LOCAL_EXECUTABLE)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleDisplayName</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(APPCONFIG_APP_NAME)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleIdentifier</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(APPCONFIG_BUNDLE_NAME)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundlePackageType</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>APPL</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>LSMinimumSystemVersion</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(TARGET_OSVER)</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>NSHighResolutionCapable</key>' $(APPEND_END)
	@$(APPEND_START) '	<true/>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleIconFile</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(basename $(notdir $(LOCAL_MACOS_ICON)))</string>' $(APPEND_END)
	@$(APPEND_START) '	<key>CFBundleIconName</key>' $(APPEND_END)
	@$(APPEND_START) '	<string>$(basename $(notdir $(LOCAL_MACOS_ICON)))</string>' $(APPEND_END)
	@$(APPEND_START) '</dict>' $(APPEND_END)
	@$(APPEND_START) '</plist>' $(APPEND_END)
endif
endif

# Optional Dock/Finder icon: LOCAL_MACOS_ICON=/path/to/AppIcon.icns → Contents/Resources/<name>.icns
# and CFBundleIconFile in Info.plist (name without extension).
ifdef LOCAL_MACOS_ICON
BUILD_MACOS_ICON := $(abspath $(dir $(BUILD_INFO_PLIST))/Resources/$(notdir $(LOCAL_MACOS_ICON)))
$(BUILD_MACOS_ICON): $(LOCAL_MACOS_ICON) $(BUILD_INFO_PLIST)
	@$(call rule_mkdir,$(dir $(BUILD_MACOS_ICON)))
	@cp -f $(LOCAL_MACOS_ICON) $(BUILD_MACOS_ICON)
all: $(BUILD_MACOS_ICON)
endif

# Ad-hoc sign the .app after Info.plist (+ optional icon) are in place. A
# linker-signed Mach-O alone leaves Info.plist unbound and Sealed Resources
# empty, so Dock/Finder ignore CFBundleIconFile and show a generic icon.
#
# Darwin host only: `codesign` is an Apple tool with no counterpart in the cross toolchain, so a
# macOS build hosted on Linux/Windows would fail on it. Those builds keep the linker's ad-hoc
# signature - enough to run, and the bundle gets re-signed whenever it is built on a Mac.
ifeq ($(UNAME),Darwin)
ifndef OSTYPE_IS_IOS
ifeq ($(LOCAL_MACOS_BUNDLE),1)
BUILD_MACOS_APP_BUNDLE := $(abspath $(dir $(BUILD_EXECUTABLE))/../..)
BUILD_MACOS_BUNDLE_SIGN := $(dir $(BUILD_MACOS_APP_BUNDLE)).$(notdir $(BUILD_MACOS_APP_BUNDLE)).adhoc-signed
BUILD_MACOS_BUNDLE_SIGN_DEPS := $(BUILD_EXECUTABLE) $(BUILD_INFO_PLIST)
ifdef LOCAL_MACOS_ICON
BUILD_MACOS_BUNDLE_SIGN_DEPS += $(BUILD_MACOS_ICON)
endif
$(BUILD_MACOS_BUNDLE_SIGN): $(BUILD_MACOS_BUNDLE_SIGN_DEPS)
	codesign --force --deep --sign - $(BUILD_MACOS_APP_BUNDLE)
	@touch $@
all: $(BUILD_MACOS_BUNDLE_SIGN)
endif # LOCAL_MACOS_BUNDLE
endif # !OSTYPE_IS_IOS
endif # UNAME == Darwin

endif # BUILD_EXECUTABLE

mac-shaders: $(BUILD_SHADERS_EMBEDDED) $(TOOLKIT_SHADERS_EMBEDDED)

.PHONY: mac-shaders
