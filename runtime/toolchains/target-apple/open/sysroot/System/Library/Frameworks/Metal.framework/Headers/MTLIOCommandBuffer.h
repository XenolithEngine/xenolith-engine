/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIOCommandBuffer.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLIOCOMMANDBUFFER_H_
#define __SPRT_OPEN_METAL_MTLIOCOMMANDBUFFER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSharedEvent;
@protocol MTLIOCommandBuffer;
@protocol MTLIOFileHandle;

typedef NS_ENUM(NSInteger, MTLIOStatus) {
    MTLIOStatusPending = 0,
    MTLIOStatusCancelled = 1,
    MTLIOStatusError = 2,
    MTLIOStatusComplete = 3,
};

typedef NS_ENUM(NSInteger, MTLIOError) {
    MTLIOErrorURLInvalid = 1,
    MTLIOErrorInternal   = 2,
};

typedef void (^MTLIOCommandBufferHandler)(id<MTLIOCommandBuffer>);

@protocol MTLIOCommandBuffer <NSObject>

- (void)addCompletedHandler:(MTLIOCommandBufferHandler)block;

- (void)loadBytes:(void *)pointer
             size:(NSUInteger)size
     sourceHandle:(id<MTLIOFileHandle>)sourceHandle
sourceHandleOffset:(NSUInteger)sourceHandleOffset;

- (void)loadBuffer:(id<MTLBuffer>)buffer
            offset:(NSUInteger)offset
              size:(NSUInteger)size
      sourceHandle:(id<MTLIOFileHandle>)sourceHandle
sourceHandleOffset:(NSUInteger)sourceHandleOffset;

- (void)loadTexture:(id<MTLTexture>)texture
              slice:(NSUInteger)slice
              level:(NSUInteger)level
               size:(MTLSize)size
  sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
  destinationOrigin:(MTLOrigin)destinationOrigin
       sourceHandle:(id<MTLIOFileHandle>)sourceHandle
 sourceHandleOffset:(NSUInteger)sourceHandleOffset;

- (void)copyStatusToBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset;

- (void)commit;
- (void)waitUntilCompleted;
- (void)tryCancel;
- (void)addBarrier;

- (void)pushDebugGroup:(NSString *)string;
- (void)popDebugGroup;

- (void)enqueue;

- (void)waitForEvent:(id<MTLSharedEvent>)event value:(uint64_t)value;
- (void)signalEvent:(id<MTLSharedEvent>)event value:(uint64_t)value;

@property (nullable, copy, atomic) NSString *label;
@property (readonly) MTLIOStatus status;
@property (nullable, readonly) NSError *error;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLIOCOMMANDBUFFER_H_ */
