/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCaptureManager.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_CAPTUREMANAGER_H_
#define __SPRT_OPEN_METAL_CAPTUREMANAGER_H_

#import <Foundation/Foundation.h>

@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLCaptureScope;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MTLCaptureError)
{
    MTLCaptureErrorNotSupported = 1,
    MTLCaptureErrorAlreadyCapturing,
    MTLCaptureErrorInvalidDescriptor,
};

typedef NS_ENUM(NSInteger, MTLCaptureDestination)
{
    MTLCaptureDestinationDeveloperTools = 1,
    MTLCaptureDestinationGPUTraceDocument,
};

@interface MTLCaptureDescriptor : NSObject
@property (nonatomic, strong, nullable) id captureObject;
@property (nonatomic, assign) MTLCaptureDestination destination;
@property (nonatomic, copy, nullable) NSURL *outputURL;
@end

@interface MTLCaptureManager : NSObject
+ (MTLCaptureManager *)sharedCaptureManager;
- (instancetype)init;
- (id<MTLCaptureScope>)newCaptureScopeWithDevice:(id<MTLDevice>)device;
- (id<MTLCaptureScope>)newCaptureScopeWithCommandQueue:(id<MTLCommandQueue>)commandQueue;
- (BOOL)supportsDestination:(MTLCaptureDestination)destination;
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(__autoreleasing NSError **)error;
- (void)startCaptureWithDevice:(id<MTLDevice>)device;
- (void)startCaptureWithCommandQueue:(id<MTLCommandQueue>)commandQueue;
- (void)startCaptureWithScope:(id<MTLCaptureScope>)captureScope;
- (void)stopCapture;
@property (nullable, readwrite, strong, atomic) id<MTLCaptureScope> defaultCaptureScope;
@property (readonly) BOOL isCapturing;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_CAPTUREMANAGER_H_ */
