/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLAccelerationStructureCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURECOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURECOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLAccelerationStructure.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLFence;
@protocol MTLHeap;
@protocol MTLCounterSampleBuffer;

/*!
 @enum MTLAccelerationStructureRefitOptions
 @abstract Controls the acceleration structure refit operation
 */
typedef NS_OPTIONS(NSUInteger, MTLAccelerationStructureRefitOptions) {
    MTLAccelerationStructureRefitOptionVertexData = (1 << 0),
    MTLAccelerationStructureRefitOptionPerPrimitiveData = (1 << 1),
};

@protocol MTLAccelerationStructureCommandEncoder <MTLCommandEncoder>

- (void)buildAccelerationStructure:(id <MTLAccelerationStructure>)accelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                     scratchBuffer:(id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset;

- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(nullable id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset;

- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(nullable id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset
                           options:(MTLAccelerationStructureRefitOptions)options;

- (void)copyAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
          toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

- (void)writeCompactedAccelerationStructureSize:(id <MTLAccelerationStructure>)accelerationStructure
                                       toBuffer:(id <MTLBuffer>)buffer
                                         offset:(NSUInteger)offset;

- (void)writeCompactedAccelerationStructureSize:(id <MTLAccelerationStructure>)accelerationStructure
                                       toBuffer:(id <MTLBuffer>)buffer
                                         offset:(NSUInteger)offset
                                   sizeDataType:(MTLDataType)sizeDataType;

- (void)copyAndCompactAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                    toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

- (void)updateFence:(id <MTLFence>)fence;

- (void)waitForFence:(id <MTLFence>)fence;

- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage;

- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage;

- (void)useHeap:(id <MTLHeap>)heap;

- (void)useHeaps:(const id <MTLHeap> __nonnull[__nonnull])heaps count:(NSUInteger)count;

- (void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                 atSampleIndex:(NSUInteger)sampleIndex
                   withBarrier:(BOOL)barrier;

@end

@interface MTLAccelerationStructurePassSampleBufferAttachmentDescriptor : NSObject <NSCopying>

@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;

@end

@interface MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray : NSObject

- (MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;
- (void)setObject:(nullable MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

/*!
 @class MTLAccelerationStructurePassDescriptor
 @abstract MTLAccelerationStructurePassDescriptor represents a collection of attachments to be used to create a concrete acceleration structure encoder.
 */
@interface MTLAccelerationStructurePassDescriptor : NSObject <NSCopying>

+ (MTLAccelerationStructurePassDescriptor *)accelerationStructurePassDescriptor;

@property (readonly) MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURECOMMANDENCODER_H_ */
