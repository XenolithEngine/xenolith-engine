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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUITEXTINPUT_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUITEXTINPUT_H_

#include "XLUiInteractiveComponent.h"
#include "XLUiStyleResolver.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dVectorSprite.h"
#include "XLDynamicStateSystem.h"
#include "XLTextInputManager.h"
#include "XLDragTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Resolved paint for a TextInput, filled by the "text-input" type applier.
//
// Padding is taken over from the default mapping on purpose: the generic applier routes padding-*
// into FlexLayoutInfo, but a text input is not a flex container - its padding insets the text
// viewport. The consequence is that `text-input { display: flex }` is not supported.
struct TextInputStyleComponent {
	static ComponentId Id;

	Color4B backgroundColor = Color4B::WHITE;
	Color4B outlineColor = Color4B::BLACK;
	float outlineWidth = 0.0f;
	document::BorderStyle outlineStyle = document::BorderStyle::Solid;
	float borderRadiusTopLeft = 0.0f;
	float borderRadiusTopRight = 0.0f;
	float borderRadiusBottomRight = 0.0f;
	float borderRadiusBottomLeft = 0.0f;
	Padding padding;

	// There is no caret-color/::selection in the document engine's CSS subset, so these come from
	// the custom properties `--caret-color` / `--selection-color` / `--marked-color`. When a
	// property is not declared the widget falls back to the resolved `color`.
	Color4B caretColor = Color4B::WHITE;
	Color4B selectionColor = Color4B::WHITE;
	Color4B markedColor = Color4B::WHITE;
	bool hasCaretColor = false;
	bool hasSelectionColor = false;
	bool hasMarkedColor = false;
};

enum class TextInputPasswordMode {
	NotPassword,
	ShowAll, // stored as a password for the OS, but rendered as plain text
	ShowNone, // every character rendered as a bullet
};

// Clipped viewport of a TextInput: the text label, the placeholder label and the caret.
//
// It exists as a separate node because horizontal overflow of a single-line field is "slide the
// label inside a fixed box": the box is this node, its scissor bounds the slide, and the caret
// rides the label as a child so it needs no offset arithmetic of its own. TextInput itself is a
// VectorSprite, which already owns a DynamicStateSystem for its image, so a second scissor cannot
// live there.
class SP_PUBLIC TextInputContainer : public Node {
public:
	virtual ~TextInputContainer();

	virtual bool init() override;

	virtual void update(const UpdateTime &) override;
	virtual void handleContentSizeDirty() override;

	// flushes a pending caret recomputation before drawing, so a string/cursor change costs one
	// layout pass per frame instead of one per mutation
	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	basic2d::Label *getLabel() const { return _label; }
	basic2d::Label *getPlaceholder() const { return _placeholder; }
	basic2d::Layer *getCaret() const { return _caret; }

	// caret visible and blinking; mirrors "the platform granted us input"
	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }

	// `activePosition` is the end of the selection the user is moving (the one opposite the
	// selection anchor); scrolling and the caret follow it, so extending a selection rightwards
	// keeps its right edge in view instead of jumping back to its left one. maxOf<uint32_t>() means
	// "no moving end known" and falls back to the start of the range.
	virtual void setCursor(TextCursor, uint32_t activePosition = maxOf<uint32_t>());
	virtual TextCursor getCursor() const { return _cursor; }

	// character index the caret and the auto-scroll track; equals _cursor.start without a selection
	virtual uint32_t getCursorActivePosition() const { return _cursorActive; }

	virtual void setMarked(TextCursor);
	virtual TextCursor getMarked() const { return _marked; }

	virtual void setCaretColor(const Color4F &);
	virtual void setSelectionColor(const Color4F &);
	virtual void setMarkedColor(const Color4F &);

	// off makes the caret solid - required for deterministic screenshots
	virtual void setCaretBlink(bool);
	virtual bool isCaretBlink() const { return _caretBlink; }

	virtual void setReadOnly(bool);

	virtual void setPlaceholderVisible(bool);

	// the label content changed: caret geometry has to be recomputed
	virtual void handleLabelChanged();

	// character position under a point in world space, or InvalidCursor outside the text
	virtual TextCursor getCursorForPosition(const Vec2 &,
			font::CharSelectMode = font::CharSelectMode::Best);

	virtual bool hasHorizontalOverflow() const;
	virtual void moveHorizontalOverflow(float d);

	// how far the label is currently slid inside the viewport (<= 0)
	virtual float getLabelOffset() const;

	// drive the label past the viewport edge while a drag-selection runs; Vec2::INVALID stops it
	virtual void setAutoScrollTarget(const Vec2 &worldLocation);

protected:
	virtual void updateCaretPosition();
	virtual void updateCaretBlink();
	virtual void runAdjustLabel(float pos);

	basic2d::Label *_label = nullptr;
	basic2d::Label *_placeholder = nullptr;
	basic2d::Layer *_caret = nullptr;
	DynamicStateSystem *_scissor = nullptr;

	TextCursor _cursor = TextCursor::InvalidCursor;
	uint32_t _cursorActive = 0;
	TextCursor _marked = TextCursor::InvalidCursor;
	Vec2 _autoScrollTarget = Vec2::INVALID;
	bool _enabled = false;
	bool _readOnly = false;
	bool _caretBlink = true;
	bool _caretDirty = false;
};

// Single-line text input.
//
// CSS: type `text-input`, class `xl-ui-text-input`. Background, outline and border radius are
// drawn by the widget itself (like ui::Button); the caret and selection colours come from the
// custom properties `--caret-color` / `--selection-color` / `--marked-color`:
//
//   text-input { width:350px; height:39px; background-color:#292929;
//     outline-width:1px; outline-color:rgba(255,255,255,.15); border-radius:7px;
//     padding:0 12px; color:#E8E8E8; font-size:14px;
//     --caret-color:#FCB400; --selection-color:rgba(252,180,0,.35); }
//   text-input:hover { outline-color:rgba(255,255,255,.30); }
//   text-input:focus { outline-color:#FCB400; }
//
// STATE OWNERSHIP. The OS-side IME owns the text input state; this widget does not. `_inputState`
// is a read-only mirror written ONLY by handleTextInput(), the TextInputHandler callback. Every
// local edit - arrow keys, Home/End, Shift-selection, a click, setText(), selectAll() - is a
// REQUEST pushed with TextInputHandler::update(); nothing moves on screen until the platform
// echoes it back. That is what lets system autocorrection, CJK composition and platform paste
// rewrite the text without the widget asking. The one exception is a field with no active handler
// (read-only, or not focused): there is no platform authority to defer to, so setText() writes
// locally - see the comment at that call site.
//
// KEYBOARD. The runtime's TextInputProcessor claims events BEFORE the scene sees them: printable
// characters, Backspace and Delete never reach this widget's key recognizer. What it declines, and
// this widget therefore binds, is everything that is a command rather than text: the arrows,
// Home/End and Shift-selection; the Ctrl chords (A/C/X/V) - which the processor has to decline
// explicitly, because the backends disagree on whether Ctrl+C's keychar is 'c' or 0x03 and either
// one would be typed into the field; and Tab/Enter, which need their modifiers intact so Shift+Tab
// can mean "the previous field". Escape stays with the processor, which cancels input; the widget
// learns about it from an echo with enabled=false.
//
// validateInput() still strips '\n'/'\r'/'\t' out of the echoed string. That is the degraded path
// for a platform that delivers Enter/Tab as text anyway (macOS insertText:) - it fires the enter
// callback, but a character carries no modifiers, so Shift+Tab degrades to Tab there.
class SP_PUBLIC TextInput : public basic2d::VectorSprite {
public:
	using ChangeCallback = Function<void(StringView)>;
	using EnterCallback = Function<void()>;

	// Tab on a focused field, `backwards` when Shift was held. Return true to consume it; with no
	// callback installed the field blurs, which is what a standalone field has always done
	using NavigateCallback = Function<bool(bool backwards)>;

	virtual ~TextInput();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty(const ComponentMask &) override;

	virtual void setText(StringView);
	virtual void setText(WideStringView);
	virtual StringView getText() const;
	virtual WideStringView getTextW() const { return _inputState.getStringView(); }

	// what is actually rendered - differs from getText() in a password field
	virtual WideStringView getDisplayText() const;

	virtual void setPlaceholder(StringView);
	virtual StringView getPlaceholder() const;

	// a read-only field still takes taps and selections (so its text can be read and selected) but
	// never acquires text input, so no OS keyboard is raised and no caret is shown
	virtual void setReadOnly(bool);
	virtual bool isReadOnly() const { return _readOnly; }

	// fired when the committed text changes. Not fired while a composition is in progress, nor for
	// a cursor-only change - a marked range is not committed text yet.
	virtual void setCallback(ChangeCallback &&);
	virtual void setEnterCallback(EnterCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	virtual void focus();
	virtual void blur();
	virtual bool isFocused() const { return _focused; }
	virtual void selectAll();

	// The selection to and from the OS clipboard. A password field refuses to copy or cut - its
	// contents are exactly what must not leave the widget.
	//
	// paste() is ASYNCHRONOUS and returns whether the read was STARTED: the clipboard answer comes
	// back on the app thread, and the insert it performs is a text-input request like any other,
	// so nothing has moved by the time this returns. A read that lands after the field was edited,
	// blurred or pasted into again is discarded.
	virtual bool copy();
	virtual bool cut();
	virtual bool paste();

	// Insert the text of a dropped payload at the caret. Wired to this field's DropTarget; public
	// so a subclass can reuse it, and so a test can exercise the insertion without a pointer
	virtual bool handleTextDrop(const DragEvent &);

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }

	virtual void setInputType(TextInputType);
	virtual TextInputType getInputType() const { return _inputType; }

	virtual void setPasswordMode(TextInputPasswordMode);
	virtual TextInputPasswordMode getPasswordMode() const { return _passwordMode; }

	// 0 = unlimited. Enforced in validateInput(), i.e. by correcting the echo, not by refusing it
	virtual void setMaxChars(size_t);
	virtual size_t getMaxChars() const { return _maxChars; }

	virtual void setCursor(TextCursor);
	virtual TextCursor getCursor() const { return _inputState.cursor; }
	virtual TextCursor getMarked() const { return _inputState.marked; }

	virtual void setCaretBlink(bool);
	virtual bool isCaretBlink() const;

	// the last state the platform reported; read-only by design (see the class comment)
	const TextInputState &getInputState() const { return _inputState; }

	basic2d::Label *getLabel() const;
	TextInputContainer *getContainer() const { return _container; }

	virtual bool setStyleValue(const ResolvedStyle &, document::ParameterName,
			const document::StyleValue &);

protected:
	/* Registers the per-attribute appliers (background, outline, radius, padding, CmdReset) for CSS
	type `type`, routing them into TextInput::setStyleValue. Every field built on this widget calls
	it from init() with its own type; repeated calls for the same type are ignored. Same seam, and
	the same reason, as Panel::registerStyleAppliers. */
	static void registerStyleAppliers(StringView type);

	// The viewport node, built once by init(). A subclass returns its own container here to replace
	// the geometry the stock one implements: caret placement, the label slide and the point->cursor
	// mapping all assume a single line, and a multi-line view has to answer all three differently.
	//
	// A factory rather than a swap after the fact, because init() wires the result into _container
	// and everything below reaches the text through it - a container replaced later would leave the
	// first frame, and any style pass before it, addressing the old one.
	virtual Rc<TextInputContainer> makeContainer();

	// (re)build the VectorImage: a (optionally rounded) rect filled with the resolved background,
	// plus an outline stroke when its width is > 0
	virtual void updateBackgroundImage();

	// the single writer of InteractiveComponent; the counters are cumulative, so each flag is only
	// pushed on an edge
	virtual void updateInteractiveState();

	virtual void acquireInput(TextCursor);

	// push a request for the current string with a new cursor/marked range
	virtual void pushRequest(TextCursor, TextCursor marked = TextCursor::InvalidCursor);
	virtual void pushRequest(TextInputString *, TextCursor,
			TextCursor marked = TextCursor::InvalidCursor);

	// Replace `replace` with `text` as a REQUEST. Falls back to a local write when no handler is
	// active, for the same reason setText() does: with no platform authority there is nothing to
	// defer to. Length and character filtering are left to validateInput() on the echo
	virtual void insertText(WideStringView text, TextCursor replace);

	// TextInputHandler::onData - the only writer of _inputState
	virtual void handleTextInput(const TextInputState &);

	// per-character filter; return false to reject a character
	virtual bool handleInputChar(char16_t);

	// correct an echoed state in place (max length, character filter, Enter/Tab). Returns true when
	// the state was modified, which makes the caller re-push it.
	virtual bool validateInput(TextInputState &);

	virtual void updateDisplayString();
	virtual void updateStyleColors();

	virtual bool handleKey(const GestureData &);
	virtual bool handleTextHotkey(HotkeyId, const InputEvent &);
	virtual bool handleTap(const GestureTap &);
	virtual bool handlePress(const GesturePress &, bool begin);

	// A press held past the long-press interval, once per interval. The first one selects the word
	// under the finger, the second the whole text; after that there is nothing left to widen to.
	virtual bool handleLongPress(const GesturePress &);

	virtual bool handleSwipeBegin(const Vec2 &);
	virtual bool handleSwipe(const Vec2 &location, const Vec2 &delta);
	virtual bool handleSwipeEnd();

	// cursor after moving `delta` characters, clamped to the string
	uint32_t offsetCursor(int32_t delta) const;
	void moveCursor(uint32_t target, bool select);

	// The word under this point, or InvalidCursor when there is no glyph there
	TextCursor getWordForPosition(const Vec2 &) const;

	// Take the cursor a gesture asks for, acquiring input first when the field is not focused yet.
	// A read-only field never acquires it - it can be selected, not edited.
	void applyGestureCursor(TextCursor);

	// Where the widget last ASKED the cursor to be. Successive local edits chain onto this instead
	// of onto _inputState.cursor, because two keystrokes can arrive in one batch (key repeat) and
	// the echo for the first has not come back yet - without it the second move would recompute
	// from the same stale position and the caret would appear stuck. Overwritten by every echo, so
	// a platform-side rewrite always wins.
	TextCursor pendingCursor() const;

	// End of `cursor` the user is moving: the one opposite _selectionAnchor. The container scrolls
	// to it, so widening a selection to the right shows its right edge and not its left one.
	// maxOf<uint32_t>() when no selection is being extended.
	uint32_t activeCursorPosition(TextCursor cursor) const;

	TextInputContainer *_container = nullptr;
	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	TextInputHandler _handler;
	TextInputState _inputState;

	ChangeCallback _callback;
	EnterCallback _enterCallback;
	NavigateCallback _navigateCallback;

	String _placeholderText;
	mutable String _textCache;

	TextInputType _inputType = TextInputType::Text_Text;
	TextInputPasswordMode _passwordMode = TextInputPasswordMode::NotPassword;
	size_t _maxChars = 0;

	// anchor of a Shift-selection or a drag-selection; InvalidCursor when none is running
	uint32_t _selectionAnchor = maxOf<uint32_t>();

	// Bumped by every paste request, so the answer to a superseded one can be recognized and
	// dropped instead of landing on top of a newer edit
	uint64_t _pasteSerial = 0;

	// see pendingCursor()
	TextCursor _pendingCursor = TextCursor::InvalidCursor;

	bool _enabled = true;
	bool _readOnly = false;
	bool _focused = false;
	bool _dragSelecting = false;
	bool _panning = false;

	// A long press ends with a release, and a release is also a tap: without this the tap that
	// closes the gesture would drop the selection the long press had just made. Cleared when the
	// next press starts.
	bool _longPressApplied = false;

	// edge trackers for InteractiveComponent's cumulative counters
	bool _hoverApplied = false;
	bool _focusApplied = false;
	bool _activeApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUITEXTINPUT_H_
