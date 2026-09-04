// Xenolith engine host — runs the wasm module in a dedicated Web Worker over a shared
// (SharedArrayBuffer) linear memory. Threads spawn as nested workers that instantiate the
// same compiled module against the same memory; see wasm-thread.mjs.
//
// The engine lives in the worker: its threads (Atomics.wait is forbidden on the main
// thread), its WebGPU device/render loop, and synchronous OPFS all require this context.

import { makeImports, PROC_CTRL_BYTES, PROC_OUT_BYTES, formatError } from "./sprt-imports.mjs";
import { makeWebgpuThunks, GPU_CTRL_BYTES } from "./webgpu.mjs";
import { fetchPacked, aliasSdkIntoMakeBundle, unpackSdk, gunzip } from "./xlmake-fs.mjs";

async function loadBundle(manifest) {
	const out = {};
	for (const [mount, url] of Object.entries(manifest || {})) {
		if (url instanceof Uint8Array) {
			out[mount] = url;
			continue;
		}
		try { out[mount] = new Uint8Array(await (await fetch(url)).arrayBuffer()); } catch (_) {}
	}
	return out;
}

function note(post, text) {
	post({ type: "stderr", text: text.endsWith("\n") ? text : text + "\n" });
}

function allocSab(post, bytes, label) {
	try {
		const buf = new SharedArrayBuffer(bytes);
		note(post, "xlmake: " + label + " " + Math.round(bytes / 1048576) + "MiB");
		return buf;
	} catch (e) {
		throw new Error("xlmake: " + label + " SharedArrayBuffer("
			+ Math.round(bytes / 1048576) + "MiB) failed: " + formatError(e));
	}
}

function createMemory(post, initial, maximum) {
	const ini = Math.max(512, initial | 0 || 512);
	const max = Math.max(ini, maximum | 0 || 8192);
	const attempts = [[ini, max]];
	if (max > 4096) {
		attempts.push([Math.min(ini, 2048), 4096]);
	}
	if (ini > 1024) {
		attempts.push([1024, 2048]);
	}
	let last = null;
	const seen = new Set();
	for (const [i, m] of attempts) {
		const key = i + ":" + m;
		if (seen.has(key)) {
			continue;
		}
		seen.add(key);
		try {
			const memory = new WebAssembly.Memory({ initial: i, maximum: m, shared: true });
			note(post, "xlmake: wasm memory " + i + ".." + m + " pages ("
				+ Math.round(i * 64 / 1024) + ".." + Math.round(m * 64 / 1024) + "MiB)");
			return memory;
		} catch (e) {
			last = e;
			note(post, "xlmake: Memory(" + i + "," + m + ") failed: " + formatError(e));
		}
	}
	throw last || new Error("xlmake: WebAssembly.Memory(shared) failed");
}

self.onmessage = async (e) => {
	const { wasmUrl, bundleManifest, argv0, args, hasCanvas, dispW, dispH, dispDensity,
		sdkGzUrl, mkGzUrl, productBundle, memoryInitial, memoryMaximum, sdkSab: sdkSabIn } = e.data;
	const post = (m) => self.postMessage(m);
	try {
		const bundle = await loadBundle(bundleManifest);
		let sdkSab = sdkSabIn instanceof SharedArrayBuffer ? sdkSabIn : null;
		if (sdkSab) {
			note(post, "xlmake: sdk from shared buffer (" + Math.round(sdkSab.byteLength / 1048576) + "MiB)");
			aliasSdkIntoMakeBundle(bundle, unpackSdk(new Uint8Array(sdkSab)));
			note(post, "xlmake: sdk ready (" + Object.keys(bundle).length + " files)");
		} else if (sdkGzUrl) {
			note(post, "xlmake: unpacking sdk.bin.gz");
			const res = await fetch(sdkGzUrl);
			if (!res.ok) {
				throw new Error("fetch " + sdkGzUrl + " -> " + res.status);
			}
			const raw = await gunzip(new Uint8Array(await res.arrayBuffer()));
			sdkSab = new SharedArrayBuffer(raw.byteLength);
			new Uint8Array(sdkSab).set(raw);
			aliasSdkIntoMakeBundle(bundle, unpackSdk(new Uint8Array(sdkSab)));
			note(post, "xlmake: sdk ready (" + Object.keys(bundle).length + " files, "
				+ Math.round(sdkSab.byteLength / 1048576) + "MiB shared)");
		}
		if (mkGzUrl) {
			for (const { guest, data } of await fetchPacked(mkGzUrl)) {
				bundle[guest] = data;
				if (guest.startsWith("/")) {
					bundle[guest.slice(1)] = data;
				}
			}
		}
		if (productBundle) {
			for (const [path, data] of Object.entries(productBundle)) {
				if (data) {
					bundle[path] = data;
				}
			}
		}
		note(post, "xlmake: compiling xlmake.wasm");
		const module = await WebAssembly.compile(await (await fetch(wasmUrl)).arrayBuffer());
		const memory = createMemory(post, memoryInitial, memoryMaximum);

		// Shared atomic tid source: every worker draws unique native thread ids from it.
		const tidBuf = new SharedArrayBuffer(4);
		const tidCounter = new Int32Array(tidBuf);
		Atomics.store(tidCounter, 0, 2); // 1 is the main entry thread

		// Control block for the persistent-filesystem (/opfs) broker: shared by every
		// worker that issues opfs_call and by the OPFS worker (created on the main thread).
		const opfsSab = new SharedArrayBuffer(64 * 4);

		// Completions for spawnProcess (clang.wasm Web Workers). Layout: Int32
		// write_idx, read_idx, claim_idx, then N slots of {id, code, outOff, outLen}.
		const processCtrl = new SharedArrayBuffer(PROC_CTRL_BYTES);
		const processOut = allocSab(post, PROC_OUT_BYTES, "process SAB");

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
			memory, bundle, argv: [argv0 || "app", ...(args || [])],
			log: (s, t) => post({ type: s, text: t }),
			spawn, opfsSab, dispW, dispH, dispDensity,
			processCtrl, processOut, postProcess: post,
			onFilePut: (path, bytes) => post({ type: "file-put", path, bytes }),
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
		post({
			type: "init-threads", module, memory,
			bundle: (sdkGzUrl || sdkSab) ? {} : bundle,
			tidBuf, opfsSab, gpuCtrl, scratchPtr, dispW, dispH, dispDensity, processCtrl, processOut,
			sdkSab,
		});

		instance.exports._start();
		post({ type: "exit", code: 0 });
	} catch (err) {
		if (err && typeof err === "object" && "__exit" in err) return;
		const text = formatError(err);
		post({ type: "stderr", text: "xlmake worker: " + text + "\n" });
		post({ type: "error", message: text });
	}
};
