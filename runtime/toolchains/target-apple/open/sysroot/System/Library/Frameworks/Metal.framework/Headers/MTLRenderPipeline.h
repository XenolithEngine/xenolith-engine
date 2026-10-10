/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLRenderPipeline.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLRENDERPIPELINE_H_
#define __SPRT_OPEN_METAL_MTLRENDERPIPELINE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLRenderCommandEncoder.h>   /* MTLWinding, MTLRenderStages */
#include <__sprt_abi/Metal/MTLRenderPipeline.h>

@protocol MTLDevice;
@protocol MTLFunction;
@class MTLVertexDescriptor;
@class MTLRenderPipelineColorAttachmentDescriptorArray;

NS_ASSUME_NONNULL_BEGIN

@interface MTLRenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>
@property (nonatomic) MTLPixelFormat pixelFormat;
@property (nonatomic, getter = isBlendingEnabled) BOOL blendingEnabled;
@property (nonatomic) MTLBlendFactor sourceRGBBlendFactor;
@property (nonatomic) MTLBlendFactor destinationRGBBlendFactor;
@property (nonatomic) MTLBlendOperation rgbBlendOperation;
@property (nonatomic) MTLBlendFactor sourceAlphaBlendFactor;
@property (nonatomic) MTLBlendFactor destinationAlphaBlendFactor;
@property (nonatomic) MTLBlendOperation alphaBlendOperation;
@property (nonatomic) MTLColorWriteMask writeMask;
@end

@interface MTLRenderPipelineColorAttachmentDescriptorArray : NSObject
- (MTLRenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLRenderPipelineColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLRenderPipelineReflection : NSObject
@end

@interface MTLRenderPipelineDescriptor : NSObject <NSCopying>

@property (nullable, copy, nonatomic) NSString *label;

@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> vertexFunction;
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> fragmentFunction;

@property (nullable, copy, nonatomic) MTLVertexDescriptor *vertexDescriptor;

@property (readwrite, nonatomic) NSUInteger sampleCount;
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;
@property (readwrite, nonatomic, getter = isAlphaToCoverageEnabled) BOOL alphaToCoverageEnabled;
@property (readwrite, nonatomic, getter = isAlphaToOneEnabled) BOOL alphaToOneEnabled;
@property (readwrite, nonatomic, getter = isRasterizationEnabled) BOOL rasterizationEnabled;

@property (readwrite, nonatomic) NSUInteger maxVertexAmplificationCount;

@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;

@property (nonatomic) MTLPixelFormat depthAttachmentPixelFormat;
@property (nonatomic) MTLPixelFormat stencilAttachmentPixelFormat;

@property (readwrite, nonatomic) MTLPrimitiveTopologyClass inputPrimitiveTopology;

@property (readwrite, nonatomic) MTLTessellationPartitionMode tessellationPartitionMode;
@property (readwrite, nonatomic) NSUInteger maxTessellationFactor;
@property (readwrite, nonatomic, getter = isTessellationFactorScaleEnabled) BOOL tessellationFactorScaleEnabled;
@property (readwrite, nonatomic) MTLTessellationFactorFormat tessellationFactorFormat;
@property (readwrite, nonatomic) MTLTessellationFactorStepFunction tessellationFactorStepFunction;
@property (readwrite, nonatomic) MTLWinding tessellationOutputWindingOrder;

@property (readwrite, nonatomic) BOOL supportIndirectCommandBuffers;

- (void)reset;

@end

@protocol MTLRenderPipelineState <NSObject>

@property (nullable, readonly) NSString *label;
@property (readonly) id <MTLDevice> device;
@property (readonly) MTLResourceID gpuResourceID;

- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions;

@property (readonly) BOOL supportIndirectCommandBuffers;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLRENDERPIPELINE_H_ */
