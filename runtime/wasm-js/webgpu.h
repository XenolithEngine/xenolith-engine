/* Minimal webgpu.h — FOUNDATION subset for the browser binding proof (triangle).
 *
 * A self-consistent slice of the WebGPU C ABI: enough to bring up an instance, adapter,
 * device and queue, compile a WGSL shader, build a render pipeline, and draw to a surface
 * (the page's canvas). Field layouts here are matched byte-for-byte by webgpu.mjs; they
 * follow the canonical webgpu-native shapes but are trimmed. Growing this toward the full
 * header (buffers/textures/bind groups/compute) is additive.
 */
#ifndef XL_WASM_WEBGPU_H
#define XL_WASM_WEBGPU_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t WGPUFlags;
typedef uint32_t WGPUBool;

/* Opaque handles are host-side integers (see the handle table in webgpu.mjs). */
typedef uint32_t WGPUInstance;
typedef uint32_t WGPUAdapter;
typedef uint32_t WGPUDevice;
typedef uint32_t WGPUQueue;
typedef uint32_t WGPUSurface;
typedef uint32_t WGPUTexture;
typedef uint32_t WGPUTextureView;
typedef uint32_t WGPUShaderModule;
typedef uint32_t WGPURenderPipeline;
typedef uint32_t WGPUPipelineLayout;
typedef uint32_t WGPUCommandEncoder;
typedef uint32_t WGPURenderPassEncoder;
typedef uint32_t WGPUCommandBuffer;

typedef struct WGPUStringView { const char *data; size_t length; } WGPUStringView;
#define WGPU_STRLEN ((size_t)-1)

typedef enum WGPUTextureFormat { WGPUTextureFormat_RGBA8Unorm = 18, WGPUTextureFormat_BGRA8Unorm = 23 } WGPUTextureFormat;
typedef enum WGPULoadOp { WGPULoadOp_Load = 1, WGPULoadOp_Clear = 2 } WGPULoadOp;
typedef enum WGPUStoreOp { WGPUStoreOp_Store = 1, WGPUStoreOp_Discard = 2 } WGPUStoreOp;
typedef enum WGPUPrimitiveTopology { WGPUPrimitiveTopology_TriangleList = 3, WGPUPrimitiveTopology_TriangleStrip = 4 } WGPUPrimitiveTopology;
typedef enum WGPUCullMode { WGPUCullMode_None = 0, WGPUCullMode_Front = 1, WGPUCullMode_Back = 2 } WGPUCullMode;
typedef enum WGPUFrontFace { WGPUFrontFace_CCW = 0, WGPUFrontFace_CW = 1 } WGPUFrontFace;
typedef enum WGPUSType { WGPUSType_ShaderSourceWGSL = 3 } WGPUSType;

typedef struct WGPUColor { double r, g, b, a; } WGPUColor;

typedef struct WGPUChainedStruct { const struct WGPUChainedStruct *next; WGPUSType sType; } WGPUChainedStruct;

/* Shader: WGSL source is a chained struct off the descriptor. */
typedef struct WGPUShaderSourceWGSL { WGPUChainedStruct chain; WGPUStringView code; } WGPUShaderSourceWGSL;
typedef struct WGPUShaderModuleDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUShaderModuleDescriptor;

typedef struct WGPUInstanceDescriptor { const WGPUChainedStruct *nextInChain; } WGPUInstanceDescriptor;

/* Async request callbacks (resolved synchronously by the host: bootstrap is done up front). */
typedef void (*WGPURequestAdapterCallback)(uint32_t status, WGPUAdapter adapter, const char *msgData, size_t msgLen, void *userdata);
typedef void (*WGPURequestDeviceCallback)(uint32_t status, WGPUDevice device, const char *msgData, size_t msgLen, void *userdata);
typedef struct WGPURequestAdapterOptions { const WGPUChainedStruct *nextInChain; } WGPURequestAdapterOptions;
typedef struct WGPUDeviceDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUDeviceDescriptor;

/* Pipeline */
typedef struct WGPUVertexState { const WGPUChainedStruct *nextInChain; WGPUShaderModule module; WGPUStringView entryPoint; size_t constantCount; const void *constants; size_t bufferCount; const void *buffers; } WGPUVertexState;
typedef struct WGPUPrimitiveState { const WGPUChainedStruct *nextInChain; WGPUPrimitiveTopology topology; uint32_t stripIndexFormat; WGPUFrontFace frontFace; WGPUCullMode cullMode; WGPUBool unclippedDepth; } WGPUPrimitiveState;
typedef struct WGPUColorTargetState { const WGPUChainedStruct *nextInChain; WGPUTextureFormat format; const void *blend; uint32_t writeMask; } WGPUColorTargetState;
typedef struct WGPUFragmentState { const WGPUChainedStruct *nextInChain; WGPUShaderModule module; WGPUStringView entryPoint; size_t constantCount; const void *constants; size_t targetCount; const WGPUColorTargetState *targets; } WGPUFragmentState;
typedef struct WGPUMultisampleState { const WGPUChainedStruct *nextInChain; uint32_t count; uint32_t mask; WGPUBool alphaToCoverageEnabled; } WGPUMultisampleState;
typedef struct WGPURenderPipelineDescriptor {
	const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUPipelineLayout layout;
	WGPUVertexState vertex; WGPUPrimitiveState primitive; const void *depthStencil;
	WGPUMultisampleState multisample; const WGPUFragmentState *fragment;
} WGPURenderPipelineDescriptor;

/* Render pass */
typedef struct WGPURenderPassColorAttachment {
	const WGPUChainedStruct *nextInChain; WGPUTextureView view; uint32_t depthSlice;
	WGPUTextureView resolveTarget; WGPULoadOp loadOp; WGPUStoreOp storeOp; WGPUColor clearValue;
} WGPURenderPassColorAttachment;
typedef struct WGPURenderPassDescriptor {
	const WGPUChainedStruct *nextInChain; WGPUStringView label;
	size_t colorAttachmentCount; const WGPURenderPassColorAttachment *colorAttachments;
	const void *depthStencilAttachment; WGPUQueue occlusionQuerySet; const void *timestampWrites;
} WGPURenderPassDescriptor;

typedef struct WGPUCommandEncoderDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUCommandEncoderDescriptor;
typedef struct WGPUCommandBufferDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUCommandBufferDescriptor;
typedef struct WGPUTextureViewDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUTextureViewDescriptor;

#ifdef __cplusplus
extern "C" {
#endif
#define WGPU_IMPORT(name) __attribute__((import_module("wgpu"), import_name(#name)))

WGPU_IMPORT(wgpuCreateInstance) WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor *desc);
WGPU_IMPORT(wgpuInstanceRequestAdapter) void wgpuInstanceRequestAdapter(WGPUInstance, const WGPURequestAdapterOptions *, WGPURequestAdapterCallback, void *userdata);
WGPU_IMPORT(wgpuAdapterRequestDevice) void wgpuAdapterRequestDevice(WGPUAdapter, const WGPUDeviceDescriptor *, WGPURequestDeviceCallback, void *userdata);
WGPU_IMPORT(wgpuDeviceGetQueue) WGPUQueue wgpuDeviceGetQueue(WGPUDevice);
WGPU_IMPORT(wgpuGetCanvasSurface) WGPUSurface wgpuGetCanvasSurface(WGPUInstance); /* browser: the page canvas as a surface */
WGPU_IMPORT(wgpuDeviceCreateShaderModule) WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice, const WGPUShaderModuleDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateRenderPipeline) WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice, const WGPURenderPipelineDescriptor *);
WGPU_IMPORT(wgpuSurfaceGetCurrentTexture) WGPUTexture wgpuSurfaceGetCurrentTexture(WGPUSurface);
WGPU_IMPORT(wgpuTextureCreateView) WGPUTextureView wgpuTextureCreateView(WGPUTexture, const WGPUTextureViewDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateCommandEncoder) WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice, const WGPUCommandEncoderDescriptor *);
WGPU_IMPORT(wgpuCommandEncoderBeginRenderPass) WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder, const WGPURenderPassDescriptor *);
WGPU_IMPORT(wgpuRenderPassEncoderSetPipeline) void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder, WGPURenderPipeline);
WGPU_IMPORT(wgpuRenderPassEncoderDraw) void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
WGPU_IMPORT(wgpuRenderPassEncoderEnd) void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder);
WGPU_IMPORT(wgpuCommandEncoderFinish) WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder, const WGPUCommandBufferDescriptor *);
WGPU_IMPORT(wgpuQueueSubmit) void wgpuQueueSubmit(WGPUQueue, size_t commandCount, const WGPUCommandBuffer *commands);
WGPU_IMPORT(wgpuSurfacePresent) void wgpuSurfacePresent(WGPUSurface);
WGPU_IMPORT(wgpuTextureViewRelease) void wgpuTextureViewRelease(WGPUTextureView);
WGPU_IMPORT(wgpuTextureRelease) void wgpuTextureRelease(WGPUTexture);
WGPU_IMPORT(wgpuCommandEncoderRelease) void wgpuCommandEncoderRelease(WGPUCommandEncoder);
WGPU_IMPORT(wgpuRenderPassEncoderRelease) void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder);
WGPU_IMPORT(wgpuCommandBufferRelease) void wgpuCommandBufferRelease(WGPUCommandBuffer);
#ifdef __cplusplus
}
#endif
#endif
