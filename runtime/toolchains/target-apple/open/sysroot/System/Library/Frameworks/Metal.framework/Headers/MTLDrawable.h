/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDrawable.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLDRAWABLE_H_
#define __SPRT_OPEN_METAL_MTLDRAWABLE_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLDrawable.h>

@protocol MTLDrawable;

typedef void (^MTLDrawablePresentedHandler)(id<MTLDrawable>);

@protocol MTLDrawable <NSObject>

- (void)present;

- (void)addPresentedHandler:(MTLDrawablePresentedHandler)block;

@property (nonatomic, readonly) CFTimeInterval presentedTime;

@end

#endif /* __SPRT_OPEN_METAL_MTLDRAWABLE_H_ */
