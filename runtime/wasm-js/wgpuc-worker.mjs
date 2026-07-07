// Worker for the webgpu.h-binding triangle: the wasm calls the real webgpu.h C ABI, bound
// here to navigator.gpu via makeWebgpuImports. Async bootstrap (adapter/device) is resolved
// up front while the event loop is free, so the C side sees synchronous request callbacks;
// the per-frame path (encoder/pass/draw/submit) is synchronous in the browser anyway.

import { makeImports } from "./sprt-imports.mjs";
import { makeWebgpuImports } from "./webgpu.mjs";

self.onmessage = async (e) => {
	const { canvas, wasmUrl } = e.data;
	try {
		if (!navigator.gpu) throw new Error("WebGPU not available in this worker");
		const adapter = await navigator.gpu.requestAdapter();
		const device = await adapter.requestDevice();
		const context = canvas.getContext("webgpu");
		const format = navigator.gpu.getPreferredCanvasFormat();
		context.configure({ device, format, alphaMode: "opaque" });

		const memory = new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });
		let inst = null;
		const imports = makeImports({ memory, log: (s, t) => self.postMessage({ type: "log", text: t }) });
		imports.wgpu = makeWebgpuImports({
			memory,
			gpu: { adapter, device, queue: device.queue, context, format },
			getTable: () => inst.exports.__indirect_function_table,
		});
		const res = await WebAssembly.instantiate(await (await fetch(wasmUrl)).arrayBuffer(), imports);
		inst = res.instance || res;

		self.postMessage({ type: "ready", info: `${adapter.info?.vendor || "gpu"} · ${format} · real webgpu.h C-ABI` });
		const loop = (t) => { inst.exports.xl_frame(t); requestAnimationFrame(loop); };
		requestAnimationFrame(loop);
	} catch (err) {
		self.postMessage({ type: "error", message: String((err && err.stack) || err) });
	}
};
