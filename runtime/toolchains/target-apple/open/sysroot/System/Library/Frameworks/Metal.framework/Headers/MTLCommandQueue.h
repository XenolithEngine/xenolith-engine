/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCommandQueue.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMMANDQUEUE_H_
#define __SPRT_OPEN_METAL_MTLCOMMANDQUEUE_H_

#import <Foundation/Foundation.h>
#include "MTLCommandBuffer.h"   /* MTLCommandBuffer protocol + MTLCommandBufferDescriptor */

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;

/* A serial queue of command buffers to be executed by the device. */
@protocol MTLCommandQueue <NSObject>

@property (nullable, copy, atomic) NSString *label;
@property (readonly) id <MTLDevice> device;

- (nullable id <MTLCommandBuffer>)commandBuffer;
- (nullable id <MTLCommandBuffer>)commandBufferWithDescriptor:(MTLCommandBufferDescriptor *)descriptor;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLCOMMANDQUEUE_H_ */
