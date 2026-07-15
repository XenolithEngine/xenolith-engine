#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Batch driver: run every tracked scope of the libc++ suite against the sprt STL on
# x86_64-pc-windows-msvc, executed under wine, reusing a single runtime build, and
# print one machine-readable summary line per scope:
#   SCOPE|discovered|pass|compile_fail|link_fail|run_fail|unsupported|unresolved
# Feeds the windows conformance dashboard. The windows analogue of run-all-wasm.sh:
# the sprt libc++ is built for the Windows PE/COFF target (MSVC C++ ABI, sprt's own
# POSIX libc — _LIBCPP_WIN32API is suppressed), linked with lld-link against the
# vendored Windows import lib, and each .exe is executed by SPRT_EXEC="wine".
#
# Usage:
#   tests/libcxx/run-win.sh [<scope>...]     # default: the full tracked scope set
# Examples:
#   tests/libcxx/run-win.sh                   # full sweep
#   tests/libcxx/run-win.sh input.output time # just these scopes
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="x86_64-pc-windows-msvc"
TC="$ROOT/runtime/toolchains"
HOSTBIN="$TC/hosts/x86_64-unknown-linux-gnu/bin"
SYSROOT="$TC/targets/$TARGET"
RESDIR="$SYSROOT/lib/clang"
CLANGINC="$HOSTBIN/../lib/clang/21/include"
BUILTINS="$SYSROOT/lib/clang/lib/windows/clang_rt.builtins-x86_64.lib"
LLVM="$TC/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"
STDROOT="$LLVM/libcxx/test/std"

BUILD="$HERE/build/$TARGET/upstream"
mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"

echo "== building sprt runtime once ($TARGET) ==" >&2
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" -j8 >/dev/null 2>&1 || true
OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# operator new/delete: drop the runtime's *strong* replaceable set so libc++'s
# allocation-replacement tests do not collide (see run-all.sh).
ND_OBJ="$BUILD/SPRTCxxNewDelete.nostrong.o"
"$HOSTBIN/c++" --target="$TARGET" --sysroot="$SYSROOT" -resource-dir "$RESDIR" \
  -std=gnu++2a -fexceptions -fno-cxx-exceptions -frtti -nostdinc -D_WIN32_WINNT=0x0A00 \
  -fms-compatibility-version=19.40 -DSPRT_NO_STRONG_OPERATOR_NEW_DELETE \
  -I"$ROOT/runtime/include" \
  -I"$ROOT/runtime/include/sprt/wrappers/windows" \
  -I"$ROOT/runtime/include_libc/cxx" \
  -I"$ROOT/runtime/libcxx/include" \
  -I"$ROOT/runtime/include_libc" \
  -c -o "$ND_OBJ" "$ROOT/runtime/libc_wrapper/cxx/SPRTCxxNewDelete.cpp" 2>/dev/null \
  && { grep -v 'SPRTCxxNewDelete\.cpp\.o$' "$RTLIST" > "$RTLIST.tmp" && echo "$ND_OBJ" >> "$RTLIST.tmp" && mv "$RTLIST.tmp" "$RTLIST"; } \
  || echo "WARN: ND nostrong rebuild failed, using strong operators" >&2

echo "== $(wc -l < "$RTLIST") runtime objects ==" >&2

export SPRT_CXX="$HOSTBIN/c++" SPRT_CC="$HOSTBIN/cc" SPRT_STD_VER="20"
export SPRT_COMPILE_FLAGS="\
-std=gnu++2a -frtti -funwind-tables -fexceptions -fno-cxx-exceptions -g -gcodeview \
-DDEBUG -DSTAPPLER_LOG_LEVEL=2 -D_MT -D_WIN32_WINNT=0x0A00 -DSP_BUILD_APPLICATION \
-fms-compatibility-version=19.40 -nostdinc \
-Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations \
-Wno-microsoft-include -Wno-unused-command-line-argument \
-idirafter $CLANGINC --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR \
-I$ROOT/runtime/include \
-I$ROOT/runtime/include/sprt/wrappers/windows \
-I$SYSROOT/usr/include \
-isystem $ROOT/runtime/include_libc/cxx \
-isystem $ROOT/runtime/libcxx/include \
-isystem $ROOT/runtime/include_libc \
-I$SUPPORT"
export SPRT_LINK_FLAGS="\
-L$SYSROOT/usr/lib -limport $BUILTINS \
-g -fuse-ld=lld -Xlinker -nodefaultlib \
--target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR \
-D_WIN32_WINNT=0x0A00 -fexceptions -fms-compatibility-version=19.40 -nostdlib"
export SPRT_EXEC="wine"
export SPRT_RT_OBJS_FILE="$RTLIST"
export WINEDEBUG="-all"
export LC_ALL="C"

LIT="$LLVM/llvm/utils/lit/lit.py"
field() { local v; v=$(sed -n "s/^[[:space:]]*$1[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p" <<<"$2" | head -1); echo "${v:-0}"; }

if [ "$#" -gt 0 ]; then SCOPES=("$@"); else
  SCOPES=(
    containers/associative containers/sequences containers/unord containers/container.adaptors
    containers/views containers/container.requirements algorithms strings utilities iterators
    numerics language.support diagnostics concepts localization input.output ranges time thread
    atomics re experimental depr library containers/container.node containers/containers.general
  )
fi
for s in "${SCOPES[@]}"; do
  [ -d "$STDROOT/$s" ] || { echo "$s|MISSING"; continue; }
  export SPRT_TEST_ROOT="$STDROOT/$s" SPRT_BUILD_DIR="$BUILD/work"
  out="$(python3 "$LIT" -j"$(nproc)" --config-prefix=lit "$HERE" -s 2>/dev/null || true)"
  disc=$(sed -n 's/.*Total Discovered Tests: \([0-9]*\).*/\1/p' <<<"$out" | head -1)
  echo "$s|${disc:-0}|$(field Passed "$out")|$(field 'Compile Failed' "$out")|$(field 'Link Failed' "$out")|$(field 'Runtime Failed' "$out")|$(field Unsupported "$out")|$(field Unresolved "$out")"
done
