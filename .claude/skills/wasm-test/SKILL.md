---
name: wasm-test
description: Build a Xenolith wasm test, serve it with COOP/COEP, and open it in the browser. Use when someone wants to run/see a wasm test (libc, threads, WebGPU) locally without manual build+serve steps.
---

# Running a Xenolith wasm test in the browser

Everything is wrapped in `runtime/wasm-js/xwasm.sh`. It builds the target for wasm32 (or wasm64,
see below) with the host toolchain, copies the `.wasm` next to the loader, starts a COOP/COEP dev server
(required for SharedArrayBuffer / threads), and opens the page in the browser.

## Usage

```bash
runtime/wasm-js/xwasm.sh <test>            # build + serve + open
runtime/wasm-js/xwasm.sh <test> --no-open  # build + serve, just print the URL
runtime/wasm-js/xwasm.sh --list            # list tests
```

Tests:
- `libc`    — the full tests/libc suite (files, threads 4/4, strftime, STL). 53 sections.
- `thread`  — minimal pthread create/join smoke test.
- `bundled` — reads an external file the browser fetches (LocationCategory::Bundled).
- `wwin`    — a window through WebGPU (wasm32 only).

## Toolchain paths

Auto-detected for the stock installer layout, falling back to the toolchain built in this
repository (`runtime/toolchains/hosts`, `runtime/toolchains/targets`). Override if needed:
- `XENOLITH_WASM_TARGET` — `wasm32-unknown-unknown` (default) or `wasm64-unknown-unknown`
- `XENOLITH_HOST`   — host toolchain dir (contains `bin/make` and `host.mk`)
- `XENOLITH_TARGET` — path to the `target.mk` of that triple
- `XENOLITH_PORT`   — server port (default 8080)

## wasm64

`XENOLITH_WASM_TARGET=wasm64-unknown-unknown` builds against memory64. The JS host detects the
module's memory type itself (`readMemoryImport` in `sprt-imports.mjs`), so the same pages run
both. Chromium and Firefox run it; Safari has no memory64. WebGPU (`wwin`) is wasm32-only
until `webgpu.mjs` stops assuming 32-bit pointers.

Headless, without the browser: `tests/runtime/run-wasm.sh` and `tests/libcxx/run-wasm.sh`
take `SPRT_WASM_TARGET` the same way, and `node runtime/wasm-js/run-node.mjs <module.wasm>`
runs either width.

## Notes
- The COOP/COEP server (`coop-server.py`) is what makes threads work; a plain static server
  won't set `crossOriginIsolated` and threads will degrade.
- To verify headlessly (no window), use `--no-open` and drive the printed URL via CDP; a plain
  `--dump-dom` won't wait for the worker's async fetch, so poll `document.title`.
