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

BUILD_INFO_PLIST := $(abspath $(dir $(BUILD_EXECUTABLE))../Info.plist)

all: $(BUILD_INFO_PLIST)


ifeq ($(GLOBAL_SHELL),powershell)
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
$(BUILD_INFO_PLIST): $(BUILD_EXECUTABLE)
	@$(call rule_mkdir,$(dir $(BUILD_INFO_PLIST)))
	@echo '<?xml version=$"1.0$" encoding=$"UTF-8$"?>' > $@
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $@
	@echo '<plist version="1.0">' >> $@
	@echo '<dict>' >> $@
	@echo '	<key>CFBundleExecutable</key>' >> $@
	@echo '	<string>$(LOCAL_EXECUTABLE)</string>' >> $@
	@echo '	<key>CFBundleIdentifier</key>' >> $@
	@echo '	<string>$(APPCONFIG_BUNDLE_NAME)</string>' >> $@
	@echo '	<key>LSMinimumSystemVersion</key>' >> $@
	@echo '	<string>$(TARGET_OSVER)</string>' >> $@
	@echo '	<key>NSHighResolutionCapable</key>' >> $@
	@echo '	<true/>' >> $@
	@echo '</dict>' >> $@
	@echo '</plist>' >> $@
endif

endif # BUILD_EXECUTABLE


MACOS_PROJECT_DIR := $(addsuffix proj.macos,$(dir $(realpath $(LOCAL_MAKEFILE))))

MACOS_CONSUMED_MODULES := $(sort $(foreach module,$(LOCAL_MODULES),$($(MODULE_$(module))_SHARED_CONSUME)))

MACOS_CONSUMED_LDFLAGS := $(foreach module,$(MACOS_CONSUMED_MODULES),$($(MODULE_$(module))_GENERAL_LDFLAGS))

MACOS_GENERAL_CFLAGS := \
	-DSTAPPLER -DSTAPPLER_SHARED \
	$(TOOLKIT_GENERAL_CFLAGS) \
	$(LOCAL_CFLAGS)

MACOS_GENERAL_CXXFLAGS := \
	-DSTAPPLER -DSTAPPLER_SHARED \
	$(TOOLKIT_GENERAL_CXXFLAGS) \
	$(LOCAL_CXXFLAGS)

MACOS_EXEC_CFLAGS := \
	$(GLOBAL_EXEC_CFLAGS) \
	$(TOOLKIT_EXEC_CFLAGS)

MACOS_EXEC_CXXFLAGS := \
	$(GLOBAL_EXEC_CXXFLAGS) \
	$(TOOLKIT_EXEC_CXXFLAGS)

MACOS_LIB_CFLAGS := \
	$(GLOBAL_LIB_CFLAGS) \
	$(TOOLKIT_LIB_CFLAGS)

MACOS_LIB_CXXFLAGS := \
	$(GLOBAL_LIB_CXXFLAGS) \
	$(TOOLKIT_LIB_CXXFLAGS)

MACOS_GENERAL_LDFLAGS := \
	$(GLOBAL_GENERAL_LDFLAGS) \
	$(TOOLKIT_GENERAL_LDFLAGS) \
	$(LOCAL_LDFLAGS) \
	$(MACOS_CONSUMED_LDFLAGS)

MACOS_EXEC_LDFLAGS := \
	$(GLOBAL_EXEC_LDFLAGS) \
	$(TOOLKIT_EXEC_LDFLAGS)

MACOS_LIB_LDFLAGS := \
	$(GLOBAL_LIB_LDFLAGS) \
	$(TOOLKIT_LIB_LDFLAGS) \
	$(if $(filter-out $(LOCAL_BUILD_SHARED),1),$(OSTYPE_STANDALONE_LDFLAGS))

MACOS_HEADER_SEARCH_PATHS := \
	$(realpath $(TOOLKIT_INCLUDES)) \
	$(realpath $(BUILD_INCLUDES)) \
	$(BUILD_SHADERS_OUTDIR)


MACOS_LIBS := $(call sp_toolkit_transform_lib_ldflag, $(TOOLKIT_LIBS))

mac-shaders: $(BUILD_SHADERS_EMBEDDED) $(TOOLKIT_SHADERS_EMBEDDED)

.PHONY: mac-shaders
