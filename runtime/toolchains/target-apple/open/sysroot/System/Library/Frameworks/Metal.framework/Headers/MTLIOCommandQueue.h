/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIOCommandQueue.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLIOCOMMANDQUEUE_H_
#define __SPRT_OPEN_METAL_MTLIOCOMMANDQUEUE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLIOCommandBuffer;
@protocol MTLIOScratchBuffer;
@protocol MTLIOScratchBufferAllocator;

typedef NS_ENUM(NSInteger, MTLIOPriority) {
    MTLIOPriorityHigh   = 0,
    MTLIOPriorityNormal = 1,
    MTLIOPriorityLow    = 2,
};

typedef NS_ENUM(NSInteger, MTLIOCommandQueueType) {
    MTLIOCommandQueueTypeConcurrent = 0,
    MTLIOCommandQueueTypeSerial     = 1,
};

@protocol MTLIOCommandQueue <NSObject>

- (void)enqueueBarrier;

- (id<MTLIOCommandBuffer>)commandBuffer;
- (id<MTLIOCommandBuffer>)commandBufferWithUnretainedReferences;

@property (nullable, copy, atomic) NSString *label;

@end

@protocol MTLIOScratchBuffer <NSObject>
@property (readonly) id<MTLBuffer> buffer;
@end

@protocol MTLIOScratchBufferAllocator <NSObject>
- (nullable id<MTLIOScratchBuffer>)newScratchBufferWithMinimumSize:(NSUInteger)minimumSize;
@end

@interface MTLIOCommandQueueDescriptor : NSObject <NSCopying>
@property (nonatomic, readwrite) NSUInteger maxCommandBufferCount;
@property (nonatomic, readwrite) MTLIOPriority priority;
@property (nonatomic, readwrite) MTLIOCommandQueueType type;
@property (nonatomic, readwrite) NSUInteger maxCommandsInFlight;
@property (nullable, readwrite, retain) id<MTLIOScratchBufferAllocator> scratchBufferAllocator;
@end

@protocol MTLIOFileHandle <NSObject>
@property (nullable, copy, atomic) NSString *label;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLIOCOMMANDQUEUE_H_ */
