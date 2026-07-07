---
name: wasm-test
description: Build a Xenolith wasm test, serve it with COOP/COEP, and open it in the browser. Use when someone wants to run/see a wasm test (libc, threads, WebGPU) locally without manual build+serve steps.
---

# Running a Xenolith wasm test in the browser

Everything is wrapped in `runtime/wasm-js/xwasm.sh`. It builds the target for wasm32 with the
host toolchain, copies the `.wasm` next to the loader, starts a COOP/COEP dev server
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
- `gpu`     — animated WebGPU triangle via a minimal host ABI.
- `gpuc`    — WebGPU triangle through the real `webgpu.h` C ABI (the engine's binding).

## Toolchain paths

Auto-detected for the stock installer layout. Override if needed:
- `XENOLITH_HOST`   — installed host toolchain dir (contains `bin/make` and `host.mk`)
- `XENOLITH_TARGET` — path to the wasm32 `target.mk`
- `XENOLITH_PORT`   — server port (default 8080)

## Notes
- The COOP/COEP server (`coop-server.py`) is what makes threads work; a plain static server
  won't set `crossOriginIsolated` and threads will degrade.
- To verify headlessly (no window), use `--no-open` and drive the printed URL via CDP; a plain
  `--dump-dom` won't wait for the worker's async fetch, so poll `document.title`.
