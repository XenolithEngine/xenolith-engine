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


# CMAKE_SYSTEM_NAME Tau: user modules of Tau, a Unix to the code built for them (the engine's
# sprt libc), whose shared libraries root links into a module at its start (SYS-A-DynLink-1 of
# xenolith-tau). Linux's platform file gives the ELF conventions; what differs is below.
include(Platform/Linux)

# The bare-metal aarch64 driver forwards neither -shared nor the soname: the linker takes them.
foreach(lang C CXX ASM)
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-Wl,-shared")
  set(CMAKE_SHARED_MODULE_CREATE_${lang}_FLAGS "-Wl,-shared")
  set(CMAKE_SHARED_LIBRARY_SONAME_${lang}_FLAG "-Wl,-soname,")
endforeach()

# Root finds libraries by name among a module's, not by path: no rpath, no libdl.
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_DL_LIBS "")
