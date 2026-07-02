/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCaptureScope.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_CAPTURESCOPE_H_
#define __SPRT_OPEN_METAL_CAPTURESCOPE_H_

#import <Foundation/Foundation.h>

@protocol MTLDevice;
@protocol MTLCommandQueue;

NS_ASSUME_NONNULL_BEGIN

@protocol MTLCaptureScope <NSObject>
- (void)beginScope;
- (void)endScope;
@property (nullable, copy, atomic) NSString *label;
@property (nonnull, readonly, nonatomic) id<MTLDevice> device;
@property (nullable, readonly, nonatomic) id<MTLCommandQueue> commandQueue;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_CAPTURESCOPE_H_ */
