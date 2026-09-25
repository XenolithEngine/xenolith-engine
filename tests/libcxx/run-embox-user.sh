#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Driver: the upstream libc++ conformance suite against the sprt STL on Embox
# user mode (aarch64-embox-none-elf+user): each test is a static EL0 program,
# run on a pool of Embox QEMU guests by xenolith-os/scripts/embox-exec-pool.py
# (its xlrund takes a program over TCP and gives back how it ended).
#
# The pool is started apart, once, and serves every run:
#   xenolith-os/scripts/embox-exec-pool.py build
#   xenolith-os/scripts/embox-exec-pool.py serve -n 4 &
#   tests/libcxx/run-embox-user.sh containers/sequences/vector
#
# Usage: tests/libcxx/run-embox-user.sh [<scope>] [extra llvm-lit args...]
#   SPRT_EMBOX_OS   the xenolith-os checkout (default: ../xenolith-os)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="aarch64-embox-none-elf+user"
OS="${SPRT_EMBOX_OS:-$ROOT/../xenolith-os}"
POOL="$OS/scripts/embox-exec-pool.py"

TC="$ROOT/runtime/toolchains"
HOSTBIN="$TC/hosts/x86_64-unknown-linux-gnu/bin"
SYSROOT="$TC/targets/$TARGET"
RESDIR="$SYSROOT/lib/clang"
CLANGINC="$(echo "$HOSTBIN"/../lib/clang/*/include)"
LLVM="$ROOT/runtime/toolchains/src/llvm-project"
SUPPORT="$LLVM/libcxx/test/support"
STDROOT="$LLVM/libcxx/test/std"
SCOPE="${1:-containers/associative}"; shift || true

COMPILE_ONLY="${SPRT_COMPILE_ONLY:-}"
SLOTS=1
if [ -z "$COMPILE_ONLY" ]; then
  [ -x "$POOL" ] || { echo "error: no $POOL (SPRT_EMBOX_OS)" >&2; exit 1; }
  SLOTS="$(python3 "$POOL" status)" || {
    echo "error: no Embox pool is serving -- $POOL build; $POOL serve -n 4 &" >&2; exit 1; }
fi

BUILD="$HERE/build/$TARGET/upstream"
mkdir -p "$BUILD"
RTLIST="$BUILD/rt-objs.txt"

# The target's own flags, as tests/libc builds for it (make/os/embox_user).
TARGET_FLAGS="--target=aarch64-none-elf --sysroot=$SYSROOT -resource-dir $RESDIR \
-march=armv8-a -D__EMBOX_USER__ -ffunction-sections -fdata-sections"

if [ -n "$COMPILE_ONLY" ]; then
  : > "$RTLIST"
else

echo "== building sprt runtime via tests/libc ($TARGET) =="
make -C "$ROOT/tests/libc" STAPPLER_TARGET="$TARGET" -j8 >/dev/null || true

OBJDIR="$ROOT/tests/libc/stappler-build/$TARGET/debug/cc/exec_objs/objs"
ls "$OBJDIR"/*.o | grep -viE \
  '/(main|algorithm|any|bind|call_once|char_traits|chrono|coexist|complex|ctype|deque|dirent|env|fsextra|function|future|inttypes|limits|list|macros|mapset|math|multimap|optional|pair|paths|purelib|random|ratio|regex_glob|smartptr|socket|stdatomic|stdio|stdlib|stl_|stream|string|system_error|tgmath|threads|time|tuple|uchar|unistd|unordered|variant|vector_string|wchar|cfenv_csignal|container_adaptor|algorithm_ext|stl_fixes)\.' \
  > "$RTLIST"

# Replaceable operator new/delete as weak, as in run.sh (see there).
ND_OBJ="$BUILD/SPRTCxxNewDelete.nostrong.o"
"$HOSTBIN/c++" $TARGET_FLAGS -nostdinc -nostdinc++ \
  -std=gnu++2a -fno-exceptions -frtti -DSPRT_NO_STRONG_OPERATOR_NEW_DELETE \
  -I"$ROOT/runtime/include" \
  -I"$ROOT/runtime/include_libc/cxx" -I"$ROOT/runtime/libcxx/include" \
  -I"$ROOT/runtime/include_libc" \
  -idirafter "$SYSROOT/usr/include" -idirafter "$CLANGINC" \
  -c -o "$ND_OBJ" "$ROOT/runtime/libc_wrapper/cxx/SPRTCxxNewDelete.cpp"
grep -v 'SPRTCxxNewDelete\.cpp\.o$' "$RTLIST" > "$RTLIST.tmp" && echo "$ND_OBJ" >> "$RTLIST.tmp" && mv "$RTLIST.tmp" "$RTLIST"

echo "== $(wc -l < "$RTLIST") runtime objects =="

fi

export SPRT_CXX="$HOSTBIN/c++"
export SPRT_CC="$HOSTBIN/cc"
export SPRT_COMPILE_FLAGS="\
-std=gnu++2a -fno-exceptions -frtti -funwind-tables -DDEBUG -DSTAPPLER_LOG_LEVEL=2 \
-Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual -Wno-deprecated-declarations \
-Wno-unused-command-line-argument \
$TARGET_FLAGS -nostdinc -nostdinc++ \
-isystem $ROOT/runtime/include_libc/cxx \
-isystem $ROOT/runtime/libcxx/include \
-isystem $ROOT/runtime/include_libc \
-I$ROOT/runtime/include -I$SUPPORT \
-idirafter $SYSROOT/usr/include -idirafter $CLANGINC"
export SPRT_COMPILE_ONLY="$COMPILE_ONLY"
# The link line of an EL0 program (make/os/embox_user): static, at the fixed
# image base, entered at _start.
export SPRT_LINK_FLAGS="\
-nostdlib -static -fuse-ld=lld -Wl,--no-dynamic-linker -Wl,--eh-frame-hdr \
-Wl,--image-base=0x0000400000000000 -Wl,-z,max-page-size=4096 -Wl,--gc-sections \
-Wl,-e,_start -T $SYSROOT/share/app-aarch64.lds -L$SYSROOT/usr/lib \
$RESDIR/lib/embox_user/libclang_rt.builtins-aarch64.a \
$SYSROOT/usr/lib/libc++abi.a $SYSROOT/usr/lib/libunwind.a \
$TARGET_FLAGS"
export SPRT_EXEC="python3 $POOL run"
export SPRT_EXEC_SLOTS="$SLOTS"
# The pool gives a program EMBOX_EXEC_TIMEOUT (300 s: under emulation a test
# can take a minute), then restarts its guest (a boot, ~25 s); lit's own limit
# is above both.
export SPRT_RUN_TIMEOUT="${SPRT_RUN_TIMEOUT:-600}"
export SPRT_STD_VER="20"
# A scope is a path under libcxx/test/std, or a directory of one's own tests.
if [ -d "$SCOPE" ]; then
  export SPRT_TEST_ROOT="$(cd "$SCOPE" && pwd)"
else
  export SPRT_TEST_ROOT="$STDROOT/$SCOPE"
fi
export SPRT_BUILD_DIR="$BUILD/work"
export SPRT_RT_OBJS_FILE="$RTLIST"

if [ ! -d "$SPRT_TEST_ROOT" ]; then
  echo "error: test scope not found: $SPRT_TEST_ROOT" >&2; exit 1
fi

LIT="$LLVM/llvm/utils/lit/lit.py"
echo "== lit (embox user, $SLOTS guest(s)): $SCOPE =="
exec python3 "$LIT" -j"$(nproc)" --config-prefix=lit "$HERE" "$@"
