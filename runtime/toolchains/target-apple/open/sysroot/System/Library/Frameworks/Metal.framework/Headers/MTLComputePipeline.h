/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLComputePipeline.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMPUTEPIPELINE_H_
#define __SPRT_OPEN_METAL_MTLCOMPUTEPIPELINE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>   /* MTLSize, MTLResourceID */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLFunction;
@protocol MTLBinding;
@class MTLArgument;
@class MTLStageInputOutputDescriptor;

@interface MTLComputePipelineReflection : NSObject
@property (nonnull, readonly) NSArray<id<MTLBinding>> *bindings;
@property (readonly) NSArray<MTLArgument *> *arguments;
@end

@interface MTLComputePipelineDescriptor : NSObject <NSCopying>
@property (nullable, copy, nonatomic) NSString *label;
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> computeFunction;
@property (readwrite, nonatomic) BOOL threadGroupSizeIsMultipleOfThreadExecutionWidth;
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup;
@property (nullable, copy, nonatomic) MTLStageInputOutputDescriptor *stageInputDescriptor;
@property (readwrite, nonatomic) BOOL supportIndirectCommandBuffers;
- (void)reset;
@property (readwrite, nonatomic) BOOL supportAddingBinaryFunctions;
@property (readwrite, nonatomic) NSUInteger maxCallStackDepth;
@end

@protocol MTLComputePipelineState <NSObject>
@property (nullable, readonly) NSString *label;
@property (readonly) id <MTLDevice> device;
@property (readonly) NSUInteger maxTotalThreadsPerThreadgroup;
@property (readonly) NSUInteger threadExecutionWidth;
@property (readonly) NSUInteger staticThreadgroupMemoryLength;
- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions;
@property (readonly) BOOL supportIndirectCommandBuffers;
@property (readonly) MTLResourceID gpuResourceID;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMPUTEPIPELINE_H_ */
