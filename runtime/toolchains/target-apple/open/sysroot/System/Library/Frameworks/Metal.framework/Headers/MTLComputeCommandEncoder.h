/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLComputeCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMPUTECOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLCOMPUTECOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLCommandEncoder.h>   /* <MTLCommandEncoder>, MTLResourceUsage, MTLBarrierScope */
#import <Metal/MTLCommandBuffer.h>    /* MTLDispatchType */
#import <Metal/MTLTypes.h>            /* MTLSize, MTLRegion */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLComputePipelineState;
@protocol MTLResource;
@protocol MTLHeap;
@protocol MTLFence;
@protocol MTLCounterSampleBuffer;

typedef struct {
    uint32_t threadgroupsPerGrid[3];
} MTLDispatchThreadgroupsIndirectArguments;

typedef struct {
    uint32_t stageInOrigin[3];
    uint32_t stageInSize[3];
} MTLStageInRegionIndirectArguments;

@protocol MTLComputeCommandEncoder <MTLCommandEncoder>

@property (readonly) MTLDispatchType dispatchType;

- (void)setComputePipelineState:(id <MTLComputePipelineState>)state;

- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBuffers:(const id <MTLBuffer> _Nullable [_Nonnull])buffers offsets:(const NSUInteger [_Nonnull])offsets withRange:(NSRange)range;

- (void)setBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;
- (void)setBuffers:(const id <MTLBuffer> _Nullable [_Nonnull])buffers offsets:(const NSUInteger [_Nonnull])offsets attributeStrides:(const NSUInteger [_Nonnull])strides withRange:(NSRange)range;
- (void)setBufferOffset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;
- (void)setBytes:(const void *)bytes length:(NSUInteger)length attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index;

- (void)setTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setTextures:(const id <MTLTexture> _Nullable [_Nonnull])textures withRange:(NSRange)range;

- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;
- (void)setSamplerStates:(const id <MTLSamplerState> _Nullable [_Nonnull])samplers withRange:(NSRange)range;
- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index;
- (void)setSamplerStates:(const id <MTLSamplerState> _Nullable [_Nonnull])samplers lodMinClamps:(const float [_Nonnull])lodMinClamps lodMaxClamps:(const float [_Nonnull])lodMaxClamps withRange:(NSRange)range;

- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setImageblockWidth:(NSUInteger)width height:(NSUInteger)height;

- (void)setStageInRegion:(MTLRegion)region;
- (void)setStageInRegionWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset;

- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;
- (void)dispatchThreadgroupsWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;
- (void)dispatchThreads:(MTLSize)threadsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

- (void)updateFence:(id <MTLFence>)fence;
- (void)waitForFence:(id <MTLFence>)fence;

- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage;
- (void)useResources:(const id <MTLResource> _Nonnull [_Nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage;
- (void)useHeap:(id <MTLHeap>)heap;
- (void)useHeaps:(const id <MTLHeap> _Nonnull [_Nonnull])heaps count:(NSUInteger)count;

- (void)memoryBarrierWithScope:(MTLBarrierScope)scope;
- (void)memoryBarrierWithResources:(const id <MTLResource> _Nonnull [_Nonnull])resources count:(NSUInteger)count;

- (void)sampleCountersInBuffer:(id <MTLCounterSampleBuffer>)sampleBuffer atSampleIndex:(NSUInteger)sampleIndex withBarrier:(BOOL)barrier;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMPUTECOMMANDENCODER_H_ */
