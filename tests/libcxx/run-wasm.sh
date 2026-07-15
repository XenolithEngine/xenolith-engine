#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Driver: run the upstream libc++ conformance suite against the sprt STL on the
# wasm32-unknown-unknown target, executed headlessly under Node.js.
#
# It mirrors run.sh (build sprt runtime -> collect objects -> export the toolchain
# contract -> llvm-lit), but targets wasm: each test compiles with the wasm clang,
# links to a .wasm module, and RUNS via runtime/wasm-js/run-node.mjs (the headless
# Node runner over shared memory). sprt_format.py is target-agnostic — it links to
# a file and runs `$SPRT_EXEC <file>`, so a wasm module named ".exe" works verbatim
# and SPRT_EXEC = "node run-node.mjs" executes it.
#
# Usage:
#   tests/libcxx/run-wasm.sh [<test-subdir>] [extra llvm-lit args...]
# Examples:
#   tests/libcxx/run-wasm.sh utilities/optional
#   tests/libcxx/run-wasm.sh numerics -v
#   SPRT_COMPILE_ONLY=1 tests/libcxx/run-wasm.sh containers   # compile-only baseline
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="wasm32-unknown-unknown"

TC="$ROOT/runtime/toolchains"
# wasm has no dedicated host toolchain: it is the linux host clang driving
# --target=wasm32-unknown-unknown against the wasm sysroot (see toolchains/target-wasm).
HOSTBIN="$TC/hosts/x86_64-unknown-linux-gnu/bin"
SYSROOT="$TC/targets/$TARGET"
RESDIR="$SYSROOT/lib/clang"
CLANGINC="$HOSTBIN/../lib/clang/21/include"
WASM_USRINC="$SYSROOT/usr/include"
LLVM="$ROOT/runtime/toolchains/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"
STDROOT="$LLVM/libcxx/test/std"
RUNNER="$ROOT/runtime/wasm-js/run-node.mjs"
SCOPE="${1:-utilities/optional}"; shift || true

# wasm target features used by the whole runtime build (toolchains/target-wasm):
# threads (atomics + shared memory), bulk-memory, etc. Compile and link must agree.
WASM_FEATURES="-matomics -mbulk-memory -mmutable-globals -msign-ext -mnontrapping-fptoint"

COMPILE_ONLY="${SPRT_COMPILE_ONLY:-}"

BUILD="$HERE/build/$TARGET/upstream"
mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"

if [ -n "$COMPILE_ONLY" ]; then
  : > "$RTLIST"   # link stage skipped; empty object list keeps sprt_format happy
else

# --- 1. build the sprt runtime for wasm (produces the objects we link against) --
echo "== building sprt runtime via tests/libc ($TARGET) =="
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" -j8 >/dev/null || true

# --- 2. collect runtime objects (everything that is not a tests/libc test TU) ----
# Same exclusion list as run.sh: the tests/libc test TUs each carry their own main()
# and must not be linked into a standalone libc++ conformance test (which has its own).
OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# The runtime installs the global operator new/delete as *strong* symbols on wasm
# (builtin_libcxx.cpp — sprt is the sole provider). libc++'s allocation tests define
# their own replacement operators, which then collide at link (duplicate symbol).
# Rebuild that one TU with -DSPRT_WEAK_OPERATOR_NEW_DELETE so the sprt operators are
# weak; a test's replacement then wins and the unreplaced forms fall back to sprt.
# (Flags mirror the runtime build of this TU: freestanding + SPRT_BUILD_RUNTIME + the
# wasm feature set; captured from `make ... verbose=1`.) Swap it into the object list.
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

echo "== $(wc -l < "$RTLIST") runtime objects =="

fi   # end non-compile-only runtime build

# --- 3. export the toolchain contract for lit.cfg.py / sprt_format.py ------------
export SPRT_CXX="$HOSTBIN/c++"
export SPRT_CC="$HOSTBIN/cc"
# Include order mirrors run.sh: overlay (include_libc/cxx) BEFORE full libc++
# (libcxx/include) BEFORE sprt libc (include_libc); test support (-I) outranks the
# -isystem STL chain. wasm adds -nostdinc/-nostdinc++ + the wasm feature flags and
# resolves the libc/clang builtins through -idirafter (lowest priority), exactly as
# the runtime build does (captured from `make ... verbose=1`).
export SPRT_COMPILE_FLAGS="\
-std=gnu++2a -fno-exceptions -frtti -funwind-tables -DDEBUG -DSTAPPLER_LOG_LEVEL=2 \
-Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations \
-Wno-unused-command-line-argument \
--target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR \
-nostdinc -nostdinc++ $WASM_FEATURES \
-isystem $ROOT/runtime/include_libc/cxx \
-isystem $ROOT/runtime/libcxx/include \
-isystem $ROOT/runtime/include_libc \
-I$ROOT/runtime/include -I$SUPPORT \
-idirafter $WASM_USRINC -idirafter $CLANGINC"
export SPRT_COMPILE_ONLY="$COMPILE_ONLY"
# Link contract (captured from the runtime build): shared-memory + atomics module,
# the TLS/stack/malloc/thread-entry exports the Node harness wires, and the wasm
# runtime archives (compiler-rt builtins, libc++abi, libunwind). -nostdlib because
# the sprt runtime objects supply libc; lld is the wasm linker.
export SPRT_LINK_FLAGS="\
-fuse-ld=lld -nostdlib -L$SYSROOT/usr/lib \
--target=$TARGET --sysroot=$SYSROOT -resource-dir $RESDIR $WASM_FEATURES \
-Wl,--import-memory,--shared-memory,--max-memory=1073741824 \
-Wl,--export=__wasm_init_tls,--export=__tls_size,--export=__tls_align,--export=__tls_base \
-Wl,--export=__stack_pointer,--export=malloc,--export=free,--export=__xl_thread_entry \
-Wl,--export-table \
$SYSROOT/lib/clang/lib/wasi/libclang_rt.builtins-wasm32.a \
$SYSROOT/usr/lib/libc++abi.a \
$SYSROOT/usr/lib/libunwind.a"
# Run each linked .wasm headlessly under Node (shared memory + worker threads).
export SPRT_EXEC="node $RUNNER"
export SPRT_STD_VER="20"
export SPRT_TEST_ROOT="$STDROOT/$SCOPE"
export SPRT_BUILD_DIR="$BUILD/work"
export SPRT_RT_OBJS_FILE="$RTLIST"
# Node cold-start + wasm compile per test is heavier than a native exec; give runs
# more headroom than the native default (20s).
export SPRT_RUN_TIMEOUT="${SPRT_RUN_TIMEOUT:-40}"

if [ ! -d "$SPRT_TEST_ROOT" ]; then
  echo "error: test scope not found: $SPRT_TEST_ROOT" >&2; exit 1
fi

# --- 4. run llvm-lit ------------------------------------------------------------
LIT="$LLVM/llvm/utils/lit/lit.py"
echo "== lit (wasm): $SCOPE =="
exec python3 "$LIT" -j"$(nproc)" --config-prefix=lit "$HERE" "$@"
