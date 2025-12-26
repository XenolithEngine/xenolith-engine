/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_
#define XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class TextureSet;
class Loop;

// persistent object, part of Device
class SP_PUBLIC TextureSetLayout : public core::TextureSetLayout {
public:
	static uint32_t getLayoutImageCount(const Device &dev, const core::TextureSetLayoutData &);

	using AttachmentLayout = core::AttachmentLayout;

	virtual ~TextureSetLayout() = default;

	bool init(Device &dev, const core::TextureSetLayoutData &);

	VkDescriptorSetLayout getLayout() const { return _layout; }

protected:
	VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
};

class SP_PUBLIC TextureSet : public core::TextureSet {
public:
	virtual ~TextureSet() = default;

	bool init(Device &dev, const core::TextureSetLayout &);

	VkDescriptorSet getSet() const { return _set; }

	virtual void write(const core::MaterialLayout &) override;

	Vector<const ImageMemoryBarrier *> getPendingImageBarriers() const;

	void foreachPendingImageBarriers(const Callback<void(const ImageMemoryBarrier &)> &,
			bool drain) const;

	void dropPendingBarriers();

	Device *getDevice() const;

protected:
	using core::TextureSet::init;

	void writeImages(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
			std::forward_list<Vector<VkDescriptorImageInfo>> &imagesList);

	bool _partiallyBound = false;
	const TextureSetLayout *_layout = nullptr;
	uint32_t _imageCount = 0;
	VkDescriptorSet _set = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
	Vector<Image *> _pendingImageBarriers;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_ */
