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

#ifndef XENOLITH_CORE_XLCORETEXTURESET_H_
#define XENOLITH_CORE_XLCORETEXTURESET_H_

#include "XLCoreInfo.h"
#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class TextureSet;
class Loop;

class SP_PUBLIC TextureSet : public Object {
public:
	virtual ~TextureSet() = default;

	virtual void write(const MaterialLayout &);

	uint32_t size() const { return _count; }
	SpanView<uint64_t> getIndexes() const { return _layoutIndexes; }

protected:
	uint32_t _count = 0;
	Vector<uint64_t> _layoutIndexes;
};

// persistent object, part of Device
class SP_PUBLIC TextureSetLayout : public Object {
public:
	using AttachmentLayout = core::AttachmentLayout;

	virtual ~TextureSetLayout() = default;

	const uint32_t &getImageCount() const { return _imageCount; }
	const uint32_t &getSamplersCount() const { return _samplersCount; }

	const ImageData *getEmptyImage() const { return _emptyImage; }
	const ImageData *getSolidImage() const { return _solidImage; }

	Rc<TextureSet> acquireSet(Device &dev);
	void releaseSet(Rc<TextureSet> &&);

	bool isPartiallyBound() const { return _partiallyBound; }

protected:
	bool _partiallyBound = false;
	uint32_t _imageCount = 0;
	uint32_t _samplersCount = 0;

	mutable Mutex _mutex;
	Vector<Rc<Sampler>> _samplers;
	Vector<Rc<TextureSet>> _sets;

	const ImageData *_emptyImage = nullptr;
	const ImageData *_solidImage = nullptr;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORETEXTURESET_H_ */
