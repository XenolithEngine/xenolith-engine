# Описание модулей

Пример описания

```
MODULE_STAPPLER_FILESYSTEM_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_FILESYSTEM_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/filesystem
MODULE_STAPPLER_FILESYSTEM_SRCS_OBJS :=
MODULE_STAPPLER_FILESYSTEM_INCLUDES_DIRS :=
MODULE_STAPPLER_FILESYSTEM_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/filesystem
MODULE_STAPPLER_FILESYSTEM_INCLUDES_INTERNAL := $(STAPPLER_MODULE_DIR)/filesystem/detail
MODULE_STAPPLER_FILESYSTEM_DEPENDS_ON := stappler_core
MODULE_STAPPLER_FILESYSTEM_GENERAL_LDFLAGS :=
MODULE_STAPPLER_FILESYSTEM_LIBS :=

ifdef ANDROID
MODULE_STAPPLER_FILESYSTEM_DEPENDS_ON += stappler_zip
endif

ifdef MACOS
MODULE_STAPPLER_FILESYSTEM_GENERAL_LDFLAGS += -framework UniformTypeIdentifiers
endif

# Связывает модуль stappler_filesystem с его переменными с префиксом MODULE_STAPPLER_FILESYSTEM
$(call define_module, stappler_filesystem, MODULE_STAPPLER_FILESYSTEM)
```

Большая часть переменных модуля действует глобально при сборке этого модуля. То есть, включаемые файлы модуля становятся доступными всем модулям и приложению, а флаги для сборки добавляются для всех собираемых файлов в проекте. Исключение - переменные _PRIVATE_

## Переменные для определения модулей

### <MODULE>_CONFIG_FLAGS

Флаги для добавления в buildconfig.h

### <MODULE>_CONFIG_STRINGS

Строки для добавления в buildconfig.h в формате NAME=value

### <MODULE>_CONFIG_VALUES

Числовые значения добавления в buildconfig.h

### <MODULE>_PRECOMPILED_HEADERS

Заголовки для предкомпилирования, экспортируемые модулем. Предкомпиляция заголовков срабатывает до любой другой компиляции.

### <MODULE>_FLAGS

Общие флаги для компиляции, добавляются к глобальным флагам

### <MODULE>_GENERAL_CFLAGS, <MODULE>_LIB_CFLAGS, <MODULE>_EXEC_CFLAGS

Флаги для компиляции языка C: общие, для библиотек, для исполняемых файлов. Добавляются к глобальным флагам.

### <MODULE>_GENERAL_CXXFLAGS, <MODULE>_LIB_CXXFLAGS, <MODULE>_EXEC_CXXFLAGS

Флаги для компиляции языка C++: общие, для библиотек, для исполняемых файлов. Добавляются к глобальным флагам.

### <MODULE>_GENERAL_LDFLAGS, <MODULE>_LIB_LDFLAGS, <MODULE>_EXEC_LDFLAGS

Флаги для линковки (передаются компилятору): общие, для библиотек, для исполняемых файлов. Добавляются к глобальным флагам.

При сборке статических библиотек не учитываются.

### <MODULE>_SRCS_DIRS

Список директорий для рекурсивного поиска файлов для компиляции. Все найденные файлы будут собраны в конечное приложение.

### <MODULE>_SRCS_OBJS

Список файлов для компиляции.

### <MODULE>_INCLUDES_DIRS

Список директорий для рекурсивного поиска включаемых файлов. Работает глобально.

### <MODULE>_INCLUDES_OBJS

Список отдельных директорий поиска включаемых файлов. Работает глобально.

### <MODULE>_SHADERS_DIR

Список директорий, в которых расположены шейдеры для компиляции

### <MODULE>_SHADERS_INCLUDE

Список директорий для поиска включаемых *из шейдера* файлов

### <MODULE>_LIBS

Набор библиотек, требуемых модулю. Библиотеки в формате -l<name> будут включены стандартным способом для линковщика. Библиотеки в формате -l:lib<name>.a или -l:<name>.lib - статические библиотеки для включения из текущего тулчейна.

### <MODULE>_LIBS_SHARED

Набор библиотек, требуемых модулю в режиме разделяемого корня. Эти библиотеки станут зависимостями разделяемой библиотеки, в которую будет скомилирован модуль.

### <MODULE>_DEFINED_IN

Базовое определение, которое описывает, в каком файле определён модуль. В обычном случае должно быть определено как:

```make
<MODULE>_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
```

### <MODULE>_DEPENDS_ON

Список модулей, от которых зависит текущий модуль. Система либо находит и подключает все требуемые модули, либо выдаёт ошибку компиляции. В режиме разделяемого корня, эти модули также становятся зависимостями разделяемой библиотеки модуля.

### <MODULE>_INCLUDES_INTERNAL

Список путей для поиска включаемых файлов или отдельные файлы, которые не должны быть доступны вне компиляции текущего приложения или модуля. То есть, не будут установлены при сборке разделяемого корня, но будут доступны другим модулям и приложению при компиляции.

### <MODULE>_SHARED_CONSUME

Список модулей, которые будут слиты с текущим (поглощены им) при сборке разделяемого корня. Включение таких поглощённых модулей из разделяемого корня будет всегда включать этот модуль.

## <MODULE>_SHARED_SPEC_SUMMARY

Краткое описание модуля для сборки пакетов на одну строку

### <MODULE>_SHARED_SPEC_DESCRIPTION

Более подробное описание модуля для сборки пакетов, многострочное

### <MODULE>_PRIVATE_DEPS

Список файлов зависимостей для собираемых файлов модуля

### <MODULE>_PRIVATE_CFLAGS

Флаги компиляции, применяемые только к собираемым файлам модуля для языка C

### <MODULE>_PRIVATE_CXXFLAGS

Флаги компиляции, применяемые только к собираемым файлам модуля для языка C++

### <MODULE>_PRIVATE_INCLUDES

Набор директорий для поиска включаемых файлов файлами внутри этого модуля

### <MODULE>_PRIVATE_INCLUDE_PCH

Файлы предкомпилированных заголовков для включения. Должны содержать только имена файлов (без путей), ранее указанных в <MODULE>_PRECOMPILED_HEADERS (возможно, в каком-то другом модуле)

## Переменные, доступные при определени модуля

### Опредеение платформы сборки

* LINUX
* WIN32
* MACOS
* ANDROID

