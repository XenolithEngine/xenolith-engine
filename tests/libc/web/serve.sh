#!/bin/bash
# Run the tests/libc suite in a browser. Build first with the host make + Roma's target:
#   HOST=$HOME/.local/share/xenolith/data/toolchains/hosts/aarch64-apple-macosx
#   $HOST/bin/make -C tests/libc STAPPLER_TARGET=wasm32-unknown-unknown \
#     STAPPLER_HOST_FILE=$HOST/host.mk STAPPLER_TARGET_FILE=/Users/vitaliyry/wasm32-unknown-unknown/target.mk -j8
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cp "$HERE/../stappler-build/wasm32-unknown-unknown/debug/cc/libctest.wasm" "$HERE/libctest.wasm"
echo "open http://127.0.0.1:8721/index.html"
cd "$HERE" && python3 -m http.server 8721 --bind 127.0.0.1
