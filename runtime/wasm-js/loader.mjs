// Xenolith wasm main-thread broker + worker hub.
//
// The engine runs in a dedicated worker over a shared (SharedArrayBuffer) linear memory;
// threads run as additional workers over that same memory. Crucially, ALL worker creation
// happens here on the main thread: a worker that calls thread_spawn immediately blocks in
// Atomics.wait, and a worker cannot bring up a child worker while its own event loop is
// frozen. So spawn requests are posted to the main thread, whose event loop is always free,
// and it creates the thread worker. This also lets nested spawns (a thread creating a
// thread) route back here uniformly.
//
// run(wasmUrl, opts) -> Promise<exitCode>
//   opts.onStdout / onStderr / onExit, opts.bundle (mount->url), opts.argv0
export function run(wasmUrl, { onStdout, onStderr, onExit, bundle, argv0 } = {}) {
	return new Promise((resolve, reject) => {
		let shared = null; // { module, memory, bundle, tidBuf } published by the engine worker

		const wire = (w) => {
			w.onmessage = (e) => handle(e.data);
			w.onerror = (ev) => (onStderr || onStdout)?.("[worker error] " + (ev.message || ev.filename || ev) + "\n");
		};

		const handle = (m) => {
			switch (m.type) {
			case "init-threads":
				shared = m;
				// Bring up the OPFS backend worker here (the main thread can create workers,
				// and this one does async OPFS + Atomics.waitAsync — it must NOT be a worker
				// that ever blocks in Atomics.wait). It shares the wasm memory + control block.
				if (m.opfsSab) {
					const o = new Worker(new URL("./opfs-worker.mjs", import.meta.url), { type: "module" });
					o.onerror = (ev) => (onStderr || onStdout)?.("[opfs worker] " + (ev.message || ev) + "\n");
					o.postMessage({ opfsSab: m.opfsSab, mem: m.memory });
				}
				break;
			case "spawn": {
				// Create the thread worker here (free event loop) and hand it the module,
				// shared memory and the pre-allocated stack/TLS the requester set up.
				const w = new Worker(new URL("./wasm-thread.mjs", import.meta.url), { type: "module" });
				wire(w);
				w.postMessage({
					module: shared.module, memory: shared.memory, bundle: shared.bundle, tidBuf: shared.tidBuf,
					opfsSab: shared.opfsSab,
					tid: m.tid, threadPtr: m.threadPtr, stackTop: m.stackTop, stackSize: m.stackSize, tlsBase: m.tlsBase,
				});
				break;
			}
			case "stdout": onStdout?.(m.text); break;
			case "stderr": (onStderr || onStdout)?.(m.text); break;
			case "exit": onExit?.(m.code); resolve(m.code); break;
			case "error": reject(new Error(m.message)); break;
			}
		};

		const engine = new Worker(new URL("./worker.mjs", import.meta.url), { type: "module" });
		wire(engine);
		engine.onerror = (err) => reject(err);
		engine.postMessage({ wasmUrl, bundleManifest: bundle, argv0 });
	});
}
