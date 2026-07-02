/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <QuartzCore/CALayer.h> for the +open macOS target. Closed framework;
reconstructs the CALayer surface + CALayerDelegate the window backend's backing
layer uses. Symbols resolve from the baked QuartzCore.tbd.
**/

#ifndef __SPRT_OPEN_CALAYER_H_
#define __SPRT_OPEN_CALAYER_H_

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

typedef double CFTimeInterval;

typedef NS_OPTIONS(unsigned int, CAAutoresizingMask) {
	kCALayerNotSizable   = 0,
	kCALayerMinXMargin   = (1U << 0),
	kCALayerWidthSizable = (1U << 1),
	kCALayerMaxXMargin   = (1U << 2),
	kCALayerMinYMargin   = (1U << 3),
	kCALayerHeightSizable = (1U << 4),
	kCALayerMaxYMargin   = (1U << 5),
};

@class CALayer;

@protocol CALayerDelegate <NSObject>
@optional
- (void)displayLayer:(CALayer *)layer;
- (void)layoutSublayersOfLayer:(CALayer *)layer;
@end

/* contentsGravity: an NSString-typed alias + the standard gravity constants MoltenVK maps. */
typedef NSString *CALayerContentsGravity;
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

/* min/magnificationFilter: NSString-typed alias + the filter constants MoltenVK maps. */
typedef NSString *CALayerContentsFilter;
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
@property BOOL needsDisplayOnBoundsChange;
@property (weak, nullable) id<CALayerDelegate> delegate;
@property (nullable, copy) NSArray *sublayers;
@property (readonly, nullable) CALayer *superlayer;
- (void)addSublayer:(CALayer *)layer;
- (void)removeFromSuperlayer;
- (void)setNeedsDisplay;
@end

#endif /* __SPRT_OPEN_CALAYER_H_ */
