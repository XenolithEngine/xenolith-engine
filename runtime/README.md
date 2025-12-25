# Xenolith Runtime

Xenolith Runtime реализация зонтичной библиотеки libc для проекта Xenolith.

Зонтичная библиотека единый общий POSIX-совместимый интерфейс для целевых платформ, служа прослойкой между пользовательским кодом и libc платформы.

Поддерживаются:

* (Linux) GNU libc
* (Linux) MUSL libc
* (Android) NDK / Bionic libc
* (Windows) WinAPI / UCRT
* (MacOS) Apple libc / libSystem

Использование Xenolith Runtime позволяет собирать Xenolith и проекты с его использованием без участия SDK платформы (Windows SDK, Android SDK, MacOS SDK). Это снимает ряд ограничений, связанных с лицензированием кода при поставке предкомпилированных тулчейнов для Xenolith.

Другие проекты могут использовать Xenolith Runtime для упрощения платформозависимого кода.

## Подпроекты

### Скрипты сборки тулчейнов для Xenolith

### Сборочная система Xenolith на основе make