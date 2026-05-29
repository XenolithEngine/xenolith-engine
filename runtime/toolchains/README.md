# Stappler Toolchains repository

The purpose of this repository is to provide universal complete toolchain for C/C++ development with Stappler SDK on MacOS, Windows and Linux.

**Work in progress**: toolchains on very early stages, errors a very possible. Use this repo only if you has experience in toolchain building.

## What's included

### General

* C language library ([musl](https://musl.libc.org/) on Linux, [llvm-libc](https://libc.llvm.org/) on Windows)
* [llvm-libc++](https://libcxx.llvm.org/)
* [clang](https://clang.llvm.org/) with llvm tooling

### Vulkan essentials

* [SPIR-V Headers](https://github.com/KhronosGroup/SPIRV-Headers)
* [SPIR-V Tools](https://github.com/KhronosGroup/SPIRV-Tools)
* [Vulkan Headers](https://github.com/KhronosGroup/Vulkan-Headers)
* [glslang](https://github.com/KhronosGroup/glslang) shader compiler/validator

### Stappler SDK dependencies

* [zlib](https://www.zlib.net/)
* [bzip2](https://sourceware.org/bzip2/downloads.html)
* [xz](https://tukaani.org/xz/#_source_packages)
* [zstd](https://github.com/facebook/zstd/releases/tag/v1.5.7)
* [libjpeg](http://www.ijg.org/) or [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo/releases)
* [libpng](http://www.libpng.org/pub/png/libpng.html)
* [giflib](https://giflib.sourceforge.net/)
* [WebP](https://github.com/webmproject/libwebp)
* [TIFF](https://download.osgeo.org/libtiff/?C=M&O=D)
* [FreeType](https://freetype.org/)
* [harfbuzz](https://github.com/harfbuzz/harfbuzz/releases/)
* [brotli](https://github.com/google/brotli/releases) compression library
* [MbedTLS](https://github.com/Mbed-TLS/mbedtls) crypto library
* [OpenSSL](https://openssl-library.org/source/index.html) crypto library
* [openssl-gost-engine](https://github.com/gost-engine/engine) for GOST crypto support
* [nghttp3](https://github.com/ngtcp2/nghttp3/releases)
* [libcurl](https://curl.se/download.html) in MbedTLS and OpenSSL variants. Only OpenSSL variant supports HTTP/3
* [sqlite3](https://www.sqlite.org/download.html)
* [libuidna](https://github.com/SBKarr/libuidna) as IDN2 replacement
* [libzip](https://libzip.org)
* [iwasm] WebAssembly runtime library

### Linux essential headers (libraries not included)

* DBus (libdbus-1)
* XCB
* XKB
* Wayland-client

* [fdhelper](toolchain-linux/fdhelper) - helper for dlopen with static musl apps

### Russian CA certificates and GOST cyphers

libcurl in this toolchains bundled with CA certificates, that contains russian national CA authorities.

GOST cyphers can be loaded statically with stappler_crypto module.

## Build steps

Download dependencies to build

```
# From repo root
make all
```

### Linux



To build basic toolchain:

```
# From repo root
make -C toolchain-linux output
```

To build stappler dependencies with toolchain:

```
# From repo root
make -C linux all
```

## License

All build scripts and distribution itself licensed under MIT. No GPL restrictions implied on software, that uses this toolchains. No GPL-licensed tools included.

DBus headers licensed under AFL-2.1 OR GPL-2.0.
