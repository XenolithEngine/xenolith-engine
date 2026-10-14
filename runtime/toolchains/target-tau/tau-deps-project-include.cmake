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

# CMAKE_PROJECT_INCLUDE of the third-party libraries of Tau user modules (common/configure.mk,
# TAU_USER). It runs at the end of project(), after enable_language(CXX) filled the standard maps.

# The sprt STL needs C++20 whatever standard a library asks for (harfbuzz pins C++11).
set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++2a")

# Probes do not link here, so FindThreads cannot test -pthread; pthreads are in libsprt.so.
if(NOT TARGET Threads::Threads)
	add_library(Threads::Threads INTERFACE IMPORTED)
endif()
set(CMAKE_USE_PTHREADS_INIT ON)

# FreeType is built before harfbuzz, and never loads it with dlopen.
set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DYNAMIC_HARFBUZZ OFF CACHE BOOL "" FORCE)
