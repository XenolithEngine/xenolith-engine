// Xenolith WebGPU worker: the wasm engine runs here and drives rendering to an
// OffscreenCanvas through navigator.gpu. This is the browser-graphics path end to end —
// a GPUDevice is not Transferable, so the device, pipeline and the whole render loop live
// in the worker; the main thread only hands over the OffscreenCanvas.
//
// JS owns the GPU plumbing (adapter/device/pipeline); the wasm module owns per-frame logic
// and calls back through the `gpu` host import once per requestAnimationFrame tick.

import { makeImports } from "./sprt-imports.mjs";

const WGSL = `
@group(0) @binding(0) var<uniform> triColor : vec4f;
@vertex fn vs(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
  var p = array<vec2f, 3>(vec2f(0.0, 0.65), vec2f(-0.65, -0.55), vec2f(0.65, -0.55));
  return vec4f(p[i], 0.0, 1.0);
}
@fragment fn fs() -> @location(0) vec4f { return triColor; }`;

let device, context, pipeline, format, colorBuf, bindGroup, memory;
let clear = { r: 0.1, g: 0.1, b: 0.1 };

function renderFrame(br, bg, bb, tr, tg, tb) {
	clear = { r: br, g: bg, b: bb };
	device.queue.writeBuffer(colorBuf, 0, new Float32Array([tr, tg, tb, 1]));
	const enc = device.createCommandEncoder();
	const view = context.getCurrentTexture().createView();
	const pass = enc.beginRenderPass({
		colorAttachments: [{ view, clearValue: { r: br, g: bg, b: bb, a: 1 }, loadOp: "clear", storeOp: "store" }],
	});
	pass.setPipeline(pipeline);
	pass.setBindGroup(0, bindGroup);
	pass.draw(3);
	pass.end();
	device.queue.submit([enc.finish()]);
}

// The sprt host imports the module declares; unused here (we drive xl_frame directly, not
self.onmessage = async (e) => {
	const { canvas, wasmUrl } = e.data;
	try {
		if (!navigator.gpu) throw new Error("WebGPU not available in this worker (navigator.gpu is undefined)");
		const adapter = await navigator.gpu.requestAdapter();
		if (!adapter) throw new Error("no GPU adapter");
		device = await adapter.requestDevice();
		context = canvas.getContext("webgpu");
		format = navigator.gpu.getPreferredCanvasFormat();
		context.configure({ device, format, alphaMode: "opaque" });

		const mod = device.createShaderModule({ code: WGSL });
		pipeline = device.createRenderPipeline({
			layout: "auto",
			vertex: { module: mod, entryPoint: "vs" },
			fragment: { module: mod, entryPoint: "fs", targets: [{ format }] },
			primitive: { topology: "triangle-list" },
		});
		colorBuf = device.createBuffer({ size: 16, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
		bindGroup = device.createBindGroup({
			layout: pipeline.getBindGroupLayout(0),
			entries: [{ binding: 0, resource: { buffer: colorBuf } }],
		});

		memory = new WebAssembly.Memory({ initial: 512, maximum: 16384, shared: true });
		const imports = makeImports({ memory, log: (s, t) => self.postMessage({ type: "log", text: t }) });
		imports.gpu = { render: renderFrame };
		const instance = await WebAssembly.instantiate(await (await fetch(wasmUrl)).arrayBuffer(), imports);
		// wasm bytes → { instance, module }
		const inst = instance.instance || instance;

		self.postMessage({ type: "ready", info: `${adapter.info?.vendor || "gpu"} · ${format}` });
		const loop = (t) => { inst.exports.xl_frame(t); requestAnimationFrame(loop); };
		requestAnimationFrame(loop);
	} catch (err) {
		self.postMessage({ type: "error", message: String((err && err.stack) || err) });
	}
};
