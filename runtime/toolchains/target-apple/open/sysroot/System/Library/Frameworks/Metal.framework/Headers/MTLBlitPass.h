/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLBlitPass.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLBLITPASS_H_
#define __SPRT_OPEN_METAL_MTLBLITPASS_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLCounterSampleBuffer;

@interface MTLBlitPassSampleBufferAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

@interface MTLBlitPassSampleBufferAttachmentDescriptorArray : NSObject
- (MTLBlitPassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLBlitPassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLBlitPassDescriptor : NSObject <NSCopying>
+ (MTLBlitPassDescriptor *)blitPassDescriptor;
@property (readonly) MTLBlitPassSampleBufferAttachmentDescriptorArray *sampleBufferAttachments;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLBLITPASS_H_ */
