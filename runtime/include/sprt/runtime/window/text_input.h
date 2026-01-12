/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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
#include <sprt/runtime/mem/function.h>
#include <sprt/runtime/detail/value_wrapper.h>
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

	constexpr TextCursor() : start(Max<uint32_t>), length(0) { }
	constexpr TextCursor(uint32_t pos) : start(pos), length(0) { }
	constexpr TextCursor(uint32_t st, uint32_t len) : start(st), length(len) { }
	constexpr TextCursor(TextCursorPosition pos) : start(pos.get()), length(0) { }
	constexpr TextCursor(TextCursorPosition pos, TextCursorLength len)
	: start(pos.get()), length(len.get()) { }
	constexpr TextCursor(TextCursorPosition first, TextCursorPosition last)
	: start(sprt::min(first.get(), last.get()))
	, length(((first > last) ? (first - last).get() : (last - first).get()) + 1) { }

	constexpr bool operator==(const TextCursor &) const = default;
};

constexpr const TextCursor TextCursor::InvalidCursor(Max<uint32_t>, 0);

struct TextInputString : public Ref {
	virtual ~TextInputString() = default;

	template <typename... Args>
	static Rc<TextInputString> create(Args &&...args) {
		auto ret = Rc<TextInputString>::alloc();
		ret->string = StreamTraits<char16_t>::toString(sprt::forward<Args>(args)...);
		return ret;
	}

	size_t size() const { return string.size(); }

	memory::dynu16string string;
};

struct TextInputState {
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

struct TextInputRequest {
	bool empty() const { return !string || string->string.empty(); }
	size_t size() const { return string ? string->string.size() : 0; }

	Rc<TextInputString> string;
	TextCursor cursor;
	TextCursor marked;
	TextInputType type = TextInputType::Empty;

	TextInputState getState() const;
};

struct TextInputInfo {
	memory::dynfunction<bool(const TextInputRequest &)> update;
	memory::dynfunction<void(const TextInputState &)> propagate;
	memory::dynfunction<void()> cancel;
};

class TextInputProcessor : public Ref {
public:
	virtual ~TextInputProcessor() = default;

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

	void handleInputEnabled(bool enabled);
	void handleTextChanged(TextInputState &&);

	// run input capture (or update it with new params)
	// propagates all data to device input manager, enables screen keyboard if needed
	void run(const TextInputRequest &);

	// disable text input, disables keyboard connection and keyboard input event interception
	// default manager automatically disabled when app goes background
	void cancel();

	bool isRunning() const { return _state.enabled; }

	bool canHandleInputEvent(const InputEventData &);
	bool handleInputEvent(const InputEventData &);

protected:
	bool doInsertText(TextInputState &, WideStringView, InputKeyComposeState);

	TextInputInfo _info;
	TextInputState _state;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_TEXT_INPUT_H_
