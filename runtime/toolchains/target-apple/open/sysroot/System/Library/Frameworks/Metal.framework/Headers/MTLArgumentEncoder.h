/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLArgumentEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLARGUMENTENCODER_H_
#define __SPRT_OPEN_METAL_MTLARGUMENTENCODER_H_

#import <Foundation/Foundation.h>

@protocol MTLDevice;
@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;
@protocol MTLIndirectCommandBuffer;
@protocol MTLVisibleFunctionTable;
@protocol MTLAccelerationStructure;
@protocol MTLIntersectionFunctionTable;

@protocol MTLArgumentEncoder <NSObject>

@property (readonly) id<MTLDevice> device;
@property (copy, atomic) NSString *label;
@property (readonly) NSUInteger encodedLength;
@property (readonly) NSUInteger alignment;

- (void)setArgumentBuffer:(id<MTLBuffer>)argumentBuffer offset:(NSUInteger)offset;
- (void)setArgumentBuffer:(id<MTLBuffer>)argumentBuffer startOffset:(NSUInteger)startOffset arrayElement:(NSUInteger)arrayElement;

- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBuffers:(const id<MTLBuffer> *)buffers offsets:(const NSUInteger *)offsets withRange:(NSRange)range;

- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setTextures:(const id<MTLTexture> *)textures withRange:(NSRange)range;

- (void)setSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index;
- (void)setSamplerStates:(const id<MTLSamplerState> *)samplers withRange:(NSRange)range;

- (void *)constantDataAtIndex:(NSUInteger)index;

- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)pipeline atIndex:(NSUInteger)index;
- (void)setRenderPipelineStates:(const id<MTLRenderPipelineState> *)pipelines withRange:(NSRange)range;

- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline atIndex:(NSUInteger)index;
- (void)setComputePipelineStates:(const id<MTLComputePipelineState> *)pipelines withRange:(NSRange)range;

- (void)setIndirectCommandBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer atIndex:(NSUInteger)index;
- (void)setIndirectCommandBuffers:(const id<MTLIndirectCommandBuffer> *)buffers withRange:(NSRange)range;

- (void)setAccelerationStructure:(id<MTLAccelerationStructure>)accelerationStructure atIndex:(NSUInteger)index;

- (id<MTLArgumentEncoder>)newArgumentEncoderForBufferAtIndex:(NSUInteger)index;

- (void)setVisibleFunctionTable:(id<MTLVisibleFunctionTable>)visibleFunctionTable atIndex:(NSUInteger)index;
- (void)setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> *)visibleFunctionTables withRange:(NSRange)range;

- (void)setIntersectionFunctionTable:(id<MTLIntersectionFunctionTable>)intersectionFunctionTable atIndex:(NSUInteger)index;
- (void)setIntersectionFunctionTables:(const id<MTLIntersectionFunctionTable> *)intersectionFunctionTables withRange:(NSRange)range;

@end

#endif /* __SPRT_OPEN_METAL_MTLARGUMENTENCODER_H_ */
