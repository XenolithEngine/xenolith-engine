#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Batch driver: run every tracked scope of the libc++ suite against the sprt STL on
# wasm32-unknown-unknown (executed headlessly under Node), reusing a single runtime
# build, and print one machine-readable summary line per scope:
#   SCOPE|discovered|pass|compile_fail|link_fail|run_fail|unsupported|unresolved
# Feeds the wasm conformance dashboard. The wasm analogue of run-all.sh.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="wasm32-unknown-unknown"
TC="$ROOT/runtime/toolchains"
HOSTBIN="$TC/hosts/x86_64-unknown-linux-gnu/bin"   # linux host clang drives wasm
SYSROOT="$TC/targets/$TARGET"; RESDIR="$SYSROOT/lib/clang"
# the clang resource dir is version-named (lib/clang/<major>); discover it
CLANGINC="$(echo "$HOSTBIN"/../lib/clang/*/include)"; WASM_USRINC="$SYSROOT/usr/include"
LLVM="$ROOT/runtime/toolchains/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"; STDROOT="$LLVM/libcxx/test/std"
RUNNER="$ROOT/runtime/wasm-js/run-node.mjs"
WASM_FEATURES="-matomics -mbulk-memory -mmutable-globals -msign-ext -mnontrapping-fptoint"

echo "== building sprt runtime once ($TARGET) ==" >&2
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" -j8 >/dev/null
OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
BUILD="$HERE/build/$TARGET/upstream"; mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# Weaken sprt's strong wasm operator new/delete so libc++'s allocation-replacement
# tests link (see run-wasm.sh for the full rationale); swap the weak TU into the list.
BL_OBJ="$BUILD/builtin_libcxx.weak.o"
"$HOSTBIN/c++" --target="$TARGET" --sysroot="$SYSROOT" -resource-dir "$RESDIR" \
  -nostdinc -ffreestanding -fbuiltin -funwind-tables -fasynchronous-unwind-tables \
  $WASM_FEATURES -std=gnu++2a -frtti -fno-exceptions -g -DDEBUG -DSTAPPLER_LOG_LEVEL=2 \
  -DSP_BUILD_APPLICATION -DSPRT_BUILD_RUNTIME -DSPRT_WEAK_OPERATOR_NEW_DELETE \
  -Wno-unused-command-line-argument \
  -I"$ROOT/runtime/include" -I"$ROOT/runtime/include_libc" \
  -idirafter "$WASM_USRINC" -idirafter "$CLANGINC" \
  -c -o "$BL_OBJ" "$ROOT/runtime/libc_impl/src/builtin_libcxx.cpp"
grep -v 'builtin_libcxx\.cpp\.o$' "$RTLIST" > "$RTLIST.tmp" \
  && echo "$BL_OBJ" >> "$RTLIST.tmp" && mv "$RTLIST.tmp" "$RTLIST"

export SPRT_CXX="$HOSTBIN/c++" SPRT_CC="$HOSTBIN/cc" SPRT_STD_VER="20"
export SPRT_COMPILE_FLAGS="-std=gnu++2a -fno-exceptions -frtti -funwind-tables -DDEBUG -DSTAPPLER_LOG_LEVEL=2 -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations -Wno-unused-command-line-argument --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR -nostdinc -nostdinc++ $WASM_FEATURES -isystem $ROOT/runtime/include_libc/cxx -isystem $ROOT/runtime/libcxx/include -isystem $ROOT/runtime/include_libc -I$ROOT/runtime/include -I$SUPPORT -idirafter $WASM_USRINC -idirafter $CLANGINC"
export SPRT_LINK_FLAGS="-fuse-ld=lld -nostdlib -L$SYSROOT/usr/lib --target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR $WASM_FEATURES -Wl,--import-memory,--shared-memory,--max-memory=1073741824 -Wl,--export=__wasm_init_tls,--export=__tls_size,--export=__tls_align,--export=__tls_base -Wl,--export=__stack_pointer,--export=malloc,--export=free,--export=__xl_thread_entry -Wl,--export-table $SYSROOT/lib/clang/lib/wasi/libclang_rt.builtins-wasm32.a $SYSROOT/usr/lib/libc++abi.a $SYSROOT/usr/lib/libunwind.a"
export SPRT_EXEC="node $RUNNER" SPRT_RT_OBJS_FILE="$RTLIST"
export SPRT_RUN_TIMEOUT="${SPRT_RUN_TIMEOUT:-40}"

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
# Node exec is heavier than a native run and threaded tests spawn worker_threads, so
# cap run parallelism below nproc to avoid CPU oversubscription (false RUN_TIMEOUTs).
JOBS="${SPRT_LIT_JOBS:-$(( $(nproc) > 8 ? 8 : $(nproc) ))}"
field() { local v; v=$(sed -n "s/^[[:space:]]*$1[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p" <<<"$2" | head -1); echo "${v:-0}"; }
for s in "${SCOPES[@]}"; do
  [ -d "$STDROOT/$s" ] || { echo "$s|MISSING"; continue; }
  export SPRT_TEST_ROOT="$STDROOT/$s" SPRT_BUILD_DIR="$BUILD/work"
  out="$(python3 "$LIT" -j"$JOBS" --config-prefix=lit "$HERE" -s 2>/dev/null || true)"
  disc=$(sed -n 's/.*Total Discovered Tests: \([0-9]*\).*/\1/p' <<<"$out" | head -1)
  echo "$s|${disc:-0}|$(field Passed "$out")|$(field 'Compile Failed' "$out")|$(field 'Link Failed' "$out")|$(field 'Runtime Failed' "$out")|$(field Unsupported "$out")|$(field Unresolved "$out")"
done
