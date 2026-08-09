/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

// Include-only subunit: the shared body of SPRTMacosWindow (NSWindow) and SPRTMacosPanel
// (NSPanel). Included from inside an @implementation block in SPRTWinMacosWindow.mm, once per
// class, with SPRT_MACOS_WINDOW_ROLE_NAME naming the class for the debug log. Both classes declare
// the three ivars this touches; everything else is plain NSWindow API, which NSPanel inherits.

- (instancetype)initWithContentRect:(NSRect)contentRect
						  styleMask:(NSWindowStyleMask)style
							backing:(NSBackingStoreType)backingStoreType
							  defer:(BOOL)flag {
	self = [super initWithContentRect:contentRect
							styleMask:style
							  backing:backingStoreType
								defer:flag];
	_defaultStyle = style;
	_allowKey = YES;
	_allowMain = YES;
	return self;
}

- (void)configureRole:(BOOL)allowKey allowMain:(BOOL)allowMain {
	_allowKey = allowKey;
	_allowMain = allowMain;
}

- (BOOL)canBecomeKeyWindow {
	return _allowKey;
}

- (BOOL)canBecomeMainWindow {
	return _allowMain;
}

- (NSWindowStyleMask)defaultStyle {
	return _defaultStyle;
}

- (void)setFrame:(NSRect)frameRect
				  display:(BOOL)displayFlag
				 duration:(NSTimeInterval)duration
		completionHandler:(nullable void (^)(void))completionHandler {
	[(SPRTMacosViewController *)self.contentViewController setEngineLiveResize:YES];
	[NSAnimationContext
			runAnimationGroup:^(NSAnimationContext *_Nonnull context) {
			  context.duration = duration;
			  [self.animator setFrame:frameRect display:displayFlag];
			}
			completionHandler:^(void) {
			  [(SPRTMacosViewController *)self.contentViewController setEngineLiveResize:NO];
			  if (completionHandler) {
				  completionHandler();
			  }
			}];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag {
	XL_MACOS_LOG(SPRT_MACOS_WINDOW_ROLE_NAME, "setFrame: ", frameRect.origin.x, " ",
			frameRect.origin.y, " ", frameRect.size.width, " ", frameRect.size.height);
	[super setFrame:frameRect display:displayFlag];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag animate:(BOOL)animateFlag {
	XL_MACOS_LOG(SPRT_MACOS_WINDOW_ROLE_NAME, "setFrame: ", frameRect.origin.x, " ",
			frameRect.origin.y, " ", frameRect.size.width, " ", frameRect.size.height);
	if (!animateFlag) {
		[super setFrame:frameRect display:displayFlag animate:NO];
	} else {
		[self setFrame:frameRect
						  display:displayFlag
						 duration:[self animationResizeTime:frameRect]
				completionHandler:nil];
	}
}

- (void)toggleFullScreen:(id)sender withScreen:(NSScreen *)screen {
	if (screen == self.screen) {
		[self toggleFullScreen:sender];
	} else {
		auto screenFrame = screen.frame;
		auto windowFrame = self.frame;
		auto x = (screenFrame.size.width - windowFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - windowFrame.size.height) / 2.0;
		auto targetRect = NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y},
			windowFrame.size};

		__weak NSWindow *ref = self;

		[self setFrame:targetRect
						  display:YES
						 duration:[self animationResizeTime:targetRect]
				completionHandler:^() { [ref toggleFullScreen:sender]; }];
	}
}

- (NSTimeInterval)animationResizeTime:(NSRect)newFrame {
	return [super animationResizeTime:newFrame];
}

- (instancetype)animator {
	return [super animator];
}

- (id)animationForKey:(NSAnimatablePropertyKey)key {
	return [super animationForKey:key];
}
