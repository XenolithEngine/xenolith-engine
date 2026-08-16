# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

# CMAKE_PROJECT_INCLUDE for the Embox third-party deps. Loaded AFTER each dep's
# own CMakeLists.txt `project()` call (mirrors target-wasm/wasm-deps-project-include.cmake),
# so a `set(CMAKE_CXX_STANDARD ...)` the dep hard-coded in its CMakeLists.txt
# (e.g. harfbuzz's `set(CMAKE_CXX_STANDARD 11)` at CMakeLists.txt:10) can be
# overridden here. Without this, harfbuzz pins C++11 and the sprt STL headers
# (which require C++20 concepts) fail to compile.
#
# Force C++20 + C99 the sprt STL requires; the dep's own target_compile_features
# may still add an earlier -std=, but CMAKE_CXX_STANDARD set here wins for the
# plain CMake targets that do not call target_compile_features themselves.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# harfbuzz sets CMAKE_CXX_STANDARD 11 via a normal set() in CMakeLists.txt:10
# AND applies it as a per-target CXX_STANDARD property, so cmake emits -std=gnu++11
# AFTER CMAKE_CXX_FLAGS_INIT. CMAKE_CXX_STANDARD set above only wins for targets
# that do not set the property themselves. add_compile_options($<COMPILE_LANG_AND_ID:CXX,Clang,-std=gnu++20>)
# below is emitted AFTER the CXX_STANDARD-derived -std=, so it wins for every
# CXX target regardless (clang takes the last -std=). The COMPILE_LANGUAGE: CXX
# guard keeps C sources untouched (clang rejects -std=gnu++20 on C).
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>)

# find_package(Threads) fails on the Embox baremetal toolchain: FindThreads' check
# for -pthread does a link-time try_compile that cannot succeed (no standalone
# executable link at dep-build time). CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
# is already set in toolchain-libs.cmake but FindThreads still bails because the
# -pthread link test is not a normal feature probe. Deps that do
# `find_package(Threads)` then `target_link_libraries(... Threads::Threads)`
# (curl, sqlite, ...) break on the missing imported target. Pre-create it as a
# plain -pthread link flag: Embox libc has pthreads built in (no separate
# libpthread needed since glibc 2.34 / Embox always), so -pthread compiles and
# links cleanly against the sysroot libc at the final image link.
if(NOT TARGET Threads::Threads)
    add_library(Threads::Threads INTERFACE IMPORTED)
    set_target_properties(Threads::Threads PROPERTIES
        INTERFACE_COMPILE_OPTIONS "-pthread"
        INTERFACE_LINK_LIBRARIES "-pthread")
endif()
set(CMAKE_USE_PTHREADS_INIT ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# freetype/harfbuzz build order on Embox: there is no system harfbuzz to satisfy
# freetype's FT_REQUIRE_HARFBUZZ=TRUE at the first freetype pass, so the build
# stages freetype (no harfbuzz) -> harfbuzz (against freetype) -> [no freetype
# rebuild, mirroring how apple/linux stage it]. Override FT_REQUIRE_HARFBUZZ OFF
# here so the freetype configure step does not fail find_package(HARFBUZZ) even
# though freetype.mk hard-codes -D FT_REQUIRE_HARFBUZZ=TRUE on the command line.
set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "" FORCE)
