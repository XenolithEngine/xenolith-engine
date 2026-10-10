/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <QuartzCore/CALayer.h> for the +open macOS target. Closed framework;
reconstructs the CALayer surface + CALayerDelegate the window backend's backing
layer uses. Symbols resolve from the baked QuartzCore.tbd.
**/

#ifndef __SPRT_OPEN_CALAYER_H_
#define __SPRT_OPEN_CALAYER_H_
#include <__sprt_abi/QuartzCore/CALayer.h>

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

@class CALayer;

@protocol CALayerDelegate <NSObject>
@optional
- (void)displayLayer:(CALayer *)layer;
- (void)layoutSublayersOfLayer:(CALayer *)layer;
@end
extern CALayerContentsGravity const kCAGravityCenter;
extern CALayerContentsGravity const kCAGravityTop;
extern CALayerContentsGravity const kCAGravityBottom;
extern CALayerContentsGravity const kCAGravityLeft;
extern CALayerContentsGravity const kCAGravityRight;
extern CALayerContentsGravity const kCAGravityTopLeft;
extern CALayerContentsGravity const kCAGravityTopRight;
extern CALayerContentsGravity const kCAGravityBottomLeft;
extern CALayerContentsGravity const kCAGravityBottomRight;
extern CALayerContentsGravity const kCAGravityResize;
extern CALayerContentsGravity const kCAGravityResizeAspect;
extern CALayerContentsFilter const kCAFilterNearest;
extern CALayerContentsFilter const kCAFilterLinear;
extern CALayerContentsFilter const kCAFilterTrilinear;

@interface CALayer : NSObject
+ (instancetype)layer;
@property CGRect frame;
@property CGRect bounds;
@property CGPoint position;
@property CAAutoresizingMask autoresizingMask;
@property CGFloat contentsScale;
@property (copy) CALayerContentsGravity contentsGravity;
@property (copy) CALayerContentsFilter minificationFilter;
@property (copy) CALayerContentsFilter magnificationFilter;
@property (nullable, copy) NSString *name;
@property (getter=isOpaque) BOOL opaque;
@property (getter=isHidden) BOOL hidden;
@property BOOL masksToBounds;
@property CGFloat cornerRadius;
@property CGFloat borderWidth;
@property (nullable) CGColorRef borderColor;
@property (nullable) CGColorRef backgroundColor;
@property BOOL needsDisplayOnBoundsChange;
@property (weak, nullable) id<CALayerDelegate> delegate;
@property (nullable, copy) NSArray *sublayers;
@property (readonly, nullable) CALayer *superlayer;
- (void)addSublayer:(CALayer *)layer;
- (void)removeFromSuperlayer;
- (void)setNeedsDisplay;
@end

#endif /* __SPRT_OPEN_CALAYER_H_ */
