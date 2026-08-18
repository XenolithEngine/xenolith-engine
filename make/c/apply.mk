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

ifdef TARGET_LIBDIR
BUILD_LIBRARY_PATH := $(TARGET_LIBDIR)
else ifdef TARGET_SYSROOT
BUILD_LIBRARY_PATH := $(TARGET_SYSROOT)/usr/lib
endif

$(call print_verbose,(c/apply.mk) Build source lists)

# Список библиотек для включения в конечное приложение
# Для Android в пути к библоитеке используется символ-заместитель для архитектуры, потому используется abspath вместо realpath
ifdef ANDROID
BUILD_LIBS := $(call sp_toolkit_resolve_libs, $(abspath $(BUILD_LIBRARY_PATH)/$$(BUILD_ARCH)), $(TOOLKIT_LIBS))
else
RPATH_PREFIX := -Wl,-rpath,
BUILD_LIBS := \
	$(call sp_toolkit_resolve_libs,\
		$(TARGET_LIB_DIR),\
		$(TOOLKIT_LIBS),$(TOOLKIT_LIBS_SHARED))
endif # ANDROID

# Список полных путей к прекомпилируемым заголовкам
TOOLKIT_PRECOMPILED_HEADERS := $(sort $(call sp_toolkit_resolve_prefix_files,$(TOOLKIT_PRECOMPILED_HEADERS)))

# Предкомпилированные заголовки уровня приложения (по аналогии с MODULE_*_PRECOMPILED_HEADERS).
# Задаются в Makefile приложения через LOCAL_PRECOMPILED_HEADERS; относительные пути
# разрешаются относительно каталога приложения (LOCAL_ROOT). Позволяют приложению держать
# собственный PCH — например, при отладке через live reload считать заголовки движка
# стабильными и запечь их в PCH. Потребление — через LOCAL_PRIVATE_INCLUDE_PCH.
# Вливаются в общий список PCH, поэтому GCH для них строятся и для lib, и для exec,
# а зависимости и счётчики прогресса подхватываются существующими правилами.
LOCAL_PRECOMPILED_HEADERS := $(sort $(call sp_local_resolve_prefix_files,$(LOCAL_PRECOMPILED_HEADERS)))
TOOLKIT_PRECOMPILED_HEADERS := $(sort $(TOOLKIT_PRECOMPILED_HEADERS) $(LOCAL_PRECOMPILED_HEADERS))

# Список полных путей к копиям прекомпилируемых заголовков в директории сборки
# Копирование необходимо, чтобы обеспечить приоритет включения предкомпилируемых заголовков
TOOLKIT_LIB_H_GCH := $(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/lib_objs,$(TOOLKIT_PRECOMPILED_HEADERS))
TOOLKIT_EXEC_H_GCH := $(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/exec_objs,$(TOOLKIT_PRECOMPILED_HEADERS))

# Список финальных предкомпилированных заголовков
TOOLKIT_LIB_GCH := $(addsuffix $(OSTYPE_GCH_SUFFIX),$(TOOLKIT_LIB_H_GCH))
TOOLKIT_EXEC_GCH := $(addsuffix $(OSTYPE_GCH_SUFFIX),$(TOOLKIT_EXEC_H_GCH))

TOOLKIT_LIB_GCH_DIRS = $(sort $(dir $(TOOLKIT_LIB_GCH)))
TOOLKIT_EXEC_GCH_DIRS = $(sort $(dir $(TOOLKIT_EXEC_GCH)))

$(call print_verbose,(c/apply.mk) Build include lists)

# Cписок директорий для включения от фреймворка
TOOLKIT_INCLUDES := $(call sp_toolkit_include_list, $(TOOLKIT_INCLUDES_DIRS), $(TOOLKIT_INCLUDES_OBJS))

# Cписок директорий для включения от приложения
BUILD_INCLUDES := $(call sp_local_include_list,$(LOCAL_INCLUDES_DIRS),$(LOCAL_INCLUDES_OBJS))

$(call print_verbose,(c/apply.mk) Build compiler flags)

# Вычисляем окончательные флаги сборки

BUILD_PRIVATE_GENERAL_CFLAGS := \
	$(GLOBAL_GENERAL_CFLAGS) \
	$(BUILD_TYPE_CFLAGS)

BUILD_PRIVATE_GENERAL_SFLAGS := \
	$(GLOBAL_GENERAL_SFLAGS)

BUILD_PRIVATE_GENERAL_CXXFLAGS := \
	$(GLOBAL_GENERAL_CXXFLAGS) \
	$(BUILD_TYPE_CXXFLAGS)

BUILD_PRIVATE_EXEC_CFLAGS := \
	$(BUILD_PRIVATE_GENERAL_CFLAGS) \
	$(GLOBAL_EXEC_CFLAGS)

BUILD_PRIVATE_EXEC_SFLAGS := \
	$(BUILD_PRIVATE_GENERAL_SFLAGS) \
	$(GLOBAL_EXEC_SFLAGS)

BUILD_PRIVATE_EXEC_CXXFLAGS := \
	$(BUILD_PRIVATE_GENERAL_CXXFLAGS) \
	$(GLOBAL_EXEC_CXXFLAGS)

BUILD_PRIVATE_LIB_CFLAGS := \
	$(BUILD_PRIVATE_GENERAL_CFLAGS) \
	$(GLOBAL_LIB_CFLAGS)

BUILD_PRIVATE_LIB_SFLAGS := \
	$(BUILD_PRIVATE_GENERAL_SFLAGS) \
	$(GLOBAL_LIB_SFLAGS)

BUILD_PRIVATE_LIB_CXXFLAGS := \
	$(BUILD_PRIVATE_GENERAL_CXXFLAGS) \
	$(GLOBAL_LIB_CXXFLAGS)

BUILD_GENERAL_CFLAGS := \
	$(BUILD_TYPE_CFLAGS) \
	$(GLOBAL_GENERAL_CFLAGS) \
	$(TOOLKIT_GENERAL_CFLAGS) \
	$(LOCAL_CFLAGS) \
	$(addprefix -I,$(TOOLKIT_INCLUDES)) \
	$(addprefix -I,$(BUILD_INCLUDES)) \
	$(BUILD_SHADERS_TARGET_INCLUDE_ALL)

BUILD_GENERAL_CXXFLAGS := \
	$(BUILD_TYPE_CXXFLAGS) \
	$(GLOBAL_GENERAL_CXXFLAGS) \
	$(TOOLKIT_GENERAL_CXXFLAGS) \
	$(LOCAL_CXXFLAGS) \
	$(addprefix -I,$(TOOLKIT_INCLUDES)) \
	$(addprefix -I,$(BUILD_INCLUDES)) \
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
	$(BUILD_LIBS) \
	$(BUILD_TYPE_LDFLAGS) \
	$(GLOBAL_GENERAL_LDFLAGS) \
	$(TOOLKIT_GENERAL_LDFLAGS) \
	$(LOCAL_LDFLAGS) \
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
	$(filter-out -fdiagnostics-color=always,\
	$(BUILD_EXEC_CFLAGS) \
	$(BUILD_EXEC_CXXFLAGS) \
	$(BUILD_LIB_CFLAGS) \
	$(BUILD_LIB_CXXFLAGS) \
	$(BUILD_CONFIG_FLAGS) \
	$(BUILD_CONFIG_VALUES) \
	$(BUILD_CONFIG_STRINGS))

# Сравниваем итоговые флаги с кешированными
ifndef BUILD_ARCH
BUILD_ARCH := $$(BUILD_ARCH)
endif

$(call print_verbose,(c/apply.mk) Validate flags cache)

BUILD_ALL_FLAGS_CACHED := $(call shell_cat,$(TOOLKIT_CACHED_FLAGS))

BUILD_ALL_FLAGS_DIFF := \
	$(filter-out $(BUILD_ALL_FLAGS), $(BUILD_ALL_FLAGS_CACHED)) \
	$(filter-out $(BUILD_ALL_FLAGS_CACHED), $(BUILD_ALL_FLAGS))

# Игнорируем секцию для stappler-build
ifneq ($(strip $(BUILD_ALL_FLAGS_CACHED)),$(strip $(BUILD_ALL_FLAGS)))

$(call print_verbose,(c/apply.mk) Build flags changed: $(BUILD_ALL_FLAGS_DIFF))

# Обновляем кешированные флаги

$(call shell_mkdir,$(BUILD_С_OUTDIR))
$(call shell_override_file,$(TOOLKIT_CACHED_FLAGS),$(BUILD_ALL_FLAGS))

$(TOOLKIT_CACHED_FLAGS):
	@$(call rule_mkdir,$(BUILD_С_OUTDIR))
	@echo '$(BUILD_ALL_FLAGS)' > $(TOOLKIT_CACHED_FLAGS)

endif

$(call print_verbose,(c/apply.mk) Build precompiled headers list)

# Копируем заголовки для предкомпиляции
$(foreach target,$(TOOLKIT_PRECOMPILED_HEADERS),\
	$(eval $(call BUILD_include_rule,$(target),\
		$(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/lib_objs,$(target)))\
	))

$(foreach target,$(TOOLKIT_PRECOMPILED_HEADERS),\
	$(eval $(call BUILD_include_rule,$(target),\
		$(call sp_toolkit_prefix_files_list,$(BUILD_С_OUTDIR)/exec_objs,$(target)))\
	))

$(call print_verbose,(c/apply.mk) Build target source list)

# Список полных путей к компилируемым файлам фреймворка
# Сгенерированные BundleFS файлы добавляются отдельно: их ещё не существует на чистой сборке,
# а sp_*_source_list проходит через $(realpath), который отбросил бы несуществующий путь
TOOLKIT_SRCS := $(call sp_toolkit_source_list, $(TOOLKIT_SRCS_DIRS), $(TOOLKIT_SRCS_OBJS)) \
	$(BUILD_EMBED_TOOLKIT_SRCS)

# Список полных путей к компилируемым файлам приложения
BUILD_SRCS := $(call sp_local_source_list,$(LOCAL_SRCS_DIRS),$(LOCAL_SRCS_OBJS)) \
	$(BUILD_EMBED_LOCAL_SRCS)

BUILD_MAIN_SRC := $(if $(LOCAL_MAIN),$(realpath $(addprefix $(LOCAL_ROOT)/,$(LOCAL_MAIN))))

# Список файлов для сборки приложения
# Для live reload пользовательские файлы собираются как библиотека
BUILD_EXEC_SRCS := \
	$(TOOLKIT_SRCS) \
	$(if $(filter-out $(LOCAL_EXEC_LIVE_RELOAD),1),$(BUILD_SRCS) $(BUILD_MAIN_SRC))

BUILD_LIB_SRCS := \
	$(BUILD_SRCS) \
	$(if $(filter-out $(LOCAL_BUILD_SHARED),3),$(TOOLKIT_SRCS))

$(call print_verbose,(c/apply.mk) Build target objects list)

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
$(call print_verbose,(c/apply.mk) include c/library.mk)
include $(BUILD_ROOT)/c/library.mk
endif

ifdef LOCAL_EXECUTABLE
$(call print_verbose,(c/apply.mk) include c/executable.mk)
include $(BUILD_ROOT)/c/executable.mk
endif

$(call print_verbose,(c/apply.mk) include dependencies)

# Защита от исходников, перемещённых или удалённых после предыдущей сборки.
$(foreach pat,$(subst *.,%.,$(SP_SOURCE_FILES_PATTERN) *.mm),$(eval $(pat): ;))

# Побочный эффект правила выше: встроенные правила make вида "%: %.cpp" (собрать исполняемый
# файл X прямо из X.cpp) теперь считают, что недостающий исходник можно получить, и запускают
# бессмысленную компиляцию.
%: %.c
%: %.C
%: %.cc
%: %.cpp
%: %.m
%: %.s
%: %.S
%: %.o

# include dependencies
#
# $(wildcard ...) убирает попытки построить отсутствующие .d (на чистой сборке отсутствуют все):
# перестраивать их нечем, а поиск неявного правила для каждого - лишняя работа make на каждой
# сборке.
-include $(wildcard $(patsubst %.o,%.o.d,$(BUILD_EXEC_OBJS) $(BUILD_LIB_OBJS)))
-include $(wildcard $(patsubst %.h$(OSTYPE_GCH_SUFFIX),%.h$(OSTYPE_GCH_SUFFIX).d,$(TOOLKIT_EXEC_GCH) $(TOOLKIT_LIB_GCH)))

$(call print_verbose,(c/apply.mk) prepare compilation database)

BUILD_CDB_TARGET_JSON := $(addsuffix .json,$(filter-out %.s.o,$(filter-out %.S.o,$(BUILD_CDB_TARGET_OBJS))))

$(eval $(call BUILD_cdb,$(BUILD_COMPILATION_DATABASE),$(BUILD_CDB_TARGET_JSON)))
