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

#include "XLCoreTextInput.h"

#if WIN32
#ifdef DELETE
#undef DELETE
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

TextInputRequest TextInputState::getRequest() const {
	TextInputRequest req;
	req.type = type;
	req.cursor = cursor;
	req.marked = marked;
	req.string = string;
	return req;
}

TextInputState TextInputRequest::getState() const {
	TextInputState newData;
	newData.type = type;
	newData.cursor = cursor;
	newData.marked = marked;
	newData.string = string;
	return newData;
}

bool TextInputProcessor::init(TextInputInfo &&info) {
	_info = move(info);
	return true;
}

bool TextInputProcessor::hasText() { return !_state.empty(); }

void TextInputProcessor::insertText(WideStringView sInsert, InputKeyComposeState compose) {
	TextInputState newState = _state;
	if (doInsertText(newState, sInsert, compose)) {
		handleTextChanged(move(newState));
	}
}

void TextInputProcessor::insertText(WideStringView sInsert, TextCursor replacement) {
	TextInputState newState = _state;
	if (replacement.start != maxOf<uint32_t>()) {
		newState.cursor = replacement;
	}

	if (doInsertText(newState, sInsert, InputKeyComposeState::Composed)
			|| replacement.start != maxOf<uint32_t>()) {
		handleTextChanged(move(newState));
	}
}

void TextInputProcessor::setMarkedText(WideStringView sInsert, TextCursor replacement,
		TextCursor marked) {
	TextInputState newState = _state;
	if (replacement.start != maxOf<uint32_t>()) {
		newState.cursor = replacement;
	}

	auto start = newState.cursor.start;

	if (doInsertText(newState, sInsert, InputKeyComposeState::Composed)
			|| replacement.start != maxOf<uint32_t>()) {
		newState.marked = TextCursor(start + marked.start, marked.length);
		handleTextChanged(move(newState));
	}
}

void TextInputProcessor::textChanged(TextInputString *text, TextCursor cursor, TextCursor marked) {
	TextInputState newState = _state;
	if (text->size() == 0) {
		newState.string = text;
		newState.cursor.start = 0;
		newState.cursor.length = 0;
		newState.marked = TextCursor::InvalidCursor;
		handleTextChanged(move(newState));
		return;
	}

	newState.string = text;
	newState.cursor = cursor;
	newState.marked = marked;
	handleTextChanged(move(newState));
}

void TextInputProcessor::cursorChanged(TextCursor cursor) {
	TextInputState newState = _state;
	newState.cursor = cursor;
	handleTextChanged(move(newState));
}

void TextInputProcessor::markedChanged(TextCursor marked) {
	TextInputState newState = _state;
	newState.marked = marked;
	handleTextChanged(move(newState));
}

void TextInputProcessor::deleteBackward() {
	if (_state.empty()) {
		return;
	}

	if (_state.cursor.length > 0) {
		// Composing will also have cursor.length > 0
		TextInputState newState = _state;
		auto oldStr = newState.getStringView();

		newState.string = TextInputString::create(oldStr.sub(0, newState.cursor.start),
				oldStr.sub(newState.cursor.start + newState.cursor.length + 1));
		newState.cursor.length = 0;
		newState.compose = InputKeyComposeState::Nothing; // composing should be dropped on delete
		handleTextChanged(move(newState));
		return;
	}

	if (_state.cursor.start == 0) {
		// nothing to delete before cursor
		return;
	}

	TextInputState newState = _state;

	size_t nDeleteLen = 1;
	if (newState.cursor.start - 1 < newState.size()) {
		auto c = newState.string->string.at(newState.cursor.start - 1);
		if (sprt::unicode::isUtf16HighSurrogate(c)) {
			nDeleteLen = 2;
		} else if (sprt::unicode::isUtf16LowSurrogate(c) && newState.cursor.start > 0) {
			nDeleteLen = 2;
			newState.cursor.start -= 1;
		}
	}

	if (newState.size() <= nDeleteLen) {
		newState.string = nullptr;
		newState.cursor.start = 0;
		newState.cursor.length = 0;
		handleTextChanged(move(newState));
		return;
	}

	auto oldStr = newState.getStringView();
	newState.string = TextInputString::create(oldStr.sub(0, newState.cursor.start - 1),
			oldStr.sub(newState.cursor.start - 1 + nDeleteLen));
	newState.cursor.start -= nDeleteLen;
	handleTextChanged(move(newState));
}

void TextInputProcessor::deleteForward() {
	if (_state.empty()) {
		return;
	}

	if (_state.cursor.length > 0) {
		// Composing will also have cursor.length > 0
		TextInputState newState = _state;
		auto oldStr = newState.getStringView();

		newState.string = TextInputString::create(oldStr.sub(0, newState.cursor.start),
				oldStr.sub(newState.cursor.start + newState.cursor.length + 1));
		newState.cursor.length = 0;
		newState.compose = InputKeyComposeState::Nothing; // composing should be dropped on delete
		handleTextChanged(move(newState));
		return;
	}

	if (_state.cursor.start == _state.size()) {
		// nothing to delete after cursor
		return;
	}

	TextInputState newState = _state;

	size_t nDeleteLen = 1;
	if (newState.cursor.start < newState.size()) {
		auto c = newState.string->string.at(newState.cursor.start);
		if (sprt::unicode::isUtf16HighSurrogate(c)) {
			nDeleteLen = 2;
		} else if (sprt::unicode::isUtf16LowSurrogate(c) && newState.cursor.start > 0) {
			nDeleteLen = 2;
			newState.cursor.start -= 1;
		}
	}

	if (newState.size() <= nDeleteLen) {
		newState.string = nullptr;
		newState.cursor.start = 0;
		newState.cursor.length = 0;
		handleTextChanged(move(newState));
		return;
	}


	auto oldStr = newState.getStringView();
	newState.string = TextInputString::create(oldStr.sub(0, newState.cursor.start),
			oldStr.sub(newState.cursor.start + nDeleteLen));
	handleTextChanged(move(newState));
}

void TextInputProcessor::unmarkText() { markedChanged(TextCursor::InvalidCursor); }

void TextInputProcessor::handleInputEnabled(bool enabled) {
	if (_state.enabled != enabled) {
		TextInputState newState = _state;

		newState.enabled = enabled;
		newState.compose = InputKeyComposeState::Nothing;

		_state = newState;

		_info.propagate(newState);

		if (!_state.enabled) {
			cancel();
		}
	}
}

void TextInputProcessor::handleTextChanged(TextInputState &&state) {
	_state = move(state);
	_info.propagate(_state);
}

void TextInputProcessor::run(const TextInputRequest &req) {
	auto newState = req.getState();

	if (_state.enabled) {
		newState.enabled = _state.enabled;
		newState.compose = _state.compose;
	}

	auto tmpState = _state;

	_state = newState;

	if (!_info.update(req)) {
		_state = move(tmpState);
	} else {
		_info.propagate(_state);
	}
}

void TextInputProcessor::cancel() {
	if (_state.enabled) {
		_info.cancel();
		handleInputEnabled(false);
	}
}

bool TextInputProcessor::canHandleInputEvent(const InputEventData &data) {
	if (_state.enabled && data.key.compose != InputKeyComposeState::Disabled) {
		switch (data.event) {
		case InputEventName::KeyPressed:
		case InputEventName::KeyRepeated:
		case InputEventName::KeyReleased:
		case InputEventName::KeyCanceled:
			if (data.key.keychar || data.key.keycode == InputKeyCode::BACKSPACE
					|| data.key.keycode == InputKeyCode::DELETE
					|| data.key.keycode == InputKeyCode::ESCAPE) {
				return true;
			}
			break;
		default: break;
		}
	}
	return false;
}

bool TextInputProcessor::handleInputEvent(const InputEventData &data) {
	if (data.event == InputEventName::KeyReleased) {
		if (data.key.compose != InputKeyComposeState::Forced) {
			return false;
		}
	}

	switch (data.event) {
	case InputEventName::KeyPressed:
	case InputEventName::KeyRepeated:
	case InputEventName::KeyReleased:
		if (data.key.keycode == InputKeyCode::BACKSPACE || data.key.keychar == char32_t(0x0008)) {
			deleteBackward();
			return true;
		} else if (data.key.keycode == InputKeyCode::DELETE
				|| data.key.keychar == char32_t(0x007f)) {
			deleteForward();
			return true;
		} else if (data.key.keycode == InputKeyCode::ESCAPE) {
			cancel();
		} else if (data.key.keychar) {
			auto c = data.key.keychar;
			// replace \r with \n for formatter
			if (c == '\r') {
				c = '\n';
			}
			insertText(string::toUtf16<Interface>(c), data.key.compose);
			return true;
		}
		break;
	case InputEventName::KeyCanceled: break;
	default: break;
	}
	return false;
}

bool TextInputProcessor::doInsertText(TextInputState &data, WideStringView sInsert,
		InputKeyComposeState compose) {
	if (sInsert.size() == 0) {
		return false;
	}

	switch (compose) {
	case InputKeyComposeState::Nothing:
	case InputKeyComposeState::Forced:
		if (data.compose == InputKeyComposeState::Composing) {
			data.cursor.start += data.cursor.length;
			data.cursor.length = 0;
		}
		break;
	case InputKeyComposeState::Composed: break;
	case InputKeyComposeState::Composing: break;
	case InputKeyComposeState::Disabled: break;
	}

	// Check for a complete composition
	// When InputKeyComposeState::Composed, input should remove temporary composition chars,
	// that will be in cursor, then insert final composition result from sInsert in place of them
	if (data.cursor.length > 0
			&& (compose == InputKeyComposeState::Composed
					|| data.compose != InputKeyComposeState::Composing)) {
		// remove temporary composition and clear cursor
		auto oldStr = data.getStringView();

		data.string = TextInputString::create(oldStr.sub(0, data.cursor.start),
				oldStr.sub(data.cursor.start + data.cursor.length + 1));
		data.cursor.length = 0;
	}

	auto oldStr = data.getStringView();
	if (data.cursor.start < data.size()) {
		data.string = TextInputString::create(WideStringView(oldStr, 0, data.cursor.start), sInsert,
				WideStringView(oldStr, data.cursor.start));
	} else {
		data.string =
				TextInputString::create(WideStringView(oldStr, 0, data.cursor.start), sInsert);
	}

	if (compose == InputKeyComposeState::Composing) {
		// When we are in composition process - do not shift cursor,
		// but add inserted symbols into cursor
		data.cursor.length += sInsert.size();
	} else {
		data.cursor.start += sInsert.size();
	}

	data.compose = compose;
	return true;
}

} // namespace stappler::xenolith::core
