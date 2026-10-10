/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLCOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLCommandEncoder.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;

/* Common interface for objects that write commands into MTLCommandBuffers. */
@protocol MTLCommandEncoder <NSObject>

@property (readonly) id <MTLDevice> device;
@property (nullable, copy, atomic) NSString *label;

- (void)endEncoding;

- (void)insertDebugSignpost:(NSString *)string;
- (void)pushDebugGroup:(NSString *)string;
- (void)popDebugGroup;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMMANDENCODER_H_ */
