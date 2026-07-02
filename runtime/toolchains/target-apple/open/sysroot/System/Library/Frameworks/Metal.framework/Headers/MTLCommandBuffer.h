/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCommandBuffer.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMMANDBUFFER_H_
#define __SPRT_OPEN_METAL_MTLCOMMANDBUFFER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLBlitCommandEncoder;
@protocol MTLRenderCommandEncoder;
@protocol MTLComputeCommandEncoder;
@protocol MTLDrawable;
@protocol MTLEvent;
@protocol MTLCommandBuffer;
@class MTLRenderPassDescriptor;

/* Current stage in the lifetime of a MTLCommandBuffer. */
typedef NS_ENUM(NSUInteger, MTLCommandBufferStatus) {
    MTLCommandBufferStatusNotEnqueued = 0,
    MTLCommandBufferStatusEnqueued    = 1,
    MTLCommandBufferStatusCommitted   = 2,
    MTLCommandBufferStatusScheduled   = 3,
    MTLCommandBufferStatusCompleted   = 4,
    MTLCommandBufferStatusError       = 5,
};

/* Error codes that can be found in MTLCommandBuffer.error. */
typedef NS_ENUM(NSUInteger, MTLCommandBufferError) {
    MTLCommandBufferErrorNone            = 0,
    MTLCommandBufferErrorInternal        = 1,
    MTLCommandBufferErrorTimeout         = 2,
    MTLCommandBufferErrorPageFault       = 3,
    MTLCommandBufferErrorBlacklisted     = 4,
    MTLCommandBufferErrorAccessRevoked   = 4,
    MTLCommandBufferErrorNotPermitted    = 7,
    MTLCommandBufferErrorOutOfMemory     = 8,
    MTLCommandBufferErrorInvalidResource = 9,
    MTLCommandBufferErrorMemoryless      = 10,
    MTLCommandBufferErrorDeviceRemoved   = 11,
    MTLCommandBufferErrorStackOverflow   = 12,
};

/* Options for controlling the error reporting of a MTLCommandBuffer. */
typedef NS_OPTIONS(NSUInteger, MTLCommandBufferErrorOption) {
    MTLCommandBufferErrorOptionNone                    = 0,
    MTLCommandBufferErrorOptionEncoderExecutionStatus  = 1 << 0,
};

/* Error state for a Metal command encoder after command buffer execution. */
typedef NS_ENUM(NSInteger, MTLCommandEncoderErrorState) {
    MTLCommandEncoderErrorStateUnknown   = 0,
    MTLCommandEncoderErrorStateCompleted = 1,
    MTLCommandEncoderErrorStateAffected  = 2,
    MTLCommandEncoderErrorStatePending   = 3,
    MTLCommandEncoderErrorStateFaulted   = 4,
};

/* Key in the userInfo of a MTLCommandBufferError NSError. */
extern NSErrorUserInfoKey const MTLCommandBufferEncoderInfoErrorKey;

/* Configures new Metal command buffer objects. */
@interface MTLCommandBufferDescriptor : NSObject <NSCopying>
@property (readwrite, nonatomic) BOOL retainedReferences;
@property (readwrite, nonatomic) MTLCommandBufferErrorOption errorOptions;
@end

/* Execution status information for a Metal command encoder. */
@protocol MTLCommandBufferEncoderInfo <NSObject>
@property (readonly, nonatomic) NSString *label;
@property (readonly, nonatomic) NSArray<NSString *> *debugSignposts;
@property (readonly, nonatomic) MTLCommandEncoderErrorState errorState;
@end

typedef void (^MTLCommandBufferHandler)(id <MTLCommandBuffer>);

/* Describes how a command encoder will execute dispatched work. */
typedef NS_ENUM(NSUInteger, MTLDispatchType) {
    MTLDispatchTypeSerial     = 0,
    MTLDispatchTypeConcurrent = 1,
};

/* A serial list of commands for the device to execute. */
@protocol MTLLogContainer;

@protocol MTLCommandBuffer <NSObject>

@property (readonly) id <MTLDevice> device;
@property (readonly) id <MTLCommandQueue> commandQueue;
@property (readonly) BOOL retainedReferences;
@property (readonly) MTLCommandBufferErrorOption errorOptions;
@property (nullable, copy, atomic) NSString *label;
@property (readonly) id<MTLLogContainer> logs;
@property (readonly) MTLCommandBufferStatus status;
@property (nullable, readonly) NSError *error;

- (void)enqueue;
- (void)commit;

- (void)addScheduledHandler:(MTLCommandBufferHandler)block;
- (void)addCompletedHandler:(MTLCommandBufferHandler)block;
- (void)waitUntilCompleted;

- (void)presentDrawable:(id <MTLDrawable>)drawable;

- (nullable id <MTLBlitCommandEncoder>)blitCommandEncoder;
- (nullable id <MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor;
- (nullable id <MTLComputeCommandEncoder>)computeCommandEncoderWithDispatchType:(MTLDispatchType)dispatchType;

- (void)encodeWaitForEvent:(id <MTLEvent>)event value:(uint64_t)value;
- (void)encodeSignalEvent:(id <MTLEvent>)event value:(uint64_t)value;

- (void)pushDebugGroup:(NSString *)string;
- (void)popDebugGroup;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMMANDBUFFER_H_ */
