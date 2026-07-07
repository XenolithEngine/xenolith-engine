// Xenolith WebGPU host binding: implements the webgpu.h C ABI as wasm host imports over
// the browser's navigator.gpu (the role Emscripten's library_webgpu.js plays). The engine
// calls wgpuCreateInstance / wgpuAdapterRequestDevice / wgpuDeviceCreateRenderPipeline /
// ... ; each maps to a GPU* object here, addressed from wasm by an integer handle.
//
// FOUNDATION (triangle subset). The descriptor byte layouts below are a self-consistent
// minimal webgpu.h (see webgpu.h in this dir); reconciling offsets with the canonical
// webgpu-native header is mechanical once the machinery is proven. Async bootstrap
// (requestAdapter/requestDevice) is resolved up front in the worker before the engine
// runs, so the C side sees synchronous callbacks; the per-frame path is already sync.

// ---- handle table: integer <-> GPU object ---------------------------------------------
export class Handles {
	constructor() { this.slots = [null]; this.free = []; } // 0 == null handle
	add(obj) { if (obj == null) return 0; const id = this.free.pop() ?? this.slots.length; this.slots[id] = obj; return id; }
	get(id) { return id ? this.slots[id] : null; }
	release(id) { if (id) { this.slots[id] = null; this.free.push(id); } }
}

// ---- enum maps (int -> WebGPU string) -------------------------------------------------
// Values match the minimal webgpu.h in this directory.
const TEXTURE_FORMAT = { 23: "bgra8unorm", 18: "rgba8unorm", 12: "r8unorm" };
const LOAD_OP = { 1: "load", 2: "clear" };
const STORE_OP = { 1: "store", 2: "discard" };
const PRIMITIVE_TOPOLOGY = { 0: "point-list", 1: "line-list", 2: "line-strip", 3: "triangle-list", 4: "triangle-strip" };
const VERTEX_FORMAT = { 23: "float32", 24: "float32x2", 25: "float32x3", 26: "float32x4" };
const INDEX_FORMAT = { 1: "uint16", 2: "uint32" };
const VERTEX_STEP = { 1: "vertex", 2: "instance" };
const CULL_MODE = { 0: "none", 1: "front", 2: "back" };
const FRONT_FACE = { 0: "ccw", 1: "cw" };

// ---- little-endian memory codec over the (shared) wasm buffer --------------------------
export function makeCodec(memory) {
	const dv = () => new DataView(memory.buffer);
	const u8 = () => new Uint8Array(memory.buffer);
	const u32 = (p) => dv().getUint32(p, true);
	const i32 = (p) => dv().getInt32(p, true);
	const u64 = (p) => Number(dv().getBigUint64(p, true));
	const f32 = (p) => dv().getFloat32(p, true);
	const f64 = (p) => dv().getFloat64(p, true);
	// WGPUStringView { const char* data; size_t length } — length SIZE_MAX means NUL-term.
	const strView = (p) => {
		const data = u32(p), len = u32(p + 4);
		if (!data) return "";
		if (len === 0xffffffff) { let e = data; const b = u8(); while (b[e]) e++; return new TextDecoder().decode(u8().slice(data, e)); }
		return new TextDecoder().decode(u8().slice(data, data + len));
	};
	return { dv, u8, u32, i32, u64, f32, f64, strView,
		TEXTURE_FORMAT, LOAD_OP, STORE_OP, PRIMITIVE_TOPOLOGY, VERTEX_FORMAT, INDEX_FORMAT, VERTEX_STEP, CULL_MODE, FRONT_FACE };
}

// ---- the `wgpu` host import table -----------------------------------------------------
// gpu: { device, queue, context, format } obtained by the worker up front (async bootstrap
// done while the event loop was free). getTable: () => the wasm __indirect_function_table,
// so async request callbacks can be invoked synchronously in the C code.
export function makeWebgpuImports({ memory, gpu, getTable }) {
	const H = new Handles();
	const c = makeCodec(memory);
	const call = (fnPtr, ...args) => getTable().get(fnPtr)(...args);

	// Decode WGPURenderPipelineDescriptor (see webgpu.h layout).
	function decodePipeline(p) {
		const vertexModule = H.get(c.u32(p + 20));
		const vertexEntry = c.strView(p + 24);
		const topology = c.PRIMITIVE_TOPOLOGY[c.u32(p + 52)] || "triangle-list";
		const layoutH = c.u32(p + 12);
		const desc = {
			layout: layoutH ? H.get(layoutH) : "auto",
			vertex: { module: vertexModule, entryPoint: vertexEntry },
			primitive: { topology },
		};
		const fragPtr = c.u32(p + 92);
		if (fragPtr) {
			const fragModule = H.get(c.u32(fragPtr + 4));
			const fragEntry = c.strView(fragPtr + 8);
			const targetCount = c.u32(fragPtr + 24);
			const targetsPtr = c.u32(fragPtr + 28);
			const targets = [];
			for (let i = 0; i < targetCount; i++) {
				const t = targetsPtr + i * 16;
				targets.push({ format: c.TEXTURE_FORMAT[c.u32(t + 4)] || gpu.format, writeMask: c.u32(t + 12) || 0xF });
			}
			desc.fragment = { module: fragModule, entryPoint: fragEntry, targets };
		}
		return desc;
	}

	// Decode WGPURenderPassDescriptor (color attachments only; stride 56).
	function decodeRenderPass(p) {
		const count = c.u32(p + 12);
		const attsPtr = c.u32(p + 16);
		const colorAttachments = [];
		for (let i = 0; i < count; i++) {
			const b = attsPtr + i * 56;
			colorAttachments.push({
				view: H.get(c.u32(b + 4)),
				loadOp: c.LOAD_OP[c.u32(b + 16)] || "clear",
				storeOp: c.STORE_OP[c.u32(b + 20)] || "store",
				clearValue: { r: c.f64(b + 24), g: c.f64(b + 32), b: c.f64(b + 40), a: c.f64(b + 48) },
			});
		}
		return { colorAttachments };
	}

	return {
		wgpuCreateInstance: (_desc) => H.add({ instance: true }),
		wgpuInstanceRequestAdapter: (_inst, _opts, cb, ud) => call(cb, 1 /*success*/, H.add(gpu.adapter || { adapter: true }), 0, 0, ud),
		wgpuAdapterRequestDevice: (_ad, _desc, cb, ud) => call(cb, 1, H.add(gpu.device), 0, 0, ud),
		wgpuDeviceGetQueue: (_dev) => H.add(gpu.queue),
		wgpuGetCanvasSurface: (_inst) => H.add({ surface: true }),
		wgpuDeviceCreateShaderModule: (dev, desc) => {
			const chain = c.u32(desc + 0); // nextInChain -> WGPUShaderSourceWGSL
			const code = chain ? c.strView(chain + 8) : "";
			return H.add(H.get(dev) ? H.get(dev).createShaderModule({ code }) : gpu.device.createShaderModule({ code }));
		},
		wgpuDeviceCreateRenderPipeline: (dev, desc) => H.add(gpu.device.createRenderPipeline(decodePipeline(desc))),
		wgpuSurfaceGetCurrentTexture: (_surf) => H.add(gpu.context.getCurrentTexture()),
		wgpuTextureCreateView: (tex, _desc) => H.add(H.get(tex).createView()),
		wgpuDeviceCreateCommandEncoder: (_dev, _desc) => H.add(gpu.device.createCommandEncoder()),
		wgpuCommandEncoderBeginRenderPass: (enc, desc) => H.add(H.get(enc).beginRenderPass(decodeRenderPass(desc))),
		wgpuRenderPassEncoderSetPipeline: (pass, pipe) => H.get(pass).setPipeline(H.get(pipe)),
		wgpuRenderPassEncoderDraw: (pass, vc, ic, fv, fi) => H.get(pass).draw(vc, ic, fv, fi),
		wgpuRenderPassEncoderEnd: (pass) => H.get(pass).end(),
		wgpuCommandEncoderFinish: (enc, _desc) => H.add(H.get(enc).finish()),
		wgpuQueueSubmit: (queue, count, cmdsPtr) => {
			const cmds = [];
			for (let i = 0; i < count; i++) cmds.push(H.get(c.u32(cmdsPtr + i * 4)));
			H.get(queue).submit(cmds);
		},
		wgpuSurfacePresent: (_surf) => { /* browser auto-presents the configured canvas */ },
		// releases: drop the handle (GC frees the GPU object)
		wgpuTextureViewRelease: (h) => H.release(h),
		wgpuTextureRelease: (h) => H.release(h),
		wgpuCommandEncoderRelease: (h) => H.release(h),
		wgpuRenderPassEncoderRelease: (h) => H.release(h),
		wgpuCommandBufferRelease: (h) => H.release(h),
	};
}
