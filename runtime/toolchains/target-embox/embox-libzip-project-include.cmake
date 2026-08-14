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

# Injected via -DCMAKE_PROJECT_libzip_INCLUDE (common/libzip.mk EMBOX branch,
# mirroring the WASM branch's wasm-libzip-project-include.cmake) so it runs at
# the END of libzip's project() call - AFTER the toolchain-libs.cmake set of
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY, and BEFORE libzip's
# check_function_exists / check_type_size calls. A plain -DCMAKE_TRY_COMPILE_TARGET_TYPE
# cannot do this: the toolchain file's set() is a normal variable that shadows
# the -D cache entry.
#
# libzip does not raise the probe target type itself, so under the toolchain's
# STATIC_LIBRARY default its function probes never link and report every symbol
# as present. Switch them to EXECUTABLE here; configure.mk (EMBOX branch) links
# each probe against the sprt libc archive with NO --allow-undefined, so an
# absent function (memcpy_s, strncpy_s, arc4random, ...) is a hard link error
# -> detected absent (compat.h falls back to its own wrappers), and genuinely
# present ones (explicit_bzero, strerror_s, ...) are detected present.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "EXECUTABLE")

# Force HAVE_STRERROR_S / HAVE_FSEEKO / HAVE_FTELLO to 1: the sprt libc shims
# (include_libc/string.h, wrappers/libc/stdio_impl.h) DECLARE these via the
# umbrella extern surface (resolved at the final image link against the engine's
# libc_impl module), but the cmake function-probes here cannot see that because
# libc_impl is not in libsprt.a (it is compiled into the engine, not the
# runtime archive). The probe link fails for strerror_s -> compat.h would
# #define strerror_s as a macro fallback, which then breaks sprt's own
# non-static declaration. Treat them as present so compat.h skips the macro
# fallbacks and libzip emits plain calls that the final image link resolves.
#
# memcpy_s/strncpy_s are intentionally LEFT at their probe-detected value (0):
# sprt does NOT declare them in its umbrella surface (only strerror_s has an
# umbrella declaration), so libzip MUST use its compat.h macro fallback for
# them. There is no declaration/macro conflict for those two because sprt
# simply does not mention them.
add_compile_definitions(HAVE_STRERROR_S=1)
# Same shape as the Annex K *_S surface above: the sprt libc shims DECLARE
# fseeko/ftello/fseek as the umbrella extern surface (resolved at the final
# image link), but the cmake function-probes here cannot see those symbols
# (libsprt.a does not carry them; they land via libc_impl in the engine). The
# probes report HAVE_FSEEKO/HAVE_FTELLO=0 -> compat.h #defines them as macros
# -> the macro breaks sprt's non-static declaration in <stdio_impl.h>. Force
# them on so compat.h skips the macro fallbacks and libzip emits plain calls.
add_compile_definitions(HAVE_FSEEKO=1 HAVE_FTELLO=1)
