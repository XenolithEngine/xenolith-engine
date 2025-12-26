/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLCoreTextureSet.h"
#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

void TextureSet::write(const MaterialLayout &set) {
	_layoutIndexes.clear();
	for (uint32_t i = 0; i < set.usedImageSlots; ++i) {
		if (set.imageSlots[i].image) {
			_layoutIndexes.emplace_back(set.imageSlots[i].image->getIndex());
		} else {
			_layoutIndexes.emplace_back(0);
		}
	}

	_layoutIndexes.resize(_count, 0);
}

Rc<TextureSet> TextureSetLayout::acquireSet(Device &dev) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_sets.empty()) {
		lock.unlock();
		return dev.makeTextureSet(*this);
	} else {
		auto v = move(_sets.back());
		_sets.pop_back();
		return v;
	}
}

void TextureSetLayout::releaseSet(Rc<TextureSet> &&set) {
	std::unique_lock<Mutex> lock(_mutex);
	_sets.emplace_back(move(set));
}

} // namespace stappler::xenolith::core
