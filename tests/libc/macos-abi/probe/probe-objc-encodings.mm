// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// +open sysroot <-> real macOS SDK: Objective-C property and method TYPES.
//
// The enumerator probe (probe-frameworks.mm) pins values. This one pins the
// other half of the hand-written framework surface: the *types* of the
// properties and methods the overlay declares. Those are what objc_msgSend
// actually dispatches on -- a property declared CGFloat where AppKit has
// NSInteger, or a method returning BOOL where it returns NSUInteger, produces a
// call that compiles cleanly on +open and misbehaves on a real Mac, and no
// value check would ever see it.
//
// @encode(__typeof__(expr)) turns a type into a compile-time string, and a
// nil-typed receiver makes the expression type-only -- nothing is evaluated,
// no message is ever sent. check.sh compiles this twice, once per sysroot, and
// diffs the strings, exactly as it does for the value probes.
//
// The set below is seeded from what runtime/window/macos/*.mm and the MoltenVK
// glue actually touch, which is also the rule the overlay headers themselves
// follow ("only the declarations actually used by real code in this
// repository", target-apple/README.md).
//
// So unlike the value probes, this one is deliberately ONE-DIRECTIONAL: the
// overlay is allowed to be narrower than AppKit -- it does not declare
// CATransform3D, CALayer.anchorPoint, NSWindow.alphaValue or
// NSApplication.activationPolicy, and that is not a defect. What is a defect is
// a declaration the overlay DOES make whose type differs from Apple's, and that
// is what every line here pins. Add an entry when you add a declaration to the
// overlay.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <AppKit/AppKit.h>

#define ENC(name, expr) \
	extern "C" __attribute__((used)) const char enc_##name[] = @encode(__typeof__(expr));

// === geometry ==============================================================
ENC(CGFloat, (CGFloat)0)
ENC(NSInteger, (NSInteger)0)
ENC(NSUInteger, (NSUInteger)0)
ENC(BOOL, (BOOL)0)
ENC(CGPoint, (CGPoint){})
ENC(CGSize, (CGSize){})
ENC(CGRect, (CGRect){})
ENC(NSPoint, (NSPoint){})
ENC(NSSize, (NSSize){})
ENC(NSRect, (NSRect){})
ENC(NSRange, (NSRange){})
ENC(CGAffineTransform, (CGAffineTransform){})
ENC(CFTimeInterval, (CFTimeInterval)0)
ENC(NSTimeInterval, (NSTimeInterval)0)

// === CALayer / CAMetalLayer (the Metal-backed view layer) ==================
ENC(CALayer_frame, ((CALayer *)nil).frame)
ENC(CALayer_bounds, ((CALayer *)nil).bounds)
ENC(CALayer_position, ((CALayer *)nil).position)
ENC(CALayer_contentsScale, ((CALayer *)nil).contentsScale)
ENC(CALayer_opaque, ((CALayer *)nil).opaque)
ENC(CALayer_hidden, ((CALayer *)nil).hidden)
ENC(CALayer_cornerRadius, ((CALayer *)nil).cornerRadius)
ENC(CALayer_borderWidth, ((CALayer *)nil).borderWidth)
ENC(CALayer_masksToBounds, ((CALayer *)nil).masksToBounds)
ENC(CAMetalLayer_device, ((CAMetalLayer *)nil).device)
ENC(CAMetalLayer_pixelFormat, ((CAMetalLayer *)nil).pixelFormat)
ENC(CAMetalLayer_framebufferOnly, ((CAMetalLayer *)nil).framebufferOnly)
ENC(CAMetalLayer_drawableSize, ((CAMetalLayer *)nil).drawableSize)
ENC(CAMetalLayer_maximumDrawableCount, ((CAMetalLayer *)nil).maximumDrawableCount)
ENC(CAMetalLayer_displaySyncEnabled, ((CAMetalLayer *)nil).displaySyncEnabled)

// === NSWindow ==============================================================
ENC(NSWindow_frame, ((NSWindow *)nil).frame)
ENC(NSWindow_contentView, ((NSWindow *)nil).contentView)
ENC(NSWindow_styleMask, ((NSWindow *)nil).styleMask)
ENC(NSWindow_level, ((NSWindow *)nil).level)
ENC(NSWindow_title, ((NSWindow *)nil).title)
ENC(NSWindow_hasShadow, ((NSWindow *)nil).hasShadow)
ENC(NSWindow_opaque, ((NSWindow *)nil).opaque)
ENC(NSWindow_backingScaleFactor, ((NSWindow *)nil).backingScaleFactor)
ENC(NSWindow_collectionBehavior, ((NSWindow *)nil).collectionBehavior)
ENC(NSWindow_screen, ((NSWindow *)nil).screen)
ENC(NSWindow_contentLayoutRect, ((NSWindow *)nil).contentLayoutRect)
ENC(NSWindow_convertRectToBacking, [(NSWindow *)nil convertRectToBacking:(NSRect){}])
ENC(NSWindow_standardWindowButton, [(NSWindow *)nil standardWindowButton:NSWindowCloseButton])

// === NSView ================================================================
ENC(NSView_frame, ((NSView *)nil).frame)
ENC(NSView_bounds, ((NSView *)nil).bounds)
ENC(NSView_layer, ((NSView *)nil).layer)
ENC(NSView_wantsLayer, ((NSView *)nil).wantsLayer)
ENC(NSView_layerContentsPlacement, ((NSView *)nil).layerContentsPlacement)
ENC(NSView_window, ((NSView *)nil).window)
ENC(NSView_convertRectToBacking, [(NSView *)nil convertRectToBacking:(NSRect){}])

// === NSScreen / NSEvent ====================================================
ENC(NSScreen_frame, ((NSScreen *)nil).frame)
ENC(NSScreen_visibleFrame, ((NSScreen *)nil).visibleFrame)
ENC(NSScreen_backingScaleFactor, ((NSScreen *)nil).backingScaleFactor)
ENC(NSEvent_type, ((NSEvent *)nil).type)
ENC(NSEvent_modifierFlags, ((NSEvent *)nil).modifierFlags)
ENC(NSEvent_locationInWindow, ((NSEvent *)nil).locationInWindow)
ENC(NSEvent_keyCode, ((NSEvent *)nil).keyCode)
ENC(NSEvent_timestamp, ((NSEvent *)nil).timestamp)

// === Foundation ============================================================
ENC(NSString_length, ((NSString *)nil).length)
ENC(NSString_UTF8String, ((NSString *)nil).UTF8String)
ENC(NSArray_count, ((NSArray *)nil).count)
ENC(NSObject_hash, ((NSObject *)nil).hash)
