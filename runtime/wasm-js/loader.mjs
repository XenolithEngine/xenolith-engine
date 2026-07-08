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
//   opts.canvas: an on-page <canvas> whose control is transferred to the engine worker as an
//   OffscreenCanvas (the engine's WebGPU surface). The worker owns it for the app's lifetime.
export function run(wasmUrl, { onStdout, onStderr, onExit, bundle, argv0, canvas, density } = {}) {
	return new Promise((resolve, reject) => {
		let shared = null; // { module, memory, bundle, tidBuf, gpuCtrl } published by the engine worker
		// Capture the canvas backing size BEFORE transfer (afterwards width/height read back 0) so
		// the engine can size its swapchain/window to the real viewport (display_size host import).
		const dispW = canvas ? (canvas.width | 0) : 0;
		const dispH = canvas ? (canvas.height | 0) : 0;
		const dispDensity = Math.round((density || 1) * 1000); // devicePixelRatio x1000
		// Control of the on-page canvas is transferred to an OffscreenCanvas, held here until the
		// GPU broker worker exists (created on init-threads), then handed to it.
		let offscreen = canvas ? canvas.transferControlToOffscreen() : null;

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
				// Bring up the GPU broker: it owns the OffscreenCanvas + navigator.gpu and
				// services every worker's marshalled wgpu* calls off a free event loop (same
				// no-blocking rule as OPFS). Created here so the transferred canvas lands on it.
				if (m.gpuCtrl && offscreen) {
					const g = new Worker(new URL("./gpu-broker.mjs", import.meta.url), { type: "module" });
					g.onmessage = (e) => handle(e.data);
					g.onerror = (ev) => (onStderr || onStdout)?.("[gpu broker] " + (ev.message || ev) + "\n");
					g.postMessage({ memory: m.memory, canvas: offscreen, ctrl: m.gpuCtrl, scratchPtr: m.scratchPtr }, [offscreen]);
					offscreen = null;
				}
				break;
			case "gpu-ready": onStdout?.("[gpu] ready (" + m.info + ")\n"); break;
			case "spawn": {
				// Create the thread worker here (free event loop) and hand it the module,
				// shared memory and the pre-allocated stack/TLS the requester set up.
				const w = new Worker(new URL("./wasm-thread.mjs", import.meta.url), { type: "module" });
				wire(w);
				w.postMessage({
					module: shared.module, memory: shared.memory, bundle: shared.bundle, tidBuf: shared.tidBuf,
					opfsSab: shared.opfsSab, gpuCtrl: shared.gpuCtrl, dispW: shared.dispW, dispH: shared.dispH, dispDensity: shared.dispDensity,
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
		engine.postMessage({ wasmUrl, bundleManifest: bundle, argv0, hasCanvas: !!offscreen, dispW, dispH, dispDensity });
	});
}
