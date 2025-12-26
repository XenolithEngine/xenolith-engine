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

#include "XLTextInputManager.h"
#include "XLCoreTextInput.h"
#include "XLDirector.h"
#include "XLAppWindow.h"

#if WIN32
#ifdef DELETE
#undef DELETE
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

TextInputHandler::~TextInputHandler() { cancel(); }

bool TextInputHandler::run(TextInputManager *manager, TextInputRequest &&req) {
	if (!isActive()) {
		this->manager = manager;
		return manager->run(this, move(req));
	}
	return false;
}

void TextInputHandler::cancel() {
	if (isActive()) {
		this->manager->cancel();
		this->manager = nullptr;
	}
}

// only if this handler is active
bool TextInputHandler::update(TextInputRequest &&req) {
	if (isActive()) {
		this->manager->update(this, move(req));
		return true;
	}
	return false;
}

WideStringView TextInputHandler::getString() const { return this->manager->getString(); }
TextCursor TextInputHandler::getCursor() const { return this->manager->getCursor(); }
TextCursor TextInputHandler::getMarked() const { return this->manager->getMarked(); }

bool TextInputHandler::isActive() const {
	return this->manager && this->manager->getHandler() == this;
}

bool TextInputManager::init(NotNull<Director> d) {
	_director = d;
	return true;
}

bool TextInputManager::run(TextInputHandler *h, TextInputRequest &&req) {
	auto oldH = _handler;
	_handler = h;

	if (oldH) {
		auto copy = _state;
		copy.enabled = false;
		oldH->onData(copy);
	}

	_handler = h;

	if (req.cursor.start > req.size()) {
		req.cursor.start = static_cast<uint32_t>(req.size());
	}

	_state = req.getState();

	_director->getWindow()->acquireTextInput(move(req));
	return true;
}

bool TextInputManager::update(TextInputHandler *h, TextInputRequest &&req) {
	if (_handler != h) {
		return false;
	}

	if (req.cursor.start > req.size()) {
		req.cursor.start = static_cast<uint32_t>(req.size());
	}

	auto newState = req.getState();

	if (_state.enabled) {
		newState.compose = _state.compose;
		newState.enabled = _state.enabled;
	}

	_state = move(newState);
	_director->getWindow()->acquireTextInput(move(req));
	return true;
}

WideStringView TextInputManager::getString() const { return _state.getStringView(); }

WideStringView TextInputManager::getStringByRange(TextCursor cursor) {
	WideStringView str = _state.getStringView();
	if (cursor.start >= str.size()) {
		return WideStringView();
	}

	str += cursor.start;
	if (cursor.length >= str.size()) {
		return str;
	}

	return str.sub(0, cursor.length);
}

TextCursor TextInputManager::getCursor() const { return _state.cursor; }

TextCursor TextInputManager::getMarked() const { return _state.marked; }

void TextInputManager::cancel() {
	if (_handler) {
		auto copy = _state;
		copy.enabled = false;
		_handler->onData(copy);
		_handler->manager = nullptr;
	}
	if (auto w = _director->getWindow()) {
		w->releaseTextInput();
	}
	_handler = nullptr;
	_state.enabled = false;
	_state.string = nullptr;
}

bool TextInputManager::isEnabled() const { return _state.enabled; }

void TextInputManager::handleInputUpdate(const TextInputState &state) {
	_state = state;

	if (!_state.enabled) {
		cancel();
	} else if (_handler) {
		_handler->onData(_state);
	}
}

} // namespace stappler::xenolith
