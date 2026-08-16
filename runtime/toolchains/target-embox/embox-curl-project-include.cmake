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

# Injected via -DCMAKE_PROJECT_CURL_INCLUDE (common/curl.mk EMBOX branch, the
# same idiom target-nuttx uses) so it runs at the END of curl's project(CURL)
# call - AFTER toolchain-libs.cmake sets CMAKE_TRY_COMPILE_TARGET_TYPE=
# STATIC_LIBRARY, and BEFORE curl's own check_function_exists /
# check_symbol_exists / curl_openssl_check_exists calls. A plain
# -DCMAKE_TRY_COMPILE_TARGET_TYPE cannot do this: the toolchain file's set() is a
# normal variable and shadows the -D cache entry.
#
# Why curl needs the probes to LINK rather than merely compile: nearly every
# capability curl asks about is a libc/OpenSSL *symbol*, not a declaration. Under
# the toolchain-wide STATIC_LIBRARY default check_function_exists never links, so
# it reports every function present - its test source only declares
# `char FUNC();` - while the OpenSSL-backed checks that do consult a header still
# need OpenSSL::SSL / OpenSSL::Crypto on the link line to be trustworthy. Raising
# the probes to EXECUTABLE makes both kinds honest: common/configure.mk's EMBOX
# branch links each probe against libsprt.a plus the Embox libc/libm with
# -Wl,--no-undefined, so an absent symbol is a hard link error (detected absent)
# and a present one links (detected present).
#
# This replaces a block of hand-maintained -DHAVE_* cache seeds that used to live
# in curl.mk: recv/send with their five argument types each, select/poll/socket
# and the fcntl/O_NONBLOCK answers. Those had to be re-derived by hand every time
# curl or the Embox libc surface moved; with honest probes cmake derives them
# itself - and gets them right, which matters now that HTTP/3 is on and curl
# probes a much larger OpenSSL surface (the QUIC TLS API).
set(CMAKE_TRY_COMPILE_TARGET_TYPE "EXECUTABLE")
