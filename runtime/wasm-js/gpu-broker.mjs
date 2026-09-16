// The GPU broker worker. It owns navigator.gpu, the device/queue/context, the OffscreenCanvas
// and the WebGPU handle table; every other worker's wgpu* calls are marshalled here over a
// shared control block (see webgpu.mjs). This worker never runs engine code — it only services
// GPU calls off its free event loop, so it must NEVER block in Atomics.wait.

import { bootstrapGpu, makeBrokerTable, runBroker } from "./webgpu.mjs";

self.onmessage = async (e) => {
	const { memory, canvas, ctrl, scratchPtr } = e.data;
	const post = (m) => self.postMessage(m);
	try {
		const gpu = await bootstrapGpu(canvas);
		// Surface async WebGPU validation errors (they don't throw synchronously).
		gpu.device.onuncapturederror = (ev) => post({ type: "stderr", text: "[gpu] VALIDATION: " + (ev.error?.message || ev) + "\n" });

		const table = makeBrokerTable({ memory, gpu, scratchPtr });
		post({ type: "gpu-ready", info: `${gpu.format}` });

		// A transferred OffscreenCanvas only pushes its latest WebGPU render to the visible
		// placeholder canvas at the worker's animation-frame boundary. The engine renders off its
		// own clock (marshalled calls, not rAF), so an rAF loop here blits the engine's latest
		// offscreen render into the canvas each frame (see makeBrokerTable's $present).
		if (typeof requestAnimationFrame === "function") {
			const tick = () => { try { table.$present(); } catch {} requestAnimationFrame(tick); };
			requestAnimationFrame(tick);
		}

		await runBroker({ ctrl, table, onError: (m) => post({ type: "stderr", text: "[gpu] " + m + "\n" }) });
	} catch (err) {
		post({ type: "error", message: "gpu-broker: " + String((err && err.stack) || err) });
	}
};
