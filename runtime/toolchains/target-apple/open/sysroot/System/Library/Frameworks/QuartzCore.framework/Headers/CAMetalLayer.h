/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <QuartzCore/CAMetalLayer.h> for the +open macOS target: the Metal-
backed layer the window backend attaches to its NSView. The Metal handle types are
opaque (see <Metal/Metal.h>); the real implementation is MoltenVK at run time.
**/

#ifndef __SPRT_OPEN_CAMETALLAYER_H_
#define __SPRT_OPEN_CAMETALLAYER_H_

#import <QuartzCore/CALayer.h>
#import <Metal/Metal.h>

@class CAMetalLayer;

/* HDR metadata attached to the layer for extended-dynamic-range presentation. MoltenVK builds
   one via the HDR10 factory and assigns it to CAMetalLayer.EDRMetadata. */
@interface CAEDRMetadata : NSObject <NSCopying>
+ (instancetype)HDR10MetadataWithDisplayInfo:(NSData *)displayData contentInfo:(NSData *)contentData opticalOutputScale:(float)scale;
@end

@protocol CAMetalDrawable <MTLDrawable>
@property (readonly) id<MTLTexture> texture;
@property (readonly) CAMetalLayer *layer;
@end

@interface CAMetalLayer : CALayer
@property (nullable, retain) id<MTLDevice> device;
@property MTLPixelFormat pixelFormat;
@property BOOL framebufferOnly;
@property CGSize drawableSize;
@property (getter=isDisplaySyncEnabled) BOOL displaySyncEnabled;
@property NSUInteger maximumDrawableCount;
@property BOOL allowsNextDrawableTimeout;
@property BOOL wantsExtendedDynamicRangeContent;
@property (nullable, strong) CAEDRMetadata *EDRMetadata;
@property (nullable) CGColorSpaceRef colorspace;
- (nullable id<CAMetalDrawable>)nextDrawable;
@end

#endif /* __SPRT_OPEN_CAMETALLAYER_H_ */
