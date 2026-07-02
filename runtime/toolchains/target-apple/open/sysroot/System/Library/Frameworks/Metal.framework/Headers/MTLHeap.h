/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLHeap.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_HEAP_H_
#define __SPRT_OPEN_METAL_HEAP_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h> /* MTLStorageMode/MTLCPUCacheMode/MTLHazardTrackingMode/MTLResourceOptions/MTLPurgeableState */

@class MTLTextureDescriptor;
@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

/* Describes the mode of operation for an MTLHeap. */
typedef NS_ENUM(NSInteger, MTLHeapType) {
	MTLHeapTypeAutomatic = 0,
	MTLHeapTypePlacement = 1,
	MTLHeapTypeSparse    = 2,
};

@interface MTLHeapDescriptor : NSObject <NSCopying>
@property (readwrite, nonatomic) NSUInteger size;
@property (readwrite, nonatomic) MTLStorageMode storageMode;
@property (readwrite, nonatomic) MTLCPUCacheMode cpuCacheMode;
@property (readwrite, nonatomic) MTLHazardTrackingMode hazardTrackingMode;
@property (readwrite, nonatomic) MTLResourceOptions resourceOptions;
@property (readwrite, nonatomic) MTLHeapType type;
@end

@protocol MTLHeap <NSObject>

@property (nullable, copy, atomic) NSString *label;
@property (readonly) id <MTLDevice> device;
@property (readonly) MTLStorageMode storageMode;
@property (readonly) MTLCPUCacheMode cpuCacheMode;
@property (readonly) MTLHazardTrackingMode hazardTrackingMode;
@property (readonly) MTLResourceOptions resourceOptions;
@property (readonly) NSUInteger size;
@property (readonly) NSUInteger usedSize;
@property (readonly) NSUInteger currentAllocatedSize;
@property (readonly) MTLHeapType type;

- (NSUInteger)maxAvailableSizeWithAlignment:(NSUInteger)alignment;
- (nullable id <MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor;
- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state;
- (nullable id <MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options offset:(NSUInteger)offset;
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor offset:(NSUInteger)offset;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_HEAP_H_ */
