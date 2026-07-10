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

#ifndef XENOLITH_BACKEND_MTL_XLMTLTEXTURESET_H_
#define XENOLITH_BACKEND_MTL_XLMTLTEXTURESET_H_

#include "XLMtlObject.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

/* Engine's texture set (material textures array) over a Metal 3 argument
 * buffer: a plain shared MTLBuffer of 8-byte GPU resource handles
 * (MTLResourceID), written directly via gpuResourceID - no argument encoder.
 *
 * The buffer layout mirrors the engine's texture set convention and MUST
 * match the MSL argument struct:
 *   struct TextureSetArgs {
 *       array<sampler, SAMPLERS> samplers;
 *       array<texture2d<float>, IMAGES> textures;
 *   };
 * bound as `constant TextureSetArgs & [[buffer(TextureSetBufferIndex)]]`.
 *
 * Raw handles bypass the driver's residency tracking, so the bound textures
 * are made resident with useResource at bind time
 * (CommandBuffer::cmdBindTextureSet); samplers need no residency */
class SP_PUBLIC TextureSetLayout final : public core::TextureSetLayout {
public:
	static uint32_t getLayoutImageCount(Device &, const core::TextureSetLayoutData &);

	virtual ~TextureSetLayout() = default;

	bool init(Device &, const core::TextureSetLayoutData &);

	Device *getDevice() const { return _device; }

	SpanView<Rc<core::Sampler>> getCompiledSamplers() const { return _compiledSamplers; }

protected:
	using core::Object::init;

	Device *_device = nullptr;
	Vector<Rc<core::Sampler>> _compiledSamplers;
};

class SP_PUBLIC TextureSet final : public core::TextureSet {
public:
	virtual ~TextureSet() = default;

	bool init(Device &, const TextureSetLayout &);

	// rewrites the argument buffer in place: sampler handles first, then
	// texture handles by image slot (empty image for unused slots);
	// loop thread only
	virtual void write(const core::MaterialLayout &) override;

#if __OBJC__
	id<MTLBuffer> getArgumentBuffer() const { return bridgeHandle<id<MTLBuffer>>(_buffer); }
#endif

	// textures referenced by the argument buffer, for useResource at bind time
	SpanView<Rc<core::ImageView>> getBoundViews() const { return _boundViews; }

protected:
	using core::Object::init;

	Device *_device = nullptr;
	const TextureSetLayout *_setLayout = nullptr;
	void *_buffer = nullptr; // __bridge_retained id<MTLBuffer>
	Vector<Rc<core::ImageView>> _boundViews; // by image slot, may hold nulls
};

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTLTEXTURESET_H_ */
