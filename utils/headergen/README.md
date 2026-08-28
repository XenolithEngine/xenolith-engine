# Приложение для обновления заголовоков Vulkan внутри Xenolith

А также других генерируемых ресурсов

Приложение загружает актуальный [регистр функций Vulkan](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml) и подготавливает файлы для Xenolith

## Сборка

```
make
make install
```

## Работа приложения

Запуск приложения:

```
$ ./headergen
headergen <options> registry|icons|material
Options:
    -v (--verbose)
    -h (--help)
```

* registry - генерирует новые файлы Vulkan
* icons - генерирует иконки для клиентских декораций Wayland
* material - генерирует векторные иконки Material Design

## Что перенесено в это дерево

Работает `material`:

```
$ ./headergen material /path/to/material-design-icons/src
```

Путь указывает на `src` внутри выгрузки, а не на её корень: имя иконки строится из
пути, и от корня все имена получат лишний префикс `Src_`.

Он пишет `XL2dIcons.h` и `XL2dIcons.cpp` в `gen/`; для текущего набора иконок
результат побайтно совпадает с тем, что лежит в
`xenolith/renderer/basic2d/icons`.

`registry` и `icons` **не перенесены**: они пишут через `std::ofstream` и читают
собственный поток через `data()`/`size()`, чего среда сборки этого дерева не даёт.
Причина записана в Makefile; `material` ничего из этого не трогает.
