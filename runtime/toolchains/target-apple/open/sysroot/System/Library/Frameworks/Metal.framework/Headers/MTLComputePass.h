/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLComputePass.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMPUTEPASS_H_
#define __SPRT_OPEN_METAL_MTLCOMPUTEPASS_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLCommandBuffer.h>   /* MTLDispatchType */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLCounterSampleBuffer;

@interface MTLComputePassSampleBufferAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

@interface MTLComputePassSampleBufferAttachmentDescriptorArray : NSObject
- (MTLComputePassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLComputePassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLComputePassDescriptor : NSObject <NSCopying>
+ (MTLComputePassDescriptor *)computePassDescriptor;
@property (nonatomic) MTLDispatchType dispatchType;
@property (readonly) MTLComputePassSampleBufferAttachmentDescriptorArray *sampleBufferAttachments;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMPUTEPASS_H_ */
