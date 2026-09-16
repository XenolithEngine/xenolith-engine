/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLCOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLCOMMANDENCODER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;

/* Describes how a resource will be used by a shader through an argument buffer. */
typedef NS_OPTIONS(NSUInteger, MTLResourceUsage) {
    MTLResourceUsageRead  = 1 << 0,
    MTLResourceUsageWrite = 1 << 1,
};

/* Describes the types of resources that a barrier operates on. */
typedef NS_OPTIONS(NSUInteger, MTLBarrierScope) {
    MTLBarrierScopeBuffers       = 1 << 0,
    MTLBarrierScopeTextures      = 1 << 1,
    MTLBarrierScopeRenderTargets = 1 << 2,
};

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
