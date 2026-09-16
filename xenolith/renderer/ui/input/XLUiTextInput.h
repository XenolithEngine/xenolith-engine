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

#include "XLInteractiveComponent.h"
#include "XLUiStyleResolver.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dVectorSprite.h"
#include "XLDynamicStateSystem.h"
#include "XLTextInputManager.h"
#include "XLDragTypes.h"
#include "XLUiTextHistory.h"
#include "XLUiControlLock.h"

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
// A separate node so its scissor bounds the horizontal slide of the label; the caret is a child of
// the label. TextInput itself is a VectorSprite whose DynamicStateSystem is already taken.
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

	// caret visible and blinking; mirrors "the platform granted us input", which is not the
	// control state `:enabled` reads, so it is kept here rather than in InteractiveComponent
	virtual void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

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
	// Centres the text and the placeholder vertically. ui::TextViewContainer overrides it to
	// start at the top.
	virtual void updateLabelPosition();

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
// The line is centred vertically in what `height` and `padding` leave (an ui::InlineEditor gives
// it the height of the row it covers).
//
// The OS-side IME owns the text input state. `_inputState` is a read-only mirror written only by
// handleTextInput(), the TextInputHandler callback. Every local edit (arrows, Home/End, selection,
// a click, setText(), selectAll()) is a request pushed with TextInputHandler::update() and shows
// only once the platform echoes it, so autocorrection, composition and platform paste can rewrite
// the text. Without an active handler (read-only or not focused) setText() writes locally.
//
// The runtime's TextInputProcessor claims printable characters, Backspace and Delete before the
// scene sees them. This widget binds what it declines: arrows, Home/End, Shift-selection, the Ctrl
// chords A/C/X/V (declined explicitly, since backends disagree on Ctrl+C's keychar), and Tab/Enter
// with modifiers intact for Shift+Tab. Escape stays with the processor, which cancels input; the
// widget sees an echo with enabled=false.
//
// validateInput() also strips '\n'/'\r'/'\t' from the echo, for platforms that deliver Enter/Tab as
// text (macOS insertText:); there Shift+Tab degrades to Tab.
//
// TextHistoryTarget is implemented here and by TextView: the field asks the platform, the view
// edits its own document.
class SP_PUBLIC TextInput : public basic2d::VectorSprite,
						   public TextHistoryTarget,
						   public EditLockTarget {
public:
	using ChangeCallback = Function<void(StringView)>;
	using EnterCallback = Function<void()>;

	// Tab on a focused field, `backwards` when Shift was held. Return true to consume it; with no
	// callback installed the field blurs
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
	virtual bool isReadOnly() const { return isControlReadOnly(this); }

	// fired when the committed text changes. Not fired while a composition is in progress, nor for
	// a cursor-only change - a marked range is not committed text yet.
	virtual void setCallback(ChangeCallback &&);
	virtual void setEnterCallback(EnterCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	virtual void focus();
	virtual void blur();
	virtual bool isFocused() const { return _focused; }
	virtual void selectAll();

	// The selection to and from the OS clipboard. A password field refuses to copy or cut.
	//
	// paste() is asynchronous and returns whether the read was started; the insert is a text-input
	// request on the app thread. A read that lands after the field was edited, blurred or pasted
	// into again is discarded.
	virtual bool copy();
	virtual bool cut();
	virtual bool paste();

	// Insert the text of a dropped payload at the caret. Wired to this field's DropTarget; public
	// so a subclass can reuse it, and so a test can exercise the insertion without a pointer
	virtual bool handleTextDrop(const DragEvent &);

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

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

	/* Undo is off by default here (on in TextView), so Ctrl+Z in a plain field reaches the
	application's document undo. */
	virtual void setUndoEnabled(bool);
	virtual bool isUndoEnabled() const { return _history.isEnabled(); }

	virtual bool undo();
	virtual bool redo();
	virtual bool canUndo() const { return _history.canUndo(); }
	virtual bool canRedo() const { return _history.canRedo(); }

	// What Ctrl+Z would take back, for a menu that names it. Empty when there is nothing.
	virtual StringView getUndoName() const { return _history.getUndoName(); }
	virtual StringView getRedoName() const { return _history.getRedoName(); }

	TextHistory &getHistory() { return _history; }
	const TextHistory &getHistory() const { return _history; }

	// -- TextHistoryTarget --

	virtual WideStringView sliceForHistory(uint32_t pos, uint32_t len) const override;
	virtual void applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView) override;
	virtual void setHistoryCursor(TextCursor) override;
	virtual void beginHistoryBatch() override;
	virtual void endHistoryBatch() override;

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
	it from init() with its own type; repeated calls for the same type are ignored. */
	static void registerStyleAppliers(StringView type);

	// The viewport node, built once by init(). A subclass returns its own container to replace the
	// single-line caret placement, label slide and point->cursor mapping.
	virtual Rc<TextInputContainer> makeContainer();

	/* Extra room a subclass takes out of the text viewport, on top of the CSS padding (e.g.
	ui::NumberField's unit label). The base stays the single writer of the container's geometry.
	Read before the container is sized, so the subclass must measure first. */
	virtual Padding getViewportInset() const { return Padding(); }

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

	// Replace `replace` with `text` as a request. Writes locally when no handler is active, as
	// setText() does. Length and character filtering are left to validateInput() on the echo
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

	// Only scheduled while a history is enabled, to feed it the frame time as its clock.
	virtual void update(const UpdateTime &) override;

	virtual bool handleKey(const GestureData &);
	virtual bool handleTextHotkey(HotkeyId, const InputEvent &);
	virtual bool handleTap(const GestureTap &);
	virtual bool handlePress(const GesturePress &, bool begin);

	// A press held past the long-press interval, once per interval. The first one selects the word
	// under the finger, the second the whole text.
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

	// Where the widget last asked the cursor to be. Successive local edits chain onto this rather
	// than _inputState.cursor, since two keystrokes can arrive before the first echo. Overwritten
	// by every echo, so a platform-side rewrite wins.
	TextCursor pendingCursor() const;

	// End of `cursor` the user is moving: the one opposite _selectionAnchor. The container scrolls
	// to it, so widening a selection to the right shows its right edge and not its left one.
	// maxOf<uint32_t>() when no selection is being extended.
	uint32_t activeCursorPosition(TextCursor cursor) const;

	/* Hooks TextView overrides so copy/cut/paste/drop are shared. Two cursor hooks: cut() removes
	the echoed selection, while paste lands at the requested, not yet echoed caret (see
	pendingCursor). */

	// What copy() copies and cut() removes.
	virtual TextCursor selectionCursor() const { return _inputState.cursor; }

	// Where a paste or a drop lands.
	virtual TextCursor insertionCursor() const { return pendingCursor(); }

	// The text under `cursor`. A view into live storage, valid until the next edit.
	virtual WideStringView getTextForCursor(TextCursor) const;

	// Whether the selection may leave the widget: false for a masked field. TextView never masks.
	virtual bool canCopySelection() const {
		return _passwordMode == TextInputPasswordMode::NotPassword;
	}

	// Built on first use, because most fields never touch the clipboard at all.
	ClipboardSession *acquireClipboard();

	/* Record one edit against the history before the text changes. Called from the mutation point
	(the echo here, applyDocEdit in TextView); `cursorBefore` is the caret an undo restores. */
	void recordHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted,
			TextCursor cursorBefore);

	/* Scoped name for the edit in flight. Defaults to typing (plain echoes); paste, cut and similar
	operations set it around their call so they form their own entry. */
	struct HistoryEditName {
		HistoryEditName(TextInput *input, StringView name)
		: _input(input), _previous(input->_historyEditName) {
			_input->_historyEditName = name;
		}
		~HistoryEditName() { _input->_historyEditName = _previous; }

		TextInput *_input;
		StringView _previous;
	};

	TextInputContainer *_container = nullptr;
	InputListener *_listener = nullptr;
	InputListener *_focusListener = nullptr;

	TextInputHandler _handler;
	TextInputState _inputState;

	TextHistory _history;
	StringView _historyEditName = TextHistory::NameTyping;

	// The last frame time seen, in the UpdateTime `global` domain. `app` is not used: AppThread
	// computes it as (start - now), so it runs backwards.
	uint64_t _historyClock = 0;

	/* Undo in an IME-owned field is a request: the caret is applied only after the echo (pushing it
	earlier would carry the old string), and that echo is not recorded as a new edit.
	`_historyEchoes` counts history-driven edits in flight; `_historyPendingCursor` waits for the
	echo. Unused by TextView. */
	uint32_t _historyEchoes = 0;
	TextCursor _historyPendingCursor = TextCursor::InvalidCursor;

	// The text an undo builds edit by edit, so a multi-edit entry becomes one platform request.
	WideString _historyShadow;
	bool _historyBatch = false;

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

	// The clipboard transport, built on first use. Carries the staleness serial; cancelled by
	// blur() and by a platform focus loss.
	Rc<ClipboardSession> _clipboard;

	// see pendingCursor()
	TextCursor _pendingCursor = TextCursor::InvalidCursor;

	bool _focused = false;
	bool _dragSelecting = false;
	bool _panning = false;

	// Suppresses the tap from a long press's release, which would drop its selection. Cleared
	// when the next press starts.
	bool _longPressApplied = false;

	// edge trackers for InteractiveComponent's cumulative counters
	bool _hoverApplied = false;
	bool _focusApplied = false;
	bool _activeApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUITEXTINPUT_H_
