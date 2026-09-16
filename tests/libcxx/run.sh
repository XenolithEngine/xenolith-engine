#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Driver: run the upstream libc++ conformance suite against the sprt STL.
#
# It (1) builds the sprt runtime objects via the existing tests/libc build,
# (2) collects them, (3) exports the toolchain command lines that lit.cfg.py
# consumes, and (4) invokes llvm-lit on a chosen slice of libcxx/test/std.
#
# Usage:
#   tests/libcxx/run.sh [<test-subdir>] [extra llvm-lit args...]
#     <test-subdir>  path under libcxx/test/std (default: containers/associative)
# Examples:
#   tests/libcxx/run.sh
#   tests/libcxx/run.sh containers/sequences/vector
#   tests/libcxx/run.sh algorithms -v          # show output of failing tests
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="${STAPPLER_TARGET:-x86_64-unknown-linux-gnu}"

TC="$ROOT/runtime/toolchains"
HOSTBIN="$TC/hosts/$TARGET/bin"
SYSROOT="$TC/targets/$TARGET"
RESDIR="$SYSROOT/lib/clang"
# the clang resource dir is version-named (lib/clang/<major>); discover it
CLANGINC="$(echo "$HOSTBIN"/../lib/clang/*/include)"
LLVM="$ROOT/runtime/toolchains/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"
STDROOT="$LLVM/libcxx/test/std"
SCOPE="${1:-containers/associative}"; shift || true

# Compile-only baseline: measure COMPILATION conformance only — skip the runtime
# build, object collection and the whole link stage (SPRT_COMPILE_ONLY=1). Linking
# is a separate porting stage; this isolates "does it compile against the ported
# libc++" for the dashboard.
COMPILE_ONLY="${SPRT_COMPILE_ONLY:-}"

# --- threading ABI mode -------------------------------------------------------
# SPRT_STD_THREADING=sprt: whole-stack sprt-primitive std threading. The runtime's
# libcxx module AND every test TU are compiled with -DSPRT_STD_THREADING_SPRT (the
# include_libc/cxx overlay then backs std::mutex/condition_variable/thread with sprt
# primitives). Default (unset/upstream): vendored upstream libc++ classes.
# The two modes are DIFFERENT ABIs — the libcxx module objects are invalidated on a
# mode switch so no stale-flavor object is ever linked.
THREADING_MODE="${SPRT_STD_THREADING:-upstream}"
THREADING_DEFINE=""
THREADING_MAKEARGS=()
if [ "$THREADING_MODE" = "sprt" ]; then
  THREADING_DEFINE="-DSPRT_STD_THREADING_SPRT"
  THREADING_MAKEARGS=(SPRT_STD_THREADING_SPRT=1)
fi

BUILD="$HERE/build/$TARGET/$THREADING_MODE"
mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"

if [ -n "$COMPILE_ONLY" ]; then
  : > "$RTLIST"   # link stage skipped; empty object list keeps sprt_format happy
else

# --- 1. build the sprt runtime (produces the object files we link against) ----
echo "== building sprt runtime via tests/libc ($TARGET, threading=$THREADING_MODE) =="
OBJDIR_RT="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
MODE_MARKER="$ROOT/tests/libc/stappler-build/$TARGET/.sprt-threading-mode"
if [ "$(cat "$MODE_MARKER" 2>/dev/null)" != "$THREADING_MODE" ]; then
  # Mode switch: drop every libcxx-module object (they are ABI-flavored by the mode).
  for src in "$ROOT"/runtime/libcxx/src/SPRTCxx*.cpp; do
    rm -f "$OBJDIR_RT/$(basename "$src").o"
  done
  mkdir -p "$(dirname "$MODE_MARKER")" && echo "$THREADING_MODE" > "$MODE_MARKER"
fi
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" "${THREADING_MAKEARGS[@]}" -j8 >/dev/null || true

# --- 2. collect runtime objects (everything that is not a tests/libc test TU) --
OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# The runtime installs the global replaceable operator new/delete as *strong*
# symbols (SPRTCxxNewDelete.cpp) so it owns the allocator. libc++'s allocation
# tests define their own replacement operators, which then collide at link time
# (duplicate symbol). Rebuild that one TU with SPRT_NO_STRONG_OPERATOR_NEW_DELETE
# so only the sprt-internal (non-replaceable) nothrow operators remain; libc++abi's
# weak set then backs everything a test does not itself replace.
ND_SRC="$ROOT/runtime/libc_wrapper/cxx/SPRTCxxNewDelete.cpp"
ND_OBJ="$BUILD/SPRTCxxNewDelete.nostrong.o"
"$HOSTBIN/c++" --target="$TARGET" --sysroot="$SYSROOT" -resource-dir "$RESDIR" \
  -std=gnu++2a -fno-exceptions -frtti -DSPRT_NO_STRONG_OPERATOR_NEW_DELETE \
  -I"$ROOT/runtime/include" \
  -idirafter "$ROOT/runtime/include_libc/cxx" \
  -idirafter "$ROOT/runtime/libcxx/include" \
  -idirafter "$ROOT/runtime/include_libc" -c -o "$ND_OBJ" "$ND_SRC"
grep -v 'SPRTCxxNewDelete\.cpp\.o$' "$RTLIST" > "$RTLIST.tmp" && echo "$ND_OBJ" >> "$RTLIST.tmp" && mv "$RTLIST.tmp" "$RTLIST"

# The vendored libc++ <regex> / pmr ports (runtime/libcxx) supply their out-of-line
# symbols from the libcxx build module (SPRTCxxRegex.cpp.o / SPRTCxxMemoryResource.cpp.o),
# which are built into the runtime and picked up in $RTLIST above — no special handling
# needed here.

echo "== $(wc -l < "$RTLIST") runtime objects =="

fi   # end non-compile-only runtime build

# --- 3. export the toolchain contract for lit.cfg.py --------------------------
export SPRT_CXX="$HOSTBIN/c++"
export SPRT_CC="$HOSTBIN/cc"
export SPRT_COMPILE_FLAGS="\
$THREADING_DEFINE \
-std=gnu++2a -fno-exceptions -frtti -funwind-tables -DDEBUG -DSTAPPLER_LOG_LEVEL=2 \
-Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations \
-idirafter $CLANGINC --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR \
-isystem $ROOT/runtime/include_libc/cxx \
-isystem $ROOT/runtime/libcxx/include \
-isystem $ROOT/runtime/include_libc \
-I$ROOT/runtime/include -I$SUPPORT"
export SPRT_COMPILE_ONLY="$COMPILE_ONLY"
export SPRT_LINK_FLAGS="\
-L$SYSROOT/usr/lib -l:libbacktrace.a -l:libc++abi.a -lm -Wl,--build-id=none -ldl \
--target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR -lc++abi"
export SPRT_EXEC=""     # native target: run directly. For windows: "wine".
export SPRT_STD_VER="20"
export SPRT_TEST_ROOT="$STDROOT/$SCOPE"
export SPRT_BUILD_DIR="$BUILD/work"
export SPRT_RT_OBJS_FILE="$RTLIST"

if [ ! -d "$SPRT_TEST_ROOT" ]; then
  echo "error: test scope not found: $SPRT_TEST_ROOT" >&2; exit 1
fi

# --- 4. run llvm-lit ----------------------------------------------------------
LIT="$LLVM/llvm/utils/lit/lit.py"
echo "== lit: $SCOPE =="
exec python3 "$LIT" -j"$(nproc)" --config-prefix=lit "$HERE" "$@"
