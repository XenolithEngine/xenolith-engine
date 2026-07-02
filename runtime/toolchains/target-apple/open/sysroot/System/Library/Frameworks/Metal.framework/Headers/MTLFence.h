/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFence.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_FENCE_H_
#define __SPRT_OPEN_METAL_FENCE_H_

#import <Foundation/Foundation.h>

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

@protocol MTLFence <NSObject>
@property (nonnull, readonly) id <MTLDevice> device;
@property (nullable, copy, atomic) NSString *label;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_FENCE_H_ */
