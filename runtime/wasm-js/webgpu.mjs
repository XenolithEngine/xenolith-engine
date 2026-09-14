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
// ABI: the contract header (xenolith/backend/webgpu/webgpu/webgpu.h) uses real C types —
// handles are opaque-struct POINTERS, lengths are size_t, callbacks are function pointers —
// so the byte layout follows the module's address size. wasm32: pointer-sized fields are 4B,
// WGPUStringView = {ptr,len} (8B). wasm64 (LP64): every pointer-sized field is 8B, StringView
// 16B, u64 fields 8-aligned; enums/WGPUBool/WGPUFlags stay u32. All offsets are DERIVED from
// the header by the layout engine below (no hand-maintained constants); the wasm32 column
// reproduces the pre-wasm64 constants byte-for-byte. Handles remain u32 broker-table ids
// (0 == null) regardless of their on-wire width.

import { isMemory64 } from "./sprt-imports.mjs";

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

const WHOLE32 = 0xFFFFFFFF; // low+high both all-ones -> WGPU_WHOLE_SIZE (wasm32 read)
const WHOLE64 = 0xFFFFFFFFFFFFFFFFn;

// ---- struct layout engine -------------------------------------------------------------------
// Field kinds: P = pointer-sized (pointers, handles, callbacks), SV = WGPUStringView,
// Z = size_t (4B on wasm32/ILP32, 8B on wasm64/LP64), u64 = uint64_t (8B on both),
// u32/i32 = enums/flags/bool, u16/f32/f64 as in C. Sizes/alignments per the module ABI;
// offsets are C rules (member alignment, struct padded to max align).
function layout(w64, fields) {
	const k = {
		P: [w64 ? 8 : 4, w64 ? 8 : 4],
		SV: [w64 ? 16 : 8, w64 ? 8 : 4],
		Z: [w64 ? 8 : 4, w64 ? 8 : 4],
		u64: [8, 8], u32: [4, 4], i32: [4, 4], u16: [2, 2], f32: [4, 4], f64: [8, 8],
	};
	let cur = 0, maxA = 1;
	const off = {};
	for (const [name, kind] of fields) {
		const [size, align] = kind.S ? [kind.S._size, kind.S._align] : k[kind];
		cur = Math.ceil(cur / align) * align;
		off[name] = cur;
		cur += size;
		maxA = Math.max(maxA, align);
	}
	off._size = Math.ceil(cur / maxA) * maxA;
	off._align = maxA;
	return off;
}

// Exported for verification gates: the wasm32 column must keep matching the
// on-wire ABI the engine was built against before the layout engine existed.
export function webgpuLayouts(w64) {
	const L = {};
	L.P = w64 ? 8 : 4;
	L.stringView = layout(w64, [["data", "P"], ["length", "Z"]]);
	L.chained = layout(w64, [["next", "P"], ["sType", "u32"]]);
	L.shaderWgsl = layout(w64, [["chain", { S: L.chained }], ["code", "SV"]]);
	L.extent3d = layout(w64, [["width", "u32"], ["height", "u32"], ["depthOrArrayLayers", "u32"]]);
	L.origin3d = layout(w64, [["x", "u32"], ["y", "u32"], ["z", "u32"]]);
	L.color = layout(w64, [["r", "f64"], ["g", "f64"], ["b", "f64"], ["a", "f64"]]);
	L.stencilFace = layout(w64, [["compare", "u32"], ["failOp", "u32"], ["depthFailOp", "u32"], ["passOp", "u32"]]);
	L.adapterInfo = layout(w64, [["next", "P"], ["vendor", "SV"], ["architecture", "SV"], ["device", "SV"],
		["description", "SV"], ["backendType", "u32"], ["adapterType", "u32"], ["vendorID", "u32"], ["deviceID", "u32"]]);
	L.limits = layout(w64, [["next", "P"],
		["maxTextureDimension1D", "u32"], ["maxTextureDimension2D", "u32"], ["maxTextureDimension3D", "u32"],
		["maxTextureArrayLayers", "u32"], ["maxBindGroups", "u32"], ["maxBindGroupsPlusVertexBuffers", "u32"],
		["maxBindingsPerBindGroup", "u32"], ["maxDynamicUniformBuffersPerPipelineLayout", "u32"],
		["maxDynamicStorageBuffersPerPipelineLayout", "u32"], ["maxSampledTexturesPerShaderStage", "u32"],
		["maxSamplersPerShaderStage", "u32"], ["maxStorageBuffersPerShaderStage", "u32"],
		["maxStorageTexturesPerShaderStage", "u32"], ["maxUniformBuffersPerShaderStage", "u32"],
		["maxUniformBufferBindingSize", "u64"], ["maxStorageBufferBindingSize", "u64"],
		["minUniformBufferOffsetAlignment", "u32"], ["minStorageBufferOffsetAlignment", "u32"],
		["maxVertexBuffers", "u32"], ["maxBufferSize", "u64"],
		["maxVertexAttributes", "u32"], ["maxVertexBufferArrayStride", "u32"], ["maxInterStageShaderVariables", "u32"],
		["maxColorAttachments", "u32"], ["maxColorAttachmentBytesPerSample", "u32"],
		["maxComputeWorkgroupStorageSize", "u32"], ["maxComputeInvocationsPerWorkgroup", "u32"],
		["maxComputeWorkgroupSizeX", "u32"], ["maxComputeWorkgroupSizeY", "u32"], ["maxComputeWorkgroupSizeZ", "u32"],
		["maxComputeWorkgroupsPerDimension", "u32"]]);
	L.supportedFeatures = layout(w64, [["featureCount", "Z"], ["features", "P"]]);
	L.shaderModuleDesc = layout(w64, [["next", "P"], ["label", "SV"]]);
	L.bufferDesc = layout(w64, [["next", "P"], ["label", "SV"], ["usage", "u32"], ["size", "u64"], ["mappedAtCreation", "u32"]]);
	L.samplerDesc = layout(w64, [["next", "P"], ["label", "SV"],
		["addressModeU", "u32"], ["addressModeV", "u32"], ["addressModeW", "u32"],
		["magFilter", "u32"], ["minFilter", "u32"], ["mipmapFilter", "u32"],
		["lodMinClamp", "f32"], ["lodMaxClamp", "f32"], ["compare", "u32"], ["maxAnisotropy", "u16"]]);
	L.textureDesc = layout(w64, [["next", "P"], ["label", "SV"], ["usage", "u32"], ["dimension", "u32"],
		["size", { S: L.extent3d }], ["format", "u32"], ["mipLevelCount", "u32"], ["sampleCount", "u32"],
		["viewFormatCount", "Z"], ["viewFormats", "P"]]);
	L.textureViewDesc = layout(w64, [["next", "P"], ["label", "SV"], ["format", "u32"], ["dimension", "u32"],
		["baseMipLevel", "u32"], ["mipLevelCount", "u32"], ["baseArrayLayer", "u32"], ["arrayLayerCount", "u32"],
		["aspect", "u32"], ["usage", "u32"]]);
	L.bufferBinding = layout(w64, [["next", "P"], ["type", "u32"], ["hasDynamicOffset", "u32"], ["minBindingSize", "u64"]]);
	L.samplerBinding = layout(w64, [["next", "P"], ["type", "u32"]]);
	L.textureBinding = layout(w64, [["next", "P"], ["sampleType", "u32"], ["viewDimension", "u32"], ["multisampled", "u32"]]);
	L.storageTextureBinding = layout(w64, [["next", "P"], ["access", "u32"], ["format", "u32"], ["viewDimension", "u32"]]);
	L.bglEntry = layout(w64, [["next", "P"], ["binding", "u32"], ["visibility", "u32"],
		["buffer", { S: L.bufferBinding }], ["sampler", { S: L.samplerBinding }],
		["texture", { S: L.textureBinding }], ["storageTexture", { S: L.storageTextureBinding }]]);
	L.bglDesc = layout(w64, [["next", "P"], ["label", "SV"], ["entryCount", "Z"], ["entries", "P"]]);
	L.bgEntry = layout(w64, [["next", "P"], ["binding", "u32"], ["buffer", "P"],
		["offset", "u64"], ["size", "u64"], ["sampler", "P"], ["textureView", "P"]]);
	L.bgDesc = layout(w64, [["next", "P"], ["label", "SV"], ["layout", "P"], ["entryCount", "Z"], ["entries", "P"]]);
	L.pipelineLayoutDesc = layout(w64, [["next", "P"], ["label", "SV"], ["bindGroupLayoutCount", "Z"], ["bindGroupLayouts", "P"]]);
	L.blendComponent = layout(w64, [["operation", "u32"], ["srcFactor", "u32"], ["dstFactor", "u32"]]);
	L.colorTarget = layout(w64, [["next", "P"], ["format", "u32"], ["blend", "P"], ["writeMask", "u32"]]);
	L.vertexState = layout(w64, [["next", "P"], ["module", "P"], ["entryPoint", "SV"],
		["constantCount", "Z"], ["constants", "P"], ["bufferCount", "Z"], ["buffers", "P"]]);
	L.primitiveState = layout(w64, [["next", "P"], ["topology", "u32"], ["stripIndexFormat", "u32"],
		["frontFace", "u32"], ["cullMode", "u32"], ["unclippedDepth", "u32"]]);
	L.multisampleState = layout(w64, [["next", "P"], ["count", "u32"], ["mask", "u32"], ["alphaToCoverageEnabled", "u32"]]);
	L.depthStencilState = layout(w64, [["next", "P"], ["format", "u32"], ["depthWriteEnabled", "u32"],
		["depthCompare", "u32"], ["stencilFront", { S: L.stencilFace }], ["stencilBack", { S: L.stencilFace }],
		["stencilReadMask", "u32"], ["stencilWriteMask", "u32"],
		["depthBias", "i32"], ["depthBiasSlopeScale", "f32"], ["depthBiasClamp", "f32"]]);
	L.fragmentState = layout(w64, [["next", "P"], ["module", "P"], ["entryPoint", "SV"],
		["constantCount", "Z"], ["constants", "P"], ["targetCount", "Z"], ["targets", "P"]]);
	L.renderPipelineDesc = layout(w64, [["next", "P"], ["label", "SV"], ["layout", "P"],
		["vertex", { S: L.vertexState }], ["primitive", { S: L.primitiveState }], ["depthStencil", "P"],
		["multisample", { S: L.multisampleState }], ["fragment", "P"]]);
	L.progStage = layout(w64, [["next", "P"], ["module", "P"], ["entryPoint", "SV"],
		["constantCount", "Z"], ["constants", "P"]]);
	L.computePipelineDesc = layout(w64, [["next", "P"], ["label", "SV"], ["layout", "P"], ["compute", { S: L.progStage }]]);
	L.renderPassColorAtt = layout(w64, [["next", "P"], ["view", "P"], ["depthSlice", "u32"],
		["resolveTarget", "P"], ["loadOp", "u32"], ["storeOp", "u32"], ["clearValue", { S: L.color }]]);
	L.renderPassDepthAtt = layout(w64, [["view", "P"], ["depthLoadOp", "u32"], ["depthStoreOp", "u32"],
		["depthClearValue", "f32"], ["depthReadOnly", "u32"], ["stencilLoadOp", "u32"], ["stencilStoreOp", "u32"],
		["stencilClearValue", "u32"], ["stencilReadOnly", "u32"]]);
	L.renderPassDesc = layout(w64, [["next", "P"], ["label", "SV"], ["colorAttachmentCount", "Z"],
		["colorAttachments", "P"], ["depthStencilAttachment", "P"], ["occlusionQuerySet", "P"], ["timestampWrites", "P"]]);
	L.texelCopyBufferLayout = layout(w64, [["offset", "u64"], ["bytesPerRow", "u32"], ["rowsPerImage", "u32"]]);
	L.texelCopyBufferInfo = layout(w64, [["layout", { S: L.texelCopyBufferLayout }], ["buffer", "P"]]);
	L.texelCopyTextureInfo = layout(w64, [["texture", "P"], ["mipLevel", "u32"], ["origin", { S: L.origin3d }], ["aspect", "u32"]]);
	L.surfaceConfig = layout(w64, [["next", "P"], ["device", "P"], ["format", "u32"], ["usage", "u32"],
		["width", "u32"], ["height", "u32"], ["viewFormatCount", "Z"], ["viewFormats", "P"],
		["alphaMode", "u32"], ["presentMode", "u32"]]);
	L.surfaceCaps = layout(w64, [["next", "P"], ["usages", "u32"], ["formatCount", "Z"], ["formats", "P"],
		["presentModeCount", "Z"], ["presentModes", "P"], ["alphaModeCount", "Z"], ["alphaModes", "P"]]);
	L.surfaceTexture = layout(w64, [["next", "P"], ["texture", "P"], ["status", "u32"]]);
	L.callbackInfo = layout(w64, [["next", "P"], ["mode", "u32"], ["callback", "P"], ["userdata1", "P"], ["userdata2", "P"]]);
	return L;
}

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

// wgpu functions whose C return is pointer-sized/size_t (handle creators, EnumerateAdapters):
// a wasm64 module expects those imports to RETURN i64, so the thunk hands back a BigInt.
const I64_RET = new Set([
	"wgpuCreateInstance", "wgpuInstanceEnumerateAdapters", "wgpuInstanceCreateSurface",
	"wgpuDeviceGetQueue", "wgpuDeviceCreateShaderModule", "wgpuDeviceCreateBuffer", "wgpuDeviceCreateTexture",
	"wgpuDeviceCreateSampler", "wgpuDeviceCreateBindGroupLayout", "wgpuDeviceCreateBindGroup",
	"wgpuDeviceCreatePipelineLayout", "wgpuDeviceCreateRenderPipeline", "wgpuDeviceCreateComputePipeline",
	"wgpuDeviceCreateCommandEncoder", "wgpuCommandEncoderBeginRenderPass", "wgpuCommandEncoderBeginComputePass",
	"wgpuCommandEncoderFinish",
]);

// ---- memory codec -------------------------------------------------------------------------
// Reads/writes follow the module ABI: pointer-sized fields (pointers, handles, callbacks)
// are u32 on wasm32 and u64 on wasm64; u64/size_t fields are 8B on both.
export function makeCodec(memory, w64) {
	const dv = () => new DataView(memory.buffer);
	const u8 = () => new Uint8Array(memory.buffer);
	const u32 = (p) => dv().getUint32(p, true);
	const i32 = (p) => dv().getInt32(p, true);
	const f32 = (p) => dv().getFloat32(p, true);
	const f64 = (p) => dv().getFloat64(p, true);
	const ptr = w64
		? (p) => Number(dv().getBigUint64(p, true))
		: (p) => dv().getUint32(p, true);
	const handle = ptr; // handles are pointer-typed in the contract header
	const u64 = w64
		? (p) => Number(dv().getBigUint64(p, true))
		: (p) => { const lo = u32(p), hi = u32(p + 4); return hi * 0x100000000 + lo; };
	const isWhole = w64
		? (p) => dv().getBigUint64(p, true) === WHOLE64
		: (p) => u32(p) === WHOLE32 && u32(p + 4) === WHOLE32;
	const set32 = (p, v) => dv().setUint32(p, v >>> 0, true);
	const setPtr = w64
		? (p, v) => dv().setBigUint64(p, BigInt(v), true)
		: (p, v) => dv().setUint32(p, v >>> 0, true);
	const set64 = w64
		? (p, v) => dv().setBigUint64(p, BigInt(v), true)
		: (p, v) => { dv().setUint32(p, v >>> 0, true); dv().setUint32(p + 4, Math.floor(v / 0x100000000) >>> 0, true); };
	const z = w64 ? u64 : u32;          // size_t read (4B wasm32 / 8B wasm64)
	const setZ = w64 ? set64 : set32;   // size_t write
	const bytes = (p, n) => u8().slice(p, p + n);
	const strView = (p) => {
		const data = ptr(p), len = w64 ? u64(p + 8) : u32(p + 4);
		if (!data) return "";
		if (len === 0xffffffff || (w64 && len === 0xffffffffffffffff)) { let e = data; const b = u8(); while (b[e]) e++; return new TextDecoder().decode(u8().slice(data, e)); }
		return new TextDecoder().decode(u8().slice(data, data + len));
	};
	return { dv, u8, u32, i32, f32, f64, u64, ptr, handle, isWhole, set32, setPtr, set64, z, setZ, bytes, strView };
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
	const w64 = isMemory64(memory);
	const LO = webgpuLayouts(w64);
	const H = new Handles();
	const c = makeCodec(memory, w64);
	const mapped = new Map(); // buffer handle -> { buf, ptr, size } (mappedAtCreation upload)

	// Presentation bridge: the engine renders whenever (off its own clock), but a transferred
	// OffscreenCanvas only shows its WebGPU output at the worker's animation-frame boundary. So
	// the engine "swapchain image" is an offscreen texture it renders into, and a rAF loop in
	// gpu-broker.mjs blits that into the real canvas each frame (see $present).
	let presentTex = null, presentW = 0, presentH = 0;

	function decodeBlend(p) {
		// WGPUBlendState: two pure-enum components — same offsets under both ABIs.
		return {
			color: { operation: BLEND_OP[c.u32(p)] || "add", srcFactor: BLEND_FACTOR[c.u32(p + 4)] || "one", dstFactor: BLEND_FACTOR[c.u32(p + 8)] || "zero" },
			alpha: { operation: BLEND_OP[c.u32(p + 12)] || "add", srcFactor: BLEND_FACTOR[c.u32(p + 16)] || "one", dstFactor: BLEND_FACTOR[c.u32(p + 20)] || "zero" },
		};
	}
	function decodePipeline(p) {
		const d = LO.renderPipelineDesc, layoutH = c.handle(p + d.layout);
		const v = p + d.vertex;
		const desc = {
			layout: layoutH ? H.get(layoutH) : "auto",
			vertex: { module: H.get(c.handle(v + LO.vertexState.module)), entryPoint: c.strView(v + LO.vertexState.entryPoint) || undefined },
			primitive: (() => {
				const pr = p + d.primitive, s = LO.primitiveState;
				return {
					topology: TOPOLOGY[c.u32(pr + s.topology)] || "triangle-list",
					stripIndexFormat: INDEX_FORMAT[c.u32(pr + s.stripIndexFormat)],
					frontFace: FRONT_FACE[c.u32(pr + s.frontFace)] || "ccw",
					cullMode: CULL_MODE[c.u32(pr + s.cullMode)] || "none",
				};
			})(),
			multisample: (() => {
				const ms = p + d.multisample, s = LO.multisampleState;
				return { count: c.u32(ms + s.count) || 1, mask: c.u32(ms + s.mask) || 0xFFFFFFFF, alphaToCoverageEnabled: !!c.u32(ms + s.alphaToCoverageEnabled) };
			})(),
		};
		const dsPtr = c.ptr(p + d.depthStencil);
		if (dsPtr) {
			const s = LO.depthStencilState, dw = c.u32(dsPtr + s.depthWriteEnabled);
			desc.depthStencil = {
				format: TEXTURE_FORMAT[c.u32(dsPtr + s.format)],
				depthWriteEnabled: dw === 2 ? undefined : !!dw,
				depthCompare: COMPARE[c.u32(dsPtr + s.depthCompare)] || "always",
				depthBias: c.i32(dsPtr + s.depthBias), depthBiasSlopeScale: c.f32(dsPtr + s.depthBiasSlopeScale), depthBiasClamp: c.f32(dsPtr + s.depthBiasClamp),
			};
		}
		const fragPtr = c.ptr(p + d.fragment);
		if (fragPtr) {
			const f = LO.fragmentState, targetCount = c.z(fragPtr + f.targetCount), targetsPtr = c.ptr(fragPtr + f.targets), targets = [];
			const ts = LO.colorTarget, tstride = ts._size;
			for (let i = 0; i < targetCount; i++) {
				const t = targetsPtr + i * tstride, blendPtr = c.ptr(t + ts.blend);
				const tgt = { format: TEXTURE_FORMAT[c.u32(t + ts.format)] || gpu.format, writeMask: c.u32(t + ts.writeMask) };
				if (blendPtr) tgt.blend = decodeBlend(blendPtr);
				targets.push(tgt);
			}
			desc.fragment = { module: H.get(c.handle(fragPtr + f.module)), entryPoint: c.strView(fragPtr + f.entryPoint) || undefined, targets };
		}
		return desc;
	}
	function decodeRenderPass(p) {
		const d = LO.renderPassDesc, count = c.z(p + d.colorAttachmentCount), attsPtr = c.ptr(p + d.colorAttachments), colorAttachments = [];
		const a = LO.renderPassColorAtt, stride = a._size;
		for (let i = 0; i < count; i++) {
			const b = attsPtr + i * stride;
			colorAttachments.push({
				view: H.get(c.handle(b + a.view)),
				resolveTarget: H.get(c.handle(b + a.resolveTarget)) || undefined,
				loadOp: LOAD_OP[c.u32(b + a.loadOp)] || "clear",
				storeOp: STORE_OP[c.u32(b + a.storeOp)] || "store",
				clearValue: (() => {
					const q = b + a.clearValue, o = LO.color;
					return { r: c.f64(q + o.r), g: c.f64(q + o.g), b: c.f64(q + o.b), a: c.f64(q + o.a) };
				})(),
			});
		}
		const rp = { colorAttachments };
		const dsPtr = c.ptr(p + d.depthStencilAttachment);
		if (dsPtr) {
			const s = LO.renderPassDepthAtt, dsa = {
				view: H.get(c.handle(dsPtr + s.view)),
				depthLoadOp: LOAD_OP[c.u32(dsPtr + s.depthLoadOp)], depthStoreOp: STORE_OP[c.u32(dsPtr + s.depthStoreOp)],
				depthClearValue: c.f32(dsPtr + s.depthClearValue), depthReadOnly: !!c.u32(dsPtr + s.depthReadOnly),
			};
			const slo = c.u32(dsPtr + s.stencilLoadOp);
			if (slo) {
				dsa.stencilLoadOp = LOAD_OP[slo]; dsa.stencilStoreOp = STORE_OP[c.u32(dsPtr + s.stencilStoreOp)];
				dsa.stencilClearValue = c.u32(dsPtr + s.stencilClearValue); dsa.stencilReadOnly = !!c.u32(dsPtr + s.stencilReadOnly);
			}
			rp.depthStencilAttachment = dsa;
		}
		return rp;
	}
	function decodeBGLEntry(p) {
		const e = { binding: c.u32(p + LO.bglEntry.binding), visibility: c.u32(p + LO.bglEntry.visibility) };
		const bb = p + LO.bglEntry.buffer, sb = p + LO.bglEntry.sampler, tb = p + LO.bglEntry.texture, st = p + LO.bglEntry.storageTexture;
		const bufType = c.u32(bb + LO.bufferBinding.type), samType = c.u32(sb + LO.samplerBinding.type),
			texSample = c.u32(tb + LO.textureBinding.sampleType), stAccess = c.u32(st + LO.storageTextureBinding.access);
		if (bufType) e.buffer = { type: BUFFER_BINDING[bufType], hasDynamicOffset: !!c.u32(bb + LO.bufferBinding.hasDynamicOffset), minBindingSize: c.u64(bb + LO.bufferBinding.minBindingSize) };
		else if (samType) e.sampler = { type: SAMPLER_BINDING[samType] };
		else if (texSample) e.texture = { sampleType: TEX_SAMPLE[texSample], viewDimension: VIEW_DIM[c.u32(tb + LO.textureBinding.viewDimension)] || "2d", multisampled: !!c.u32(tb + LO.textureBinding.multisampled) };
		else if (stAccess) e.storageTexture = { access: STORAGE_ACCESS[stAccess], format: TEXTURE_FORMAT[c.u32(st + LO.storageTextureBinding.format)], viewDimension: VIEW_DIM[c.u32(st + LO.storageTextureBinding.viewDimension)] || "2d" };
		return e;
	}
	function decodeBGEntry(p) {
		const s = LO.bgEntry;
		const binding = c.u32(p + s.binding), bufH = c.handle(p + s.buffer), samH = c.handle(p + s.sampler), viewH = c.handle(p + s.textureView);
		if (bufH) {
			const r = { buffer: H.get(bufH), offset: c.u64(p + s.offset) };
			if (!c.isWhole(p + s.size)) { const sz = c.u64(p + s.size); if (sz) r.size = sz; }
			return { binding, resource: r };
		}
		if (samH) return { binding, resource: H.get(samH) };
		return { binding, resource: H.get(viewH) };
	}
	function fillLimits(p, lim) {
		const g = (k, def) => (lim && lim[k] != null ? lim[k] : def);
		const o = LO.limits;
		const put32 = (f, k, def) => c.set32(p + o[f], g(k, def));
		const put64 = (f, k, def) => c.set64(p + o[f], g(k, def));
		put32("maxTextureDimension1D", "maxTextureDimension1D", 8192); put32("maxTextureDimension2D", "maxTextureDimension2D", 8192);
		put32("maxTextureDimension3D", "maxTextureDimension3D", 2048); put32("maxTextureArrayLayers", "maxTextureArrayLayers", 256);
		put32("maxBindGroups", "maxBindGroups", 4); put32("maxBindGroupsPlusVertexBuffers", "maxBindGroupsPlusVertexBuffers", 24);
		put32("maxBindingsPerBindGroup", "maxBindingsPerBindGroup", 1000); put32("maxDynamicUniformBuffersPerPipelineLayout", "maxDynamicUniformBuffersPerPipelineLayout", 8);
		put32("maxDynamicStorageBuffersPerPipelineLayout", "maxDynamicStorageBuffersPerPipelineLayout", 4); put32("maxSampledTexturesPerShaderStage", "maxSampledTexturesPerShaderStage", 16);
		put32("maxSamplersPerShaderStage", "maxSamplersPerShaderStage", 16); put32("maxStorageBuffersPerShaderStage", "maxStorageBuffersPerShaderStage", 8);
		put32("maxStorageTexturesPerShaderStage", "maxStorageTexturesPerShaderStage", 4); put32("maxUniformBuffersPerShaderStage", "maxUniformBuffersPerShaderStage", 12);
		put64("maxUniformBufferBindingSize", "maxUniformBufferBindingSize", 65536); put64("maxStorageBufferBindingSize", "maxStorageBufferBindingSize", 134217728);
		put32("minUniformBufferOffsetAlignment", "minUniformBufferOffsetAlignment", 256); put32("minStorageBufferOffsetAlignment", "minStorageBufferOffsetAlignment", 256);
		put32("maxVertexBuffers", "maxVertexBuffers", 8); put64("maxBufferSize", "maxBufferSize", 268435456);
		put32("maxVertexAttributes", "maxVertexAttributes", 16); put32("maxVertexBufferArrayStride", "maxVertexBufferArrayStride", 2048);
		put32("maxInterStageShaderVariables", "maxInterStageShaderVariables", 16); put32("maxColorAttachments", "maxColorAttachments", 8);
		put32("maxColorAttachmentBytesPerSample", "maxColorAttachmentBytesPerSample", 32); put32("maxComputeWorkgroupStorageSize", "maxComputeWorkgroupStorageSize", 16384);
		put32("maxComputeInvocationsPerWorkgroup", "maxComputeInvocationsPerWorkgroup", 256); put32("maxComputeWorkgroupSizeX", "maxComputeWorkgroupSizeX", 256);
		put32("maxComputeWorkgroupSizeY", "maxComputeWorkgroupSizeY", 256); put32("maxComputeWorkgroupSizeZ", "maxComputeWorkgroupSizeZ", 64);
		put32("maxComputeWorkgroupsPerDimension", "maxComputeWorkgroupsPerDimension", 65535);
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
		wgpuInstanceEnumerateAdapters: (_inst, _opts, adaptersPtr) => { if (adaptersPtr) c.setPtr(adaptersPtr, H.add(gpu.adapter)); return 1; },
		wgpuInstanceCreateSurface: () => H.add({ surface: true }),
		// The engine polls this (wgpu-native model) to wait for a submission to finish before
		// presenting/recycling. WebGPU completion is async, but the browser keeps GPU objects
		// alive by refcount until the work truly finishes, so reporting "queue drained" each
		// poll is safe and lets the frame loop advance.
		wgpuDevicePoll: () => 1,

		wgpuAdapterGetInfo: (_ad, infoPtr) => {
			const o = LO.adapterInfo;
			for (const f of ["vendor", "architecture", "device", "description"]) {
				const sv = infoPtr + o[f];
				c.setPtr(sv, 0); c.setZ(sv + LO.stringView.length, 0);
			}
			c.set32(infoPtr + o.backendType, 2); c.set32(infoPtr + o.adapterType, 2);
			c.set32(infoPtr + o.vendorID, 0); c.set32(infoPtr + o.deviceID, 0);
			return 1;
		},
		wgpuAdapterInfoFreeMembers: () => {},
		wgpuAdapterGetLimits: (_ad, limPtr) => { fillLimits(limPtr, gpu.adapter.limits); return 1; },
		wgpuAdapterGetFeatures: (_ad, featPtr) => { const o = LO.supportedFeatures; c.setZ(featPtr + o.featureCount, 0); c.setPtr(featPtr + o.features, 0); },
		wgpuSupportedFeaturesFreeMembers: () => {},
		wgpuAdapterAddRef: () => {},
		wgpuAdapterRelease: (h) => H.release(h),

		wgpuDeviceGetQueue: () => H.add(gpu.queue),
		wgpuDeviceGetLimits: (_dev, limPtr) => { fillLimits(limPtr, gpu.device.limits); return 1; },
		wgpuDeviceGetFeatures: (_dev, featPtr) => { const o = LO.supportedFeatures; c.setZ(featPtr + o.featureCount, 0); c.setPtr(featPtr + o.features, 0); },
		wgpuDeviceRelease: (h) => H.release(h),

		wgpuDeviceCreateShaderModule: (_dev, descPtr) => {
			const chain = c.ptr(descPtr + LO.shaderModuleDesc.next), code = chain ? c.strView(chain + LO.shaderWgsl.code) : "";
			return H.add(gpu.device.createShaderModule({ code }));
		},
		wgpuDeviceCreateBuffer: (_dev, descPtr) => {
			const o = LO.bufferDesc, usage = c.u32(descPtr + o.usage), size = c.u64(descPtr + o.size), mappedAtCreation = !!c.u32(descPtr + o.mappedAtCreation);
			return H.add(gpu.device.createBuffer({ size, usage, mappedAtCreation }));
		},
		wgpuDeviceCreateTexture: (_dev, descPtr) => {
			const o = LO.textureDesc, sz = descPtr + o.size, e = LO.extent3d;
			return H.add(gpu.device.createTexture({
				usage: c.u32(descPtr + o.usage), dimension: TEX_DIM[c.u32(descPtr + o.dimension)] || "2d",
				size: { width: c.u32(sz + e.width), height: c.u32(sz + e.height), depthOrArrayLayers: c.u32(sz + e.depthOrArrayLayers) || 1 },
				format: TEXTURE_FORMAT[c.u32(descPtr + o.format)], mipLevelCount: c.u32(descPtr + o.mipLevelCount) || 1, sampleCount: c.u32(descPtr + o.sampleCount) || 1,
			}));
		},
		wgpuDeviceCreateSampler: (_dev, descPtr) => {
			const o = LO.samplerDesc, d = descPtr;
			const desc = {
				addressModeU: ADDRESS[c.u32(d + o.addressModeU)] || "clamp-to-edge", addressModeV: ADDRESS[c.u32(d + o.addressModeV)] || "clamp-to-edge",
				addressModeW: ADDRESS[c.u32(d + o.addressModeW)] || "clamp-to-edge", magFilter: FILTER[c.u32(d + o.magFilter)] || "nearest",
				minFilter: FILTER[c.u32(d + o.minFilter)] || "nearest", mipmapFilter: FILTER[c.u32(d + o.mipmapFilter)] || "nearest",
				lodMinClamp: c.f32(d + o.lodMinClamp), lodMaxClamp: c.f32(d + o.lodMaxClamp) || 32, maxAnisotropy: c.dv().getUint16(d + o.maxAnisotropy, true) || 1,
			};
			const cmp = c.u32(d + o.compare); if (cmp) desc.compare = COMPARE[cmp];
			return H.add(gpu.device.createSampler(desc));
		},
		wgpuDeviceCreateBindGroupLayout: (_dev, descPtr) => {
			const o = LO.bglDesc, count = c.z(descPtr + o.entryCount), ptr = c.ptr(descPtr + o.entries), entries = [];
			for (let i = 0; i < count; i++) entries.push(decodeBGLEntry(ptr + i * LO.bglEntry._size));
			return H.add(gpu.device.createBindGroupLayout({ entries }));
		},
		wgpuDeviceCreateBindGroup: (_dev, descPtr) => {
			const o = LO.bgDesc, layout = H.get(c.handle(descPtr + o.layout)), count = c.z(descPtr + o.entryCount), ptr = c.ptr(descPtr + o.entries), entries = [];
			for (let i = 0; i < count; i++) entries.push(decodeBGEntry(ptr + i * LO.bgEntry._size));
			return H.add(gpu.device.createBindGroup({ layout, entries }));
		},
		wgpuDeviceCreatePipelineLayout: (_dev, descPtr) => {
			const o = LO.pipelineLayoutDesc, count = c.z(descPtr + o.bindGroupLayoutCount), ptr = c.ptr(descPtr + o.bindGroupLayouts), bindGroupLayouts = [];
			for (let i = 0; i < count; i++) bindGroupLayouts.push(H.get(c.handle(ptr + i * LO.P)));
			return H.add(gpu.device.createPipelineLayout({ bindGroupLayouts }));
		},
		wgpuDeviceCreateRenderPipeline: (_dev, descPtr) => H.add(gpu.device.createRenderPipeline(decodePipeline(descPtr))),
		wgpuDeviceCreateComputePipeline: (_dev, descPtr) => {
			const o = LO.computePipelineDesc, layoutH = c.handle(descPtr + o.layout), cs = descPtr + o.compute;
			return H.add(gpu.device.createComputePipeline({
				layout: layoutH ? H.get(layoutH) : "auto",
				compute: { module: H.get(c.handle(cs + LO.progStage.module)), entryPoint: c.strView(cs + LO.progStage.entryPoint) || undefined },
			}));
		},
		wgpuDeviceCreateCommandEncoder: () => H.add(gpu.device.createCommandEncoder()),

		wgpuQueueSubmit: (queue, count, cmdsPtr) => {
			const cmds = []; for (let i = 0; i < count; i++) cmds.push(H.get(c.handle(cmdsPtr + i * LO.P)));
			H.get(queue).submit(cmds);
		},
		wgpuQueueWriteBuffer: (queue, buffer, offset, dataPtr, size) => H.get(queue).writeBuffer(H.get(buffer), Number(offset), c.bytes(dataPtr, size)),
		wgpuQueueWriteTexture: (queue, destPtr, dataPtr, dataSize, layoutPtr, extentPtr) => {
			const t = LO.texelCopyTextureInfo, or = destPtr + t.origin, og = LO.origin3d;
			const dest = { texture: H.get(c.handle(destPtr + t.texture)), mipLevel: c.u32(destPtr + t.mipLevel),
				origin: { x: c.u32(or + og.x), y: c.u32(or + og.y), z: c.u32(or + og.z) }, aspect: ASPECT[c.u32(destPtr + t.aspect)] || "all" };
			const bl = LO.texelCopyBufferLayout;
			const layout = { offset: c.u64(layoutPtr + bl.offset), bytesPerRow: c.u32(layoutPtr + bl.bytesPerRow), rowsPerImage: c.u32(layoutPtr + bl.rowsPerImage) || undefined };
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
			const o = LO.textureViewDesc, desc = {};
			const fmt = c.u32(descPtr + o.format); if (fmt) desc.format = TEXTURE_FORMAT[fmt];
			const dim = c.u32(descPtr + o.dimension); if (dim) desc.dimension = VIEW_DIM[dim];
			desc.baseMipLevel = c.u32(descPtr + o.baseMipLevel);
			const mlc = c.u32(descPtr + o.mipLevelCount); if (mlc) desc.mipLevelCount = mlc;
			desc.baseArrayLayer = c.u32(descPtr + o.baseArrayLayer);
			const alc = c.u32(descPtr + o.arrayLayerCount); if (alc) desc.arrayLayerCount = alc;
			const asp = c.u32(descPtr + o.aspect); if (asp) desc.aspect = ASPECT[asp];
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
			const t = LO.texelCopyTextureInfo, or = srcPtr + t.origin, og = LO.origin3d;
			const src = { texture: H.get(c.handle(srcPtr + t.texture)), mipLevel: c.u32(srcPtr + t.mipLevel),
				origin: { x: c.u32(or + og.x), y: c.u32(or + og.y), z: c.u32(or + og.z) }, aspect: ASPECT[c.u32(srcPtr + t.aspect)] || "all" };
			const bi = LO.texelCopyBufferInfo, bl = LO.texelCopyBufferLayout, lb = dstPtr + bi.layout;
			const dst = { buffer: H.get(c.handle(dstPtr + bi.buffer)), offset: c.u64(lb + bl.offset), bytesPerRow: c.u32(lb + bl.bytesPerRow), rowsPerImage: c.u32(lb + bl.rowsPerImage) || undefined };
			const extent = { width: c.u32(extentPtr), height: c.u32(extentPtr + 4), depthOrArrayLayers: c.u32(extentPtr + 8) || 1 };
			H.get(enc).copyTextureToBuffer(src, dst, extent);
		},
		wgpuCommandEncoderFinish: (enc) => H.add(H.get(enc).finish()),
		wgpuCommandEncoderRelease: (h) => H.release(h),
		wgpuCommandBufferRelease: (h) => H.release(h),

		wgpuRenderPassEncoderSetPipeline: (pass, pipe) => H.get(pass).setPipeline(H.get(pipe)),
		wgpuRenderPassEncoderSetBindGroup: (pass, index, group, offCount, offPtr) => {
			const offs = []; for (let i = 0; i < offCount; i++) offs.push(c.u32(offPtr + i * 4)); // uint32_t offsets
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
			const o = LO.surfaceCaps;
			c.set32(scratchPtr, 18 /*rgba8unorm*/);
			c.set32(scratchPtr + 4, 1 /*fifo*/); c.set32(scratchPtr + 8, 1 /*opaque*/);
			c.set32(capsPtr + o.usages, 0x13);
			c.setZ(capsPtr + o.formatCount, 1); c.setPtr(capsPtr + o.formats, scratchPtr);
			c.setZ(capsPtr + o.presentModeCount, 1); c.setPtr(capsPtr + o.presentModes, scratchPtr + 4);
			c.setZ(capsPtr + o.alphaModeCount, 1); c.setPtr(capsPtr + o.alphaModes, scratchPtr + 8);
			return 1;
		},
		wgpuSurfaceCapabilitiesFreeMembers: () => {},
		wgpuSurfaceConfigure: (_surf, cfgPtr) => {
			const o = LO.surfaceConfig;
			const width = c.u32(cfgPtr + o.width), height = c.u32(cfgPtr + o.height);
			const format = TEXTURE_FORMAT[c.u32(cfgPtr + o.format)] || gpu.format;
			if (gpu.canvas) { gpu.canvas.width = width; gpu.canvas.height = height; }
			// The canvas is the blit target (CopyDst); the engine renders into presentTex.
			gpu.context.configure({ device: gpu.device, format, usage: 0x12 /*RenderAttachment|CopyDst*/,
				alphaMode: ALPHA_MODE[c.u32(cfgPtr + o.alphaMode)] || "opaque" });
			presentW = width; presentH = height;
			presentTex = gpu.device.createTexture({ size: { width, height }, format,
				usage: 0x11 /*RenderAttachment|CopySrc*/ });
		},
		wgpuSurfaceUnconfigure: () => { try { gpu.context.unconfigure(); } catch {} presentTex = null; },
		wgpuSurfaceGetCurrentTexture: (_surf, outPtr) => {
			// hand the engine the offscreen present texture, not the live canvas texture
			const o = LO.surfaceTexture;
			c.setPtr(outPtr + o.texture, H.add(presentTex)); c.set32(outPtr + o.status, 1 /*SuccessOptimal*/);
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
	const w64 = isMemory64(memory);
	const LO = webgpuLayouts(w64);
	const ci = new Int32Array(ctrl);
	const ba = new BigInt64Array(ctrl, ARGS_OFF, ARGS_MAX);
	const c = makeCodec(memory, w64);
	// wasm64 imports take/return i64 for pointer-sized values: JS must hand over BigInt.
	const arg64 = w64 ? (v) => BigInt(v) : (v) => v;
	const call = (fn, ...a) => getTable().get(fn)(...a);
	// malloc returns i64 on wasm64 — normalize to Number (linear memory < 2^53).
	const mallocPtr = (n) => Number(getExports().malloc(w64 ? BigInt(n) : n));
	const freePtr = (p) => getExports().free(w64 ? BigInt(p) : p);
	const mappedPtr = new Map(); // buffer handle -> local wasm ptr (freed on unmap)
	let svScratch = 0;
	const emptySV = () => {
		if (!svScratch) {
			svScratch = mallocPtr(LO.stringView._size);
			c.setPtr(svScratch + LO.stringView.data, 0); c.setZ(svScratch + LO.stringView.length, 0);
		}
		return svScratch;
	};

	// synchronous marshalled call: one caller at a time (LOCK), block until the broker answers.
	function broker(funcId, args) {
		while (Atomics.compareExchange(ci, LOCK, 0, 1) !== 0) { Atomics.wait(ci, LOCK, 1); }
		Atomics.store(ci, FUNC, funcId);
		Atomics.store(ci, NARG, args.length);
		for (let i = 0; i < args.length; i++) { const a = args[i]; ba[i] = typeof a === "bigint" ? a : BigInt(a); }
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
	// Every broker-side function becomes a thin marshalling thunk. BigInt-returning (i64)
	// functions on wasm64 must hand a BigInt back to the module.
	for (const name of BROKER_FUNCS) {
		if (name[0] === "$") continue;
		const id = FUNC_ID[name];
		if (w64 && I64_RET.has(name)) {
			wgpu[name] = (...args) => BigInt(broker(id, args));
		} else {
			wgpu[name] = (...args) => broker(id, args);
		}
	}

	// --- locally-handled functions (override the generic thunks) ---------------------------
	// Device request: fetch the broker's device handle, then fire the C callback on THIS
	// worker (its indirect table). Callback signature: (status u32, WGPUDevice, StringView*,
	// void*, void*) — everything but the status is pointer-sized.
	wgpu.wgpuAdapterRequestDevice = (_ad, _desc, cbInfoPtr) => {
		const dev = broker(FUNC_ID.$getDevice, []);
		const o = LO.callbackInfo, p = cbInfoPtr;
		const cb = c.ptr(p + o.callback), ud1 = c.ptr(p + o.userdata1), ud2 = c.ptr(p + o.userdata2);
		call(cb, 1 /*Success*/, arg64(dev), arg64(emptySV()), arg64(ud1), arg64(ud2));
	};
	// Submitted-work-done + poll: the engine registers a completion callback (AllowProcessEvents
	// mode) then spins wgpuDevicePoll waiting for it — that is how fences (Fence::arm) and
	// waitIdle signal. So DON'T fire on registration; queue it, and deliver on the next poll.
	// We can't truly wait on the GPU from here, but marshalled submits are already ordered on
	// the queue, so delivering on poll is the right shape for the frame loop.
	const pendingWorkDone = [];
	wgpu.wgpuQueueOnSubmittedWorkDone = (_queue, cbInfoPtr) => {
		const o = LO.callbackInfo, p = cbInfoPtr;
		pendingWorkDone.push([c.ptr(p + o.callback), c.ptr(p + o.userdata1), c.ptr(p + o.userdata2)]);
	};
	wgpu.wgpuDevicePoll = (_dev, _wait, _wsi) => {
		while (pendingWorkDone.length) { const [cb, ud1, ud2] = pendingWorkDone.shift(); call(cb, 1 /*Success*/, arg64(emptySV()), arg64(ud1), arg64(ud2)); }
		return 1;
	};
	// Mapped range (mappedAtCreation uploads): malloc here (TLS-valid), register the pointer
	// with the broker so its unmap flushes our bytes into the GPU buffer. Returns void* —
	// i64 on wasm64.
	const getRange = (buffer, offset, size) => {
		let ptr = mappedPtr.get(buffer);
		if (!ptr) { ptr = mallocPtr(size || 4); mappedPtr.set(buffer, ptr); broker(FUNC_ID.$setMapped, [buffer, ptr, size || 4]); }
		return arg64(ptr + (offset | 0));
	};
	wgpu.wgpuBufferGetMappedRange = getRange;
	wgpu.wgpuBufferGetConstMappedRange = getRange;
	wgpu.wgpuBufferUnmap = (buffer) => {
		broker(FUNC_ID.wgpuBufferUnmap, [buffer]); // broker flushes + unmaps
		const ptr = mappedPtr.get(buffer); if (ptr) { freePtr(ptr); mappedPtr.delete(buffer); }
	};
	// Async map (readback / screenshots): not wired through the broker yet — report cancelled so
	// the engine doesn't wait forever. (Milestone: the render path uses writeBuffer + mapped
	// creation, not read-back mapping.)
	wgpu.wgpuBufferMapAsync = (_buffer, _mode, _offset, _size, cbInfoPtr) => {
		const o = LO.callbackInfo, p = cbInfoPtr;
		const cb = c.ptr(p + o.callback), ud1 = c.ptr(p + o.userdata1), ud2 = c.ptr(p + o.userdata2);
		call(cb, 2 /*CallbackCancelled*/, arg64(emptySV()), arg64(ud1), arg64(ud2));
	};
	return wgpu;
}
