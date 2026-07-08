// WebGPU host binding for the wasm engine build — GPU broker edition.
//
// A GPUDevice is bound to the worker that created it, but the engine spreads GPU calls across
// workers (the app thread is its own worker). So one dedicated worker (the "GPU broker",
// gpu-worker.mjs) owns navigator.gpu + the device + the OffscreenCanvas + the handle table,
// and every wasm worker's `wgpu` imports are thunks that marshal each call to it over a shared
// control block (SharedArrayBuffer + Atomics), blocking until the broker returns. WebGPU's JS
// API is mostly synchronous, so the broker services a call in one turn of its free event loop.
//
// Split of responsibility:
//   * broker side (makeBrokerTable) — the synchronous GPU work: reads descriptors straight out
//     of shared linear memory, writes out-params back into it. No malloc, no wasm callbacks.
//   * thunk side (makeWebgpuThunks) — marshals calls; and handles the few functions that need
//     the calling instance itself: C callbacks (need that worker's indirect table) and wasm
//     malloc (need that worker's TLS-valid allocator). Those never cross to the broker.
//
// wasm32 layout: 4-byte fields unless noted; WGPUStringView = {ptr,len} (8B); WGPUColor = 4x
// f64 (8-aligned); u64 fields 8-aligned; handles are u32 broker-table ids (0 == null).

// ---- enum decode tables (int -> WebGPU string; values per webgpu.h) -----------------------
const TEXTURE_FORMAT = { 0: undefined, 1: "r8unorm", 7: "r16float", 8: "rg8unorm", 12: "r32float",
	18: "rgba8unorm", 19: "rgba8unorm-srgb", 23: "bgra8unorm", 24: "bgra8unorm-srgb",
	33: "rgba16float", 34: "rgba32float", 39: "depth16unorm", 42: "depth24plus-stencil8",
	43: "depth32float", 44: "depth32float-stencil8" };
const FORMAT_TO_ENUM = Object.fromEntries(Object.entries(TEXTURE_FORMAT).filter(([, v]) => v).map(([k, v]) => [v, +k]));
const LOAD_OP = { 1: "load", 2: "clear" };
const STORE_OP = { 1: "store", 2: "discard" };
const TOPOLOGY = { 0: "point-list", 1: "line-list", 2: "line-strip", 3: "triangle-list", 4: "triangle-strip" };
const CULL_MODE = { 0: "none", 1: "front", 2: "back" };
const FRONT_FACE = { 0: "ccw", 1: "cw" };
const INDEX_FORMAT = { 0: undefined, 1: "uint16", 2: "uint32" };
const TEX_DIM = { 0: "1d", 1: "2d", 2: "3d" };
const VIEW_DIM = { 0: undefined, 1: "1d", 2: "2d", 3: "2d-array", 4: "cube", 5: "cube-array", 6: "3d" };
const ASPECT = { 0: undefined, 1: "all", 2: "stencil-only", 3: "depth-only" };
const ADDRESS = { 0: undefined, 1: "clamp-to-edge", 2: "repeat", 3: "mirror-repeat" };
const FILTER = { 0: undefined, 1: "nearest", 2: "linear" };
const COMPARE = { 0: undefined, 1: "never", 2: "less", 3: "equal", 4: "less-equal", 5: "greater",
	6: "not-equal", 7: "greater-equal", 8: "always" };
const BLEND_FACTOR = { 1: "zero", 2: "one", 3: "src", 4: "one-minus-src", 5: "src-alpha",
	6: "one-minus-src-alpha", 7: "dst", 8: "one-minus-dst", 9: "dst-alpha", 10: "one-minus-dst-alpha" };
const BLEND_OP = { 1: "add", 2: "subtract", 3: "reverse-subtract", 4: "min", 5: "max" };
const BUFFER_BINDING = { 2: "uniform", 3: "storage", 4: "read-only-storage" };
const SAMPLER_BINDING = { 2: "filtering", 3: "non-filtering", 4: "comparison" };
const TEX_SAMPLE = { 2: "float", 3: "unfilterable-float", 4: "depth", 5: "sint", 6: "uint" };
const STORAGE_ACCESS = { 2: "write-only", 3: "read-only", 4: "read-write" };
const ALPHA_MODE = { 0: "opaque", 1: "opaque", 2: "premultiplied", 3: "premultiplied", 4: "opaque" };

const WHOLE = 0xFFFFFFFF; // low+high both all-ones -> WGPU_WHOLE_SIZE

// ---- shared control block layout (ctrl SharedArrayBuffer) ---------------------------------
// i32 header + a BigInt64 args area (holds i32 and i64 args losslessly).
const LOCK = 0, STATUS = 1, FUNC = 2, NARG = 3, RET = 4; // Int32 slots
const ARGS_OFF = 32, ARGS_MAX = 16;                      // BigInt64 args at byte 32
const ST_IDLE = 0, ST_REQ = 1, ST_DONE = 2;
export const GPU_CTRL_BYTES = ARGS_OFF + ARGS_MAX * 8;

// Function id table: the broker dispatches by index; thunks marshal by the same index. Real
// wgpu functions plus two broker-internal ops ($getDevice returns the device handle for the
// RequestDevice callback the thunk fires locally; $setMapped registers a thunk-malloc'd
// pointer as a buffer's mapped range so the broker can flush it on unmap).
export const BROKER_FUNCS = [
	"$getDevice", "$setMapped",
	"wgpuCreateInstance", "wgpuInstanceRelease", "wgpuInstanceProcessEvents", "wgpuSetLogLevel",
	"wgpuSetLogCallback", "wgpuInstanceEnumerateAdapters", "wgpuInstanceCreateSurface",
	"wgpuAdapterGetInfo", "wgpuAdapterInfoFreeMembers", "wgpuAdapterGetLimits", "wgpuAdapterGetFeatures",
	"wgpuSupportedFeaturesFreeMembers", "wgpuAdapterAddRef", "wgpuAdapterRelease",
	"wgpuDeviceGetQueue", "wgpuDeviceGetLimits", "wgpuDeviceGetFeatures", "wgpuDeviceRelease",
	"wgpuDeviceCreateShaderModule", "wgpuDeviceCreateBuffer", "wgpuDeviceCreateTexture",
	"wgpuDeviceCreateSampler", "wgpuDeviceCreateBindGroupLayout", "wgpuDeviceCreateBindGroup",
	"wgpuDeviceCreatePipelineLayout", "wgpuDeviceCreateRenderPipeline", "wgpuDeviceCreateComputePipeline",
	"wgpuDeviceCreateCommandEncoder", "wgpuQueueSubmit", "wgpuQueueWriteBuffer", "wgpuQueueWriteTexture",
	"wgpuQueueRelease", "wgpuBufferUnmap", "wgpuBufferRelease", "wgpuTextureCreateView",
	"wgpuTextureRelease", "wgpuTextureViewRelease", "wgpuSamplerRelease", "wgpuShaderModuleRelease",
	"wgpuBindGroupRelease", "wgpuBindGroupLayoutAddRef", "wgpuBindGroupLayoutRelease",
	"wgpuPipelineLayoutRelease", "wgpuRenderPipelineRelease", "wgpuComputePipelineRelease",
	"wgpuCommandEncoderBeginRenderPass", "wgpuCommandEncoderBeginComputePass",
	"wgpuCommandEncoderCopyTextureToBuffer", "wgpuCommandEncoderFinish", "wgpuCommandEncoderRelease",
	"wgpuCommandBufferRelease", "wgpuRenderPassEncoderSetPipeline", "wgpuRenderPassEncoderSetBindGroup",
	"wgpuRenderPassEncoderSetIndexBuffer", "wgpuRenderPassEncoderSetScissorRect",
	"wgpuRenderPassEncoderDraw", "wgpuRenderPassEncoderDrawIndexed", "wgpuRenderPassEncoderEnd",
	"wgpuRenderPassEncoderRelease", "wgpuComputePassEncoderSetPipeline", "wgpuComputePassEncoderSetBindGroup",
	"wgpuComputePassEncoderDispatchWorkgroups", "wgpuComputePassEncoderEnd", "wgpuComputePassEncoderRelease",
	"wgpuSurfaceGetCapabilities", "wgpuSurfaceCapabilitiesFreeMembers", "wgpuSurfaceConfigure",
	"wgpuSurfaceUnconfigure", "wgpuSurfaceGetCurrentTexture", "wgpuSurfacePresent", "wgpuSurfaceRelease",
];
const FUNC_ID = Object.fromEntries(BROKER_FUNCS.map((n, i) => [n, i]));

// ---- memory codec -------------------------------------------------------------------------
export function makeCodec(memory) {
	const dv = () => new DataView(memory.buffer);
	const u8 = () => new Uint8Array(memory.buffer);
	const u32 = (p) => dv().getUint32(p, true);
	const i32 = (p) => dv().getInt32(p, true);
	const f32 = (p) => dv().getFloat32(p, true);
	const f64 = (p) => dv().getFloat64(p, true);
	const u64 = (p) => { const lo = u32(p), hi = u32(p + 4); return hi * 0x100000000 + lo; };
	const isWhole = (p) => u32(p) === WHOLE && u32(p + 4) === WHOLE;
	const set32 = (p, v) => dv().setUint32(p, v >>> 0, true);
	const set64 = (p, v) => { dv().setUint32(p, v >>> 0, true); dv().setUint32(p + 4, Math.floor(v / 0x100000000) >>> 0, true); };
	const bytes = (p, n) => u8().slice(p, p + n);
	const strView = (p) => {
		const data = u32(p), len = u32(p + 4);
		if (!data) return "";
		if (len === 0xffffffff) { let e = data; const b = u8(); while (b[e]) e++; return new TextDecoder().decode(u8().slice(data, e)); }
		return new TextDecoder().decode(u8().slice(data, data + len));
	};
	return { dv, u8, u32, i32, f32, f64, u64, isWhole, set32, set64, bytes, strView };
}

// ---- handle table: integer id <-> JS GPU object (lives in the broker) ---------------------
export class Handles {
	constructor() { this.m = new Map(); this.n = 1; }
	add(o) { if (o == null) return 0; const id = this.n++; this.m.set(id, o); return id; }
	get(h) { return h ? this.m.get(h) : null; }
	release(h) { if (h) this.m.delete(h); }
}

// ---- async bootstrap: resolve adapter/device/queue/context on the broker up front ---------
export async function bootstrapGpu(canvas) {
	if (!navigator.gpu) throw new Error("WebGPU not available (navigator.gpu is undefined)");
	const adapter = await navigator.gpu.requestAdapter();
	if (!adapter) throw new Error("no WebGPU adapter");
	const device = await adapter.requestDevice();
	const context = canvas.getContext("webgpu");
	const format = navigator.gpu.getPreferredCanvasFormat();
	return { adapter, device, queue: device.queue, context, format, canvas };
}

// ==========================================================================================
// Broker side: the dispatch table the gpu-worker runs. scratchPtr is a small wasm-memory
// region (allocated by the engine worker, which has a TLS-valid malloc) used for the handful
// of out-params that need to point at broker-produced arrays (surface capabilities).
// ==========================================================================================
export function makeBrokerTable({ memory, gpu, scratchPtr }) {
	const H = new Handles();
	const c = makeCodec(memory);
	const mapped = new Map(); // buffer handle -> { buf, ptr, size } (mappedAtCreation upload)

	// Presentation bridge: the engine renders whenever (off its own clock), but a transferred
	// OffscreenCanvas only shows its WebGPU output at the worker's animation-frame boundary. So
	// the engine "swapchain image" is an offscreen texture it renders into, and a rAF loop in
	// gpu-broker.mjs blits that into the real canvas each frame (see $present).
	let presentTex = null, presentW = 0, presentH = 0;

	function decodeBlend(p) {
		return {
			color: { operation: BLEND_OP[c.u32(p)] || "add", srcFactor: BLEND_FACTOR[c.u32(p + 4)] || "one", dstFactor: BLEND_FACTOR[c.u32(p + 8)] || "zero" },
			alpha: { operation: BLEND_OP[c.u32(p + 12)] || "add", srcFactor: BLEND_FACTOR[c.u32(p + 16)] || "one", dstFactor: BLEND_FACTOR[c.u32(p + 20)] || "zero" },
		};
	}
	function decodePipeline(p) {
		const layoutH = c.u32(p + 12);
		const desc = {
			layout: layoutH ? H.get(layoutH) : "auto",
			vertex: { module: H.get(c.u32(p + 20)), entryPoint: c.strView(p + 24) || undefined },
			primitive: {
				topology: TOPOLOGY[c.u32(p + 52)] || "triangle-list",
				stripIndexFormat: INDEX_FORMAT[c.u32(p + 56)],
				frontFace: FRONT_FACE[c.u32(p + 60)] || "ccw",
				cullMode: CULL_MODE[c.u32(p + 64)] || "none",
			},
			multisample: { count: c.u32(p + 80) || 1, mask: c.u32(p + 84) || 0xFFFFFFFF, alphaToCoverageEnabled: !!c.u32(p + 88) },
		};
		const dsPtr = c.u32(p + 72);
		if (dsPtr) {
			const dw = c.u32(dsPtr + 8);
			desc.depthStencil = {
				format: TEXTURE_FORMAT[c.u32(dsPtr + 4)],
				depthWriteEnabled: dw === 2 ? undefined : !!dw,
				depthCompare: COMPARE[c.u32(dsPtr + 12)] || "always",
				depthBias: c.i32(dsPtr + 56), depthBiasSlopeScale: c.f32(dsPtr + 60), depthBiasClamp: c.f32(dsPtr + 64),
			};
		}
		const fragPtr = c.u32(p + 92);
		if (fragPtr) {
			const targetCount = c.u32(fragPtr + 24), targetsPtr = c.u32(fragPtr + 28), targets = [];
			for (let i = 0; i < targetCount; i++) {
				const t = targetsPtr + i * 16, blendPtr = c.u32(t + 8);
				const tgt = { format: TEXTURE_FORMAT[c.u32(t + 4)] || gpu.format, writeMask: c.u32(t + 12) };
				if (blendPtr) tgt.blend = decodeBlend(blendPtr);
				targets.push(tgt);
			}
			desc.fragment = { module: H.get(c.u32(fragPtr + 4)), entryPoint: c.strView(fragPtr + 8) || undefined, targets };
		}
		return desc;
	}
	function decodeRenderPass(p) {
		const count = c.u32(p + 12), attsPtr = c.u32(p + 16), colorAttachments = [];
		for (let i = 0; i < count; i++) {
			const b = attsPtr + i * 56;
			colorAttachments.push({
				view: H.get(c.u32(b + 4)),
				resolveTarget: H.get(c.u32(b + 12)) || undefined,
				loadOp: LOAD_OP[c.u32(b + 16)] || "clear",
				storeOp: STORE_OP[c.u32(b + 20)] || "store",
				clearValue: { r: c.f64(b + 24), g: c.f64(b + 32), b: c.f64(b + 40), a: c.f64(b + 48) },
			});
		}
		const rp = { colorAttachments };
		const dsPtr = c.u32(p + 20);
		if (dsPtr) {
			const d = {
				view: H.get(c.u32(dsPtr + 0)),
				depthLoadOp: LOAD_OP[c.u32(dsPtr + 4)], depthStoreOp: STORE_OP[c.u32(dsPtr + 8)],
				depthClearValue: c.f32(dsPtr + 12), depthReadOnly: !!c.u32(dsPtr + 16),
			};
			const slo = c.u32(dsPtr + 20);
			if (slo) {
				d.stencilLoadOp = LOAD_OP[slo]; d.stencilStoreOp = STORE_OP[c.u32(dsPtr + 24)];
				d.stencilClearValue = c.u32(dsPtr + 28); d.stencilReadOnly = !!c.u32(dsPtr + 32);
			}
			rp.depthStencilAttachment = d;
		}
		return rp;
	}
	// WGPUBindGroupLayoutEntry (stride 80, 8-aligned): binding@4, visibility@8, then four
	// sub-layouts. WGPUBufferBindingLayout is 8-aligned (u64 minBindingSize) so `buffer` starts
	// at @16 (padded), pushing every following field: buffer.type@20, hasDynamicOffset@24,
	// minBindingSize@32; sampler.type@44; texture{sampleType@52,viewDim@56,multisampled@60};
	// storageTexture{access@68,format@72,viewDim@76}.
	function decodeBGLEntry(p) {
		const e = { binding: c.u32(p + 4), visibility: c.u32(p + 8) };
		const bufType = c.u32(p + 20), samType = c.u32(p + 44), texSample = c.u32(p + 52), stAccess = c.u32(p + 68);
		if (bufType) e.buffer = { type: BUFFER_BINDING[bufType], hasDynamicOffset: !!c.u32(p + 24), minBindingSize: c.u64(p + 32) };
		else if (samType) e.sampler = { type: SAMPLER_BINDING[samType] };
		else if (texSample) e.texture = { sampleType: TEX_SAMPLE[texSample], viewDimension: VIEW_DIM[c.u32(p + 56)] || "2d", multisampled: !!c.u32(p + 60) };
		else if (stAccess) e.storageTexture = { access: STORAGE_ACCESS[stAccess], format: TEXTURE_FORMAT[c.u32(p + 72)], viewDimension: VIEW_DIM[c.u32(p + 76)] || "2d" };
		return e;
	}
	function decodeBGEntry(p) {
		const binding = c.u32(p + 4), bufH = c.u32(p + 8), samH = c.u32(p + 32), viewH = c.u32(p + 36);
		if (bufH) {
			const r = { buffer: H.get(bufH), offset: c.u64(p + 16) };
			if (!c.isWhole(p + 24)) { const s = c.u64(p + 24); if (s) r.size = s; }
			return { binding, resource: r };
		}
		if (samH) return { binding, resource: H.get(samH) };
		return { binding, resource: H.get(viewH) };
	}
	function fillLimits(p, lim) {
		const g = (k, d) => (lim && lim[k] != null ? lim[k] : d);
		c.set32(p + 4, g("maxTextureDimension1D", 8192)); c.set32(p + 8, g("maxTextureDimension2D", 8192));
		c.set32(p + 12, g("maxTextureDimension3D", 2048)); c.set32(p + 16, g("maxTextureArrayLayers", 256));
		c.set32(p + 20, g("maxBindGroups", 4)); c.set32(p + 24, g("maxBindGroupsPlusVertexBuffers", 24));
		c.set32(p + 28, g("maxBindingsPerBindGroup", 1000)); c.set32(p + 32, g("maxDynamicUniformBuffersPerPipelineLayout", 8));
		c.set32(p + 36, g("maxDynamicStorageBuffersPerPipelineLayout", 4)); c.set32(p + 40, g("maxSampledTexturesPerShaderStage", 16));
		c.set32(p + 44, g("maxSamplersPerShaderStage", 16)); c.set32(p + 48, g("maxStorageBuffersPerShaderStage", 8));
		c.set32(p + 52, g("maxStorageTexturesPerShaderStage", 4)); c.set32(p + 56, g("maxUniformBuffersPerShaderStage", 12));
		c.set64(p + 64, g("maxUniformBufferBindingSize", 65536)); c.set64(p + 72, g("maxStorageBufferBindingSize", 134217728));
		c.set32(p + 80, g("minUniformBufferOffsetAlignment", 256)); c.set32(p + 84, g("minStorageBufferOffsetAlignment", 256));
		c.set32(p + 88, g("maxVertexBuffers", 8)); c.set64(p + 96, g("maxBufferSize", 268435456));
		c.set32(p + 104, g("maxVertexAttributes", 16)); c.set32(p + 108, g("maxVertexBufferArrayStride", 2048));
		c.set32(p + 112, g("maxInterStageShaderVariables", 16)); c.set32(p + 116, g("maxColorAttachments", 8));
		c.set32(p + 120, g("maxColorAttachmentBytesPerSample", 32)); c.set32(p + 124, g("maxComputeWorkgroupStorageSize", 16384));
		c.set32(p + 128, g("maxComputeInvocationsPerWorkgroup", 256)); c.set32(p + 132, g("maxComputeWorkgroupSizeX", 256));
		c.set32(p + 136, g("maxComputeWorkgroupSizeY", 256)); c.set32(p + 140, g("maxComputeWorkgroupSizeZ", 64));
		c.set32(p + 144, g("maxComputeWorkgroupsPerDimension", 65535));
	}

	return {
		// broker-internal ops
		$getDevice: () => H.add(gpu.device),
		$setMapped: (bufH, ptr, size) => { const buf = H.get(bufH); if (buf) mapped.set(bufH, { buf, ptr, size }); },
		// Called from the rAF loop: blit the engine's latest offscreen render into the canvas.
		$present: () => {
			if (!presentTex) return "no-presentTex";
			const enc = gpu.device.createCommandEncoder();
			enc.copyTextureToTexture({ texture: presentTex }, { texture: gpu.context.getCurrentTexture() },
				{ width: presentW, height: presentH, depthOrArrayLayers: 1 });
			gpu.queue.submit([enc.finish()]);
			return "blit " + presentW + "x" + presentH;
		},

		wgpuCreateInstance: () => H.add({ instance: true }),
		wgpuInstanceRelease: (h) => H.release(h),
		wgpuInstanceProcessEvents: () => {},
		wgpuSetLogLevel: () => {},
		wgpuSetLogCallback: () => {},
		wgpuInstanceEnumerateAdapters: (_inst, _opts, adaptersPtr) => { if (adaptersPtr) c.set32(adaptersPtr, H.add(gpu.adapter)); return 1; },
		wgpuInstanceCreateSurface: () => H.add({ surface: true }),
		// The engine polls this (wgpu-native model) to wait for a submission to finish before
		// presenting/recycling. WebGPU completion is async, but the browser keeps GPU objects
		// alive by refcount until the work truly finishes, so reporting "queue drained" each
		// poll is safe and lets the frame loop advance.
		wgpuDevicePoll: () => 1,

		wgpuAdapterGetInfo: (_ad, infoPtr) => {
			for (let o = 4; o <= 28; o += 8) { c.set32(infoPtr + o, 0); c.set32(infoPtr + o + 4, 0); }
			c.set32(infoPtr + 36, 2); c.set32(infoPtr + 40, 2); c.set32(infoPtr + 44, 0); c.set32(infoPtr + 48, 0);
			return 1;
		},
		wgpuAdapterInfoFreeMembers: () => {},
		wgpuAdapterGetLimits: (_ad, limPtr) => { fillLimits(limPtr, gpu.adapter.limits); return 1; },
		wgpuAdapterGetFeatures: (_ad, featPtr) => { c.set32(featPtr, 0); c.set32(featPtr + 4, 0); },
		wgpuSupportedFeaturesFreeMembers: () => {},
		wgpuAdapterAddRef: () => {},
		wgpuAdapterRelease: (h) => H.release(h),

		wgpuDeviceGetQueue: () => H.add(gpu.queue),
		wgpuDeviceGetLimits: (_dev, limPtr) => { fillLimits(limPtr, gpu.device.limits); return 1; },
		wgpuDeviceGetFeatures: (_dev, featPtr) => { c.set32(featPtr, 0); c.set32(featPtr + 4, 0); },
		wgpuDeviceRelease: (h) => H.release(h),

		wgpuDeviceCreateShaderModule: (_dev, descPtr) => {
			const chain = c.u32(descPtr), code = chain ? c.strView(chain + 8) : "";
			return H.add(gpu.device.createShaderModule({ code }));
		},
		wgpuDeviceCreateBuffer: (_dev, descPtr) => {
			const usage = c.u32(descPtr + 12), size = c.u64(descPtr + 16), mappedAtCreation = !!c.u32(descPtr + 24);
			return H.add(gpu.device.createBuffer({ size, usage, mappedAtCreation }));
		},
		wgpuDeviceCreateTexture: (_dev, descPtr) => H.add(gpu.device.createTexture({
			usage: c.u32(descPtr + 12), dimension: TEX_DIM[c.u32(descPtr + 16)] || "2d",
			size: { width: c.u32(descPtr + 20), height: c.u32(descPtr + 24), depthOrArrayLayers: c.u32(descPtr + 28) || 1 },
			format: TEXTURE_FORMAT[c.u32(descPtr + 32)], mipLevelCount: c.u32(descPtr + 36) || 1, sampleCount: c.u32(descPtr + 40) || 1,
		})),
		wgpuDeviceCreateSampler: (_dev, descPtr) => {
			const desc = {
				addressModeU: ADDRESS[c.u32(descPtr + 12)] || "clamp-to-edge", addressModeV: ADDRESS[c.u32(descPtr + 16)] || "clamp-to-edge",
				addressModeW: ADDRESS[c.u32(descPtr + 20)] || "clamp-to-edge", magFilter: FILTER[c.u32(descPtr + 24)] || "nearest",
				minFilter: FILTER[c.u32(descPtr + 28)] || "nearest", mipmapFilter: FILTER[c.u32(descPtr + 32)] || "nearest",
				lodMinClamp: c.f32(descPtr + 36), lodMaxClamp: c.f32(descPtr + 40) || 32, maxAnisotropy: c.dv().getUint16(descPtr + 48, true) || 1,
			};
			const cmp = c.u32(descPtr + 44); if (cmp) desc.compare = COMPARE[cmp];
			return H.add(gpu.device.createSampler(desc));
		},
		wgpuDeviceCreateBindGroupLayout: (_dev, descPtr) => {
			const count = c.u32(descPtr + 12), ptr = c.u32(descPtr + 16), entries = [];
			for (let i = 0; i < count; i++) entries.push(decodeBGLEntry(ptr + i * 80));
			return H.add(gpu.device.createBindGroupLayout({ entries }));
		},
		wgpuDeviceCreateBindGroup: (_dev, descPtr) => {
			const layout = H.get(c.u32(descPtr + 12)), count = c.u32(descPtr + 16), ptr = c.u32(descPtr + 20), entries = [];
			for (let i = 0; i < count; i++) entries.push(decodeBGEntry(ptr + i * 40));
			return H.add(gpu.device.createBindGroup({ layout, entries }));
		},
		wgpuDeviceCreatePipelineLayout: (_dev, descPtr) => {
			const count = c.u32(descPtr + 12), ptr = c.u32(descPtr + 16), bindGroupLayouts = [];
			for (let i = 0; i < count; i++) bindGroupLayouts.push(H.get(c.u32(ptr + i * 4)));
			return H.add(gpu.device.createPipelineLayout({ bindGroupLayouts }));
		},
		wgpuDeviceCreateRenderPipeline: (_dev, descPtr) => H.add(gpu.device.createRenderPipeline(decodePipeline(descPtr))),
		wgpuDeviceCreateComputePipeline: (_dev, descPtr) => {
			const layoutH = c.u32(descPtr + 12);
			return H.add(gpu.device.createComputePipeline({
				layout: layoutH ? H.get(layoutH) : "auto",
				compute: { module: H.get(c.u32(descPtr + 20)), entryPoint: c.strView(descPtr + 24) || undefined },
			}));
		},
		wgpuDeviceCreateCommandEncoder: () => H.add(gpu.device.createCommandEncoder()),

		wgpuQueueSubmit: (queue, count, cmdsPtr) => {
			const cmds = []; for (let i = 0; i < count; i++) cmds.push(H.get(c.u32(cmdsPtr + i * 4)));
			H.get(queue).submit(cmds);
		},
		wgpuQueueWriteBuffer: (queue, buffer, offset, dataPtr, size) => H.get(queue).writeBuffer(H.get(buffer), Number(offset), c.bytes(dataPtr, size)),
		wgpuQueueWriteTexture: (queue, destPtr, dataPtr, dataSize, layoutPtr, extentPtr) => {
			const dest = { texture: H.get(c.u32(destPtr)), mipLevel: c.u32(destPtr + 4),
				origin: { x: c.u32(destPtr + 8), y: c.u32(destPtr + 12), z: c.u32(destPtr + 16) }, aspect: ASPECT[c.u32(destPtr + 20)] || "all" };
			const layout = { offset: c.u64(layoutPtr), bytesPerRow: c.u32(layoutPtr + 8), rowsPerImage: c.u32(layoutPtr + 12) || undefined };
			const extent = { width: c.u32(extentPtr), height: c.u32(extentPtr + 4), depthOrArrayLayers: c.u32(extentPtr + 8) || 1 };
			H.get(queue).writeTexture(dest, c.bytes(dataPtr, dataSize), layout, extent);
		},
		wgpuQueueRelease: (h) => H.release(h),

		wgpuBufferUnmap: (buffer) => {
			const m = mapped.get(buffer);
			if (m) {
				new Uint8Array(m.buf.getMappedRange(0, m.size)).set(c.bytes(m.ptr, m.size)); m.buf.unmap(); mapped.delete(buffer);
			}
			else { const b = H.get(buffer); if (b) b.unmap(); }
		},
		wgpuBufferRelease: (h) => { mapped.delete(h); H.release(h); },

		wgpuTextureCreateView: (tex, descPtr) => {
			if (!descPtr) return H.add(H.get(tex).createView());
			const desc = {};
			const fmt = c.u32(descPtr + 12); if (fmt) desc.format = TEXTURE_FORMAT[fmt];
			const dim = c.u32(descPtr + 16); if (dim) desc.dimension = VIEW_DIM[dim];
			desc.baseMipLevel = c.u32(descPtr + 20);
			const mlc = c.u32(descPtr + 24); if (mlc) desc.mipLevelCount = mlc;
			desc.baseArrayLayer = c.u32(descPtr + 28);
			const alc = c.u32(descPtr + 32); if (alc) desc.arrayLayerCount = alc;
			const asp = c.u32(descPtr + 36); if (asp) desc.aspect = ASPECT[asp];
			return H.add(H.get(tex).createView(desc));
		},
		wgpuTextureRelease: (h) => H.release(h),
		wgpuTextureViewRelease: (h) => H.release(h),
		wgpuSamplerRelease: (h) => H.release(h),
		wgpuShaderModuleRelease: (h) => H.release(h),
		wgpuBindGroupRelease: (h) => H.release(h),
		wgpuBindGroupLayoutAddRef: () => {},
		wgpuBindGroupLayoutRelease: (h) => H.release(h),
		wgpuPipelineLayoutRelease: (h) => H.release(h),
		wgpuRenderPipelineRelease: (h) => H.release(h),
		wgpuComputePipelineRelease: (h) => H.release(h),

		wgpuCommandEncoderBeginRenderPass: (enc, descPtr) => H.add(H.get(enc).beginRenderPass(decodeRenderPass(descPtr))),
		wgpuCommandEncoderBeginComputePass: (enc) => H.add(H.get(enc).beginComputePass()),
		wgpuCommandEncoderCopyTextureToBuffer: (enc, srcPtr, dstPtr, extentPtr) => {
			const src = { texture: H.get(c.u32(srcPtr)), mipLevel: c.u32(srcPtr + 4),
				origin: { x: c.u32(srcPtr + 8), y: c.u32(srcPtr + 12), z: c.u32(srcPtr + 16) }, aspect: ASPECT[c.u32(srcPtr + 20)] || "all" };
			const dst = { buffer: H.get(c.u32(dstPtr + 16)), offset: c.u64(dstPtr), bytesPerRow: c.u32(dstPtr + 8), rowsPerImage: c.u32(dstPtr + 12) || undefined };
			const extent = { width: c.u32(extentPtr), height: c.u32(extentPtr + 4), depthOrArrayLayers: c.u32(extentPtr + 8) || 1 };
			H.get(enc).copyTextureToBuffer(src, dst, extent);
		},
		wgpuCommandEncoderFinish: (enc) => H.add(H.get(enc).finish()),
		wgpuCommandEncoderRelease: (h) => H.release(h),
		wgpuCommandBufferRelease: (h) => H.release(h),

		wgpuRenderPassEncoderSetPipeline: (pass, pipe) => H.get(pass).setPipeline(H.get(pipe)),
		wgpuRenderPassEncoderSetBindGroup: (pass, index, group, offCount, offPtr) => {
			const offs = []; for (let i = 0; i < offCount; i++) offs.push(c.u32(offPtr + i * 4));
			H.get(pass).setBindGroup(index, H.get(group), offs);
		},
		wgpuRenderPassEncoderSetIndexBuffer: (pass, buffer, format, offset, size) =>
			H.get(pass).setIndexBuffer(H.get(buffer), INDEX_FORMAT[format] || "uint16", Number(offset), size ? Number(size) : undefined),
		wgpuRenderPassEncoderSetScissorRect: (pass, x, y, w, h) => H.get(pass).setScissorRect(x, y, w, h),
		wgpuRenderPassEncoderDraw: (pass, vc, ic, fv, fi) => H.get(pass).draw(vc, ic, fv, fi),
		wgpuRenderPassEncoderDrawIndexed: (pass, ic, inst, fi, bv, finst) => H.get(pass).drawIndexed(ic, inst, fi, bv, finst),
		wgpuRenderPassEncoderEnd: (pass) => H.get(pass).end(),
		wgpuRenderPassEncoderRelease: (h) => H.release(h),

		wgpuComputePassEncoderSetPipeline: (pass, pipe) => H.get(pass).setPipeline(H.get(pipe)),
		wgpuComputePassEncoderSetBindGroup: (pass, index, group, offCount, offPtr) => {
			const offs = []; for (let i = 0; i < offCount; i++) offs.push(c.u32(offPtr + i * 4));
			H.get(pass).setBindGroup(index, H.get(group), offs);
		},
		wgpuComputePassEncoderDispatchWorkgroups: (pass, x, y, z) => H.get(pass).dispatchWorkgroups(x, y, z),
		wgpuComputePassEncoderEnd: (pass) => H.get(pass).end(),
		wgpuComputePassEncoderRelease: (h) => H.release(h),

		wgpuSurfaceGetCapabilities: (_surf, _adapter, capsPtr) => {
			// The 2d renderer builds its pipelines for rgba8unorm; advertise that as the surface
			// format (always canvas-compatible) so pipeline + swapchain + blit all agree. The device
			// prefers bgra, so the context incurs one extra internal copy at present — a benign perf
			// warning we accept until the 2d pipelines can be built against the preferred format.
			// point the caps arrays at the shared scratch region (persistent; FreeMembers no-ops)
			c.set32(scratchPtr, 18 /*rgba8unorm*/);
			c.set32(scratchPtr + 4, 1 /*fifo*/); c.set32(scratchPtr + 8, 1 /*opaque*/);
			c.set32(capsPtr + 4, 0x13);
			c.set32(capsPtr + 8, 1); c.set32(capsPtr + 12, scratchPtr);
			c.set32(capsPtr + 16, 1); c.set32(capsPtr + 20, scratchPtr + 4);
			c.set32(capsPtr + 24, 1); c.set32(capsPtr + 28, scratchPtr + 8);
			return 1;
		},
		wgpuSurfaceCapabilitiesFreeMembers: () => {},
		wgpuSurfaceConfigure: (_surf, cfgPtr) => {
			const width = c.u32(cfgPtr + 16), height = c.u32(cfgPtr + 20);
			const format = TEXTURE_FORMAT[c.u32(cfgPtr + 8)] || gpu.format;
			if (gpu.canvas) { gpu.canvas.width = width; gpu.canvas.height = height; }
			// The canvas is the blit target (CopyDst); the engine renders into presentTex.
			gpu.context.configure({ device: gpu.device, format, usage: 0x12 /*RenderAttachment|CopyDst*/,
				alphaMode: ALPHA_MODE[c.u32(cfgPtr + 32)] || "opaque" });
			presentW = width; presentH = height;
			presentTex = gpu.device.createTexture({ size: { width, height }, format,
				usage: 0x11 /*RenderAttachment|CopySrc*/ });
		},
		wgpuSurfaceUnconfigure: () => { try { gpu.context.unconfigure(); } catch {} presentTex = null; },
		wgpuSurfaceGetCurrentTexture: (_surf, outPtr) => {
			// hand the engine the offscreen present texture, not the live canvas texture
			c.set32(outPtr + 4, H.add(presentTex)); c.set32(outPtr + 8, 1 /*SuccessOptimal*/);
		},
		wgpuSurfacePresent: () => 1,
		wgpuSurfaceRelease: (h) => H.release(h),
	};
}

// ==========================================================================================
// gpu-worker pump loop: service marshalled calls off the control block. Non-blocking
// (Atomics.waitAsync) so this worker's event loop stays free — required, because a broker that
// blocked in Atomics.wait could never spawn/settle its own async work.
// ==========================================================================================
export async function runBroker({ ctrl, table, onError }) {
	const ci = new Int32Array(ctrl);
	const ba = new BigInt64Array(ctrl, ARGS_OFF, ARGS_MAX);
	for (;;) {
		const s = Atomics.load(ci, STATUS);
		if (s !== ST_REQ) { const w = Atomics.waitAsync(ci, STATUS, s); if (w.async) await w.value; continue; }
		const name = BROKER_FUNCS[Atomics.load(ci, FUNC)], n = Atomics.load(ci, NARG), args = [];
		for (let i = 0; i < n; i++) args.push(Number(ba[i]));
		let ret = 0;
		try { const r = table[name](...args); if (typeof r === "number") ret = r | 0; }
		catch (e) { onError?.(name + ": " + ((e && e.stack) || e)); }
		Atomics.store(ci, RET, ret);
		Atomics.store(ci, STATUS, ST_DONE);
		Atomics.notify(ci, STATUS);
	}
}

// ==========================================================================================
// Thunk side: the `wgpu` import table every wasm worker instantiates against. Each function
// marshals to the broker; the few that need this instance's own indirect table (C callbacks)
// or TLS-valid malloc are done locally.
// ==========================================================================================
export function makeWebgpuThunks({ memory, ctrl, getTable, getExports }) {
	const ci = new Int32Array(ctrl);
	const ba = new BigInt64Array(ctrl, ARGS_OFF, ARGS_MAX);
	const c = makeCodec(memory);
	const call = (fn, ...a) => getTable().get(fn)(...a);
	const malloc = (n) => getExports().malloc(n);
	const mappedPtr = new Map(); // buffer handle -> local wasm ptr (freed on unmap)
	let svScratch = 0;
	const emptySV = () => { if (!svScratch) { svScratch = malloc(8); c.set32(svScratch, 0); c.set32(svScratch + 4, 0); } return svScratch; };

	// synchronous marshalled call: one caller at a time (LOCK), block until the broker answers.
	function broker(funcId, args) {
		while (Atomics.compareExchange(ci, LOCK, 0, 1) !== 0) { Atomics.wait(ci, LOCK, 1); }
		Atomics.store(ci, FUNC, funcId);
		Atomics.store(ci, NARG, args.length);
		for (let i = 0; i < args.length; i++) { const a = args[i]; ba[i] = typeof a === "bigint" ? a : BigInt(a | 0); }
		Atomics.store(ci, STATUS, ST_REQ);
		Atomics.notify(ci, STATUS);
		while (Atomics.load(ci, STATUS) !== ST_DONE) { Atomics.wait(ci, STATUS, ST_REQ); }
		const ret = Atomics.load(ci, RET);
		Atomics.store(ci, STATUS, ST_IDLE);
		Atomics.store(ci, LOCK, 0);
		Atomics.notify(ci, LOCK);
		return ret;
	}

	const wgpu = {};
	// Every broker-side function becomes a thin marshalling thunk.
	for (const name of BROKER_FUNCS) {
		if (name[0] === "$") continue;
		const id = FUNC_ID[name];
		wgpu[name] = (...args) => broker(id, args);
	}

	// --- locally-handled functions (override the generic thunks) ---------------------------
	// Device request: fetch the broker's device handle, then fire the C callback on THIS
	// worker (its indirect table). cbInfo: {..., callback@8, ud1@12, ud2@16}.
	wgpu.wgpuAdapterRequestDevice = (_ad, _desc, cbInfoPtr) => {
		const dev = broker(FUNC_ID.$getDevice, []);
		const cb = c.u32(cbInfoPtr + 8), ud1 = c.u32(cbInfoPtr + 12), ud2 = c.u32(cbInfoPtr + 16);
		call(cb, 1 /*Success*/, dev, emptySV(), ud1, ud2);
	};
	// Submitted-work-done + poll: the engine registers a completion callback (AllowProcessEvents
	// mode) then spins wgpuDevicePoll waiting for it — that is how fences (Fence::arm) and
	// waitIdle signal. So DON'T fire on registration; queue it, and deliver on the next poll.
	// We can't truly wait on the GPU from here, but marshalled submits are already ordered on
	// the queue, so delivering on poll is the right shape for the frame loop.
	const pendingWorkDone = [];
	wgpu.wgpuQueueOnSubmittedWorkDone = (_queue, cbInfoPtr) => {
		pendingWorkDone.push([c.u32(cbInfoPtr + 8), c.u32(cbInfoPtr + 12), c.u32(cbInfoPtr + 16)]);
	};
	wgpu.wgpuDevicePoll = (_dev, _wait, _wsi) => {
		while (pendingWorkDone.length) { const [cb, ud1, ud2] = pendingWorkDone.shift(); call(cb, 1 /*Success*/, emptySV(), ud1, ud2); }
		return 1;
	};
	// Mapped range (mappedAtCreation uploads): malloc here (TLS-valid), register the pointer
	// with the broker so its unmap flushes our bytes into the GPU buffer.
	const getRange = (buffer, offset, size) => {
		let ptr = mappedPtr.get(buffer);
		if (!ptr) { ptr = malloc(size || 4); mappedPtr.set(buffer, ptr); broker(FUNC_ID.$setMapped, [buffer, ptr, size || 4]); }
		return ptr + (offset | 0);
	};
	wgpu.wgpuBufferGetMappedRange = getRange;
	wgpu.wgpuBufferGetConstMappedRange = getRange;
	wgpu.wgpuBufferUnmap = (buffer) => {
		broker(FUNC_ID.wgpuBufferUnmap, [buffer]); // broker flushes + unmaps
		const ptr = mappedPtr.get(buffer); if (ptr) { getExports().free(ptr); mappedPtr.delete(buffer); }
	};
	// Async map (readback / screenshots): not wired through the broker yet — report cancelled so
	// the engine doesn't wait forever. (Milestone: the render path uses writeBuffer + mapped
	// creation, not read-back mapping.)
	wgpu.wgpuBufferMapAsync = (_buffer, _mode, _offset, _size, cbInfoPtr) => {
		const cb = c.u32(cbInfoPtr + 8), ud1 = c.u32(cbInfoPtr + 12), ud2 = c.u32(cbInfoPtr + 16);
		call(cb, 2 /*CallbackCancelled*/, emptySV(), ud1, ud2);
	};
	return wgpu;
}
