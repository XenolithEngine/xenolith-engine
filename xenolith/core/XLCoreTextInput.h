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

#ifndef XENOLITH_CORE_XLCORETEXTINPUT_H_
#define XENOLITH_CORE_XLCORETEXTINPUT_H_

#include "SPString.h"
#include "XLCoreInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct TextInputState;
struct TextInputRequest;

enum class TextInputFlags : uint32_t {
	None,
	RunIfDisabled = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(TextInputFlags)

struct TextInputString : public Ref {
	virtual ~TextInputString() = default;

	template <typename... Args>
	static Rc<TextInputString> create(Args &&...args) {
		auto ret = Rc<TextInputString>::alloc();
		ret->string = string::toWideString<memory::StandartInterface>(std::forward<Args>(args)...);
		return ret;
	}

	size_t size() const { return string.size(); }

	WideString string;
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
	Function<bool(const TextInputRequest &)> update;
	Function<void(const TextInputState &)> propagate;
	Function<void()> cancel;
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

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORETEXTINPUT_H_ */
