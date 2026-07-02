/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDrawable.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLDRAWABLE_H_
#define __SPRT_OPEN_METAL_MTLDRAWABLE_H_

#import <Foundation/Foundation.h>

/* CFTimeInterval lives in CoreFoundation on a real SDK; this open sysroot only
 * self-defines it (as double) in the frameworks that need it. Redefining an
 * identical typedef is well-formed in C11/ObjC, so this stays compatible with
 * QuartzCore/CALayer.h which declares it the same way. */
typedef double CFTimeInterval;

@protocol MTLDrawable;

typedef void (^MTLDrawablePresentedHandler)(id<MTLDrawable>);

@protocol MTLDrawable <NSObject>

- (void)present;

- (void)addPresentedHandler:(MTLDrawablePresentedHandler)block;

@property (nonatomic, readonly) CFTimeInterval presentedTime;

@end

#endif /* __SPRT_OPEN_METAL_MTLDRAWABLE_H_ */
