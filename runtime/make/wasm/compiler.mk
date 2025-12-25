# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

BUILD_WIT_OUTDIR := $(BUILD_OUTDIR)/$(notdir $(WIT_BINDGEN))
BUILD_WASM_OUTDIR := $(BUILD_OUTDIR)/wasm/$(notdir $(WASI_SDK_CC))

ifeq ($(WASI_THREADS),1)
BUILD_WASM_CFLAGS_DEFAULT := -DSTAPPLER -DWASM -mreference-types -mbulk-memory -msign-ext -mmultivalue -mtail-call \
	 --target=wasm32-wasi-threads
else
BUILD_WASM_CFLAGS_DEFAULT := -DSTAPPLER -DWASM -mreference-types -mbulk-memory -msign-ext -mmultivalue -mtail-call \
	 --target=wasm32-wasi
endif

ifdef WASI_SYSROOT
BUILD_WASM_CFLAGS_DEFAULT += --sysroot=$(WASI_SYSROOT)
endif

ifeq ($(GLOBAL_WASM_DEBUG),1)
$(info WASM debug enabled)
ifdef RELEASE
BUILD_WASM_CFLAGS_DEFAULT += $(GLOBAL_WASM_OPTIMIZATION) -DNDEBUG
else
BUILD_WASM_CFLAGS_DEFAULT += -g -DDEBUG
endif # ifdef RELEASE
else
BUILD_WASM_CFLAGS_DEFAULT += $(GLOBAL_WASM_OPTIMIZATION) -DNDEBUG
endif

# Force WASI to export memory allocation functions
BUILD_WASM_LDFLAGS := -z stack-size=4096 -Wl,--no-entry -Wl,--export=malloc -Wl,--export=free -Wl,--export=realloc -mexec-model=reactor

BUILD_WASM_CFLAGS_INIT := $(BUILD_WASM_CFLAGS_DEFAULT) -std=$(GLOBAL_STD)
BUILD_WASM_CXXFLAGS_INIT := $(BUILD_WASM_CFLAGS_DEFAULT) -std=$(GLOBAL_STDXX) -fno-exceptions

sp_compile_wasm_c = $(GLOBAL_QUIET_WASM_CC) $(GLOBAL_MKDIR) $(dir $@); $(WASI_SDK_CC) \
	$(OSTYPE_C_FILE) $(call sp_compile_dep, $@, $(1)) -c -o $@ $<

sp_compile_wasm_cpp = $(GLOBAL_QUIET_WASM_CXX) $(GLOBAL_MKDIR) $(dir $@); $(WASI_SDK_CXX) \
	$(OSTYPE_CPP_FILE) $(call sp_compile_dep, $@, $(1))  -c -o $@ $<

sp_toolkit_wit_list = $(call sp_make_general_source_list,$(1),$(2),$(GLOBAL_ROOT),*.wit,%.c %.cpp %.mm)

sp_local_wit_list = $(call sp_make_general_source_list,$(1),$(2),$(LOCAL_ROOT),*.wit,%.c %.cpp %.mm)

sp_wasm_srcs_list =  $(call sp_make_general_source_list,$(1),$(2),$(3),*.cpp *.c)

sp_wasm_include_list = $(call sp_make_general_include_list,$(1),$(2),$(3))

sp_wasm_object_list = \
	$(addprefix $(1),$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(realpath $(2)))))

sp_copy_wit = $(GLOBAL_QUIET_WIT) $(GLOBAL_MKDIR) $(BUILD_WIT_OUTDIR)/wit; $(GLOBAL_CP) $(1) $(BUILD_WIT_OUTDIR)/wit/$(notdir $(1))
