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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DragData::init(Rc<sprt::window::ClipboardData> &&data, Rc<Ref> &&local, StringView localType) {
	_clipboard = sp::move(data);
	_local = sp::move(local);
	_localType = localType.str<Interface>();
	return true;
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

	// The modifier asked for something this source does not offer. Refusing outright would make
	// a stray Ctrl silently break a drag, so fall back to whatever IS offered
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
