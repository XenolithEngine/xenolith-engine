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

#include "XLDragTypes.h"
#include "XLAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// App thread only, so the counters need no atomics
struct DragData::External : public Ref {
	Rc<sprt::window::DropOffer> offer;
	Rc<AppThread> app;
	uint32_t pending = 0;
	DragActions performed = DragActions::None;
	bool settled = false;
	bool finished = false;

	void tryFinish() {
		if (settled && pending == 0 && !finished) {
			finished = true;
			offer->finish(performed);
		}
	}
};

bool DragData::init(Rc<sprt::window::ClipboardData> &&data, Rc<Ref> &&local, StringView localType) {
	_clipboard = sp::move(data);
	_local = sp::move(local);
	_localType = localType.str<Interface>();
	return true;
}

bool DragData::init(NotNull<sprt::window::DropOffer> offer, NotNull<AppThread> app) {
	_clipboard = Rc<sprt::window::ClipboardData>::create();
	for (auto &it : offer->getTypes()) { _clipboard->types.emplace_back(it); }

	auto ext = Rc<External>::alloc();
	ext->offer = offer.get();
	ext->app = app.get();
	_external = sp::move(ext);
	return true;
}

DragData::External *DragData::getExternal() const {
	return static_cast<External *>(_external.get());
}

SpanView<sprt::window::String> DragData::getTypes() const {
	if (!_clipboard) {
		return SpanView<sprt::window::String>();
	}
	return SpanView<sprt::window::String>(_clipboard->types.data(), _clipboard->types.size());
}

bool DragData::hasType(StringView type) const {
	for (auto &it : getTypes()) {
		if (StringView(it) == type) {
			return true;
		}
	}
	return false;
}

StringView DragData::preferType(SpanView<StringView> preference) const {
	auto types = getTypes();

	Vector<StringView> views;
	views.reserve(types.size());
	for (auto &it : types) { views.emplace_back(StringView(it)); }

	return preferMimeType(views, preference);
}

sprt::window::Bytes DragData::encode(StringView type) const {
	if (!_clipboard || !_clipboard->encodeCallback || !hasType(type)) {
		return sprt::window::Bytes();
	}
	return _clipboard->encodeCallback(type);
}

bool DragData::read(StringView type, ReadCallback &&cb, Ref *target) {
	if (!cb || !hasType(type)) {
		return false;
	}

	auto ext = getExternal();
	if (!ext) {
		auto bytes = encode(type);
		cb(bytes.empty() ? Status::Declined : Status::Ok, bytes);
		return true;
	}

	if (ext->finished) {
		return false;
	}

	++ext->pending;

	Rc<External> state = ext;
	Rc<Ref> keep = target;
	ext->offer->read(type, [state, keep, cb = sp::move(cb)](Status st, BytesView data) mutable {
		// Copied here: the view is borrowed for this call only, and the answer moves threads
		state->app->performOnAppThread(
				[state, keep, cb = sp::move(cb), st, bytes = data.bytes<Interface>()]() mutable {
			cb(st, bytes);
			--state->pending;
			state->tryFinish();
		}, state);
	});
	return true;
}

void DragData::settle(DragActions performed) {
	if (auto ext = getExternal()) {
		if (!ext->settled) {
			ext->settled = true;
			ext->performed = performed;
			ext->tryFinish();
		}
	}
}

Rc<sprt::window::ClipboardData> DragOffer::takeClipboardData(Ref *owner) {
	auto data = Rc<sprt::window::ClipboardData>::create();
	data->label = StringView(label).str<sprt::window::String>();
	for (auto &it : types) { data->types.emplace_back(StringView(it).str<sprt::window::String>()); }
	if (encode) {
		data->encodeCallback = sp::move(encode);
	}
	data->owner = owner;
	return data;
}

DragActions modifiersToActions(InputModifier mods, DragActions allowed, DragActions dflt) {
	if (allowed == DragActions::None) {
		return DragActions::None;
	}

	const bool ctrl = hasFlag(mods, InputModifier::Ctrl);
	const bool shift = hasFlag(mods, InputModifier::Shift);

	DragActions want = dflt;
	if (ctrl && shift) {
		want = DragActions::Link;
	} else if (ctrl) {
		want = DragActions::Copy;
	} else if (shift) {
		want = DragActions::Move;
	}

	if ((want & allowed) != DragActions::None) {
		return want & allowed;
	}

	// The requested action is not offered; fall back to what is, so a stray Ctrl does not break it
	for (auto it : {DragActions::Copy, DragActions::Move, DragActions::Link}) {
		if (hasFlag(allowed, it)) {
			return it;
		}
	}
	return DragActions::None;
}

DragActions pickAction(DragActions mask) {
	for (auto it : {DragActions::Copy, DragActions::Move, DragActions::Link}) {
		if (hasFlag(mask, it)) {
			return it;
		}
	}
	return DragActions::None;
}

WindowCursor actionToCursor(DragActions action) {
	switch (action) {
	case DragActions::Copy: return WindowCursor::Copy;
	case DragActions::Move: return WindowCursor::Move;
	case DragActions::Link: return WindowCursor::Alias;
	default: break;
	}
	return WindowCursor::NoDrop;
}

} // namespace stappler::xenolith
