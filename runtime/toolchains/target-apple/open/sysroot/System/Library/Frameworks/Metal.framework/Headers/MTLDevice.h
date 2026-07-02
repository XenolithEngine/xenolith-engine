/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDevice.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLDEVICE_H_
#define __SPRT_OPEN_METAL_MTLDEVICE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>       /* MTLSize, MTLRegion, MTLSamplePosition */
#import <Metal/MTLPixelFormat.h> /* MTLPixelFormat */
#import <Metal/MTLResource.h>    /* MTLResourceOptions, @protocol MTLResource */
#import <Metal/MTLTexture.h>     /* MTLTextureType (+ @protocol MTLTexture, MTLTextureDescriptor, MTLBuffer) */
#import <Metal/MTLArgument.h>    /* MTLDataType, MTLBindingAccess, @protocol MTLBufferBinding */

/* --- Metal protocols referenced only as id<> return/param types --- */
@protocol MTLCommandQueue;
@protocol MTLBuffer;
@protocol MTLDepthStencilState;
@protocol MTLFunction;
@protocol MTLLibrary;
@protocol MTLSamplerState;
@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;
@protocol MTLHeap;
@protocol MTLFence;
@protocol MTLArgumentEncoder;
@protocol MTLEvent;
@protocol MTLSharedEvent;
@protocol MTLCounterSet;
@protocol MTLCounterSampleBuffer;

/* --- descriptor / helper classes owned by sibling headers --- */
@class MTLHeapDescriptor;
@class MTLDepthStencilDescriptor;
@class MTLSamplerDescriptor;
@class MTLRenderPipelineDescriptor;
@class MTLComputePipelineDescriptor;
@class MTLRenderPipelineReflection;
@class MTLComputePipelineReflection;
@class MTLCompileOptions;
@class MTLSharedEventHandle;
@class MTLSharedTextureHandle;
@class MTLCounterSampleBufferDescriptor;

/* MTLCreateSystemDefaultDevice/MTLCopyAllDevices return id<MTLDevice> / arrays of it. */
@protocol MTLDevice;
@protocol MTLResidencySet;
@class MTLResidencySetDescriptor;

typedef NS_ENUM(NSUInteger, MTLDeviceLocation) {
	MTLDeviceLocationBuiltIn = 0,
	MTLDeviceLocationSlot = 1,
	MTLDeviceLocationExternal = 2,
	MTLDeviceLocationUnspecified = NSUIntegerMax,
};

extern id <MTLDevice> MTLCreateSystemDefaultDevice(void);
extern NSArray <id<MTLDevice>> *MTLCopyAllDevices(void);

typedef NS_ENUM(NSInteger, MTLGPUFamily)
{
    MTLGPUFamilyApple1  = 1001,
    MTLGPUFamilyApple2  = 1002,
    MTLGPUFamilyApple3  = 1003,
    MTLGPUFamilyApple4  = 1004,
    MTLGPUFamilyApple5  = 1005,
    MTLGPUFamilyApple6  = 1006,
    MTLGPUFamilyApple7  = 1007,
    MTLGPUFamilyApple8  = 1008,
    MTLGPUFamilyApple9  = 1009,

    MTLGPUFamilyMac1 = 2001,
    MTLGPUFamilyMac2 = 2002,

    MTLGPUFamilyCommon1 = 3001,
    MTLGPUFamilyCommon2 = 3002,
    MTLGPUFamilyCommon3 = 3003,

    MTLGPUFamilyMacCatalyst1 = 4001,
    MTLGPUFamilyMacCatalyst2 = 4002,

    MTLGPUFamilyMetal3 = 5001,
};

typedef NS_OPTIONS(NSUInteger, MTLPipelineOption)
{
    MTLPipelineOptionNone                    = 0,
    MTLPipelineOptionArgumentInfo            = 1 << 0,
    MTLPipelineOptionBufferTypeInfo          = 1 << 1,
    MTLPipelineOptionFailOnBinaryArchiveMiss = 1 << 2,
};

typedef NS_ENUM(NSUInteger, MTLReadWriteTextureTier)
{
    MTLReadWriteTextureTierNone = 0,
    MTLReadWriteTextureTier1    = 1,
    MTLReadWriteTextureTier2    = 2,
};

typedef NS_ENUM(NSUInteger, MTLArgumentBuffersTier)
{
    MTLArgumentBuffersTier1 = 0,
    MTLArgumentBuffersTier2 = 1,
};

typedef NS_ENUM(NSUInteger, MTLCounterSamplingPoint)
{
    MTLCounterSamplingPointAtStageBoundary,
    MTLCounterSamplingPointAtDrawBoundary,
    MTLCounterSamplingPointAtDispatchBoundary,
    MTLCounterSamplingPointAtTileDispatchBoundary,
    MTLCounterSamplingPointAtBlitBoundary,
};

/* Represents a memory size and alignment in bytes. */
typedef struct {
    NSUInteger size;
    NSUInteger align;
} MTLSizeAndAlign;

typedef uint64_t MTLTimestamp;

/* Convenience typedefs for reflection out-parameters. */
typedef MTLRenderPipelineReflection  *MTLAutoreleasedRenderPipelineReflection;
typedef MTLComputePipelineReflection *MTLAutoreleasedComputePipelineReflection;

typedef void (^MTLNewLibraryCompletionHandler)(id <MTLLibrary> library, NSError *error);
typedef void (^MTLNewRenderPipelineStateCompletionHandler)(id <MTLRenderPipelineState> renderPipelineState, NSError *error);
typedef void (^MTLNewRenderPipelineStateWithReflectionCompletionHandler)(id <MTLRenderPipelineState> renderPipelineState, MTLRenderPipelineReflection *reflection, NSError *error);
typedef void (^MTLNewComputePipelineStateCompletionHandler)(id <MTLComputePipelineState> computePipelineState, NSError *error);
typedef void (^MTLNewComputePipelineStateWithReflectionCompletionHandler)(id <MTLComputePipelineState> computePipelineState, MTLComputePipelineReflection *reflection, NSError *error);

/* Represents a member of an argument buffer. */
@interface MTLArgumentDescriptor : NSObject <NSCopying>
+ (MTLArgumentDescriptor *)argumentDescriptor;
@property (nonatomic) MTLDataType dataType;
@property (nonatomic) NSUInteger index;
@property (nonatomic) NSUInteger arrayLength;
@property (nonatomic) MTLBindingAccess access;
@property (nonatomic) MTLTextureType textureType;
@property (nonatomic) NSUInteger constantBlockAlignment;
@end

/* MTLDevice represents a processor capable of data parallel computations. */
@protocol MTLDevice <NSObject>

@property (readonly) NSString *name;
@property (readonly) uint64_t registryID;
@property (readonly) MTLSize maxThreadsPerThreadgroup;
@property (readonly, getter=isLowPower) BOOL lowPower;
@property (readonly, getter=isHeadless) BOOL headless;
@property (readonly, getter=isRemovable) BOOL removable;
@property (readonly) BOOL hasUnifiedMemory;
@property (readonly) uint64_t recommendedMaxWorkingSetSize;
@property (readonly) uint64_t maxTransferRate;
@property (readonly, getter=isDepth24Stencil8PixelFormatSupported) BOOL depth24Stencil8PixelFormatSupported;
@property (readonly) MTLReadWriteTextureTier readWriteTextureSupport;
@property (readonly) MTLArgumentBuffersTier argumentBuffersSupport;
@property (readonly, getter=areRasterOrderGroupsSupported) BOOL rasterOrderGroupsSupported;
@property (readonly) BOOL supports32BitFloatFiltering;
@property (readonly) BOOL supports32BitMSAA;
@property (readonly) BOOL supportsQueryTextureLOD;
@property (readonly) BOOL supportsBCTextureCompression;
@property (readonly) BOOL supportsPullModelInterpolation;
@property (readonly) BOOL supportsShaderBarycentricCoordinates;
@property (readonly) NSUInteger currentAllocatedSize;
@property (readonly) NSUInteger maxThreadgroupMemoryLength;
@property (readonly) NSUInteger maxArgumentBufferSamplerCount;
@property (readonly) NSUInteger maxBufferLength;
@property (readonly, getter=areProgrammableSamplePositionsSupported) BOOL programmableSamplePositionsSupported;
@property (readonly) NSArray<id<MTLCounterSet>> *counterSets;
@property (readonly) BOOL supportsDynamicLibraries;
@property (readonly) BOOL supportsRaytracing;
@property (readonly) BOOL supportsFunctionPointers;
@property (readonly) BOOL supportsFunctionPointersFromRender;
@property (readonly) BOOL supportsRaytracingFromRender;
@property (readonly) BOOL supportsPrimitiveMotionBlur;

- (id <MTLCommandQueue>)newCommandQueue;
- (id <MTLCommandQueue>)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)maxCommandBufferCount;

- (MTLSizeAndAlign)heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)desc;
- (MTLSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
- (id <MTLHeap>)newHeapWithDescriptor:(MTLHeapDescriptor *)descriptor;

- (id <MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
- (id <MTLBuffer>)newBufferWithBytes:(const void *)pointer length:(NSUInteger)length options:(MTLResourceOptions)options;
- (id <MTLBuffer>)newBufferWithBytesNoCopy:(void *)pointer length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:(void (^)(void *pointer, NSUInteger length))deallocator;

- (id <MTLDepthStencilState>)newDepthStencilStateWithDescriptor:(MTLDepthStencilDescriptor *)descriptor;

- (id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor;
- (id <MTLTexture>)newSharedTextureWithDescriptor:(MTLTextureDescriptor *)descriptor;
- (id <MTLTexture>)newSharedTextureWithHandle:(MTLSharedTextureHandle *)sharedHandle;

- (id <MTLSamplerState>)newSamplerStateWithDescriptor:(MTLSamplerDescriptor *)descriptor;

- (id <MTLLibrary>)newDefaultLibrary;
- (id <MTLLibrary>)newDefaultLibraryWithBundle:(NSBundle *)bundle error:(NSError **)error;
- (id <MTLLibrary>)newLibraryWithFile:(NSString *)filepath error:(NSError **)error;
- (id <MTLLibrary>)newLibraryWithURL:(NSURL *)url error:(NSError **)error;
- (id <MTLLibrary>)newLibraryWithSource:(NSString *)source options:(MTLCompileOptions *)options error:(NSError **)error;
- (void)newLibraryWithSource:(NSString *)source options:(MTLCompileOptions *)options completionHandler:(MTLNewLibraryCompletionHandler)completionHandler;

- (id <MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor error:(NSError **)error;
- (id <MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)options reflection:(MTLAutoreleasedRenderPipelineReflection *)reflection error:(NSError **)error;
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler;
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)options completionHandler:(MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler;

- (id <MTLComputePipelineState>)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction error:(NSError **)error;
- (id <MTLComputePipelineState>)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction options:(MTLPipelineOption)options reflection:(MTLAutoreleasedComputePipelineReflection *)reflection error:(NSError **)error;
- (void)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler;
- (void)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction options:(MTLPipelineOption)options completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler;
- (id <MTLComputePipelineState>)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:(MTLPipelineOption)options reflection:(MTLAutoreleasedComputePipelineReflection *)reflection error:(NSError **)error;
- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:(MTLPipelineOption)options completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler;

- (id <MTLFence>)newFence;
- (id <MTLEvent>)newEvent;
- (id <MTLSharedEvent>)newSharedEvent;
- (id <MTLSharedEvent>)newSharedEventWithHandle:(MTLSharedEventHandle *)sharedEventHandle;

- (BOOL)supportsFamily:(MTLGPUFamily)gpuFamily;
- (BOOL)supportsTextureSampleCount:(NSUInteger)sampleCount;
- (BOOL)supportsVertexAmplificationCount:(NSUInteger)count;
- (BOOL)supportsCounterSampling:(MTLCounterSamplingPoint)samplingPoint;

- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format;
- (NSUInteger)minimumTextureBufferAlignmentForPixelFormat:(MTLPixelFormat)format;

- (void)getDefaultSamplePositions:(MTLSamplePosition *)positions count:(NSUInteger)count;

- (id <MTLArgumentEncoder>)newArgumentEncoderWithArguments:(NSArray <MTLArgumentDescriptor *> *)arguments;
- (id <MTLArgumentEncoder>)newArgumentEncoderWithBufferBinding:(id <MTLBufferBinding>)bufferBinding;

- (id <MTLCounterSampleBuffer>)newCounterSampleBufferWithDescriptor:(MTLCounterSampleBufferDescriptor *)descriptor error:(NSError **)error;
- (void)sampleTimestamps:(MTLTimestamp *)cpuTimestamp gpuTimestamp:(MTLTimestamp *)gpuTimestamp;

/* macOS 15 residency sets (MTLResidencySet.h); MoltenVK calls this behind an OS check. */
- (nullable id <MTLResidencySet>)newResidencySetWithDescriptor:(MTLResidencySetDescriptor *)desc error:(NSError **)error;
@property (readonly) NSUInteger maximumConcurrentCompilationTaskCount;
@property (readonly) MTLDeviceLocation location;
@property (readonly) NSUInteger locationNumber;
@property (readonly) NSUInteger peerGroupID;
@property (readonly) uint32_t peerIndex;
@property (readonly) uint32_t peerCount;
@property (readonly) BOOL areBarycentricCoordsSupported;

@end

#endif /* __SPRT_OPEN_METAL_MTLDEVICE_H_ */
