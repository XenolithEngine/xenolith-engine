/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLResource.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLRESOURCE_H_
#define __SPRT_OPEN_METAL_MTLRESOURCE_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, MTLPurgeableState)
{
    MTLPurgeableStateNonVolatile = 2,
    MTLPurgeableStateVolatile    = 3,
};

typedef NS_ENUM(NSUInteger, MTLCPUCacheMode)
{
    MTLCPUCacheModeDefaultCache = 0,
};

typedef NS_ENUM(NSUInteger, MTLStorageMode)
{
    MTLStorageModeShared     = 0,
    MTLStorageModeManaged    = 1,
    MTLStorageModePrivate    = 2,
    MTLStorageModeMemoryless = 3,
};

typedef NS_ENUM(NSUInteger, MTLHazardTrackingMode)
{
    MTLHazardTrackingModeUntracked = 1,
    MTLHazardTrackingModeTracked   = 2,
};

#define MTLResourceCPUCacheModeShift        0
#define MTLResourceStorageModeShift         4
#define MTLResourceHazardTrackingModeShift  8

typedef NS_OPTIONS(NSUInteger, MTLResourceOptions)
{
    MTLResourceCPUCacheModeDefaultCache    = MTLCPUCacheModeDefaultCache    << MTLResourceCPUCacheModeShift,

    MTLResourceStorageModeShared           = MTLStorageModeShared           << MTLResourceStorageModeShift,
    MTLResourceStorageModePrivate          = MTLStorageModePrivate          << MTLResourceStorageModeShift,

    MTLResourceHazardTrackingModeUntracked = MTLHazardTrackingModeUntracked << MTLResourceHazardTrackingModeShift,

    /* Deprecated spelling, kept because MoltenVK still references it. */
    MTLResourceOptionCPUCacheModeDefault   = MTLResourceCPUCacheModeDefaultCache,
};

@protocol MTLDevice;
@protocol MTLHeap;

@protocol MTLAllocation <NSObject>
@end

@protocol MTLResource <MTLAllocation>

@property (copy, atomic) NSString *label;
@property (readonly) id <MTLDevice> device;
@property (readonly) MTLCPUCacheMode cpuCacheMode;
@property (readonly) MTLStorageMode storageMode;
@property (readonly) MTLHazardTrackingMode hazardTrackingMode;
@property (readonly) MTLResourceOptions resourceOptions;

- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state;

@property (readonly) id <MTLHeap> heap;

- (void)makeAliasable;
- (BOOL)isAliasable;

@end

#endif /* __SPRT_OPEN_METAL_MTLRESOURCE_H_ */
