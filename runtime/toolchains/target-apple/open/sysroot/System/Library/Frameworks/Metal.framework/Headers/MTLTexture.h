/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLTexture.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_TEXTURE_H_
#define __SPRT_OPEN_METAL_TEXTURE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLTypes.h>
#import <IOSurface/IOSurfaceRef.h>
#include <__sprt_abi/Metal/MTLTexture.h>

@protocol MTLBuffer;

NS_ASSUME_NONNULL_BEGIN

static inline MTLTextureSwizzleChannels MTLTextureSwizzleChannelsMake(MTLTextureSwizzle r, MTLTextureSwizzle g, MTLTextureSwizzle b, MTLTextureSwizzle a)
{
	MTLTextureSwizzleChannels swizzle;
	swizzle.red = r;
	swizzle.green = g;
	swizzle.blue = b;
	swizzle.alpha = a;
	return swizzle;
}

@interface MTLTextureDescriptor : NSObject <NSCopying>

+ (MTLTextureDescriptor *)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat width:(NSUInteger)width height:(NSUInteger)height mipmapped:(BOOL)mipmapped;
+ (MTLTextureDescriptor *)textureCubeDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat size:(NSUInteger)size mipmapped:(BOOL)mipmapped;
+ (MTLTextureDescriptor *)textureBufferDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat width:(NSUInteger)width resourceOptions:(MTLResourceOptions)resourceOptions usage:(MTLTextureUsage)usage;

@property (readwrite, nonatomic) MTLTextureType textureType;
@property (readwrite, nonatomic) MTLPixelFormat pixelFormat;
@property (readwrite, nonatomic) NSUInteger width;
@property (readwrite, nonatomic) NSUInteger height;
@property (readwrite, nonatomic) NSUInteger depth;
@property (readwrite, nonatomic) NSUInteger mipmapLevelCount;
@property (readwrite, nonatomic) NSUInteger sampleCount;
@property (readwrite, nonatomic) NSUInteger arrayLength;
@property (readwrite, nonatomic) MTLResourceOptions resourceOptions;
@property (readwrite, nonatomic) MTLCPUCacheMode cpuCacheMode;
@property (readwrite, nonatomic) MTLStorageMode storageMode;
@property (readwrite, nonatomic) MTLHazardTrackingMode hazardTrackingMode;
@property (readwrite, nonatomic) MTLTextureUsage usage;
@property (readwrite, nonatomic) BOOL allowGPUOptimizedContents;
@property (readwrite, nonatomic) MTLTextureCompressionType compressionType;
@property (readwrite, nonatomic) MTLTextureSwizzleChannels swizzle;

@end

/* A collection of 1D, 2D, or 3D images. */
@protocol MTLTexture <MTLResource>

@property (nullable, readonly) id <MTLTexture> parentTexture;
@property (readonly) NSUInteger parentRelativeLevel;
@property (readonly) NSUInteger parentRelativeSlice;
@property (nullable, readonly) id <MTLBuffer> buffer;
@property (readonly) NSUInteger bufferOffset;
@property (readonly) NSUInteger bufferBytesPerRow;
@property (nullable, readonly) IOSurfaceRef iosurface;
@property (readonly) NSUInteger iosurfacePlane;
@property (readonly) MTLTextureType textureType;
@property (readonly) MTLPixelFormat pixelFormat;
@property (readonly) NSUInteger width;
@property (readonly) NSUInteger height;
@property (readonly) NSUInteger depth;
@property (readonly) NSUInteger mipmapLevelCount;
@property (readonly) NSUInteger sampleCount;
@property (readonly) NSUInteger arrayLength;
@property (readonly) MTLTextureUsage usage;
@property (readonly, getter=isFramebufferOnly) BOOL framebufferOnly;
@property (readonly) BOOL allowGPUOptimizedContents;
@property (readonly) MTLTextureCompressionType compressionType;
@property (readonly) MTLResourceID gpuResourceID;
@property (readonly, nonatomic) MTLTextureSwizzleChannels swizzle;

- (void)getBytes:(void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow bytesPerImage:(NSUInteger)bytesPerImage fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice;
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice withBytes:(const void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow bytesPerImage:(NSUInteger)bytesPerImage;
- (void)getBytes:(void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level;
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level withBytes:(const void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow;

- (nullable id <MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat;
- (nullable id <MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat textureType:(MTLTextureType)textureType levels:(NSRange)levelRange slices:(NSRange)sliceRange;
- (nullable id <MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat textureType:(MTLTextureType)textureType levels:(NSRange)levelRange slices:(NSRange)sliceRange swizzle:(MTLTextureSwizzleChannels)swizzle;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_TEXTURE_H_ */
