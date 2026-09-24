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

#include <sprt/runtime/window/drop.h>
#include <sprt/runtime/window/context.h>

namespace sprt::window {

DropOffer::~DropOffer() {
	if (_timeout) {
		_timeout->cancel();
		_timeout = nullptr;
	}
}

bool DropOffer::init(NotNull<dispatch::Looper> looper, Vector<String> &&types,
		DragActions allowed) {
	_looper = looper;
	_types = sprt::move(types);
	_allowed = allowed;
	return true;
}

bool DropOffer::hasType(StringView type) const {
	for (auto &it : _types) {
		if (StringView(it) == type) {
			return true;
		}
	}
	return false;
}

void DropOffer::read(StringView type, ReadCallback &&cb) {
	if (!cb) {
		return;
	}

	_looper->performOnThread([this, type = type.str<String>(), cb = sprt::move(cb)]() mutable {
		if (_finished) {
			cb(Status::ErrorCancelled, BytesView());
			return;
		}
		if (!hasType(type)) {
			cb(Status::ErrorInvalidArguemnt, BytesView());
			return;
		}
		handleRead(type, sprt::move(cb));
	}, this, true);
}

void DropOffer::status(DragActions actions) {
	_looper->performOnThread([this, actions] {
		if (_finished || _dropped) {
			return;
		}
		_status = actions;
		handleStatus(actions);
	}, this, true);
}

void DropOffer::finish(DragActions performed) {
	_looper->performOnThread([this, performed] { performFinish(performed); }, this, true);
}

void DropOffer::refuse(DropPhase phase) {
	switch (phase) {
	case DropPhase::Enter:
	case DropPhase::Motion: status(DragActions::None); break;
	case DropPhase::Drop: finish(DragActions::None); break;
	case DropPhase::Leave: break;
	}
}

void DropOffer::setDropped() {
	if (_dropped) {
		return;
	}

	_dropped = true;
	_timeout = _looper->schedule(FinishTimeout, [this](dispatch::Handle *, bool success) {
		if (success) {
			performFinish(DragActions::None);
		}
	}, this);
}

void DropOffer::performFinish(DragActions performed) {
	if (!_dropped || _finished) {
		return;
	}

	_finished = true;
	if (_timeout) {
		_timeout->cancel();
		_timeout = nullptr;
	}
	handleFinish(performed);
}

String makeFileUriList(SpanView<StringView> paths) {
	static constexpr char Hex[] = "0123456789ABCDEF";

	String ret;
	for (auto &path : paths) {
		ret.append("file://");
		for (auto c : path) {
			auto b = uint8_t(c);
			// Unreserved characters, the separator, and the colon of a Windows drive
			if ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9')
					|| b == '/' || b == '-' || b == '.' || b == '_' || b == '~' || b == ':') {
				ret.push_back(c);
			} else {
				ret.push_back('%');
				ret.push_back(Hex[b >> 4]);
				ret.push_back(Hex[b & 0xF]);
			}
		}
		ret.append("\r\n");
	}
	return ret;
}

void Context::handleNativeWindowDrop(NotNull<NativeWindow>, DropEvent &&ev) {
	if (ev.offer) {
		ev.offer->refuse(ev.phase);
	}
}

bool MemoryDropOffer::init(NotNull<dispatch::Looper> looper, Vector<Representation> &&data,
		DragActions allowed) {
	Vector<String> types;
	for (auto &it : data) { types.emplace_back(it.type); }

	if (!DropOffer::init(looper, sprt::move(types), allowed)) {
		return false;
	}

	_data = sprt::move(data);
	return true;
}

void MemoryDropOffer::handleRead(StringView type, ReadCallback &&cb) {
	++_reads;
	for (auto &it : _data) {
		if (StringView(it.type) == type) {
			cb(Status::Ok, it.data);
			return;
		}
	}
	cb(Status::ErrorNotFound, BytesView());
}

void MemoryDropOffer::handleStatus(DragActions actions) { _lastStatus = toInt(actions); }

void MemoryDropOffer::handleFinish(DragActions performed) {
	_performed = toInt(performed);
	_finishedFlag = true;
}

} // namespace sprt::window
