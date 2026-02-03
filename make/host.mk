# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

.DEFAULT_GOAL := all
BUILD_HOST := 1
BUILD_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(BUILD_ROOT)/general/compile.mk

all: $(BUILD_SHARED_LIBRARY) $(BUILD_EXECUTABLE) $(BUILD_STATIC_LIBRARY) $(BUILD_WASM_MODULE)

ifdef LOCAL_FORCE_INSTALL
all: install
endif # LOCAL_FORCE_INSTALL

ifdef OSTYPE_IS_MACOS
ifneq ($(RELEASE),1)
ifdef BUILD_EXECUTABLE
BUILD_EXECUTABLE_DSYM := $(BUILD_EXECUTABLE).dSYM

# Цель должна указывать на конкретный файл, чтобы сравнить даты создания
BUILD_EXECUTABLE_DSYM_GOAL := $(BUILD_EXECUTABLE_DSYM)/Contents/Resources/DWARF/$(notdir $(BUILD_EXECUTABLE))

$(BUILD_EXECUTABLE_DSYM_GOAL) : $(BUILD_EXECUTABLE)
	dsymutil $(BUILD_EXECUTABLE) -o $(BUILD_EXECUTABLE_DSYM)

all: $(BUILD_EXECUTABLE_DSYM_GOAL)
endif # BUILD_EXECUTABLE
endif # ($(RELEASE),1)

include $(BUILD_ROOT)/distrib/macos.mk

endif # OSTYPE_IS_MACOS

$(BUILD_EXECUTABLE) : $(BUILD_COMPILATION_DATABASE) $(BUILD_EXEC_OBJS) $(BUILD_WASM_OUTPUT)
	$(GLOBAL_QUIET_LINK) $(GLOBAL_CXX) $(BUILD_EXEC_OBJS) $(BUILD_EXEC_LDFLAGS) -o $(BUILD_EXECUTABLE)

$(BUILD_SHARED_LIBRARY): $(BUILD_COMPILATION_DATABASE) $(BUILD_LIB_OBJS) $(BUILD_WASM_OUTPUT)
	$(GLOBAL_QUIET_LINK) $(GLOBAL_CXX) -shared $(BUILD_LIB_OBJS) $(BUILD_LIB_LDFLAGS) -o $(BUILD_SHARED_LIBRARY)

$(BUILD_STATIC_LIBRARY): $(BUILD_COMPILATION_DATABASE) $(BUILD_LIB_OBJS) $(BUILD_WASM_OUTPUT)
	$(GLOBAL_QUIET_LINK) $(GLOBAL_AR) $(BUILD_STATIC_LIBRARY) $(BUILD_LIB_OBJS)

$(BUILD_INSTALL_SHARED_LIBRARY): $(BUILD_SHARED_LIBRARY)
	@$(call rule_mkdir,$(dir $(BUILD_INSTALL_SHARED_LIBRARY)))
	$(GLOBAL_CP) $(BUILD_SHARED_LIBRARY) $(BUILD_INSTALL_SHARED_LIBRARY)

$(BUILD_INSTALL_STATIC_LIBRARY): $(BUILD_STATIC_LIBRARY)
	@$(call rule_mkdir,$(dir $(BUILD_INSTALL_STATIC_LIBRARY)))
	$(GLOBAL_CP) $(BUILD_STATIC_LIBRARY) $(BUILD_INSTALL_STATIC_LIBRARY)

$(BUILD_INSTALL_EXECUTABLE): $(BUILD_EXECUTABLE) $(BUILD_EXECUTABLE_DSYM_GOAL)
	@$(call rule_mkdir,$(dir $(BUILD_INSTALL_EXECUTABLE)))
	$(GLOBAL_CP) $(BUILD_EXECUTABLE) $(BUILD_INSTALL_EXECUTABLE)

$(BUILD_INSTALL_WASM_MODULE): $(BUILD_WASM_MODULE)
	@$(call rule_mkdir,$(dir $(BUILD_INSTALL_WASM_MODULE)))
	$(GLOBAL_CP) $(BUILD_WASM_MODULE) $(BUILD_INSTALL_WASM_MODULE)

# For self-coverage
report: $(BUILD_OBJS) $(BUILD_EXECUTABLE)
	$(BUILD_EXECUTABLE)
	$(call rule_mkdir,$(BUILD_С_OUTDIR)/gcov)
	lcov --capture --directory . --output-file $(BUILD_С_OUTDIR)/gcov/coverage.info \
		--exclude '/usr/include/*' --exclude '/usr/lib/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/'build/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/'deps/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/'tests/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/core/'thirdparty/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/xenolith/'thirdparty/*' \
		--exclude $(realpath $(STAPPLER_ROOT))/xenolith/'utils/*'
	genhtml $(BUILD_С_OUTDIR)/gcov/coverage.info --demangle-cpp --output-directory $(BUILD_С_OUTDIR)/html

install: $(BUILD_INSTALL_SHARED_LIBRARY) $(BUILD_INSTALL_EXECUTABLE) $(BUILD_INSTALL_STATIC_LIBRARY)  $(BUILD_INSTALL_WASM_MODULE)

clean:
	$(GLOBAL_RM) -r $(BUILD_С_OUTDIR) $(BUILD_SHADERS_OUTDIR) $(BUILD_WIT_OUTDIR) $(BUILD_WASM_OUTDIR)

.PHONY: all report install clean
