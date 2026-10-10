/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLResource.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLRESOURCE_H_
#define __SPRT_OPEN_METAL_MTLRESOURCE_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLResource.h>

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
