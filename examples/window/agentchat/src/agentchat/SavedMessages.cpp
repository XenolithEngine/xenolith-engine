/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "agentchat/SavedMessages.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

const SavedMessage &SavedMessages::add(StringView title, StringView text, StringView source) {
	auto &entry = _entries.emplace_back();
	entry.title = title.str<Interface>();
	entry.text = text.str<Interface>();
	entry.source = source.str<Interface>();

	notify();
	return _entries.back();
}

void SavedMessages::clear() {
	if (_entries.empty()) {
		return;
	}

	_entries.clear();
	notify();
}

void SavedMessages::notify() {
	if (_onChange) {
		_onChange();
	}
}

Value SavedMessages::encode() const {
	Value entries;
	for (auto &it : _entries) {
		Value entry;
		if (!it.title.empty()) {
			entry.setString(it.title, "title");
		}
		entry.setString(it.text, "text");
		entry.setString(it.source, "source");
		entries.addValue(sp::move(entry));
	}
	return entries;
}

} // namespace stappler::xenolith::examples
