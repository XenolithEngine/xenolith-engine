/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLResourceStatePass.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLRESOURCESTATEPASS_H_
#define __SPRT_OPEN_METAL_MTLRESOURCESTATEPASS_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLCounterSampleBuffer;

@interface MTLResourceStatePassSampleBufferAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

@interface MTLResourceStatePassSampleBufferAttachmentDescriptorArray : NSObject
- (MTLResourceStatePassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLResourceStatePassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;
@end

@interface MTLResourceStatePassDescriptor : NSObject <NSCopying>
+ (MTLResourceStatePassDescriptor *)resourceStatePassDescriptor;
@property (readonly) MTLResourceStatePassSampleBufferAttachmentDescriptorArray *sampleBufferAttachments;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLRESOURCESTATEPASS_H_ */
