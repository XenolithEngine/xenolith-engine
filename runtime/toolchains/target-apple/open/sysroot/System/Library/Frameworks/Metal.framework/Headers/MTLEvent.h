/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLEvent.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_EVENT_H_
#define __SPRT_OPEN_METAL_EVENT_H_

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>   /* dispatch_queue_t (not pulled in by the minimal Foundation) */

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

@protocol MTLEvent <NSObject>
@property (nullable, readonly) id <MTLDevice> device;
@property (nullable, copy, atomic) NSString *label;
@end

/* Handles the dispatching of MTLSharedEvent notifications from Metal. */
@interface MTLSharedEventListener : NSObject
- (instancetype)init;
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue;
@property (nonnull, readonly) dispatch_queue_t dispatchQueue;
@end

@class MTLSharedEventHandle;
@protocol MTLSharedEvent;

typedef void (^MTLSharedEventNotificationBlock)(id <MTLSharedEvent>, uint64_t value);

@protocol MTLSharedEvent <MTLEvent>
- (void)notifyListener:(MTLSharedEventListener *)listener atValue:(uint64_t)value block:(MTLSharedEventNotificationBlock)block;
- (MTLSharedEventHandle *)newSharedEventHandle;
- (BOOL)waitUntilSignaledValue:(uint64_t)value timeoutMS:(uint64_t)milliseconds;
@property (readwrite) uint64_t signaledValue;
@end

/* May be passed between processes via XPC and used to recreate a MTLSharedEvent. */
@interface MTLSharedEventHandle : NSObject
@property (readonly, nullable) NSString *label;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_EVENT_H_ */
