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

#include "XLRemoteBearerKeys.h"
#include "SPCoreCrypto.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

void BearerKeyTable::add(BytesView key, StringView label, bool singleUse) {
	remove(label);
	_entries.emplace_back(Entry{key.bytes<Interface>(), label.str<Interface>(), singleUse});
}

bool BearerKeyTable::remove(StringView label) {
	for (auto it = _entries.begin(); it != _entries.end(); ++it) {
		if (it->label == label) {
			_entries.erase(it);
			return true;
		}
	}
	return false;
}

bool BearerKeyTable::match(BytesView presented, String &outLabel) const {
	const Entry *found = nullptr;
	if (presented.empty()) {
		return false;
	}
	for (auto &it : _entries) {
		auto equal = crypto::isEqualConstantTime(presented, it.key);
		if (equal && !found) {
			found = &it;
		}
	}
	if (found) {
		outLabel = found->label;
		return true;
	}
	return false;
}

void BearerKeyTable::consume(StringView label) {
	for (auto it = _entries.begin(); it != _entries.end(); ++it) {
		if (it->label == label) {
			if (it->singleUse) {
				_entries.erase(it);
			}
			return;
		}
	}
}

} // namespace stappler::xenolith::remote
