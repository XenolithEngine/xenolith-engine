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

#include "XLCoreDescriptorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

DescriptorBinding::~DescriptorBinding() { data.clear(); }

DescriptorBinding::DescriptorBinding(DescriptorType type, DescriptorFlags flags, uint32_t count)
: type(type), flags(flags) {
	data.resize(count, DescriptorData{core::ObjectHandle::zero(), nullptr});
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferInfo &&info) {
	if (idx >= data.size()) {
		return nullptr;
	}

	auto &desc = data[idx];
	auto empty = desc.empty();
	auto ret = move(desc.data);
	desc = DescriptorData{info.buffer->getObjectData().handle, move(info.buffer)};
	if (empty && !desc.empty()) {
		++bound;
	} else if (!empty && desc.empty()) {
		--bound;
	}
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorImageInfo &&info) {
	if (idx >= data.size()) {
		return nullptr;
	}

	auto &desc = data[idx];
	auto empty = desc.empty();
	auto ret = move(desc.data);
	desc = DescriptorData{info.imageView->getObjectData().handle, move(info.imageView)};
	if (empty && !desc.empty()) {
		++bound;
	} else if (!empty && desc.empty()) {
		--bound;
	}
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferViewInfo &&info) {
	if (idx >= data.size()) {
		return nullptr;
	}

	auto &desc = data[idx];
	auto empty = desc.empty();
	auto ret = move(desc.data);
	desc = DescriptorData{info.buffer->getObjectData().handle, move(info.buffer)};
	if (empty && !desc.empty()) {
		++bound;
	} else if (!empty && desc.empty()) {
		--bound;
	}
	return ret;
}

const DescriptorData &DescriptorBinding::get(uint32_t idx) const { return data[idx]; }

uint32_t DescriptorBinding::size() const { return static_cast<uint32_t>(data.size()); }

} // namespace stappler::xenolith::core
