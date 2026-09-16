/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIndirectCommandBuffer.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLINDIRECTCOMMANDBUFFER_H_
#define __SPRT_OPEN_METAL_MTLINDIRECTCOMMANDBUFFER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLIndirectCommandEncoder.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, MTLIndirectCommandType) {
    MTLIndirectCommandTypeDraw                      = (1 << 0),
    MTLIndirectCommandTypeDrawIndexed               = (1 << 1),
    MTLIndirectCommandTypeDrawPatches               = (1 << 2),
    MTLIndirectCommandTypeDrawIndexedPatches        = (1 << 3),
    MTLIndirectCommandTypeConcurrentDispatch        = (1 << 5),
    MTLIndirectCommandTypeConcurrentDispatchThreads = (1 << 6),
    MTLIndirectCommandTypeDrawMeshThreadgroups      = (1 << 7),
    MTLIndirectCommandTypeDrawMeshThreads           = (1 << 8),
};

typedef struct {
    uint32_t location;
    uint32_t length;
} MTLIndirectCommandBufferExecutionRange;

static inline MTLIndirectCommandBufferExecutionRange MTLIndirectCommandBufferExecutionRangeMake(uint32_t location, uint32_t length) {
    MTLIndirectCommandBufferExecutionRange icbRange = {location, length};
    return icbRange;
}

@interface MTLIndirectCommandBufferDescriptor : NSObject <NSCopying>
@property (readwrite, nonatomic) MTLIndirectCommandType commandTypes;
@property (readwrite, nonatomic) BOOL inheritPipelineState;
@property (readwrite, nonatomic) BOOL inheritBuffers;
@property (readwrite, nonatomic) NSUInteger maxVertexBufferBindCount;
@property (readwrite, nonatomic) NSUInteger maxFragmentBufferBindCount;
@property (readwrite, nonatomic) NSUInteger maxKernelBufferBindCount;
@property (readwrite, nonatomic) NSUInteger maxKernelThreadgroupMemoryBindCount;
@property (readwrite, nonatomic) NSUInteger maxObjectBufferBindCount;
@property (readwrite, nonatomic) NSUInteger maxMeshBufferBindCount;
@property (readwrite, nonatomic) NSUInteger maxObjectThreadgroupMemoryBindCount;
@property (readwrite, nonatomic) BOOL supportRayTracing;
@property (readwrite, nonatomic) BOOL supportDynamicAttributeStride;
@end

@protocol MTLIndirectCommandBuffer <MTLResource>

@property (readonly) NSUInteger size;
@property (readonly) MTLResourceID gpuResourceID;

- (void)resetWithRange:(NSRange)range;

- (id<MTLIndirectRenderCommand>)indirectRenderCommandAtIndex:(NSUInteger)commandIndex;
- (id<MTLIndirectComputeCommand>)indirectComputeCommandAtIndex:(NSUInteger)commandIndex;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLINDIRECTCOMMANDBUFFER_H_ */
