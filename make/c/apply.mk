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

BUILD_LIBRARY_PATH := $(OSTYPE_PREBUILT_PATH)
ifdef TOOLCHAIN_SYSROOT
BUILD_LIBRARY_PATH := $(TOOLCHAIN_SYSROOT)/lib
endif

# Список библиотек для включения в конечное приложение
# Для Android в пути к библоитеке используется символ-заместитель для архитектуры, потому используется abspath вместо realpath
ifndef BUILD_SHARED
ifdef ANDROID
BUILD_LIBS := $(call sp_toolkit_resolve_libs, $(abspath $(addprefix $(GLOBAL_ROOT)/,$(BUILD_LIBRARY_PATH))), $(TOOLKIT_LIBS)) $(LDFLAGS)
else
RPATH_PREFIX := -Wl,-rpath,
BUILD_LIBS := \
	$(if $(BUILD_HOST),\
		$(addprefix -L,$(SHARED_LIBDIR))\
		$(if $(SHARED_RPATH),$(addprefix $(RPATH_PREFIX),$(SHARED_RPATH)))) \
	$(call sp_toolkit_resolve_libs,\
		$(if $(SHARED_LIBDIR),,$(if $(BUILD_SHARED_DEPS),,$(realpath $(addprefix $(GLOBAL_ROOT)/,$(BUILD_LIBRARY_PATH))))),\
		$(TOOLKIT_LIBS),$(TOOLKIT_LIBS_SHARED))\
	$(LDFLAGS)
endif # ANDROID
else
BUILD_LIBS :=
endif # BUILD_SHARED


# Список полных путей к прекомпилируемым заголовкам
TOOLKIT_PRECOMPILED_HEADERS := $(call sp_toolkit_resolve_prefix_files,$(TOOLKIT_PRECOMPILED_HEADERS))

# Список полных путей к копиям прекомпилируемых заголовков в директории сборки
# Копирование необходимо, чтобы обеспечить приоритет включения предкомпилируемых заголовков
TOOLKIT_LIB_H_GCH := $(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/lib_objs,$(TOOLKIT_PRECOMPILED_HEADERS))
TOOLKIT_EXEC_H_GCH := $(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/exec_objs,$(TOOLKIT_PRECOMPILED_HEADERS))

# Список финальных предкомпилированных заголовков
TOOLKIT_LIB_GCH := $(addsuffix $(OSTYPE_GCH_SUFFIX),$(TOOLKIT_LIB_H_GCH))
TOOLKIT_EXEC_GCH := $(addsuffix $(OSTYPE_GCH_SUFFIX),$(TOOLKIT_EXEC_H_GCH))

TOOLKIT_LIB_GCH_DIRS = $(sort $(dir $(TOOLKIT_LIB_GCH)))
TOOLKIT_EXEC_GCH_DIRS = $(sort $(dir $(TOOLKIT_EXEC_GCH)))

# Cписок директорий для включения от фреймворка
TOOLKIT_INCLUDES := $(call sp_toolkit_include_list, $(TOOLKIT_INCLUDES_DIRS), $(TOOLKIT_INCLUDES_OBJS))

# Cписок директорий для включения от приложения
BUILD_INCLUDES := $(call sp_local_include_list,$(LOCAL_INCLUDES_DIRS),$(LOCAL_INCLUDES_OBJS))

# Вычисляем окончательные флаги сборки
BUILD_GENERAL_CFLAGS := \
	$(BUILD_TYPE_CFLAGS) \
	$(GLOBAL_GENERAL_CFLAGS) \
	$(TOOLKIT_GENERAL_CFLAGS) \
	$(LOCAL_CFLAGS) \
	$(addprefix -I,$(TOOLKIT_INCLUDES)) \
	$(addprefix -I,$(BUILD_INCLUDES)) \
	$(addprefix -I,$(OSTYPE_INCLUDE)) \
	$(BUILD_SHADERS_TARGET_INCLUDE_ALL)

BUILD_GENERAL_CXXFLAGS := \
	$(BUILD_TYPE_CXXFLAGS) \
	$(GLOBAL_GENERAL_CXXFLAGS) \
	$(TOOLKIT_GENERAL_CXXFLAGS) \
	$(LOCAL_CXXFLAGS) \
	$(addprefix -I,$(TOOLKIT_INCLUDES)) \
	$(addprefix -I,$(BUILD_INCLUDES)) \
	$(addprefix -I,$(OSTYPE_INCLUDE)) \
	$(BUILD_SHADERS_TARGET_INCLUDE_ALL)

BUILD_EXEC_CFLAGS := \
	$(addprefix -I,$(TOOLKIT_EXEC_GCH_DIRS)) \
	$(BUILD_GENERAL_CFLAGS) \
	$(GLOBAL_EXEC_CFLAGS) \
	$(TOOLKIT_EXEC_CFLAGS)

BUILD_EXEC_CXXFLAGS := \
	$(addprefix -I,$(TOOLKIT_EXEC_GCH_DIRS)) \
	$(BUILD_GENERAL_CXXFLAGS) \
	$(GLOBAL_EXEC_CXXFLAGS) \
	$(TOOLKIT_EXEC_CXXFLAGS)

BUILD_LIB_CFLAGS := \
	$(addprefix -I,$(TOOLKIT_LIB_GCH_DIRS)) \
	$(BUILD_GENERAL_CFLAGS) \
	$(GLOBAL_LIB_CFLAGS) \
	$(TOOLKIT_LIB_CFLAGS)

BUILD_LIB_CXXFLAGS := \
	$(addprefix -I,$(TOOLKIT_LIB_GCH_DIRS)) \
	$(BUILD_GENERAL_CXXFLAGS) \
	$(GLOBAL_LIB_CXXFLAGS) \
	$(TOOLKIT_LIB_CXXFLAGS)

BUILD_GENERAL_LDFLAGS := \
	$(BUILD_TYPE_LDFLAGS) \
	$(GLOBAL_GENERAL_LDFLAGS) \
	$(TOOLKIT_GENERAL_LDFLAGS) \
	$(LOCAL_LDFLAGS) \
	$(BUILD_LIBS) \
	$(call sp_toolkit_resolve_libs, $(LOCAL_LIBS))

BUILD_EXEC_LDFLAGS := \
	$(GLOBAL_EXEC_LDFLAGS) \
	$(BUILD_GENERAL_LDFLAGS) \
	$(TOOLKIT_EXEC_LDFLAGS)

BUILD_LIB_LDFLAGS := \
	$(GLOBAL_LIB_LDFLAGS) \
	$(BUILD_GENERAL_LDFLAGS) \
	$(TOOLKIT_LIB_LDFLAGS) \
	$(if $(filter-out $(LOCAL_BUILD_SHARED),1),$(OSTYPE_STANDALONE_LDFLAGS))

BUILD_CONFIG_FLAGS := $(OSTYPE_CONFIG_FLAGS) $(GLOBAL_CONFIG_FLAGS) $(TOOLKIT_CONFIG_FLAGS)
BUILD_CONFIG_VALUES := $(GLOBAL_CONFIG_VALUES) $(TOOLKIT_CONFIG_VALUES)
BUILD_CONFIG_STRINGS := $(GLOBAL_CONFIG_STRINGS) $(TOOLKIT_CONFIG_STRINGS)

BUILD_ALL_FLAGS := \
	$(BUILD_EXEC_CFLAGS) \
	$(BUILD_EXEC_CXXFLAGS) \
	$(BUILD_LIB_CFLAGS) \
	$(BUILD_LIB_CXXFLAGS) \
	$(BUILD_CONFIG_FLAGS) \
	$(BUILD_CONFIG_VALUES) \
	$(BUILD_CONFIG_STRINGS)

# Сравниваем итоговые флаги с кешированными
ifndef TARGET_ARCH_ABI
TARGET_ARCH_ABI := $$(TARGET_ARCH_ABI)
endif

-include $(TOOLKIT_CACHED_FLAGS)

BUILD_ALL_FLAGS_CACHED := \
	$(BUILD_EXEC_CFLAGS_CACHED) \
	$(BUILD_EXEC_CXXFLAGS_CACHED) \
	$(BUILD_LIB_CFLAGS_CACHED) \
	$(BUILD_LIB_CXXFLAGS_CACHED) \
	$(BUILD_CONFIG_FLAGS_CACHED) \
	$(BUILD_CONFIG_VALUES_CACHED) \
	$(BUILD_CONFIG_STRINGS_CACHED)

BUILD_ALL_FLAGS_DIFF := \
	$(filter-out $(BUILD_ALL_FLAGS), $(BUILD_ALL_FLAGS_CACHED)) \
	$(filter-out $(BUILD_ALL_FLAGS_CACHED), $(BUILD_ALL_FLAGS))

# Игнорируем секцию для stappler-build
ifndef SPBUILDTOOL
ifneq ($(strip $(BUILD_ALL_FLAGS_CACHED)),$(strip $(BUILD_ALL_FLAGS)))

$(info Build flags changed: $(BUILD_ALL_FLAGS_DIFF))

# Обновляем кешированные флаги
$(shell $(GLOBAL_MKDIR) $(BUILD_С_OUTDIR))
$(shell echo 'BUILD_EXEC_CFLAGS_CACHED:= $(BUILD_EXEC_CFLAGS)' > $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_EXEC_CXXFLAGS_CACHED:= $(BUILD_EXEC_CXXFLAGS)' >> $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_LIB_CFLAGS_CACHED:= $(BUILD_LIB_CFLAGS)' >> $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_LIB_CXXFLAGS_CACHED:= $(BUILD_LIB_CXXFLAGS)' >> $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_CONFIG_FLAGS_CACHED:= $(BUILD_CONFIG_FLAGS)' >> $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_CONFIG_VALUES_CACHED:= $(BUILD_CONFIG_VALUES)' >> $(TOOLKIT_CACHED_FLAGS))
$(shell echo 'BUILD_CONFIG_STRINGS_CACHED:= $(BUILD_CONFIG_STRINGS)' >> $(TOOLKIT_CACHED_FLAGS))

$(TOOLKIT_CACHED_FLAGS):
	@$(GLOBAL_MKDIR) $(BUILD_С_OUTDIR)
	@echo 'BUILD_EXEC_CFLAGS_CACHED:= $(BUILD_EXEC_CFLAGS)' > $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_EXEC_CXXFLAGS_CACHED:= $(BUILD_EXEC_CXXFLAGS)' >> $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_LIB_CFLAGS_CACHED:= $(BUILD_LIB_CFLAGS)' >> $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_LIB_CXXFLAGS_CACHED:= $(BUILD_LIB_CXXFLAGS)' >> $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_CONFIG_FLAGS_CACHED:= $(BUILD_CONFIG_FLAGS)' >> $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_CONFIG_VALUES_CACHED:= $(BUILD_CONFIG_VALUES)' >> $(TOOLKIT_CACHED_FLAGS)
	@echo 'BUILD_CONFIG_STRINGS_CACHED:= $(BUILD_CONFIG_STRINGS)' >> $(TOOLKIT_CACHED_FLAGS)

endif
endif

# Копируем заголовки для предкомпиляции
$(foreach target,$(TOOLKIT_PRECOMPILED_HEADERS),\
	$(eval $(call BUILD_include_rule,$(target),\
		$(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/lib_objs,$(target)))\
	))

$(foreach target,$(TOOLKIT_PRECOMPILED_HEADERS),\
	$(eval $(call BUILD_include_rule,$(target),\
		$(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/exec_objs,$(target)))\
	))

# Список полных путей к компилируемым файлам фреймворка
TOOLKIT_SRCS := $(call sp_toolkit_source_list, $(TOOLKIT_SRCS_DIRS), $(TOOLKIT_SRCS_OBJS))

# Список полных путей к компилируемым файлам приложения
BUILD_SRCS := $(call sp_local_source_list,$(LOCAL_SRCS_DIRS),$(LOCAL_SRCS_OBJS))

BUILD_MAIN_SRC := $(if $(LOCAL_MAIN),$(realpath $(addprefix $(LOCAL_ROOT)/,$(LOCAL_MAIN))))

# Список файлов для сборки приложения
# Для live reload пользовательские файлы собираются как библиотека
BUILD_EXEC_SRCS := \
	$(TOOLKIT_SRCS) \
	$(if $(filter-out $(LOCAL_EXEC_LIVE_RELOAD),1),$(BUILD_SRCS) $(BUILD_MAIN_SRC))

BUILD_LIB_SRCS := \
	$(BUILD_SRCS) \
	$(if $(filter-out $(LOCAL_BUILD_SHARED),3),$(TOOLKIT_SRCS))

# Список объектных файлов, относящихся к фреймворку
TOOLKIT_LIB_OBJS := $(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/lib_objs,$(TOOLKIT_SRCS))
TOOLKIT_EXEC_OBJS := $(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/exec_objs,$(TOOLKIT_SRCS))

BUILD_MAIN_OBJ := $(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/exec_objs,$(BUILD_MAIN_SRC))

# Список терминов для подсчёта прогресса
BUILD_LIB_WORDS := $(words $(sort $(TOOLKIT_LIB_GCH) $(BUILD_LIB_SRCS)))
BUILD_EXEC_WORDS := $(words $(sort $(TOOLKIT_EXEC_GCH) $(BUILD_EXEC_SRCS)) appconfig) # use filler to add 1 to counter

# Настраиваем шаблон прогресса
include $(BUILD_ROOT)/utils/verbose.mk

BUILD_CDB_TARGET_SRCS :=
BUILD_CDB_TARGET_OBJS :=

ifdef LOCAL_LIBRARY
include $(BUILD_ROOT)/c/library.mk
endif

ifdef LOCAL_EXECUTABLE
include $(BUILD_ROOT)/c/executable.mk
endif

# include dependencies
-include $(patsubst %.o,%.o.d,$(BUILD_EXEC_OBJS) $(BUILD_LIB_OBJS))
-include $(patsubst %.h$(OSTYPE_GCH_SUFFIX),%.h$(OSTYPE_GCH_SUFFIX).d,$(TOOLKIT_EXEC_GCH) $(TOOLKIT_LIB_GCH))

BUILD_CDB_TARGET_JSON := $(addsuffix .json,$(filter-out %.S.o,$(BUILD_CDB_TARGET_OBJS)))

$(eval $(call BUILD_cdb,$(BUILD_COMPILATION_DATABASE),$(BUILD_CDB_TARGET_JSON)))
