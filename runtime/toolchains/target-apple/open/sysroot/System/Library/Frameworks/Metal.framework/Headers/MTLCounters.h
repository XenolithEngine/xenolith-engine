/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCounters.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_COUNTERS_H_
#define __SPRT_OPEN_METAL_COUNTERS_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h>   /* MTLStorageMode */

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

#ifndef SPRT_METAL_EXTERN
#if defined(__cplusplus)
#define SPRT_METAL_EXTERN extern "C"
#else
#define SPRT_METAL_EXTERN extern
#endif
#endif

#define MTLCounterDontSample ((NSUInteger)-1)

/* Common counter names (the runtime constants are supplied by the Metal impl). */
typedef NSString * const MTLCommonCounter;
SPRT_METAL_EXTERN MTLCommonCounter MTLCommonCounterTimestamp;

/* Common counter set names. */
typedef NSString * const MTLCommonCounterSet;
SPRT_METAL_EXTERN MTLCommonCounterSet MTLCommonCounterSetTimestamp;

@protocol MTLCounter <NSObject>
@property (readonly, copy) NSString *name;
@end

@protocol MTLCounterSet <NSObject>
@property (readonly, copy) NSString *name;
@property (readonly, copy) NSArray<id<MTLCounter>> *counters;
@end

@interface MTLCounterSampleBufferDescriptor : NSObject
@property (nullable, readwrite, retain) id<MTLCounterSet> counterSet;
@property (readwrite, copy) NSString *label;
@property (readwrite) MTLStorageMode storageMode;
@property (readwrite) NSUInteger sampleCount;
@end

@protocol MTLCounterSampleBuffer <NSObject>
@property (readonly) id<MTLDevice> device;
@property (readonly) NSString *label;
@property (readonly) NSUInteger sampleCount;
- (nullable NSData *)resolveCounterRange:(NSRange)range;
@end

typedef NS_ENUM(NSInteger, MTLCounterSampleBufferError)
{
    MTLCounterSampleBufferErrorOutOfMemory,
    MTLCounterSampleBufferErrorInvalid,
    MTLCounterSampleBufferErrorInternal,
};

/* MTLCounterSamplingPoint is defined in <Metal/MTLDevice.h> (its owner in the SDK too);
   the umbrella imports MTLDevice.h before this header, so it is already in scope here. */

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_COUNTERS_H_ */
