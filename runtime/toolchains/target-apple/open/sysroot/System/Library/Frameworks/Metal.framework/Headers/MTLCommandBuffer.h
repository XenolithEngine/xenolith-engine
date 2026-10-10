/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCommandBuffer.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMMANDBUFFER_H_
#define __SPRT_OPEN_METAL_MTLCOMMANDBUFFER_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLCommandBuffer.h>

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
