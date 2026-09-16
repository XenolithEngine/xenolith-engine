/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLRenderCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLRENDERCOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLRENDERCOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#include "MTLTypes.h"                       /* MTLSize */
#include "MTLCommandEncoder.h"              /* MTLCommandEncoder, MTLResourceUsage, MTLBarrierScope */
#include "MTLRenderPass.h"                  /* MTLStoreAction */
#include "MTLStageInputOutputDescriptor.h"  /* MTLIndexType */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLDepthStencilState;
@protocol MTLRenderPipelineState;
@protocol MTLResource;
@protocol MTLFence;
@protocol MTLCounterSampleBuffer;

typedef NS_ENUM(NSUInteger, MTLPrimitiveType) {
    MTLPrimitiveTypePoint         = 0,
    MTLPrimitiveTypeLine          = 1,
    MTLPrimitiveTypeLineStrip     = 2,
    MTLPrimitiveTypeTriangle      = 3,
    MTLPrimitiveTypeTriangleStrip = 4,
};

typedef NS_ENUM(NSUInteger, MTLVisibilityResultMode) {
    MTLVisibilityResultModeDisabled = 0,
    MTLVisibilityResultModeBoolean  = 1,
    MTLVisibilityResultModeCounting = 2,
};

typedef struct {
    NSUInteger x, y, width, height;
} MTLScissorRect;

typedef struct {
    double originX, originY, width, height, znear, zfar;
} MTLViewport;

typedef NS_ENUM(NSUInteger, MTLCullMode) {
    MTLCullModeNone  = 0,
    MTLCullModeFront = 1,
    MTLCullModeBack  = 2,
};

typedef NS_ENUM(NSUInteger, MTLWinding) {
    MTLWindingClockwise        = 0,
    MTLWindingCounterClockwise = 1,
};

typedef NS_ENUM(NSUInteger, MTLDepthClipMode) {
    MTLDepthClipModeClip  = 0,
    MTLDepthClipModeClamp = 1,
};

typedef NS_ENUM(NSUInteger, MTLTriangleFillMode) {
    MTLTriangleFillModeFill  = 0,
    MTLTriangleFillModeLines = 1,
};

typedef struct {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t vertexStart;
    uint32_t baseInstance;
} MTLDrawPrimitivesIndirectArguments;

typedef struct {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t indexStart;
    int32_t  baseVertex;
    uint32_t baseInstance;
} MTLDrawIndexedPrimitivesIndirectArguments;

typedef struct {
    uint32_t patchCount;
    uint32_t instanceCount;
    uint32_t patchStart;
    uint32_t baseInstance;
} MTLDrawPatchIndirectArguments;

typedef struct {
    /* edgeTessellationFactor and insideTessellationFactor are interpreted as half (16-bit floats) */
    uint16_t edgeTessellationFactor[4];
    uint16_t insideTessellationFactor[2];
} MTLQuadTessellationFactorsHalf;

/* Generic render stage; also used for points at which a fence may be waited on or signaled. */
typedef NS_OPTIONS(NSUInteger, MTLRenderStages) {
    MTLRenderStageVertex   = (1UL << 0),
    MTLRenderStageFragment = (1UL << 1),
};

/* Container for graphics rendering state and the encoded draw commands. */
@protocol MTLRenderCommandEncoder <MTLCommandEncoder>

- (void)setRenderPipelineState:(id <MTLRenderPipelineState>)pipelineState;

/* Vertex resources */
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;
- (void)setVertexBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setVertexBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setVertexBufferOffset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;
- (void)setVertexTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setVertexSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;

/* Fixed-function state */
- (void)setViewport:(MTLViewport)viewport;
- (void)setViewports:(const MTLViewport [__nonnull])viewports count:(NSUInteger)count;
- (void)setFrontFacingWinding:(MTLWinding)frontFacingWinding;
- (void)setCullMode:(MTLCullMode)cullMode;
- (void)setDepthClipMode:(MTLDepthClipMode)depthClipMode;
- (void)setDepthBias:(float)depthBias slopeScale:(float)slopeScale clamp:(float)clamp;
- (void)setScissorRect:(MTLScissorRect)rect;
- (void)setScissorRects:(const MTLScissorRect [__nonnull])scissorRects count:(NSUInteger)count;
- (void)setTriangleFillMode:(MTLTriangleFillMode)fillMode;

/* Fragment resources */
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setFragmentBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setFragmentTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setFragmentSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;

- (void)setBlendColorRed:(float)red green:(float)green blue:(float)blue alpha:(float)alpha;
- (void)setDepthStencilState:(nullable id <MTLDepthStencilState>)depthStencilState;
- (void)setStencilReferenceValue:(uint32_t)referenceValue;
- (void)setStencilFrontReferenceValue:(uint32_t)frontReferenceValue backReferenceValue:(uint32_t)backReferenceValue;
- (void)setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:(NSUInteger)offset;

- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)colorAttachmentIndex;
- (void)setDepthStoreAction:(MTLStoreAction)storeAction;
- (void)setStencilStoreAction:(MTLStoreAction)storeAction;

/* Drawing */
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount;
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount;
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance;
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset;

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount baseVertex:(NSInteger)baseVertex baseInstance:(NSUInteger)baseInstance;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset;

- (void)textureBarrier;

/* Fences */
- (void)updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages;
- (void)waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages;

/* Tessellation */
- (void)setTessellationFactorBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset instanceStride:(NSUInteger)instanceStride;
- (void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance;
- (void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset;

/* Argument-buffer residency */
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage;
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage stages:(MTLRenderStages)stages;
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage;
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage stages:(MTLRenderStages)stages;

/* Barriers */
- (void)memoryBarrierWithScope:(MTLBarrierScope)scope afterStages:(MTLRenderStages)after beforeStages:(MTLRenderStages)before;
- (void)memoryBarrierWithResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count afterStages:(MTLRenderStages)after beforeStages:(MTLRenderStages)before;

/* Counters */
- (void)sampleCountersInBuffer:(id <MTLCounterSampleBuffer>)sampleBuffer atSampleIndex:(NSUInteger)sampleIndex withBarrier:(BOOL)barrier;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLRENDERCOMMANDENCODER_H_ */
