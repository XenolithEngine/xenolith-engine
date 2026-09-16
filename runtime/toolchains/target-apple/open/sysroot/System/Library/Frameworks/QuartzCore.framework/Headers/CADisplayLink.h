/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <QuartzCore/CADisplayLink.h> for the +open macOS target: the vsync
callback the window backend drives its frame loop with. Constructed via
-[NSWindow displayLinkWithTarget:selector:] (declared in AppKit). Symbols resolve
from the baked QuartzCore.tbd.
**/

#ifndef __SPRT_OPEN_CADISPLAYLINK_H_
#define __SPRT_OPEN_CADISPLAYLINK_H_

#import <Foundation/Foundation.h>
#import <QuartzCore/CALayer.h>  /* CFTimeInterval */

@interface CADisplayLink : NSObject
- (void)addToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)invalidate;
@property (getter=isPaused) BOOL paused;
@property NSInteger preferredFramesPerSecond;
@property (readonly) CFTimeInterval timestamp;
@property (readonly) CFTimeInterval duration;
@property (readonly) CFTimeInterval targetTimestamp;
@end

#endif /* __SPRT_OPEN_CADISPLAYLINK_H_ */
