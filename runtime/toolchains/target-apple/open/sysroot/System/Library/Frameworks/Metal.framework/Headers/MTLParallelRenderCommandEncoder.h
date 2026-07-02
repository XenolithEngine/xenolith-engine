/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLParallelRenderCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLPARALLELRENDERCOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLPARALLELRENDERCOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLCommandEncoder.h>   /* <MTLCommandEncoder> */
#import <Metal/MTLRenderPass.h>       /* MTLStoreAction */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLRenderCommandEncoder;

@protocol MTLParallelRenderCommandEncoder <MTLCommandEncoder>

- (nullable id <MTLRenderCommandEncoder>)renderCommandEncoder;

- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)colorAttachmentIndex;
- (void)setDepthStoreAction:(MTLStoreAction)storeAction;
- (void)setStencilStoreAction:(MTLStoreAction)storeAction;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLPARALLELRENDERCOMMANDENCODER_H_ */
