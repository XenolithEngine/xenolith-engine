/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLResidencySet.h> for *-apple-macosx+open — only the surface MoltenVK
 * uses. MTLResidencySet is a macOS-15 API, absent from the 14.5 SDK reference snapshot, so it
 * is reconstructed here from MoltenVK's actual usage (addAllocation:/removeAllocation:/commit
 * on the set; label/initialCapacity on the descriptor; newResidencySetWithDescriptor:error: on
 * MTLDevice). MoltenVK gates it behind an OS check at run time; this only needs to compile. */

#ifndef __SPRT_OPEN_METAL_MTLRESIDENCYSET_H_
#define __SPRT_OPEN_METAL_MTLRESIDENCYSET_H_

#import <Foundation/Foundation.h>

@protocol MTLAllocation;

@interface MTLResidencySetDescriptor : NSObject <NSCopying>
@property (nullable, copy) NSString *label;
@property NSUInteger initialCapacity;
@end

@protocol MTLResidencySet <NSObject>
- (void)addAllocation:(id<MTLAllocation>)allocation;
- (void)removeAllocation:(id<MTLAllocation>)allocation;
- (void)commit;
@end

#endif /* __SPRT_OPEN_METAL_MTLRESIDENCYSET_H_ */
