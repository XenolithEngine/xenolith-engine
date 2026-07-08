/* webgpu.h — self-consistent WebGPU C ABI for the wasm/browser build.
 *
 * This is NOT wgpu-native's header: on wasm the engine talks to navigator.gpu through the
 * host binding in runtime/wasm-js/webgpu.mjs, which owns the byte layout of every struct
 * and the value of every enum here. The engine references these by NAME, so the numeric
 * values are internal — the only contract is header <-> webgpu.mjs. Native-only extensions
 * (WGPUInstanceExtras, wgpuDevicePoll, ...) are gated out by XL_WGPU_NATIVE_API == 0.
 *
 * Layout rules (wasm32): every field is 4-byte unless noted; WGPUStringView = {ptr,len} (8B);
 * WGPUColor = 4x f64 (8-byte aligned); handles are u32.
 */
#ifndef XL_WASM_WEBGPU_H
#define XL_WASM_WEBGPU_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t WGPUFlags;
typedef uint32_t WGPUBool;

/* Opaque handles: pointer types (like the canonical webgpu.h, so `= nullptr` works). On
 * wasm32 a pointer is a u32, which the host binding uses directly as its handle-table id
 * (0 == null). See webgpu.mjs. */
typedef struct WGPUInstanceImpl *WGPUInstance;
typedef struct WGPUAdapterImpl *WGPUAdapter;
typedef struct WGPUDeviceImpl *WGPUDevice;
typedef struct WGPUQueueImpl *WGPUQueue;
typedef struct WGPUSurfaceImpl *WGPUSurface;
typedef struct WGPUTextureImpl *WGPUTexture;
typedef struct WGPUTextureViewImpl *WGPUTextureView;
typedef struct WGPUSamplerImpl *WGPUSampler;
typedef struct WGPUShaderModuleImpl *WGPUShaderModule;
typedef struct WGPUBindGroupImpl *WGPUBindGroup;
typedef struct WGPUBindGroupLayoutImpl *WGPUBindGroupLayout;
typedef struct WGPUPipelineLayoutImpl *WGPUPipelineLayout;
typedef struct WGPURenderPipelineImpl *WGPURenderPipeline;
typedef struct WGPUComputePipelineImpl *WGPUComputePipeline;
typedef struct WGPUCommandEncoderImpl *WGPUCommandEncoder;
typedef struct WGPURenderPassEncoderImpl *WGPURenderPassEncoder;
typedef struct WGPUComputePassEncoderImpl *WGPUComputePassEncoder;
typedef struct WGPUCommandBufferImpl *WGPUCommandBuffer;
typedef struct WGPUBufferImpl *WGPUBuffer;

typedef struct WGPUStringView { const char *data; size_t length; } WGPUStringView;
#define WGPU_STRLEN ((size_t)-1)

/* ---- enums (values internal; kept in sync with webgpu.mjs) --------------------------- */
typedef enum WGPUSType {
	WGPUSType_ShaderSourceWGSL = 3,
	WGPUSType_InstanceExtras = 0x00030006,
	WGPUSType_NativeLimits = 0x00030004,
} WGPUSType;

typedef enum WGPUStatus { WGPUStatus_Success = 1, WGPUStatus_Error = 2 } WGPUStatus;
typedef enum WGPUOptionalBool { WGPUOptionalBool_False = 0, WGPUOptionalBool_True = 1, WGPUOptionalBool_Undefined = 2 } WGPUOptionalBool;
typedef enum WGPULogLevel { WGPULogLevel_Off = 0, WGPULogLevel_Error = 1, WGPULogLevel_Warn = 2, WGPULogLevel_Info = 3, WGPULogLevel_Debug = 4, WGPULogLevel_Trace = 5 } WGPULogLevel;

typedef enum WGPUBackendType {
	WGPUBackendType_Undefined = 0, WGPUBackendType_Null = 1, WGPUBackendType_WebGPU = 2,
	WGPUBackendType_D3D11 = 3, WGPUBackendType_D3D12 = 4, WGPUBackendType_Metal = 5,
	WGPUBackendType_Vulkan = 6, WGPUBackendType_OpenGL = 7, WGPUBackendType_OpenGLES = 8,
	WGPUBackendType_Force32 = 0x7FFFFFFF,
} WGPUBackendType;

typedef enum WGPUAdapterType {
	WGPUAdapterType_DiscreteGPU = 1, WGPUAdapterType_IntegratedGPU = 2, WGPUAdapterType_CPU = 3,
	WGPUAdapterType_Unknown = 4, WGPUAdapterType_Force32 = 0x7FFFFFFF,
} WGPUAdapterType;

typedef enum WGPUFeatureName {
	WGPUFeatureName_Undefined = 0,
	WGPUFeatureName_Depth32FloatStencil8 = 4,
	WGPUFeatureName_TextureComponentSwizzle = 0x00030001,
	WGPUFeatureName_Force32 = 0x7FFFFFFF,
} WGPUFeatureName;

typedef enum WGPUNativeFeature {
	WGPUNativeFeature_RayQuery = 0x00030009,
	WGPUNativeFeature_CooperativeMatrix = 0x00030016,
} WGPUNativeFeature;

typedef enum WGPUTextureFormat {
	WGPUTextureFormat_Undefined = 0,
	WGPUTextureFormat_R8Unorm = 1,
	WGPUTextureFormat_R16Float = 7,
	WGPUTextureFormat_RG8Unorm = 8,
	WGPUTextureFormat_R32Float = 12,
	WGPUTextureFormat_RGBA8Unorm = 18,
	WGPUTextureFormat_RGBA8UnormSrgb = 19,
	WGPUTextureFormat_BGRA8Unorm = 23,
	WGPUTextureFormat_BGRA8UnormSrgb = 24,
	WGPUTextureFormat_RGBA16Float = 33,
	WGPUTextureFormat_RGBA32Float = 34,
	WGPUTextureFormat_Depth16Unorm = 39,
	WGPUTextureFormat_Depth24PlusStencil8 = 42,
	WGPUTextureFormat_Depth32Float = 43,
	WGPUTextureFormat_Depth32FloatStencil8 = 44,
	WGPUTextureFormat_Force32 = 0x7FFFFFFF,
} WGPUTextureFormat;

typedef enum WGPULoadOp { WGPULoadOp_Undefined = 0, WGPULoadOp_Load = 1, WGPULoadOp_Clear = 2 } WGPULoadOp;
typedef enum WGPUStoreOp { WGPUStoreOp_Undefined = 0, WGPUStoreOp_Store = 1, WGPUStoreOp_Discard = 2 } WGPUStoreOp;
typedef enum WGPUPrimitiveTopology { WGPUPrimitiveTopology_PointList = 0, WGPUPrimitiveTopology_LineList = 1, WGPUPrimitiveTopology_LineStrip = 2, WGPUPrimitiveTopology_TriangleList = 3, WGPUPrimitiveTopology_TriangleStrip = 4 } WGPUPrimitiveTopology;
typedef enum WGPUCullMode { WGPUCullMode_None = 0, WGPUCullMode_Front = 1, WGPUCullMode_Back = 2 } WGPUCullMode;
typedef enum WGPUFrontFace { WGPUFrontFace_CCW = 0, WGPUFrontFace_CW = 1 } WGPUFrontFace;
typedef enum WGPUIndexFormat { WGPUIndexFormat_Undefined = 0, WGPUIndexFormat_Uint16 = 1, WGPUIndexFormat_Uint32 = 2 } WGPUIndexFormat;
typedef enum WGPUTextureDimension { WGPUTextureDimension_1D = 0, WGPUTextureDimension_2D = 1, WGPUTextureDimension_3D = 2 } WGPUTextureDimension;
typedef enum WGPUTextureViewDimension { WGPUTextureViewDimension_Undefined = 0, WGPUTextureViewDimension_1D = 1, WGPUTextureViewDimension_2D = 2, WGPUTextureViewDimension_2DArray = 3, WGPUTextureViewDimension_Cube = 4, WGPUTextureViewDimension_CubeArray = 5, WGPUTextureViewDimension_3D = 6 } WGPUTextureViewDimension;
typedef enum WGPUTextureAspect { WGPUTextureAspect_Undefined = 0, WGPUTextureAspect_All = 1, WGPUTextureAspect_StencilOnly = 2, WGPUTextureAspect_DepthOnly = 3 } WGPUTextureAspect;
typedef enum WGPUAddressMode { WGPUAddressMode_Undefined = 0, WGPUAddressMode_ClampToEdge = 1, WGPUAddressMode_Repeat = 2, WGPUAddressMode_MirrorRepeat = 3 } WGPUAddressMode;
typedef enum WGPUFilterMode { WGPUFilterMode_Undefined = 0, WGPUFilterMode_Nearest = 1, WGPUFilterMode_Linear = 2 } WGPUFilterMode;
typedef enum WGPUMipmapFilterMode { WGPUMipmapFilterMode_Undefined = 0, WGPUMipmapFilterMode_Nearest = 1, WGPUMipmapFilterMode_Linear = 2 } WGPUMipmapFilterMode;
typedef enum WGPUCompareFunction { WGPUCompareFunction_Undefined = 0, WGPUCompareFunction_Never = 1, WGPUCompareFunction_Less = 2, WGPUCompareFunction_Equal = 3, WGPUCompareFunction_LessEqual = 4, WGPUCompareFunction_Greater = 5, WGPUCompareFunction_NotEqual = 6, WGPUCompareFunction_GreaterEqual = 7, WGPUCompareFunction_Always = 8 } WGPUCompareFunction;
typedef enum WGPUBlendFactor { WGPUBlendFactor_Undefined = 0, WGPUBlendFactor_Zero = 1, WGPUBlendFactor_One = 2, WGPUBlendFactor_Src = 3, WGPUBlendFactor_OneMinusSrc = 4, WGPUBlendFactor_SrcAlpha = 5, WGPUBlendFactor_OneMinusSrcAlpha = 6, WGPUBlendFactor_Dst = 7, WGPUBlendFactor_OneMinusDst = 8, WGPUBlendFactor_DstAlpha = 9, WGPUBlendFactor_OneMinusDstAlpha = 10 } WGPUBlendFactor;
typedef enum WGPUBlendOperation { WGPUBlendOperation_Undefined = 0, WGPUBlendOperation_Add = 1, WGPUBlendOperation_Subtract = 2, WGPUBlendOperation_ReverseSubtract = 3, WGPUBlendOperation_Min = 4, WGPUBlendOperation_Max = 5 } WGPUBlendOperation;
typedef enum WGPUPresentMode { WGPUPresentMode_Undefined = 0, WGPUPresentMode_Fifo = 1, WGPUPresentMode_FifoRelaxed = 2, WGPUPresentMode_Immediate = 3, WGPUPresentMode_Mailbox = 4 } WGPUPresentMode;
typedef enum WGPUCompositeAlphaMode { WGPUCompositeAlphaMode_Auto = 0, WGPUCompositeAlphaMode_Opaque = 1, WGPUCompositeAlphaMode_Premultiplied = 2, WGPUCompositeAlphaMode_Unpremultiplied = 3, WGPUCompositeAlphaMode_Inherit = 4 } WGPUCompositeAlphaMode;
typedef enum WGPUBufferBindingType { WGPUBufferBindingType_BindingNotUsed = 0, WGPUBufferBindingType_Uniform = 2, WGPUBufferBindingType_Storage = 3, WGPUBufferBindingType_ReadOnlyStorage = 4 } WGPUBufferBindingType;
typedef enum WGPUSamplerBindingType { WGPUSamplerBindingType_BindingNotUsed = 0, WGPUSamplerBindingType_Filtering = 2, WGPUSamplerBindingType_NonFiltering = 3, WGPUSamplerBindingType_Comparison = 4 } WGPUSamplerBindingType;
typedef enum WGPUTextureSampleType { WGPUTextureSampleType_BindingNotUsed = 0, WGPUTextureSampleType_Float = 2, WGPUTextureSampleType_UnfilterableFloat = 3, WGPUTextureSampleType_Depth = 4, WGPUTextureSampleType_Sint = 5, WGPUTextureSampleType_Uint = 6 } WGPUTextureSampleType;
typedef enum WGPUStorageTextureAccess { WGPUStorageTextureAccess_BindingNotUsed = 0, WGPUStorageTextureAccess_WriteOnly = 2, WGPUStorageTextureAccess_ReadOnly = 3, WGPUStorageTextureAccess_ReadWrite = 4 } WGPUStorageTextureAccess;
typedef enum WGPUComponentSwizzle { WGPUComponentSwizzle_Undefined = 0, WGPUComponentSwizzle_Zero = 1, WGPUComponentSwizzle_One = 2, WGPUComponentSwizzle_R = 3, WGPUComponentSwizzle_G = 4, WGPUComponentSwizzle_B = 5, WGPUComponentSwizzle_A = 6 } WGPUComponentSwizzle;
typedef enum WGPUSurfaceGetCurrentTextureStatus { WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal = 1, WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal = 2, WGPUSurfaceGetCurrentTextureStatus_Timeout = 3, WGPUSurfaceGetCurrentTextureStatus_Outdated = 4, WGPUSurfaceGetCurrentTextureStatus_Lost = 5 } WGPUSurfaceGetCurrentTextureStatus;
typedef enum WGPUMapAsyncStatus { WGPUMapAsyncStatus_Success = 1, WGPUMapAsyncStatus_CallbackCancelled = 2, WGPUMapAsyncStatus_Error = 3, WGPUMapAsyncStatus_Aborted = 4 } WGPUMapAsyncStatus;
typedef enum WGPUQueueWorkDoneStatus { WGPUQueueWorkDoneStatus_Success = 1, WGPUQueueWorkDoneStatus_CallbackCancelled = 2, WGPUQueueWorkDoneStatus_Error = 3 } WGPUQueueWorkDoneStatus;
typedef enum WGPURequestDeviceStatus { WGPURequestDeviceStatus_Success = 1, WGPURequestDeviceStatus_CallbackCancelled = 2, WGPURequestDeviceStatus_Error = 3 } WGPURequestDeviceStatus;
typedef enum WGPURequestAdapterStatus { WGPURequestAdapterStatus_Success = 1, WGPURequestAdapterStatus_CallbackCancelled = 2, WGPURequestAdapterStatus_Unavailable = 3, WGPURequestAdapterStatus_Error = 4 } WGPURequestAdapterStatus;
typedef enum WGPUDeviceLostReason { WGPUDeviceLostReason_Unknown = 1, WGPUDeviceLostReason_Destroyed = 2, WGPUDeviceLostReason_CallbackCancelled = 3, WGPUDeviceLostReason_FailedCreation = 4 } WGPUDeviceLostReason;
typedef enum WGPUErrorType { WGPUErrorType_NoError = 1, WGPUErrorType_Validation = 2, WGPUErrorType_OutOfMemory = 3, WGPUErrorType_Internal = 4, WGPUErrorType_Unknown = 5 } WGPUErrorType;
typedef enum WGPUCallbackMode { WGPUCallbackMode_WaitAnyOnly = 1, WGPUCallbackMode_AllowProcessEvents = 2, WGPUCallbackMode_AllowSpontaneous = 3 } WGPUCallbackMode;

/* bit flags */
typedef WGPUFlags WGPUBufferUsage;
#define WGPUBufferUsage_None 0x0
#define WGPUBufferUsage_MapRead 0x1
#define WGPUBufferUsage_MapWrite 0x2
#define WGPUBufferUsage_CopySrc 0x4
#define WGPUBufferUsage_CopyDst 0x8
#define WGPUBufferUsage_Index 0x10
#define WGPUBufferUsage_Vertex 0x20
#define WGPUBufferUsage_Uniform 0x40
#define WGPUBufferUsage_Storage 0x80
#define WGPUBufferUsage_Indirect 0x100
typedef WGPUFlags WGPUTextureUsage;
#define WGPUTextureUsage_None 0x0
#define WGPUTextureUsage_CopySrc 0x1
#define WGPUTextureUsage_CopyDst 0x2
#define WGPUTextureUsage_TextureBinding 0x4
#define WGPUTextureUsage_StorageBinding 0x8
#define WGPUTextureUsage_RenderAttachment 0x10
typedef WGPUFlags WGPUShaderStage;
#define WGPUShaderStage_None 0x0
#define WGPUShaderStage_Vertex 0x1
#define WGPUShaderStage_Fragment 0x2
#define WGPUShaderStage_Compute 0x4
typedef WGPUFlags WGPUMapMode;
#define WGPUMapMode_None 0x0
#define WGPUMapMode_Read 0x1
#define WGPUMapMode_Write 0x2
typedef WGPUFlags WGPUInstanceBackend;
#define WGPUInstanceBackend_All 0x0
typedef WGPUFlags WGPUInstanceFlag;
#define WGPUInstanceFlag_Default 0x0
#define WGPUInstanceFlag_Debug 0x1
#define WGPUInstanceFlag_Validation 0x2

/* ---- structs ------------------------------------------------------------------------ */
typedef struct WGPUChainedStruct { const struct WGPUChainedStruct *next; WGPUSType sType; } WGPUChainedStruct;
typedef struct WGPUColor { double r, g, b, a; } WGPUColor;
typedef struct WGPUExtent3D { uint32_t width; uint32_t height; uint32_t depthOrArrayLayers; } WGPUExtent3D;
typedef struct WGPUOrigin3D { uint32_t x, y, z; } WGPUOrigin3D;

typedef struct WGPUAdapterInfo {
	WGPUChainedStruct *nextInChain; WGPUStringView vendor; WGPUStringView architecture;
	WGPUStringView device; WGPUStringView description; WGPUBackendType backendType;
	WGPUAdapterType adapterType; uint32_t vendorID; uint32_t deviceID;
} WGPUAdapterInfo;

typedef struct WGPULimits {
	const WGPUChainedStruct *nextInChain;
	uint32_t maxTextureDimension1D, maxTextureDimension2D, maxTextureDimension3D, maxTextureArrayLayers;
	uint32_t maxBindGroups, maxBindGroupsPlusVertexBuffers, maxBindingsPerBindGroup;
	uint32_t maxDynamicUniformBuffersPerPipelineLayout, maxDynamicStorageBuffersPerPipelineLayout;
	uint32_t maxSampledTexturesPerShaderStage, maxSamplersPerShaderStage, maxStorageBuffersPerShaderStage;
	uint32_t maxStorageTexturesPerShaderStage, maxUniformBuffersPerShaderStage;
	uint64_t maxUniformBufferBindingSize, maxStorageBufferBindingSize;
	uint32_t minUniformBufferOffsetAlignment, minStorageBufferOffsetAlignment;
	uint32_t maxVertexBuffers; uint64_t maxBufferSize;
	uint32_t maxVertexAttributes, maxVertexBufferArrayStride, maxInterStageShaderVariables;
	uint32_t maxColorAttachments, maxColorAttachmentBytesPerSample;
	uint32_t maxComputeWorkgroupStorageSize, maxComputeInvocationsPerWorkgroup;
	uint32_t maxComputeWorkgroupSizeX, maxComputeWorkgroupSizeY, maxComputeWorkgroupSizeZ;
	uint32_t maxComputeWorkgroupsPerDimension;
} WGPULimits;
typedef struct WGPUNativeLimits { WGPUChainedStruct chain; uint32_t maxPushConstantSize, maxNonSamplerBindings; } WGPUNativeLimits;

typedef struct WGPUSupportedFeatures { size_t featureCount; const WGPUFeatureName *features; } WGPUSupportedFeatures;

typedef struct WGPUShaderSourceWGSL { WGPUChainedStruct chain; WGPUStringView code; } WGPUShaderSourceWGSL;
typedef struct WGPUShaderModuleDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUShaderModuleDescriptor;
typedef struct WGPUInstanceDescriptor { const WGPUChainedStruct *nextInChain; } WGPUInstanceDescriptor;
typedef struct WGPUInstanceExtras { WGPUChainedStruct chain; WGPUInstanceBackend backends; WGPUFlags flags; int dx12ShaderCompiler; int gles3MinorVersion; WGPUStringView dxilPath; WGPUStringView dxcPath; } WGPUInstanceExtras;

typedef struct WGPURequestAdapterOptions { const WGPUChainedStruct *nextInChain; WGPUSurface compatibleSurface; int powerPreference; WGPUBackendType backendType; WGPUBool forceFallbackAdapter; } WGPURequestAdapterOptions;

/* callback-info style (modern webgpu.h): messages are WGPUStringView by value, two userdata. */
typedef void (*WGPURequestAdapterCallback)(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPURequestDeviceCallback)(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPUBufferMapCallback)(WGPUMapAsyncStatus status, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPUQueueWorkDoneCallback)(WGPUQueueWorkDoneStatus status, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPUDeviceLostCallback)(WGPUDevice const *device, WGPUDeviceLostReason reason, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPUUncapturedErrorCallback)(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *userdata1, void *userdata2);
typedef struct WGPURequestAdapterCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUCallbackMode mode; WGPURequestAdapterCallback callback; void *userdata1; void *userdata2; } WGPURequestAdapterCallbackInfo;
typedef struct WGPURequestDeviceCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUCallbackMode mode; WGPURequestDeviceCallback callback; void *userdata1; void *userdata2; } WGPURequestDeviceCallbackInfo;
typedef struct WGPUBufferMapCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUCallbackMode mode; WGPUBufferMapCallback callback; void *userdata1; void *userdata2; } WGPUBufferMapCallbackInfo;
typedef struct WGPUQueueWorkDoneCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUCallbackMode mode; WGPUQueueWorkDoneCallback callback; void *userdata1; void *userdata2; } WGPUQueueWorkDoneCallbackInfo;
typedef struct WGPUDeviceLostCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUCallbackMode mode; WGPUDeviceLostCallback callback; void *userdata1; void *userdata2; } WGPUDeviceLostCallbackInfo;
typedef struct WGPUUncapturedErrorCallbackInfo { const WGPUChainedStruct *nextInChain; WGPUUncapturedErrorCallback callback; void *userdata1; void *userdata2; } WGPUUncapturedErrorCallbackInfo;

typedef struct WGPUDeviceDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; size_t requiredFeatureCount; const WGPUFeatureName *requiredFeatures; const WGPULimits *requiredLimits; WGPUDeviceLostCallbackInfo deviceLostCallbackInfo; WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo; } WGPUDeviceDescriptor;

/* Native surface sources (X11/Wayland) — unused in the browser (the canvas is the surface),
 * stubbed so the platform surface-creation code compiles under XL_WGPU_NATIVE_API == 0. */
typedef struct WGPUSurfaceDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUSurfaceDescriptor;
typedef struct WGPUSurfaceSourceXcbWindow { WGPUChainedStruct chain; void *connection; uint32_t window; } WGPUSurfaceSourceXcbWindow;
typedef WGPUSurfaceSourceXcbWindow WGPUSurfaceSourceXCBWindow;
typedef struct WGPUSurfaceSourceWaylandSurface { WGPUChainedStruct chain; void *display; void *surface; } WGPUSurfaceSourceWaylandSurface;
#define WGPU_SURFACE_DESCRIPTOR_INIT WGPUSurfaceDescriptor{}
#define WGPU_SURFACE_SOURCE_XCB_WINDOW_INIT WGPUSurfaceSourceXcbWindow{}
#define WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT WGPUSurfaceSourceWaylandSurface{}

typedef struct WGPUBufferDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUBufferUsage usage; uint64_t size; WGPUBool mappedAtCreation; } WGPUBufferDescriptor;
typedef struct WGPUSamplerDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUAddressMode addressModeU, addressModeV, addressModeW; WGPUFilterMode magFilter, minFilter; WGPUMipmapFilterMode mipmapFilter; float lodMinClamp, lodMaxClamp; WGPUCompareFunction compare; uint16_t maxAnisotropy; } WGPUSamplerDescriptor;
typedef struct WGPUTextureDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUTextureUsage usage; WGPUTextureDimension dimension; WGPUExtent3D size; WGPUTextureFormat format; uint32_t mipLevelCount; uint32_t sampleCount; size_t viewFormatCount; const WGPUTextureFormat *viewFormats; } WGPUTextureDescriptor;
typedef struct WGPUTextureComponentSwizzle { WGPUComponentSwizzle r, g, b, a; } WGPUTextureComponentSwizzle;
typedef struct WGPUTextureComponentSwizzleDescriptor { WGPUChainedStruct chain; WGPUTextureComponentSwizzle swizzle; } WGPUTextureComponentSwizzleDescriptor;
typedef struct WGPUTextureViewDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUTextureFormat format; WGPUTextureViewDimension dimension; uint32_t baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount; WGPUTextureAspect aspect; WGPUTextureUsage usage; } WGPUTextureViewDescriptor;

typedef struct WGPUBufferBindingLayout { const WGPUChainedStruct *nextInChain; WGPUBufferBindingType type; WGPUBool hasDynamicOffset; uint64_t minBindingSize; } WGPUBufferBindingLayout;
typedef struct WGPUSamplerBindingLayout { const WGPUChainedStruct *nextInChain; WGPUSamplerBindingType type; } WGPUSamplerBindingLayout;
typedef struct WGPUTextureBindingLayout { const WGPUChainedStruct *nextInChain; WGPUTextureSampleType sampleType; WGPUTextureViewDimension viewDimension; WGPUBool multisampled; } WGPUTextureBindingLayout;
typedef struct WGPUStorageTextureBindingLayout { const WGPUChainedStruct *nextInChain; WGPUStorageTextureAccess access; WGPUTextureFormat format; WGPUTextureViewDimension viewDimension; } WGPUStorageTextureBindingLayout;
typedef struct WGPUBindGroupLayoutEntry { const WGPUChainedStruct *nextInChain; uint32_t binding; WGPUShaderStage visibility; WGPUBufferBindingLayout buffer; WGPUSamplerBindingLayout sampler; WGPUTextureBindingLayout texture; WGPUStorageTextureBindingLayout storageTexture; } WGPUBindGroupLayoutEntry;
typedef struct WGPUBindGroupLayoutDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; size_t entryCount; const WGPUBindGroupLayoutEntry *entries; } WGPUBindGroupLayoutDescriptor;
typedef struct WGPUBindGroupEntry { const WGPUChainedStruct *nextInChain; uint32_t binding; WGPUBuffer buffer; uint64_t offset; uint64_t size; WGPUSampler sampler; WGPUTextureView textureView; } WGPUBindGroupEntry;
typedef struct WGPUBindGroupDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUBindGroupLayout layout; size_t entryCount; const WGPUBindGroupEntry *entries; } WGPUBindGroupDescriptor;
typedef struct WGPUPipelineLayoutDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; size_t bindGroupLayoutCount; const WGPUBindGroupLayout *bindGroupLayouts; } WGPUPipelineLayoutDescriptor;

typedef struct WGPUBlendComponent { WGPUBlendOperation operation; WGPUBlendFactor srcFactor; WGPUBlendFactor dstFactor; } WGPUBlendComponent;
typedef struct WGPUBlendState { WGPUBlendComponent color; WGPUBlendComponent alpha; } WGPUBlendState;
typedef struct WGPUColorTargetState { const WGPUChainedStruct *nextInChain; WGPUTextureFormat format; const WGPUBlendState *blend; uint32_t writeMask; } WGPUColorTargetState;
typedef struct WGPUStencilFaceState { WGPUCompareFunction compare; int failOp, depthFailOp, passOp; } WGPUStencilFaceState;
typedef struct WGPUDepthStencilState { const WGPUChainedStruct *nextInChain; WGPUTextureFormat format; WGPUOptionalBool depthWriteEnabled; WGPUCompareFunction depthCompare; WGPUStencilFaceState stencilFront, stencilBack; uint32_t stencilReadMask, stencilWriteMask; int32_t depthBias; float depthBiasSlopeScale, depthBiasClamp; } WGPUDepthStencilState;
typedef struct WGPUVertexState { const WGPUChainedStruct *nextInChain; WGPUShaderModule module; WGPUStringView entryPoint; size_t constantCount; const void *constants; size_t bufferCount; const void *buffers; } WGPUVertexState;
typedef struct WGPUPrimitiveState { const WGPUChainedStruct *nextInChain; WGPUPrimitiveTopology topology; WGPUIndexFormat stripIndexFormat; WGPUFrontFace frontFace; WGPUCullMode cullMode; WGPUBool unclippedDepth; } WGPUPrimitiveState;
typedef struct WGPUMultisampleState { const WGPUChainedStruct *nextInChain; uint32_t count; uint32_t mask; WGPUBool alphaToCoverageEnabled; } WGPUMultisampleState;
typedef struct WGPUFragmentState { const WGPUChainedStruct *nextInChain; WGPUShaderModule module; WGPUStringView entryPoint; size_t constantCount; const void *constants; size_t targetCount; const WGPUColorTargetState *targets; } WGPUFragmentState;
typedef struct WGPURenderPipelineDescriptor {
	const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUPipelineLayout layout;
	WGPUVertexState vertex; WGPUPrimitiveState primitive; const WGPUDepthStencilState *depthStencil;
	WGPUMultisampleState multisample; const WGPUFragmentState *fragment;
} WGPURenderPipelineDescriptor;
typedef struct WGPUProgrammableStageDescriptor { const WGPUChainedStruct *nextInChain; WGPUShaderModule module; WGPUStringView entryPoint; size_t constantCount; const void *constants; } WGPUProgrammableStageDescriptor;
typedef struct WGPUComputePipelineDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; WGPUPipelineLayout layout; WGPUProgrammableStageDescriptor compute; } WGPUComputePipelineDescriptor;

typedef struct WGPURenderPassColorAttachment { const WGPUChainedStruct *nextInChain; WGPUTextureView view; uint32_t depthSlice; WGPUTextureView resolveTarget; WGPULoadOp loadOp; WGPUStoreOp storeOp; WGPUColor clearValue; } WGPURenderPassColorAttachment;
typedef struct WGPURenderPassDepthStencilAttachment { WGPUTextureView view; WGPULoadOp depthLoadOp; WGPUStoreOp depthStoreOp; float depthClearValue; WGPUBool depthReadOnly; WGPULoadOp stencilLoadOp; WGPUStoreOp stencilStoreOp; uint32_t stencilClearValue; WGPUBool stencilReadOnly; } WGPURenderPassDepthStencilAttachment;
typedef struct WGPURenderPassDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; size_t colorAttachmentCount; const WGPURenderPassColorAttachment *colorAttachments; const WGPURenderPassDepthStencilAttachment *depthStencilAttachment; void *occlusionQuerySet; const void *timestampWrites; } WGPURenderPassDescriptor;
typedef struct WGPUComputePassDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; const void *timestampWrites; } WGPUComputePassDescriptor;
typedef struct WGPUCommandEncoderDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUCommandEncoderDescriptor;
typedef struct WGPUCommandBufferDescriptor { const WGPUChainedStruct *nextInChain; WGPUStringView label; } WGPUCommandBufferDescriptor;

typedef struct WGPUTexelCopyBufferLayout { uint64_t offset; uint32_t bytesPerRow; uint32_t rowsPerImage; } WGPUTexelCopyBufferLayout;
typedef struct WGPUTexelCopyBufferInfo { WGPUTexelCopyBufferLayout layout; WGPUBuffer buffer; } WGPUTexelCopyBufferInfo;
typedef struct WGPUTexelCopyTextureInfo { WGPUTexture texture; uint32_t mipLevel; WGPUOrigin3D origin; WGPUTextureAspect aspect; } WGPUTexelCopyTextureInfo;

typedef struct WGPUSurfaceConfiguration { const WGPUChainedStruct *nextInChain; WGPUDevice device; WGPUTextureFormat format; WGPUTextureUsage usage; uint32_t width; uint32_t height; size_t viewFormatCount; const WGPUTextureFormat *viewFormats; WGPUCompositeAlphaMode alphaMode; WGPUPresentMode presentMode; } WGPUSurfaceConfiguration;
typedef struct WGPUSurfaceCapabilities { const WGPUChainedStruct *nextInChain; WGPUTextureUsage usages; size_t formatCount; const WGPUTextureFormat *formats; size_t presentModeCount; const WGPUPresentMode *presentModes; size_t alphaModeCount; const WGPUCompositeAlphaMode *alphaModes; } WGPUSurfaceCapabilities;
typedef struct WGPUSurfaceTexture { const WGPUChainedStruct *nextInChain; WGPUTexture texture; WGPUSurfaceGetCurrentTextureStatus status; } WGPUSurfaceTexture;

/* ---- INIT macros (zero-init; the browser binding fills the meaningful defaults) ------ */
#define WGPU_ADAPTER_INFO_INIT WGPUAdapterInfo{}
#define WGPU_BIND_GROUP_DESCRIPTOR_INIT WGPUBindGroupDescriptor{}
#define WGPU_BIND_GROUP_ENTRY_INIT WGPUBindGroupEntry{}
#define WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT WGPUBindGroupLayoutDescriptor{}
#define WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT WGPUBindGroupLayoutEntry{}
#define WGPU_BLEND_STATE_INIT WGPUBlendState{}
#define WGPU_BUFFER_DESCRIPTOR_INIT WGPUBufferDescriptor{}
#define WGPU_BUFFER_MAP_CALLBACK_INFO_INIT WGPUBufferMapCallbackInfo{}
/* writeMask defaults to All (0xF), matching wgpu-native's WGPU_COLOR_TARGET_STATE_INIT — the
 * engine relies on this default and never sets writeMask, so a zero here writes no color. */
#define WGPU_COLOR_TARGET_STATE_INIT WGPUColorTargetState{nullptr, WGPUTextureFormat_Undefined, nullptr, 0xF}
#define WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT WGPUComputePipelineDescriptor{}
#define WGPU_DEPTH_STENCIL_STATE_INIT WGPUDepthStencilState{}
#define WGPU_DEVICE_DESCRIPTOR_INIT WGPUDeviceDescriptor{}
#define WGPU_FRAGMENT_STATE_INIT WGPUFragmentState{}
#define WGPU_INSTANCE_DESCRIPTOR_INIT WGPUInstanceDescriptor{}
#define WGPU_LIMITS_INIT WGPULimits{}
#define WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT WGPUPipelineLayoutDescriptor{}
#define WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT WGPUQueueWorkDoneCallbackInfo{}
#define WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT WGPURenderPassColorAttachment{}
#define WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT WGPURenderPassDepthStencilAttachment{}
#define WGPU_RENDER_PASS_DESCRIPTOR_INIT WGPURenderPassDescriptor{}
#define WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT WGPURenderPipelineDescriptor{}
#define WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT WGPURequestDeviceCallbackInfo{}
#define WGPU_SAMPLER_DESCRIPTOR_INIT WGPUSamplerDescriptor{}
#define WGPU_SHADER_MODULE_DESCRIPTOR_INIT WGPUShaderModuleDescriptor{}
#define WGPU_SHADER_SOURCE_WGSL_INIT WGPUShaderSourceWGSL{}
#define WGPU_SURFACE_CONFIGURATION_INIT WGPUSurfaceConfiguration{}
#define WGPU_SURFACE_TEXTURE_INIT WGPUSurfaceTexture{}
#define WGPU_TEXEL_COPY_TEXTURE_INFO_INIT WGPUTexelCopyTextureInfo{}
#define WGPU_TEXTURE_COMPONENT_SWIZZLE_DESCRIPTOR_INIT WGPUTextureComponentSwizzleDescriptor{}
#define WGPU_TEXTURE_DESCRIPTOR_INIT WGPUTextureDescriptor{}
#define WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT WGPUTextureViewDescriptor{}

/* ---- functions (host imports; module "wgpu") ---------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
#define WGPU_IMPORT(name) __attribute__((import_module("wgpu"), import_name(#name)))

WGPU_IMPORT(wgpuCreateInstance) WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor *);
WGPU_IMPORT(wgpuInstanceRequestAdapter) void wgpuInstanceRequestAdapter(WGPUInstance, const WGPURequestAdapterOptions *, WGPURequestAdapterCallbackInfo);
WGPU_IMPORT(wgpuInstanceRelease) void wgpuInstanceRelease(WGPUInstance);
/* Native-model helpers used by the engine's adapter enumeration + poll loop; on the browser
 * they are no-ops (the async model runs on the worker event loop). Declared so the code
 * compiles; the host binding stubs them. */
WGPU_IMPORT(wgpuInstanceProcessEvents) void wgpuInstanceProcessEvents(WGPUInstance);
WGPU_IMPORT(wgpuInstanceCreateSurface) WGPUSurface wgpuInstanceCreateSurface(WGPUInstance, const WGPUSurfaceDescriptor *);
typedef void (*WGPULogCallback)(WGPULogLevel level, WGPUStringView message, void *userdata);
WGPU_IMPORT(wgpuSetLogLevel) void wgpuSetLogLevel(WGPULogLevel);
WGPU_IMPORT(wgpuSetLogCallback) void wgpuSetLogCallback(WGPULogCallback, void *);
WGPU_IMPORT(wgpuInstanceEnumerateAdapters) size_t wgpuInstanceEnumerateAdapters(WGPUInstance, const void *options, WGPUAdapter *adapters);
WGPU_IMPORT(wgpuDevicePoll) WGPUBool wgpuDevicePoll(WGPUDevice, WGPUBool wait, const void *wrappedSubmissionIndex);
WGPU_IMPORT(wgpuAdapterRequestDevice) void wgpuAdapterRequestDevice(WGPUAdapter, const WGPUDeviceDescriptor *, WGPURequestDeviceCallbackInfo);
WGPU_IMPORT(wgpuAdapterGetInfo) WGPUStatus wgpuAdapterGetInfo(WGPUAdapter, WGPUAdapterInfo *);
WGPU_IMPORT(wgpuAdapterGetLimits) WGPUStatus wgpuAdapterGetLimits(WGPUAdapter, WGPULimits *);
WGPU_IMPORT(wgpuAdapterGetFeatures) void wgpuAdapterGetFeatures(WGPUAdapter, WGPUSupportedFeatures *);
WGPU_IMPORT(wgpuAdapterAddRef) void wgpuAdapterAddRef(WGPUAdapter);
WGPU_IMPORT(wgpuAdapterRelease) void wgpuAdapterRelease(WGPUAdapter);
WGPU_IMPORT(wgpuAdapterInfoFreeMembers) void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo);
WGPU_IMPORT(wgpuSupportedFeaturesFreeMembers) void wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures);
WGPU_IMPORT(wgpuDeviceGetQueue) WGPUQueue wgpuDeviceGetQueue(WGPUDevice);
WGPU_IMPORT(wgpuDeviceGetLimits) WGPUStatus wgpuDeviceGetLimits(WGPUDevice, WGPULimits *);
WGPU_IMPORT(wgpuDeviceGetFeatures) void wgpuDeviceGetFeatures(WGPUDevice, WGPUSupportedFeatures *);
WGPU_IMPORT(wgpuDeviceCreateShaderModule) WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice, const WGPUShaderModuleDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateBuffer) WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice, const WGPUBufferDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateTexture) WGPUTexture wgpuDeviceCreateTexture(WGPUDevice, const WGPUTextureDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateSampler) WGPUSampler wgpuDeviceCreateSampler(WGPUDevice, const WGPUSamplerDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateBindGroupLayout) WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice, const WGPUBindGroupLayoutDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateBindGroup) WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice, const WGPUBindGroupDescriptor *);
WGPU_IMPORT(wgpuDeviceCreatePipelineLayout) WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice, const WGPUPipelineLayoutDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateRenderPipeline) WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice, const WGPURenderPipelineDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateComputePipeline) WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice, const WGPUComputePipelineDescriptor *);
WGPU_IMPORT(wgpuDeviceCreateCommandEncoder) WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice, const WGPUCommandEncoderDescriptor *);
WGPU_IMPORT(wgpuDeviceRelease) void wgpuDeviceRelease(WGPUDevice);
WGPU_IMPORT(wgpuQueueSubmit) void wgpuQueueSubmit(WGPUQueue, size_t, const WGPUCommandBuffer *);
WGPU_IMPORT(wgpuQueueWriteBuffer) void wgpuQueueWriteBuffer(WGPUQueue, WGPUBuffer, uint64_t, const void *, size_t);
WGPU_IMPORT(wgpuQueueWriteTexture) void wgpuQueueWriteTexture(WGPUQueue, const WGPUTexelCopyTextureInfo *, const void *, size_t, const WGPUTexelCopyBufferLayout *, const WGPUExtent3D *);
WGPU_IMPORT(wgpuQueueOnSubmittedWorkDone) void wgpuQueueOnSubmittedWorkDone(WGPUQueue, WGPUQueueWorkDoneCallbackInfo);
WGPU_IMPORT(wgpuQueueRelease) void wgpuQueueRelease(WGPUQueue);
WGPU_IMPORT(wgpuBufferGetMappedRange) void *wgpuBufferGetMappedRange(WGPUBuffer, size_t, size_t);
WGPU_IMPORT(wgpuBufferGetConstMappedRange) const void *wgpuBufferGetConstMappedRange(WGPUBuffer, size_t, size_t);
WGPU_IMPORT(wgpuBufferMapAsync) void wgpuBufferMapAsync(WGPUBuffer, WGPUMapMode, size_t, size_t, WGPUBufferMapCallbackInfo);
WGPU_IMPORT(wgpuBufferUnmap) void wgpuBufferUnmap(WGPUBuffer);
WGPU_IMPORT(wgpuBufferRelease) void wgpuBufferRelease(WGPUBuffer);
WGPU_IMPORT(wgpuTextureCreateView) WGPUTextureView wgpuTextureCreateView(WGPUTexture, const WGPUTextureViewDescriptor *);
WGPU_IMPORT(wgpuTextureRelease) void wgpuTextureRelease(WGPUTexture);
WGPU_IMPORT(wgpuTextureViewRelease) void wgpuTextureViewRelease(WGPUTextureView);
WGPU_IMPORT(wgpuSamplerRelease) void wgpuSamplerRelease(WGPUSampler);
WGPU_IMPORT(wgpuShaderModuleRelease) void wgpuShaderModuleRelease(WGPUShaderModule);
WGPU_IMPORT(wgpuBindGroupRelease) void wgpuBindGroupRelease(WGPUBindGroup);
WGPU_IMPORT(wgpuBindGroupLayoutAddRef) void wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout);
WGPU_IMPORT(wgpuBindGroupLayoutRelease) void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout);
WGPU_IMPORT(wgpuPipelineLayoutRelease) void wgpuPipelineLayoutRelease(WGPUPipelineLayout);
WGPU_IMPORT(wgpuRenderPipelineRelease) void wgpuRenderPipelineRelease(WGPURenderPipeline);
WGPU_IMPORT(wgpuComputePipelineRelease) void wgpuComputePipelineRelease(WGPUComputePipeline);
WGPU_IMPORT(wgpuCommandEncoderBeginRenderPass) WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder, const WGPURenderPassDescriptor *);
WGPU_IMPORT(wgpuCommandEncoderBeginComputePass) WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder, const WGPUComputePassDescriptor *);
WGPU_IMPORT(wgpuCommandEncoderCopyTextureToBuffer) void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder, const WGPUTexelCopyTextureInfo *, const WGPUTexelCopyBufferInfo *, const WGPUExtent3D *);
WGPU_IMPORT(wgpuCommandEncoderFinish) WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder, const WGPUCommandBufferDescriptor *);
WGPU_IMPORT(wgpuCommandEncoderRelease) void wgpuCommandEncoderRelease(WGPUCommandEncoder);
WGPU_IMPORT(wgpuRenderPassEncoderSetPipeline) void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder, WGPURenderPipeline);
WGPU_IMPORT(wgpuRenderPassEncoderSetBindGroup) void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder, uint32_t, WGPUBindGroup, size_t, const uint32_t *);
WGPU_IMPORT(wgpuRenderPassEncoderSetIndexBuffer) void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder, WGPUBuffer, WGPUIndexFormat, uint64_t, uint64_t);
WGPU_IMPORT(wgpuRenderPassEncoderSetScissorRect) void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder, uint32_t, uint32_t, uint32_t, uint32_t);
WGPU_IMPORT(wgpuRenderPassEncoderDraw) void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder, uint32_t, uint32_t, uint32_t, uint32_t);
WGPU_IMPORT(wgpuRenderPassEncoderDrawIndexed) void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
WGPU_IMPORT(wgpuRenderPassEncoderEnd) void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder);
WGPU_IMPORT(wgpuRenderPassEncoderRelease) void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder);
WGPU_IMPORT(wgpuComputePassEncoderSetPipeline) void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder, WGPUComputePipeline);
WGPU_IMPORT(wgpuComputePassEncoderSetBindGroup) void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder, uint32_t, WGPUBindGroup, size_t, const uint32_t *);
WGPU_IMPORT(wgpuComputePassEncoderDispatchWorkgroups) void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder, uint32_t, uint32_t, uint32_t);
WGPU_IMPORT(wgpuComputePassEncoderEnd) void wgpuComputePassEncoderEnd(WGPUComputePassEncoder);
WGPU_IMPORT(wgpuComputePassEncoderRelease) void wgpuComputePassEncoderRelease(WGPUComputePassEncoder);
WGPU_IMPORT(wgpuCommandBufferRelease) void wgpuCommandBufferRelease(WGPUCommandBuffer);
WGPU_IMPORT(wgpuSurfaceGetCapabilities) WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface, WGPUAdapter, WGPUSurfaceCapabilities *);
WGPU_IMPORT(wgpuSurfaceConfigure) void wgpuSurfaceConfigure(WGPUSurface, const WGPUSurfaceConfiguration *);
WGPU_IMPORT(wgpuSurfaceUnconfigure) void wgpuSurfaceUnconfigure(WGPUSurface);
WGPU_IMPORT(wgpuSurfaceGetCurrentTexture) void wgpuSurfaceGetCurrentTexture(WGPUSurface, WGPUSurfaceTexture *);
WGPU_IMPORT(wgpuSurfacePresent) WGPUStatus wgpuSurfacePresent(WGPUSurface);
WGPU_IMPORT(wgpuSurfaceCapabilitiesFreeMembers) void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities);
WGPU_IMPORT(wgpuSurfaceRelease) void wgpuSurfaceRelease(WGPUSurface);
#ifdef __cplusplus
}
#endif
#endif /* XL_WASM_WEBGPU_H */
