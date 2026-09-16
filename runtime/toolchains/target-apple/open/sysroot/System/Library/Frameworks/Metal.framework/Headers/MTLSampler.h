/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLSampler.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_SAMPLER_H_
#define __SPRT_OPEN_METAL_SAMPLER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLDepthStencil.h> /* MTLCompareFunction */
#import <Metal/MTLTypes.h>        /* MTLResourceID */

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MTLSamplerMinMagFilter) {
	MTLSamplerMinMagFilterNearest = 0,
	MTLSamplerMinMagFilterLinear  = 1,
};

typedef NS_ENUM(NSUInteger, MTLSamplerMipFilter) {
	MTLSamplerMipFilterNotMipmapped = 0,
	MTLSamplerMipFilterNearest      = 1,
	MTLSamplerMipFilterLinear       = 2,
};

typedef NS_ENUM(NSUInteger, MTLSamplerAddressMode) {
	MTLSamplerAddressModeClampToEdge        = 0,
	MTLSamplerAddressModeMirrorClampToEdge  = 1,
	MTLSamplerAddressModeRepeat             = 2,
	MTLSamplerAddressModeMirrorRepeat       = 3,
	MTLSamplerAddressModeClampToZero        = 4,
	MTLSamplerAddressModeClampToBorderColor = 5,
};

typedef NS_ENUM(NSUInteger, MTLSamplerBorderColor) {
	MTLSamplerBorderColorTransparentBlack = 0, /* {0,0,0,0} */
	MTLSamplerBorderColorOpaqueBlack      = 1, /* {0,0,0,1} */
	MTLSamplerBorderColorOpaqueWhite      = 2, /* {1,1,1,1} */
};

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
