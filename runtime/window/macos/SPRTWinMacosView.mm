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

#define __SPRT_BUILD 1

// These sources pull in AppKit / CoreGraphics / IOKit, which only exist on macOS.
// Guard the whole unit on SPRT_MACOS so it compiles to nothing on iOS (the window
// module is built for all Apple targets). __sprt_def.h is included first so that
// SPRT_MACOS is defined before the guard is evaluated.
#include <sprt/c/bits/__sprt_def.h>

#if SPRT_MACOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#include <Foundation/Foundation.h>

#pragma clang diagnostic pop

#include "SPRTWinMacosContextController.h"
#include "SPRTWinMacosDisplayConfigManager.h"
#include "SPRTWinMacosWindow.h"
#include "SPRTWinMacosView.h"

#if XL_MACOS_DEBUG
#define XL_MACOS_LOG(...) NSSP::log::source().debug(__VA_ARGS__)
#else
#define XL_MACOS_LOG(...)
#endif

static const NSRange kEmptyRange = {NSNotFound, 0};

@implementation SPRTMacosView

+ (Class)layerClass {
	return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSSPWIN::MacosWindow *)window {
	self = [super initWithFrame:frameRect];
	_validAttributesForMarkedText = [NSArray array];
	_textConsumedEvent = false;
	_window = window;

	_mainArea = nullptr;
	_cursorAreas = nullptr;

	//self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
	self.layerContentsPlacement = NSViewLayerContentsPlacementCenter;
	return self;
}

- (BOOL)wantsUpdateLayer {
	return YES;
}

- (CALayer *)makeBackingLayer {
	auto layer = [CAMetalLayer layer];
	layer.delegate = self;
	layer.needsDisplayOnBoundsChange = YES;
	layer.autoresizingMask = kCALayerNotSizable;

	self.layer = layer;
	CGSize viewScale = [self convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	layer.contentsScale = MIN(viewScale.width, viewScale.height);

	return self.layer;
}

- (BOOL)layer:(CALayer *)layer
		shouldInheritContentsScale:(CGFloat)newScale
						fromWindow:(NSWindow *)window {
	_window->emitAppFrame();
	XL_MACOS_LOG("XLMacosView", "shouldInheritContentsScale: ", newScale);
	return YES;
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
	return YES;
}

- (void)viewDidMoveToWindow {
}

- (void)updateTrackingAreas {
	[self removeTrackingArea:_mainArea];

	// ActiveInActiveApp, not ActiveInKeyWindow: auxiliary windows never become key, so
	// key-window tracking would never deliver them a mouse-move and menu hover could not work.
	_mainArea = [[NSTrackingArea alloc]
			initWithRect:[self bounds]
				 options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
			| NSTrackingActiveInActiveApp | NSTrackingInVisibleRect
				   owner:self
				userInfo:nil];
	[self addTrackingArea:_mainArea];
}


// ---------------------------------------------------------------------------------------------
// Text input.
//
// The state lives in the window's TextInputProcessor; this view is the IME driving it. AppKit
// calls the NSTextInputClient methods below out of interpretKeyEvents:, and every query is
// answered from that same state so the input method sees the document the application sees.
// ---------------------------------------------------------------------------------------------

- (NSSPWIN::TextInputProcessor *)textInputProcessor {
	return _window ? _window->getTextInputProcessor() : nullptr;
}

static NSSPWIN::WideString SPRTMacosView_makeWideString(id string) {
	NSString *characters = [string isKindOfClass:[NSAttributedString class]]
			? [(NSAttributedString *)string string]
			: (NSString *)string;

	NSSPWIN::WideString str;
	if (!characters) {
		return str;
	}

	// NSString is already UTF-16, so characterAtIndex: hands back exactly the code units the
	// processor stores - no transcoding, and surrogate pairs survive intact
	str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++i) { str.push_back([characters characterAtIndex:i]); }
	return str;
}

static NSString *SPRTMacosView_makeNSString(NSSP::WideStringView str) {
	if (str.empty()) {
		return @"";
	}
	return [NSString stringWithCharacters:(const unichar *)str.data() length:str.size()];
}

// AppKit passes NSNotFound for "the range I am replacing is the one that is currently marked".
// The processor has no such implicit rule - it replaces exactly the range it is given - so the
// marked range has to be spelled out here, or the second keystroke of a composition would append
// to the first instead of replacing it.
- (NSSPWIN::TextCursor)resolveReplacementRange:(NSRange)range {
	if (range.location != NSNotFound) {
		return NSSPWIN::TextCursor(uint32_t(range.location), uint32_t(range.length));
	}

	if (auto textInput = [self textInputProcessor]) {
		auto marked = textInput->getState().marked;
		if (marked != NSSPWIN::TextCursor::InvalidCursor && marked.length > 0) {
			return marked;
		}
	}

	// No composition running: insert at the caret, replacing whatever is selected
	return NSSPWIN::TextCursor::InvalidCursor;
}

// Which keystrokes the input method is allowed to see. Deliberately the same rule the shared
// TextInputProcessor applies for the backends that have no IME of their own, because a widget
// cannot tell Shift+Tab from Tab once the keystroke has been turned into text.
- (BOOL)shouldInterpretKeyEvent:(NSEvent *)event
					   forInput:(NSSPWIN::TextInputProcessor *)textInput {
	// While a composition is running everything belongs to the IME - that is how a candidate list
	// is navigated and committed
	if ([self hasMarkedText]) {
		return YES;
	}

	const NSEventModifierFlags mods = [event modifierFlags];

	// A chord is a command, not text. Ctrl+Alt is left alone: that combination still produces
	// characters on a number of layouts
	if ((mods & NSEventModifierFlagCommand) != 0) {
		return NO;
	}
	if ((mods & NSEventModifierFlagControl) != 0 && (mods & NSEventModifierFlagOption) == 0) {
		return NO;
	}

	const bool multiline = NSSP::hasFlag(textInput->getState().type,
			NSSPWIN::TextInputType::MultiLineBit);

	switch ([event keyCode]) {
	case 48: // kVK_Tab - navigation everywhere, multi-line fields included
	case 53: // kVK_Escape - releases input; the processor's cancel path handles it
		return NO;
	case 36: // kVK_Return
	case 76: // kVK_ANSI_KeypadEnter
		return multiline ? YES : NO;
	default: break;
	}
	return YES;
}

- (BOOL)interpretKeyEventForTextInput:(NSEvent *)event {
	auto textInput = [self textInputProcessor];
	if (!textInput || !textInput->isRunning()) {
		return NO;
	}

	if (![self shouldInterpretKeyEvent:event forInput:textInput]) {
		return NO;
	}

	// interpretKeyEvents: reports nothing back, so the callbacks below raise this flag; without it
	// there is no way to know whether AppKit turned the keystroke into text or ignored it
	_textConsumedEvent = false;
	[self interpretKeyEvents:[NSArray arrayWithObject:event]];
	return _textConsumedEvent ? YES : NO;
}

- (void)runTextInput {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return;
	}

	if ([self window] && [[self window] firstResponder] != self) {
		[[self window] makeFirstResponder:self];
	}
	[[self inputContext] activate];

	// The application only ever REQUESTED input; enablement is the IME's answer, and this is the
	// IME. Until this lands the application sees enabled=false and shows no caret.
	textInput->handleInputEnabled(true);
}

- (void)cancelTextInput {
	[[self inputContext] discardMarkedText];
	[[self inputContext] deactivate];

	if (auto textInput = [self textInputProcessor]) {
		// Re-entrant by design: this may itself have come from TextInputProcessor::cancel(), and
		// the second handleInputEnabled(false) finds the flag already down and stops there
		textInput->handleInputEnabled(false);
	}
}

- (BOOL)resignFirstResponder {
	// Input follows the responder: the OS took the keyboard away, so the application has to learn
	// that its handler is no longer live
	if (auto textInput = [self textInputProcessor]) {
		if (textInput->isRunning()) {
			[[self inputContext] discardMarkedText];
			textInput->handleInputEnabled(false);
		}
	}
	return [super resignFirstResponder];
}

// AppKit routes everything that is not plain text through here: Backspace, Delete, the arrows,
// Enter. Only the two deletions are ours - the rest is left alone ON PURPOSE, because the widget
// handles them from the key event itself. Calling super for an unhandled selector would beep.
- (void)doCommandBySelector:(SEL)selector {
	if (selector == @selector(deleteBackward:)) {
		[self deleteBackward:nil];
	} else if (selector == @selector(deleteForward:)) {
		[self deleteForward:nil];
	} else if (selector == @selector(insertNewline:)) {
		[self insertNewline:nil];
	}
}

// Only reachable in a multi-line field: shouldInterpretKeyEvent: keeps Enter away from the input
// method everywhere else, because there it means "submit" and the widget has to see the key
- (void)insertNewline:(nullable id)sender {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return;
	}

	_textConsumedEvent = true;

	static const char16_t newline[] = {u'\n'};
	textInput->insertText(NSSP::WideStringView(newline, 1),
			NSSPWIN::TextCursor::InvalidCursor);
}

- (void)deleteForward:(nullable id)sender {
	if (auto textInput = [self textInputProcessor]) {
		_textConsumedEvent = true;
		textInput->deleteForward();
	}
}

- (void)deleteBackward:(nullable id)sender {
	if (auto textInput = [self textInputProcessor]) {
		_textConsumedEvent = true;
		textInput->deleteBackward();
	}
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. */
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return;
	}

	auto str = SPRTMacosView_makeWideString(string);
	auto replacement = [self resolveReplacementRange:replacementRange];

	_textConsumedEvent = true;

	// Composed, not Nothing: this is the committed result of whatever was being assembled, and it
	// takes the place of the temporary composition
	textInput->insertText(NSSP::WideStringView(str), replacement);

	// The processor does not clear the marked range on insert, and a stale one would make the
	// widget keep underlining text that is no longer being composed
	if ([self hasMarkedText]) {
		textInput->unmarkText();
	}
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. selectedRange specifies the selection inside the string being inserted; hence, the location is relative to the beginning of string. When string is an NSString, the receiver is expected to render the marked text with distinguishing appearance (i.e. NSTextView renders with -markedTextAttributes). */
- (void)setMarkedText:(id)string
		   selectedRange:(NSRange)selectedRange
		replacementRange:(NSRange)replacementRange {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return;
	}

	auto str = SPRTMacosView_makeWideString(string);
	auto replacement = [self resolveReplacementRange:replacementRange];

	_textConsumedEvent = true;

	if (str.empty()) {
		// An empty marked string ends the composition and takes the marked run with it
		if (replacement != NSSPWIN::TextCursor::InvalidCursor && replacement.length > 0) {
			textInput->cursorChanged(replacement);
			// cursor.length > 0 makes this remove the whole range and drop the compose state
			textInput->deleteBackward();
		}
		textInput->unmarkText();
		return;
	}

	// Where the run will start once `replacement` has been taken out
	const uint32_t base = (replacement != NSSPWIN::TextCursor::InvalidCursor)
			? replacement.start
			: textInput->getState().cursor.start;

	// The whole inserted run is what is marked; `marked` is relative to the insertion point
	textInput->setMarkedText(NSSP::WideStringView(str), replacement,
			NSSPWIN::TextCursor(0, uint32_t(str.size())));

	// AppKit's selectedRange is the caret INSIDE the run being composed. The processor keeps the
	// caret and the marked range apart, and setMarkedText leaves the caret past the run, so the
	// position has to be restated
	if (selectedRange.location != NSNotFound) {
		textInput->cursorChanged(NSSPWIN::TextCursor(base + uint32_t(selectedRange.location),
				uint32_t(selectedRange.length)));
	}
}

/* The receiver unmarks the marked text. If no marked text, the invocation of this method has no effect. */
- (void)unmarkText {
	if (auto textInput = [self textInputProcessor]) {
		if ([self hasMarkedText]) {
			_textConsumedEvent = true;
			textInput->unmarkText();
		}
	}
	[[self inputContext] discardMarkedText];
}

/* Returns the selection range. The valid location is from 0 to the document length. */
- (NSRange)selectedRange {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return kEmptyRange;
	}

	auto cursor = textInput->getState().cursor;
	if (cursor == NSSPWIN::TextCursor::InvalidCursor) {
		return kEmptyRange;
	}

	// A caret is a valid selection of length 0 - returning kEmptyRange (NSNotFound) here would
	// tell the input method there is no insertion point at all, and composition would not start
	return NSMakeRange(cursor.start, cursor.length);
}

/* Returns the marked range. Returns {NSNotFound, 0} if no marked range. */
- (NSRange)markedRange {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return kEmptyRange;
	}

	auto marked = textInput->getState().marked;
	if (marked == NSSPWIN::TextCursor::InvalidCursor || marked.length == 0) {
		return kEmptyRange;
	}
	return NSMakeRange(marked.start, marked.length);
}

/* Returns whether or not the receiver has marked text. */
- (BOOL)hasMarkedText {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return NO;
	}

	auto marked = textInput->getState().marked;
	return (marked != NSSPWIN::TextCursor::InvalidCursor && marked.length > 0) ? YES : NO;
}

/* Returns attributed string specified by range. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc). */
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
														 actualRange:(nullable NSRangePointer)
																			 actualRange {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return nil;
	}

	auto str = textInput->getState().getStringView();
	if (range.location == NSNotFound || range.location > str.size()) {
		return nil;
	}

	const size_t length = NSSP::min(size_t(range.length), str.size() - range.location);
	if (actualRange != nil) {
		actualRange->location = range.location;
		actualRange->length = length;
	}

	return [[NSAttributedString alloc]
			initWithString:SPRTMacosView_makeNSString(str.sub(range.location, length))];
}

/* Returns an array of attribute names recognized by the receiver.
*/
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
	return _validAttributesForMarkedText;
}

/* Returns the first logical rectangular area for range. The return value is in the screen coordinate. The size value can be negative if the text flows to the left. If non-NULL, actuallRange contains the character range corresponding to the returned area.
*/
- (NSRect)firstRectForCharacterRange:(NSRange)range
						 actualRange:(nullable NSRangePointer)actualRange {
	if (actualRange != nil) {
		*actualRange = range;
	}

	// LIMITATION: the runtime has no caret geometry - TextInputState carries the string and the
	// cursor, not where either is drawn; only the widget knows that. So the candidate window is
	// anchored to the view's leading edge instead of to the caret. Everything else about
	// composition works; fixing this means plumbing a caret rect from the application side.
	NSRect rect = NSMakeRect(0.0, 0.0, 1.0, NSHeight([self bounds]));
	rect = [self convertRect:rect toView:nil];
	if (auto window = [self window]) {
		return [window convertRectToScreen:rect];
	}
	return rect;
}

/* Returns the index for character that is nearest to point. point is in the screen coordinate system.
*/
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	// Same reason as firstRectForCharacterRange: without the widget's layout there is no mapping
	// from a point to a character. NSNotFound is the documented "no character here"
	return NSNotFound;
}

- (NSAttributedString *)attributedString {
	auto textInput = [self textInputProcessor];
	if (!textInput) {
		return nil;
	}

	return [[NSAttributedString alloc]
			initWithString:SPRTMacosView_makeNSString(textInput->getState().getStringView())];
}

/* Returns the fraction of distance for point from the left side of the character. This allows caller to perform precise selection handling.
*/
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point {
	return 0.0f;
}

/* Returns the baseline position relative to the origin of rectangle returned by -firstRectForCharacterRange:actualRange:. This information allows the caller to access finer-grained character position inside the NSTextInputClient document.
*/
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)anIndex {
	return 0.0f;
}

/* Returns if the marked text is in vertical layout.
 */
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
	return NO;
}

@end

#endif // SPRT_MACOS
