/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLRenderPass.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLRENDERPASS_H_
#define __SPRT_OPEN_METAL_MTLRENDERPASS_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>   /* MTLSamplePosition */

typedef struct
{
    double red;
    double green;
    double blue;
    double alpha;
} MTLClearColor;

static inline MTLClearColor MTLClearColorMake(double red, double green, double blue, double alpha)
{
    MTLClearColor result;
    result.red = red;
    result.green = green;
    result.blue = blue;
    result.alpha = alpha;
    return result;
}

@protocol MTLDevice;
@protocol MTLTexture;
@protocol MTLBuffer;
@protocol MTLCounterSampleBuffer;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MTLLoadAction) {
    MTLLoadActionDontCare = 0,
    MTLLoadActionLoad = 1,
    MTLLoadActionClear = 2,
};

typedef NS_ENUM(NSUInteger, MTLStoreAction) {
    MTLStoreActionDontCare = 0,
    MTLStoreActionStore = 1,
    MTLStoreActionMultisampleResolve = 2,
    MTLStoreActionStoreAndMultisampleResolve = 3,
    MTLStoreActionUnknown = 4,
    MTLStoreActionCustomSampleDepthStore = 5,
};

typedef NS_OPTIONS(NSUInteger, MTLStoreActionOptions) {
    MTLStoreActionOptionNone                  = 0,
    MTLStoreActionOptionCustomSamplePositions = 1 << 0,
};

typedef NS_ENUM(NSUInteger, MTLMultisampleDepthResolveFilter) {
    MTLMultisampleDepthResolveFilterSample0 = 0,
    MTLMultisampleDepthResolveFilterMin = 1,
    MTLMultisampleDepthResolveFilterMax = 2,
};

typedef NS_ENUM(NSUInteger, MTLMultisampleStencilResolveFilter) {
    MTLMultisampleStencilResolveFilterSample0             = 0,
    MTLMultisampleStencilResolveFilterDepthResolvedSample = 1,
};

@interface MTLRenderPassAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, nonatomic, strong) id <MTLTexture> texture;
@property (nonatomic) NSUInteger level;
@property (nonatomic) NSUInteger slice;
@property (nonatomic) NSUInteger depthPlane;
@property (nullable, nonatomic, strong) id <MTLTexture> resolveTexture;
@property (nonatomic) NSUInteger resolveLevel;
@property (nonatomic) NSUInteger resolveSlice;
@property (nonatomic) NSUInteger resolveDepthPlane;
@property (nonatomic) MTLLoadAction loadAction;
@property (nonatomic) MTLStoreAction storeAction;
@property (nonatomic) MTLStoreActionOptions storeActionOptions;
@end

@interface MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property (nonatomic) MTLClearColor clearColor;
@end

@interface MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property (nonatomic) double clearDepth;
@property (nonatomic) MTLMultisampleDepthResolveFilter depthResolveFilter;
@end

@interface MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property (nonatomic) uint32_t clearStencil;
@property (nonatomic) MTLMultisampleStencilResolveFilter stencilResolveFilter;
@end

@interface MTLRenderPassColorAttachmentDescriptorArray : NSObject
- (MTLRenderPassColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLRenderPassColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLRenderPassSampleBufferAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;
@property (nonatomic) NSUInteger startOfVertexSampleIndex;
@property (nonatomic) NSUInteger endOfVertexSampleIndex;
@property (nonatomic) NSUInteger startOfFragmentSampleIndex;
@property (nonatomic) NSUInteger endOfFragmentSampleIndex;
@end

@interface MTLRenderPassSampleBufferAttachmentDescriptorArray : NSObject
- (MTLRenderPassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLRenderPassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLRenderPassDescriptor : NSObject <NSCopying>

+ (MTLRenderPassDescriptor *)renderPassDescriptor;

@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;
@property (copy, nonatomic, null_resettable) MTLRenderPassDepthAttachmentDescriptor *depthAttachment;
@property (copy, nonatomic, null_resettable) MTLRenderPassStencilAttachmentDescriptor *stencilAttachment;

@property (nullable, nonatomic, strong) id <MTLBuffer> visibilityResultBuffer;
@property (nonatomic) NSUInteger renderTargetArrayLength;
@property (nonatomic) NSUInteger imageblockSampleLength;
@property (nonatomic) NSUInteger threadgroupMemoryLength;
@property (nonatomic) NSUInteger tileWidth;
@property (nonatomic) NSUInteger tileHeight;
@property (nonatomic) NSUInteger defaultRasterSampleCount;
@property (nonatomic) NSUInteger renderTargetWidth;
@property (nonatomic) NSUInteger renderTargetHeight;

- (void)setSamplePositions:(const MTLSamplePosition * _Nullable)positions count:(NSUInteger)count;
- (NSUInteger)getSamplePositions:(MTLSamplePosition * _Nullable)positions count:(NSUInteger)count;

@property (readonly) MTLRenderPassSampleBufferAttachmentDescriptorArray *sampleBufferAttachments;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLRENDERPASS_H_ */
