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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TEXT_INPUT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TEXT_INPUT_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/detail/value_wrapper.h>
#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/input.h>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/stream.h>

namespace sprt::window {

struct TextInputState;
struct TextInputRequest;

enum class TextInputFlags : uint32_t {
	None,
	RunIfDisabled = 1 << 0,
};

SPRT_DEFINE_ENUM_AS_MASK(TextInputFlags)

enum class TextInputType : uint32_t {
	Empty = 0,
	Date_Date = 1,
	Date_DateTime = 2,
	Date_Time = 3,
	Date = Date_DateTime,

	Number_Numbers = 4,
	Number_Decimial = 5,
	Number_Signed = 6,
	Number = Number_Numbers,

	Phone = 7,

	Text_Text = 8,
	Text_Search = 9,
	Text_Punctuation = 10,
	Text_Email = 11,
	Text_Url = 12,
	Text = Text_Text,

	Default = Text_Text,

	ClassMask = 0b0001'1111,
	PasswordBit = 0b0010'0000,
	MultiLineBit = 0b0100'0000,
	AutoCorrectionBit = 0b1000'0000,

	ReturnKeyMask = 0b0000'1111 << 8,

	ReturnKeyDefault = 1 << 8,
	ReturnKeyGo = 2 << 8,
	ReturnKeyGoogle = 3 << 8,
	ReturnKeyJoin = 4 << 8,
	ReturnKeyNext = 5 << 8,
	ReturnKeyRoute = 6 << 8,
	ReturnKeySearch = 7 << 8,
	ReturnKeySend = 8 << 8,
	ReturnKeyYahoo = 9 << 8,
	ReturnKeyDone = 10 << 8,
	ReturnKeyEmergencyCall = 11 << 8,
};

SPRT_DEFINE_ENUM_AS_MASK(TextInputType);

using TextCursorPosition = ValueWrapper<uint32_t, class TextCursorPositionFlag>;
using TextCursorLength = ValueWrapper<uint32_t, class TextCursorStartFlag>;

struct SPRT_API TextCursor {
	static const TextCursor InvalidCursor;

	uint32_t start;
	uint32_t length;

	// Inclusive length between two positions, saturating at Max<uint32_t> so the
	// `+ 1` cannot overflow to 0 (e.g. first=0, last=Max — the InvalidCursor span).
	static constexpr uint32_t inclusiveLength(uint32_t a, uint32_t b) {
		uint32_t d = (a > b) ? (a - b) : (b - a);
		return d == Max<uint32_t> ? d : d + 1;
	}

	constexpr TextCursor() : start(Max<uint32_t>), length(0) { }
	constexpr TextCursor(uint32_t pos) : start(pos), length(0) { }
	constexpr TextCursor(uint32_t st, uint32_t len) : start(st), length(len) { }
	constexpr TextCursor(TextCursorPosition pos) : start(pos.get()), length(0) { }
	constexpr TextCursor(TextCursorPosition pos, TextCursorLength len)
	: start(pos.get()), length(len.get()) { }
	constexpr TextCursor(TextCursorPosition first, TextCursorPosition last)
	: start(sprt::min(first.get(), last.get()))
	, length(inclusiveLength(first.get(), last.get())) { }

	constexpr bool operator==(const TextCursor &) const = default;
};

constexpr const TextCursor TextCursor::InvalidCursor(Max<uint32_t>, 0);

struct SPRT_API TextInputString : public Ref {
	virtual ~TextInputString() = default;

	template <typename... Args>
	static Rc<TextInputString> create(Args &&...args) {
		auto ret = Rc<TextInputString>::alloc();
		ret->string =
				StreamTraits<char16_t>::toString<__malloc_u16string>(sprt::forward<Args>(args)...);
		return ret;
	}

	size_t size() const { return string.size(); }

	WideString string;
};

struct SPRT_API TextInputState {
	bool empty() const { return !string || string->string.empty(); }
	size_t size() const { return string ? string->string.size() : 0; }

	WideStringView getStringView() const {
		return string ? WideStringView(string->string) : WideStringView();
	}

	Rc<TextInputString> string;
	TextCursor cursor;
	TextCursor marked;

	bool enabled = false;
	TextInputType type = TextInputType::Empty;
	InputKeyComposeState compose = InputKeyComposeState::Nothing;

	TextInputRequest getRequest() const;
};

struct SPRT_API TextInputRequest {
	bool empty() const { return !string || string->string.empty(); }
	size_t size() const { return string ? string->string.size() : 0; }

	Rc<TextInputString> string;
	TextCursor cursor;
	TextCursor marked;
	TextInputType type = TextInputType::Empty;

	TextInputState getState() const;
};

enum class TextInputCommandOp : uint32_t {
	Insert,
	SetMarked,
	Unmark,
	DeleteBackward,
	DeleteForward,
	Cancel,
};

// One editing operation addressed to a window's TextInputProcessor, as an IME would issue it.
//
// This exists because a key event cannot express composition: SetMarked/Unmark are what an IME
// does while a CJK syllable or a dead-key sequence is being assembled, and there is no keystroke
// that means "the marked range is now these three characters". Delivered through
// NativeWindow::performTextInput.
struct SPRT_API TextInputCommand {
	TextInputCommandOp op = TextInputCommandOp::Insert;
	WideString text;
	TextCursor replacement = TextCursor::InvalidCursor;
	TextCursor marked = TextCursor::InvalidCursor;
	InputKeyComposeState compose = InputKeyComposeState::Nothing;
};

struct SPRT_API TextInputInfo {
	Function<bool(const TextInputRequest &)> update;
	Function<void(const TextInputState &)> propagate;
	Function<void()> cancel;
};

// The editing engine behind a window's text input.
//
// Ownership rule: the state belongs to the IME on the OS side, never to the application. The
// application only ever *requests* a state through run()/update; what it gets back through
// TextInputInfo::propagate is the answer. That is what makes system autocorrection, CJK
// composition and platform paste work - all of them rewrite the text without the application
// asking. A window backend that has no separate IME (X11, Wayland, Win32, headless) plays the
// IME role itself: this processor does the editing on its behalf, and the backend reports
// enablement through handleInputEnabled().
class SPRT_API TextInputProcessor : public Ref {
public:
	virtual ~TextInputProcessor();

	bool init(TextInputInfo &&);

	void insertText(WideStringView sInsert, InputKeyComposeState);
	void insertText(WideStringView sInsert, TextCursor replacement);
	void setMarkedText(WideStringView sInsert, TextCursor replacement, TextCursor marked);
	void deleteBackward();
	void deleteForward();
	void unmarkText();

	bool hasText();
	void textChanged(TextInputString *, TextCursor, TextCursor);
	void cursorChanged(TextCursor);
	void markedChanged(TextCursor);

	// Called by the window backend (the IME) to report that it has taken or released text input.
	// run() never sets this flag: enablement is the IME's answer, not the application's request.
	// Until it is reported, isRunning() stays false, keyboard interception does not happen and the
	// application-side manager tears its handler down on the first propagate.
	void handleInputEnabled(bool enabled);
	void handleTextChanged(TextInputState &&);

	// run input capture (or update it with new params)
	// propagates all data to device input manager, enables screen keyboard if needed
	//
	// This is a *request*: it hands the desired string/cursor/type to the backend through
	// TextInputInfo::update and rolls the state back if the backend declines. Whether input is
	// actually enabled is reported separately, by the backend, through handleInputEnabled().
	void run(const TextInputRequest &);

	// disable text input, disables keyboard connection and keyboard input event interception
	// default manager automatically disabled when app goes background
	void cancel();

	bool isRunning() const { return _state.enabled; }

	// The state as it stands. Read-only by design - the same rule as everywhere else here: the
	// state is the IME's, and everyone else only requests changes. A backend that IS the IME
	// (macOS answers NSTextInputClient queries out of this) needs to read it to reply.
	const TextInputState &getState() const { return _state; }

	bool canHandleInputEvent(const InputEventData &);
	bool handleInputEvent(const InputEventData &);

	/* A predicate that marks a key event as RESERVED for something other than text — a global
	   hotkey, in practice. canHandleInputEvent consults it and declines whatever it claims, which
	   is the only way such a combination can reach the scene at all: everything this processor
	   claims is rewritten to KeyCanceled before the application ever sees it.

	   Without it only the combinations hard-coded above survive a focused text field: Ctrl
	   without Alt, Tab, and Enter outside a multi-line field. Anything else that carries a
	   `keychar` — Alt+F, Super+P — is typed instead of dispatched.

	   Process-wide on purpose: the reservation is an application-level policy, not a per-window
	   one, and the filter runs on whatever thread delivers input. It is therefore a plain
	   function pointer published atomically, not a closure: nothing to own, nothing to keep
	   alive, and no lock on the hot path. Set it once during startup; null clears it. */
	using ReservedKeyFilter = bool (*)(const InputEventData &);

	static void setReservedKeyFilter(ReservedKeyFilter);
	static ReservedKeyFilter getReservedKeyFilter();

protected:
	bool doInsertText(TextInputState &, WideStringView, InputKeyComposeState);

	TextInputInfo _info;
	TextInputState _state;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TEXT_INPUT_H_
