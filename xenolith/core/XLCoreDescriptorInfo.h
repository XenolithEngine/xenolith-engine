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

#ifndef XENOLITH_CORE_XLCOREDESCRIPTORINFO_H_
#define XENOLITH_CORE_XLCOREDESCRIPTORINFO_H_

#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct SP_PUBLIC DescriptorData {
	ObjectHandle object;
	Rc<Ref> data;

	bool empty() const { return object == ObjectHandle::zero(); }
};

struct SP_PUBLIC DescriptorInfo {
	DescriptorInfo(const PipelineDescriptor *desc, uint32_t index)
	: descriptor(desc), index(index) { }

	const PipelineDescriptor *descriptor;
	uint32_t index;
};

struct SP_PUBLIC DescriptorImageInfo : public DescriptorInfo {
	~DescriptorImageInfo() = default;

	DescriptorImageInfo(const PipelineDescriptor *desc, uint32_t index, ImageView *v = nullptr,
			Sampler *s = nullptr)
	: DescriptorInfo(desc, index), imageView(v), sampler(s) { }

	Rc<ImageView> imageView;
	Rc<Sampler> sampler;
	AttachmentLayout layout = AttachmentLayout::Undefined;
};

struct SP_PUBLIC DescriptorBufferInfo : public DescriptorInfo {
	~DescriptorBufferInfo() = default;

	DescriptorBufferInfo(const PipelineDescriptor *desc, uint32_t index, BufferObject *b = nullptr,
			uint64_t off = 0, uint64_t r = maxOf<uint64_t>())
	: DescriptorInfo(desc, index), buffer(b), offset(off), range(r) { }

	Rc<BufferObject> buffer;
	uint64_t offset = 0;
	uint64_t range = maxOf<uint64_t>();
};

struct SP_PUBLIC DescriptorBufferViewInfo : public DescriptorInfo {
	~DescriptorBufferViewInfo() = default;

	DescriptorBufferViewInfo(const PipelineDescriptor *desc, uint32_t index)
	: DescriptorInfo(desc, index) { }

	Rc<BufferObject> buffer;
	ObjectHandle target = ObjectHandle(0);
};

struct SP_PUBLIC DescriptorBinding {
	DescriptorType type;
	DescriptorFlags flags;
	uint32_t bound = 0;
	Vector<DescriptorData> data;

	~DescriptorBinding();

	DescriptorBinding(DescriptorType, DescriptorFlags, uint32_t count);

	// returns previously associated data
	Rc<Ref> write(uint32_t, DescriptorBufferInfo &&);
	Rc<Ref> write(uint32_t, DescriptorImageInfo &&);
	Rc<Ref> write(uint32_t, DescriptorBufferViewInfo &&);

	const DescriptorData &get(uint32_t) const;

	uint32_t size() const;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREDESCRIPTORINFO_H_ */
