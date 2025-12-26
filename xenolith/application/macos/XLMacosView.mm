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

#import "XLMacosView.h"
#import "XLMacosWindow.h"

#if XL_MACOS_DEBUG
#define XL_MACOS_LOG(...) NSSP::log::source().debug(__VA_ARGS__)
#else
#define XL_MACOS_LOG(...)
#endif

static const NSRange kEmptyRange = {NSNotFound, 0};

@implementation XLMacosView

+ (Class)layerClass {
	return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSXLPL::MacosWindow *)window {
	self = [super initWithFrame:frameRect];
	_validAttributesForMarkedText = [NSArray array];
	_textDirty = false;
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

- (void)viewDidMoveToWindow {
}

- (void)updateTrackingAreas {
	[self removeTrackingArea:_mainArea];

	_mainArea = [[NSTrackingArea alloc]
			initWithRect:[self bounds]
				 options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
			| NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
				   owner:self
				userInfo:nil];
	[self addTrackingArea:_mainArea];
}

- (void)keyDown:(NSEvent *)theEvent {
	//TextInputManager *m = _textInput.get();
	//if (m->isInputEnabled()) {
	//[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
	if (_textDirty) {
		//[(XLMacViewController *)self.window.contentViewController submitTextData:m->getString() cursor:m->getCursor() marked:m->getMarked()];
		_textDirty = false;
	}
	//}
	[super keyDown:theEvent];
}

- (void)deleteForward:(nullable id)sender {
	std::cout << "deleteForward\n";
	//TextInputManager *m = _textInput.get();
	//m->deleteForward();
}

- (void)deleteBackward:(nullable id)sender {
	std::cout << "deleteBackward\n";
	//TextInputManager *m = _textInput.get();
	//m->deleteBackward();
}

- (void)insertTab:(nullable id)sender {
	//TextInputManager *m = _textInput.get();
	//m->insertText(u"\t");
}

- (void)insertBacktab:(nullable id)sender {
}

- (void)insertNewline:(nullable id)sender {
	//TextInputManager *m = _textInput.get();
	//m->insertText(u"\n");
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. */
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	stappler::xenolith::WideString str;
	str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++i) {
		str.push_back([characters characterAtIndex:i]);
	}

	//TextInputManager *m = _textInput.get();
	//m->insertText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)));
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. selectedRange specifies the selection inside the string being inserted; hence, the location is relative to the beginning of string. When string is an NSString, the receiver is expected to render the marked text with distinguishing appearance (i.e. NSTextView renders with -markedTextAttributes). */
- (void)setMarkedText:(id)string
		   selectedRange:(NSRange)selectedRange
		replacementRange:(NSRange)replacementRange {
	std::cout << "setMarkedText\n";
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	stappler::xenolith::WideString str;
	str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++i) {
		str.push_back([characters characterAtIndex:i]);
	}

	//TextInputManager *m = _textInput.get();
	//m->setMarkedText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)), TextCursor(uint32_t(selectedRange.location), uint32_t(selectedRange.length)));
}

/* The receiver unmarks the marked text. If no marked text, the invocation of this method has no effect. */
- (void)unmarkText {
	//TextInputManager *m = _textInput.get();
	//m->unmarkText();
}

/* Returns the selection range. The valid location is from 0 to the document length. */
- (NSRange)selectedRange {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getCursor();
	//if (cursor.length > 0) {
	//	return NSRange{cursor.start, cursor.length};
	//}
	return kEmptyRange;
}

/* Returns the marked range. Returns {NSNotFound, 0} if no marked range. */
- (NSRange)markedRange {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getMarked();
	//if (cursor.length > 0) {
	//	return NSRange{cursor.start, cursor.length};
	//}
	return kEmptyRange;
}

/* Returns whether or not the receiver has marked text. */
- (BOOL)hasMarkedText {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getMarked();
	//if (cursor.length > 0) {
	//	return YES;
	//}
	return NO;
}

/* Returns attributed string specified by range. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc). */
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
														 actualRange:(nullable NSRangePointer)
																			 actualRange {
	std::cout << "attributedSubstringForProposedRange\n";
	//TextInputManager *m = _textInput.get();
	//WideStringView str = m->getStringByRange(TextCursor{uint32_t(range.location), uint32_t(range.length)});
	//if (actualRange != nil) {
	//	auto fullString = m->getString();

	//	actualRange->location = (str.data() - fullString.data());
	//	actualRange->length = str.size();
	//}

	//return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
	return nil;
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
	std::cout << "firstRectForCharacterRange\n";
	return self.frame;
}

/* Returns the index for character that is nearest to point. point is in the screen coordinate system.
*/
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	std::cout << "characterIndexForPoint\n";
	return 0;
}

- (NSAttributedString *)attributedString {
	std::cout << "attributedString\n";
	//TextInputManager *m = _textInput.get();
	//auto str = m->getString();

	//return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
	return nil;
}

/* Returns the fraction of distance for point from the left side of the character. This allows caller to perform precise selection handling.
*/
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point {
	std::cout << "fractionOfDistanceThroughGlyphForPoint\n";
	return 0.0f;
}

/* Returns the baseline position relative to the origin of rectangle returned by -firstRectForCharacterRange:actualRange:. This information allows the caller to access finer-grained character position inside the NSTextInputClient document.
*/
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)anIndex {
	std::cout << "baselineDeltaForCharacterAtIndex\n";
	return 0.0f;
}

/* Returns if the marked text is in vertical layout.
 */
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
	std::cout << "drawsVerticallyForCharacterAtIndex\n";
	return NO;
}

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len {
	//TextInputManager *m = _textInput.get();
	//m->cursorChanged(TextCursor(pos, len));
}

- (void)updateTextInputWithText:(stappler::WideStringView)str
					   position:(uint32_t)pos
						 length:(uint32_t)len
						   type:(stappler::xenolith::core::TextInputType)type {
	_textDirty = false;
	//TextInputManager *m = _textInput.get();
	//m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
}

- (void)runTextInputWithText:(stappler::WideStringView)str
					position:(uint32_t)pos
					  length:(uint32_t)len
						type:(stappler::xenolith::core::TextInputType)type {
	_textDirty = false;
	//TextInputManager *m = _textInput.get();
	//m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
	//m->setInputEnabled(true);
}

- (void)cancelTextInput {
	//TextInputManager *m = _textInput.get();
	//m->cancel();
	_textDirty = false;
}

@end
