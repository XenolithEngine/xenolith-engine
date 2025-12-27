# Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

# Процесс компиляции

# Загружаем конфигурацию разделяемого окружения ОС
-include $(BUILD_ROOT)/shared-config.mk

# Загружаем предустановки
include $(BUILD_ROOT)/utils/defaults.mk
include $(BUILD_ROOT)/utils/find-recursive.mk
include $(BUILD_ROOT)/utils/detect-build-number.mk

ifdef STAPPLER_TARGET
# Вычисляем компилятор и параметры компиляции

include $(BUILD_ROOT)/shaders/compiler.mk

include $(BUILD_ROOT)/c/compiler.mk

include $(BUILD_ROOT)/wasm/compiler.mk

# Вычисляем цели для сборки (библиотеки и приложение)
include $(BUILD_ROOT)/utils/build-targets.mk

# Вычисляем сумму файлов на основании запрошенных модулей
include $(BUILD_ROOT)/utils/find-runtime.mk
include $(BUILD_ROOT)/utils/resolve-modules.mk

# Применяем к целевым файлам компиляторы
include $(BUILD_ROOT)/shaders/apply.mk

include $(BUILD_ROOT)/wasm/apply.mk

include $(BUILD_ROOT)/c/apply.mk

$(info Configured!)
endif
