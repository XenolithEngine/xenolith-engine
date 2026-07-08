// Xenolith engine host — runs the wasm module in a dedicated Web Worker over a shared
// (SharedArrayBuffer) linear memory. Threads spawn as nested workers that instantiate the
// same compiled module against the same memory; see wasm-thread.mjs.
//
// The engine lives in the worker: its threads (Atomics.wait is forbidden on the main
// thread), its WebGPU device/render loop, and synchronous OPFS all require this context.

import { makeImports } from "./sprt-imports.mjs";
import { makeWebgpuThunks, GPU_CTRL_BYTES } from "./webgpu.mjs";

async function loadBundle(manifest) {
	const out = {};
	for (const [mount, url] of Object.entries(manifest || {})) {
		try { out[mount] = new Uint8Array(await (await fetch(url)).arrayBuffer()); } catch (_) {}
	}
	return out;
}

self.onmessage = async (e) => {
	const { wasmUrl, bundleManifest, argv0, hasCanvas, dispW, dispH, dispDensity } = e.data;
	const post = (m) => self.postMessage(m);
	try {
		const bundle = await loadBundle(bundleManifest);
		const module = await WebAssembly.compile(await (await fetch(wasmUrl)).arrayBuffer());
		const memory = new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });

		// Shared atomic tid source: every worker draws unique native thread ids from it.
		const tidBuf = new SharedArrayBuffer(4);
		const tidCounter = new Int32Array(tidBuf);
		Atomics.store(tidCounter, 0, 2); // 1 is the main entry thread

		// Control block for the persistent-filesystem (/opfs) broker: shared by every
		// worker that issues opfs_call and by the OPFS worker (created on the main thread).
		const opfsSab = new SharedArrayBuffer(64 * 4);

		// Control block for the GPU broker: every worker marshals its wgpu* calls through this
		// to the one worker that owns the device + OffscreenCanvas (see gpu-broker.mjs).
		const gpuCtrl = hasCanvas ? new SharedArrayBuffer(GPU_CTRL_BYTES) : null;

		// thread_spawn cannot create the worker here: this worker is about to block in
		// Atomics.wait, which would stall a child worker's startup. Draw a unique tid and
		// delegate creation to the main thread (see loader.mjs).
		const spawn = (threadPtr, stackTop, stackSize, tlsBase) => {
			const tid = Atomics.add(tidCounter, 0, 1);
			post({ type: "spawn", tid, threadPtr, stackTop, stackSize, tlsBase });
			return tid;
		};

		const imports = makeImports({
			memory, bundle, argv: [argv0 || "app"],
			log: (s, t) => post({ type: s, text: t }),
			spawn, opfsSab, dispW, dispH, dispDensity,
			onExit: (c) => post({ type: "exit", code: c }),
		});

		// The `wgpu` table marshals every call to the GPU broker over gpuCtrl. Its local bits
		// (C callbacks, mapped-range malloc) need this module's indirect table + malloc, which
		// only exist after instantiation — bind them late via the `instance` holder.
		let instance;
		if (gpuCtrl) {
			imports.wgpu = makeWebgpuThunks({
				memory, ctrl: gpuCtrl,
				getTable: () => instance.exports.__indirect_function_table,
				getExports: () => instance.exports,
			});
		}
		instance = await WebAssembly.instantiate(module, imports); // module is compiled → Instance

		// Small persistent scratch in wasm memory for the broker's out-param arrays (surface
		// capabilities). Allocated here where malloc has a valid TLS; handed to the broker.
		const scratchPtr = gpuCtrl ? instance.exports.malloc(256) : 0;

		// Publish module + shared memory + control blocks so the main thread can create the
		// thread / OPFS / GPU workers on demand, then run the program.
		post({ type: "init-threads", module, memory, bundle, tidBuf, opfsSab, gpuCtrl, scratchPtr, dispW, dispH, dispDensity });

		instance.exports._start();
		post({ type: "exit", code: 0 });
	} catch (err) {
		if (err && typeof err === "object" && "__exit" in err) return;
		post({ type: "error", message: String((err && err.stack) || err) });
	}
};
