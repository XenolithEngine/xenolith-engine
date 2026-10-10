/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLRenderCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLRENDERCOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLRENDERCOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#include "MTLTypes.h"                       /* MTLSize */
#include "MTLCommandEncoder.h"              /* MTLCommandEncoder, MTLResourceUsage, MTLBarrierScope */
#include "MTLRenderPass.h"                  /* MTLStoreAction */
#include "MTLStageInputOutputDescriptor.h"  /* MTLIndexType */
#include <__sprt_abi/Metal/MTLRenderCommandEncoder.h>

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
