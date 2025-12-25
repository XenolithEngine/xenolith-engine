# Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

# Рекурсивный поиск директорий и файлов без использования shell
# Требует GNU Make

sp_add_root = $(addprefix $(addsuffix /,$(patsubst %/,%,$(1))),$(2))

### sp_find_files : Находит все файлы в директориях по шаблону
# $(1) - Список директорий
# $(2) - Список шаблонов
#
# Пример: $(call sp_find_files,$(DIR_LIST),*.c *.cpp)

sp_find_files = $(foreach pat,$(addprefix /,$(2)), $(wildcard $(addsuffix $(pat),$(1))))


### sp_find_dirs : Находит все поддиректории в директории
# $(1) - Целевая директория
#
# Пример: $(call sp_find_dirs,$(SINGLE_DIR))

sp_find_dirs = $(strip $(foreach dir,\
	$(patsubst %/,%,$(sort $(dir $(wildcard $(addsuffix /*/,$(patsubst %/,%,$(1))))))),\
	$(if $(subst $(1),,$(dir)),$(dir))))


### sp_find_dirs_recursive : Возвращает полный рекурсивный список директорий для исходной директории (включая её саму)
# $(1) - Целевая директория
#
# Пример: $(call sp_find_dirs_recursive,$(SINGLE_DIR))

sp_find_dirs_recursive = \
	$(if $(1),\
		$(1) \
		$(foreach dir,$(call sp_find_dirs,$(1)),$(call sp_find_dirs_recursive,$(dir))))

### sp_find_recursive : Находит все файлы по шаблону рекурсивно в поддиректориях
# $(1) - Целевая директория
# $(2) - Список шаблонов
#
# Пример: $(call sp_find_recursive,$(DIR_LIST),*.c *.cpp)

sp_find_recursive = $(foreach dir,$(1),$(call sp_find_files,$(call sp_find_dirs_recursive,$(dir)),$(2)))


### sp_make_general_source_list : подготавливает список компилируемых файлов c, cpp, mm,
### с использованием полных путей для файла. Относительные пути дополняются с явным указанием корня.
# $(1) - Список директорий для рекурсивного поиска
# $(2) - Список отдельных файлов исходного кода
# $(3) - Корень для относительных путей
# $(4) - Паттерн для поиска
# $(5) - Паттерн для фильтра

sp_make_general_source_list = \
	$(realpath \
		$(call sp_find_recursive,\
			$(filter /%,$(1)) $(call sp_add_root,$(3),$(filter-out /%,$(1))),\
			$(4)) \
		$(filter /%,\
			$(if $(5),$(filter-out $(5),$(2)),$(2)))\
		$(call sp_add_root,$(3),$(filter-out /%,\
			$(if $(5),$(filter-out $(5),$(2)),$(2))))\
	)


### sp_make_general_include_list : подготавливает список директорий для поиска заголовков
### с использованием полных путей. Относительные пути дополняются с явным указанием корня.
# $(1) - Список директорий для рекурсивного поиска
# $(2) - Список отдельных файлов исходного кода
# $(3) - Корень для относительных путей

sp_make_general_include_list = \
	$(realpath \
		$(foreach dir,$(1),\
			$(if $(filter /%,$(dir)),\
				$(call sp_find_dirs_recursive,$(dir)),\
				$(call sp_find_dirs_recursive,$(call sp_add_root,$(3),$(dir)))\
			)\
		)\
		$(call sp_add_root,$(3),$(filter-out /%,$(2))) \
		$(filter /%,$(2)) \
	)
