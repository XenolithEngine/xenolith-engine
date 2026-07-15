#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Batch driver: run every tracked scope of the libc++ suite against the sprt STL
# in one shot, reusing a single runtime build, and print a machine-readable summary
# line per scope (SCOPE|discovered|pass|fail|unsupported|unresolved). Feeds the
# conformance dashboard.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="${STAPPLER_TARGET:-x86_64-unknown-linux-gnu}"
TC="$ROOT/runtime/toolchains"; HOSTBIN="$TC/hosts/$TARGET/bin"
SYSROOT="$TC/targets/$TARGET"; RESDIR="$SYSROOT/lib/clang"
CLANGINC="$HOSTBIN/../lib/clang/21/include"
LLVM="$ROOT/runtime/toolchains/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"; STDROOT="$LLVM/libcxx/test/std"

echo "== building sprt runtime once ==" >&2
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" -j8 >/dev/null
OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
BUILD="$HERE/build/$TARGET"; mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# Rebuild SPRTCxxNewDelete.cpp with SPRT_NO_STRONG_OPERATOR_NEW_DELETE and swap it
# into the object list: the runtime's *strong* global operator new/delete otherwise
# collide with the replacement operators libc++'s allocation tests define. With the
# macro only the sprt-internal (non-replaceable) nothrow operators remain and
# libc++abi's weak set backs the rest, so those tests link. (See run.sh for detail.)
ND_OBJ="$BUILD/SPRTCxxNewDelete.nostrong.o"
"$HOSTBIN/c++" --target="$TARGET" --sysroot="$SYSROOT" -resource-dir "$RESDIR" \
  -std=gnu++2a -fno-exceptions -frtti -DSPRT_NO_STRONG_OPERATOR_NEW_DELETE \
  -I"$ROOT/runtime/include" -I"$ROOT/runtime/include_libc" \
  -c -o "$ND_OBJ" "$ROOT/runtime/libc_wrapper/cxx/SPRTCxxNewDelete.cpp"
grep -v 'SPRTCxxNewDelete\.cpp\.o$' "$RTLIST" > "$RTLIST.tmp" && echo "$ND_OBJ" >> "$RTLIST.tmp" && mv "$RTLIST.tmp" "$RTLIST"

export SPRT_CXX="$HOSTBIN/c++" SPRT_CC="$HOSTBIN/cc" SPRT_STD_VER="20"
export SPRT_COMPILE_FLAGS="-std=gnu++2a -fno-exceptions -frtti -funwind-tables -DDEBUG -DSTAPPLER_LOG_LEVEL=2 -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations -idirafter $CLANGINC --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR -idirafter $ROOT/runtime/include_libc -I$ROOT/runtime/include -I$SUPPORT"
export SPRT_LINK_FLAGS="-L$SYSROOT/usr/lib -l:libbacktrace.a -l:libc++abi.a -lm -Wl,--build-id=none -ldl --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR -lc++abi"
export SPRT_EXEC="" SPRT_RT_OBJS_FILE="$RTLIST"

# Run the scopes given as arguments, or the full tracked set by default.
if [ "$#" -gt 0 ]; then
  SCOPES=("$@")
else
  SCOPES=(
    containers/associative containers/sequences containers/unord containers/container.adaptors
    containers/views containers/container.requirements algorithms strings utilities iterators
    numerics language.support diagnostics concepts localization input.output ranges time thread
    atomics re experimental depr library modules containers/container.node containers/containers.general
  )
fi
LIT="$LLVM/llvm/utils/lit/lit.py"
# Anchor on the line-start label so "Failed" does not also catch "Expectedly Failed"
# and "Passed" does not catch "Unexpectedly Passed" (which would split the fields).
field() { local v; v=$(sed -n "s/^[[:space:]]*$1[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p" <<<"$2" | head -1); echo "${v:-0}"; }
for s in "${SCOPES[@]}"; do
  [ -d "$STDROOT/$s" ] || { echo "$s|MISSING"; continue; }
  export SPRT_TEST_ROOT="$STDROOT/$s" SPRT_BUILD_DIR="$BUILD/work"
  out="$(python3 "$LIT" -j"$(nproc)" --config-prefix=lit "$HERE" -s 2>/dev/null || true)"
  disc=$(sed -n 's/.*Total Discovered Tests: \([0-9]*\).*/\1/p' <<<"$out" | head -1)
  # Failures are split by phase: Compile / Link / Runtime (see sprt_format.py).
  # SCOPE|discovered|pass|compile_fail|link_fail|run_fail|unsupported|unresolved
  echo "$s|${disc:-0}|$(field Passed "$out")|$(field 'Compile Failed' "$out")|$(field 'Link Failed' "$out")|$(field 'Runtime Failed' "$out")|$(field Unsupported "$out")|$(field Unresolved "$out")"
done
