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

#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId::ComponentId() : value(Component::GetNextId()) { }

uint32_t Component::GetNextId() {
	static std::atomic<uint32_t> s_counter = 1;
	return s_counter.fetch_add(1);
}

void Component::clear() const {
	if (destructor) {
		if (soo) {
			destructor(staticStorage.bytes);
		} else if (dynamicStorage.data) {
			destructor(dynamicStorage.data);
		}
	}

	soo = 0;
	destructor = nullptr;
	dynamicStorage.size = 0;
	dynamicStorage.data = nullptr;
}

void ComponentContainer::removeAllComponents() {
	_components.clear();
	_componentsDirty = true;
}

} // namespace stappler::xenolith
