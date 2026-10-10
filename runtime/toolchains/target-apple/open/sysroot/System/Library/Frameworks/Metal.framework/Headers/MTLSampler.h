/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLSampler.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_SAMPLER_H_
#define __SPRT_OPEN_METAL_SAMPLER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLDepthStencil.h> /* MTLCompareFunction */
#import <Metal/MTLTypes.h>        /* MTLResourceID */
#include <__sprt_abi/Metal/MTLSampler.h>

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

@interface MTLSamplerDescriptor : NSObject <NSCopying>
@property (nonatomic) MTLSamplerMinMagFilter minFilter;
@property (nonatomic) MTLSamplerMinMagFilter magFilter;
@property (nonatomic) MTLSamplerMipFilter mipFilter;
@property (nonatomic) NSUInteger maxAnisotropy;
@property (nonatomic) MTLSamplerAddressMode sAddressMode;
@property (nonatomic) MTLSamplerAddressMode tAddressMode;
@property (nonatomic) MTLSamplerAddressMode rAddressMode;
@property (nonatomic) MTLSamplerBorderColor borderColor;
@property (nonatomic) BOOL normalizedCoordinates;
@property (nonatomic) float lodMinClamp;
@property (nonatomic) float lodMaxClamp;
@property (nonatomic) MTLCompareFunction compareFunction;
@property (nonatomic) BOOL supportArgumentBuffers;
@property (nullable, copy, nonatomic) NSString *label;
@end

@protocol MTLSamplerState <NSObject>
@property (nullable, readonly) NSString *label;
@property (readonly) id <MTLDevice> device;
@property (readonly) MTLResourceID gpuResourceID;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_SAMPLER_H_ */
