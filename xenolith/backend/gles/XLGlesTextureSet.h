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

#ifndef XENOLITH_BACKEND_GLES_XLGLESTEXTURESET_H_
#define XENOLITH_BACKEND_GLES_XLGLESTEXTURESET_H_

#include "XLGlesDevice.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// M1 stores, it does not bind: the samplers are compiled here and the slot views kept on the set,
// but nothing is uploaded to a unit yet - that happens with the draw path in M2. The shape already
// matches what a real binding will need (samplers by layout index, views by image slot), so the
// renderer side can be written against it now.
class SP_PUBLIC TextureSetLayout final : public core::TextureSetLayout {
public:
	virtual ~TextureSetLayout() = default;

	bool init(Device &, const core::TextureSetLayoutData &);

	SpanView<Rc<core::Sampler>> getCompiledSamplers() const { return _compiledSamplers; }

protected:
	using core::Object::init;

	Vector<Rc<core::Sampler>> _compiledSamplers;
};

class SP_PUBLIC TextureSet final : public core::TextureSet {
public:
	virtual ~TextureSet() = default;

	bool init(Device &, const TextureSetLayout &);

	virtual void write(const core::MaterialLayout &) override;

	// View bound to an image slot, or null when the slot is unused. The M2 draw path resolves a
	// material's samplerImageIdx through this and binds texture+sampler for one unit at a time.
	core::ImageView *getSlotView(uint32_t slot) const {
		return slot < _slotViews.size() ? _slotViews[slot].get() : nullptr;
	}

	SpanView<Rc<core::ImageView>> getSlotViews() const { return _slotViews; }

protected:
	using core::Object::init;

	const TextureSetLayout *_setLayout = nullptr;
	Vector<Rc<core::ImageView>> _slotViews; // by image slot, may hold nulls
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESTEXTURESET_H_ */
