// Xenolith engine host — runs the wasm module in a dedicated Web Worker over a shared
// (SharedArrayBuffer) linear memory. Threads spawn as nested workers that instantiate the
// same compiled module against the same memory; see wasm-thread.mjs.
//
// The engine lives in the worker: its threads (Atomics.wait is forbidden on the main
// thread), its WebGPU device/render loop, and synchronous OPFS all require this context.

import { makeImports, INPUT_SAB_BYTES, DISPLAY_SAB_BYTES, writeDisplay, PROC_CTRL_BYTES, PROC_OUT_BYTES, readMemoryImport, createMemory } from "./sprt-imports.mjs";
import { makeWebgpuThunks, GPU_CTRL_BYTES } from "./webgpu.mjs";

async function loadBundle(manifest) {
	const out = {};
	for (const [mount, url] of Object.entries(manifest || {})) {
		if (url instanceof Uint8Array) {
			out[mount] = url;
			continue;
		}
		if (url && typeof url === "object" && url.buffer instanceof ArrayBuffer) {
			out[mount] = url instanceof Uint8Array ? url : new Uint8Array(url);
			continue;
		}
		try {
			const res = await fetch(url);
			if (!res.ok) {
				console.warn("bundle miss", mount, url, res.status);
				continue;
			}
			out[mount] = new Uint8Array(await res.arrayBuffer());
		} catch (err) {
			console.warn("bundle fetch", mount, url, err);
		}
	}
	return out;
}

function createXlmakeMemory(post, initial, maximum) {
	const ini = Math.max(512, initial | 0 || 2048);
	const max = Math.max(ini, maximum | 0 || 4096);
	const attempts = [[ini, max], [Math.min(ini, 2048), Math.min(max, 4096)], [1024, 2048]];
	const seen = new Set();
	let last = null;
	for (const [i, m] of attempts) {
		const key = i + ":" + m;
		if (seen.has(key)) {
			continue;
		}
		seen.add(key);
		try {
			const memory = new WebAssembly.Memory({ initial: i, maximum: m, shared: true });
			post({ type: "stdout", text: "xlmake: wasm memory " + i + ".." + m + " pages ("
				+ Math.round(i * 64 / 1024) + ".." + Math.round(m * 64 / 1024) + "MiB)\n" });
			return memory;
		} catch (e) {
			last = e;
			post({ type: "stderr", text: "xlmake: Memory(" + i + "," + m + ") failed: " + e + "\n" });
		}
	}
	throw last || new Error("xlmake: WebAssembly.Memory(shared) failed");
}

const inputQueue = [];

self.onmessage = async (e) => {
	if (e.data && e.data.type === "input") {
		const evs = e.data.events;
		if (Array.isArray(evs) && evs.length) {
			for (const ev of evs) {
				inputQueue.push(ev);
			}
		}
		return;
	}
	const { wasmUrl, bundleManifest, argv0, args, hasCanvas, dispW, dispH, dispDensity,
		wantProcess, memoryInitial, memoryMaximum, productBundle } = e.data;
	const post = (m) => self.postMessage(m);
	try {
		const bundle = await loadBundle(bundleManifest);
		if (productBundle) {
			for (const [path, data] of Object.entries(productBundle)) {
				if (data) {
					bundle[path] = data;
				}
			}
		}
		const bytes = await (await fetch(wasmUrl)).arrayBuffer();
		const module = await WebAssembly.compile(bytes);
		const memDesc = readMemoryImport(bytes);
		let memory;
		if (memDesc && memDesc.memory64) {
			// wasm64: the module declares up to 16 GiB, which a tab may not be able to reserve as
			// shared memory. Commit the same 1 GiB the wasm32 path does and step the MAXIMUM down
			// instead - an imported memory only has to fit inside the declared limits.
			const initial = 16384;
			for (const maximum of [memDesc.maximum, 131072, 65536]) {
				try {
					memory = createMemory(memDesc, { initial, maximum });
					post({ type: "stdout", text: `wasm64 memory ${(initial * 64) / 1024} MiB committed, `
						+ `maximum ${(Math.min(maximum, memDesc.maximum) * 64) / 1024} MiB\n` });
					break;
				} catch (err) {
					post({ type: "stdout", text: `wasm64 memory maximum ${maximum} pages refused (${err})\n` });
				}
			}
			if (!memory) {
				throw new Error("cannot allocate shared wasm64 memory");
			}
		} else if (wantProcess && !hasCanvas) {
			memory = createXlmakeMemory(post, memoryInitial, memoryMaximum);
		} else {
			// 32 MiB initial, grow on demand up to the 1 GiB linker ceiling: growth is
			// serialized by the engine-side sbrk lock (see docs/platforms/wasm.adoc).
			// The earlier full-ceiling precommit ("grow disabled") turned that lock
			// into dead code in production. The "shared grow returns -1 once workers
			// exist" observation that motivated it was the pre-lock growth race plus
			// mimalloc's default 1 GiB arena reserve hitting the ceiling - not a
			// browser limitation.
			memory = new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });
			post({ type: "stdout", text: "wasm memory 32 MiB initial (grow enabled, max 1 GiB)\n" });
		}

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
		const inputSab = hasCanvas ? new SharedArrayBuffer(INPUT_SAB_BYTES) : null;
		const displaySab = hasCanvas ? new SharedArrayBuffer(DISPLAY_SAB_BYTES) : null;
		if (displaySab) {
			writeDisplay(displaySab, dispW, dispH, dispDensity);
		}
		const processCtrl = wantProcess ? new SharedArrayBuffer(PROC_CTRL_BYTES) : null;
		let processOut = null;
		if (wantProcess) {
			try {
				processOut = new SharedArrayBuffer(PROC_OUT_BYTES);
				post({ type: "stderr", text: "xlmake: process SAB " + Math.round(PROC_OUT_BYTES / 1048576) + "MiB\n" });
			} catch (e) {
				throw new Error("xlmake: process SAB SharedArrayBuffer("
					+ Math.round(PROC_OUT_BYTES / 1048576) + "MiB) failed: " + e);
			}
		}

		// thread_spawn cannot create the worker here: this worker is about to block in
		// Atomics.wait, which would stall a child worker's startup. Draw a unique tid and
		// delegate creation to the main thread (see loader.mjs).
		const spawn = (threadPtr, stackTop, stackSize, tlsBase) => {
			const tid = Atomics.add(tidCounter, 0, 1);
			post({ type: "spawn", tid, threadPtr, stackTop, stackSize, tlsBase });
			return tid;
		};

		const imports = makeImports({
			memory, bundle, argv: [argv0 || "app", ...(args || [])],
			log: (s, t) => post({ type: s, text: t }),
			spawn, opfsSab, dispW, dispH, dispDensity, inputQueue, inputSab, displaySab,
			processCtrl, processOut, postProcess: wantProcess ? post : null,
			onFilePut: wantProcess ? (path, bytes) => post({ type: "file-put", path, bytes }) : null,
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
		// wasm64 malloc takes/returns i64: BigInt in, Number out (memory < 2^53).
		const scratchPtr = gpuCtrl ? Number(instance.exports.malloc(memDesc.memory64 ? 256n : 256)) : 0;

		// Publish module + shared memory + control blocks so the main thread can create the
		// thread / OPFS / GPU workers on demand, then run the program.
		post({ type: "init-threads", module, memory, bundle, tidBuf, opfsSab, gpuCtrl, scratchPtr, dispW, dispH, dispDensity, inputSab, displaySab, processCtrl, processOut });

		instance.exports._start();
		post({ type: "exit", code: 0 });
	} catch (err) {
		if (err && typeof err === "object" && "__exit" in err) return;
		post({ type: "error", message: String((err && err.stack) || err) });
	}
};
