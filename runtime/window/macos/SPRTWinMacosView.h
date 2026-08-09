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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSVIEW_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSVIEW_H_

#if SPRT_MACOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CADisplayLink.h>

#pragma clang diagnostic pop

#include "SPRTWinMacos.h"

// The view IS the IME on this platform.
//
// Unlike X11/Wayland/Win32, where TextInputProcessor does the editing itself, here AppKit's input
// context does it: interpretKeyEvents: turns keystrokes into insertText:/setMarkedText:/
// doCommandBySelector:, and this view forwards those into the window's TextInputProcessor, which
// exists only to hold the state and propagate it to the application. Every NSTextInputClient query
// is answered out of that same state — AppKit must see the document the application sees, or
// composition lands in the wrong place.
@interface SPRTMacosView
: NSView <NSTextInputClient, NSViewLayerContentScaleDelegate, CALayerDelegate> {
	NSSPWIN::MacosWindow *_window;
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;

	// Set by whichever NSTextInputClient callback the input context invoked while
	// interpretKeyEventForTextInput: was running; that is the only way to learn whether AppKit
	// actually consumed the keystroke.
	bool _textConsumedEvent;

	NSTrackingArea *_mainArea;
	NSArray<NSTrackingArea *> *_cursorAreas;
};

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSSPWIN::MacosWindow *)window;

// Take/release the OS input context, and report enablement to the processor. Called from
// MacosWindow::updateTextInput / cancelTextInput
- (void)runTextInput;
- (void)cancelTextInput;

// Offer a key event to the input method. Returns YES when the IME consumed it, so the caller can
// cancel the corresponding scene event instead of letting the widget act on it twice
- (BOOL)interpretKeyEventForTextInput:(NSEvent *)event;

@end

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSVIEW_H_
