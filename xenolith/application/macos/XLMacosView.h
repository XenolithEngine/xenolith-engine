/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_MACOS_XLMACOSVIEW_H_
#define XENOLITH_APPLICATION_MACOS_XLMACOSVIEW_H_

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CADisplayLink.h>

#include "XLMacos.h"

@interface XLMacosView
: NSView <NSTextInputClient, NSViewLayerContentScaleDelegate, CALayerDelegate> {
	NSXLPL::MacosWindow *_window;
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;
	bool _textDirty;

	NSTrackingArea *_mainArea;
	NSArray<NSTrackingArea *> *_cursorAreas;
};

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSXLPL::MacosWindow *)window;

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len;

- (void)updateTextInputWithText:(stappler::WideStringView)str
					   position:(uint32_t)pos
						 length:(uint32_t)len
						   type:(stappler::xenolith::core::TextInputType)type;

- (void)runTextInputWithText:(stappler::WideStringView)str
					position:(uint32_t)pos
					  length:(uint32_t)len
						type:(stappler::xenolith::core::TextInputType)type;

- (void)cancelTextInput;

@end

#endif // XENOLITH_APPLICATION_MACOS_XLMACOSVIEW_H_
