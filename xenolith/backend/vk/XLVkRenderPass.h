/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_
#define XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_

#include "XLCoreInfo.h"
#include "XLVk.h"
#include "XLVkDeviceQueue.h"
#include "XLCoreQueuePass.h"
#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class RenderPass;
class QueuePassHandle;

struct SP_PUBLIC DescriptorSetBindings : public Ref {
	uint32_t idx = 0;
	VkDescriptorSet set;
	Vector<core::DescriptorBinding> bindings;
};

class SP_PUBLIC Framebuffer : public core::Framebuffer {
public:
	virtual ~Framebuffer() { }

	bool init(Device &dev, RenderPass *renderPass, SpanView<Rc<core::ImageView>> imageViews);

	VkFramebuffer getFramebuffer() const { return _framebuffer; }

protected:
	using core::Framebuffer::init;

	VkFramebuffer _framebuffer = VK_NULL_HANDLE;
};

class SP_PUBLIC PipelineLayout : public core::Object {
public:
	struct DescriptorBindingInfo {
		core::DescriptorType type;
		core::DescriptorFlags flags;
		uint32_t count;
	};

	virtual ~PipelineLayout();

	bool init(Device &dev, const core::PipelineLayoutData &, uint32_t index);

	uint32_t getIndex() const { return _index; }
	VkPipelineLayout getLayout() const { return _layout; }
	SpanView<VkDescriptorSetLayout> getLayouts() const { return _layouts; }
	SpanView<VkDescriptorPoolSize> getSizes() const { return _sizes; }
	SpanView<DescriptorBindingInfo> getDescriptorsInfo(uint32_t idx) const {
		return _descriptors[idx];
	}
	uint32_t getMaxSets() const { return _maxSets; }
	bool haveUpdateAfterBind() const { return _updateAfterBind; }

protected:
	uint32_t _index = 0;
	VkPipelineLayout _layout = VK_NULL_HANDLE;
	Vector<VkDescriptorSetLayout> _layouts;
	Vector<VkDescriptorPoolSize> _sizes;
	Vector<Vector<DescriptorBindingInfo>> _descriptors;

	Rc<core::TextureSetLayout> _textureSetLayout;

	uint32_t _maxSets = 0;
	bool _updateAfterBind = false;
};

class SP_PUBLIC DescriptorPool : public core::Object {
public:
	virtual ~DescriptorPool();

	bool init(Device &dev, PipelineLayout *);

	PipelineLayout *getLayout() const { return _layout; }
	uint32_t getLayoutIndex() const { return _layoutIndex; }

	DescriptorSetBindings *getSet(uint32_t idx) const { return _sets[idx]; }
	SpanView<Rc<DescriptorSetBindings>> getSets() const { return _sets; }

protected:
	uint32_t _layoutIndex = 0;
	Rc<PipelineLayout> _layout;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
	Vector<Rc<DescriptorSetBindings>> _sets;
};

class SP_PUBLIC RenderPass : public core::RenderPass {
public:
	struct Data {
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkRenderPass renderPassAlternative = VK_NULL_HANDLE;
		Vector<Rc<PipelineLayout>> layouts;

		bool cleanup(Device &dev);
	};

	virtual ~RenderPass() = default;

	virtual bool init(Device &dev, QueuePassData &);

	VkRenderPass getRenderPass(bool alt = false) const;
	PipelineLayout *getPipelineLayout(uint32_t idx) const { return _data->layouts[idx]; }

	//const Vector<Rc<DescriptorSetBindings>> &getDescriptorSets(uint32_t idx) const { return _data->layouts[idx].descriptors[0].sets; }

	const Vector<VkClearValue> &getClearValues() const { return _clearValues; }

	Rc<DescriptorPool> acquireDescriptorPool(Device &dev, uint32_t idx);
	void releaseDescriptorPool(Rc<DescriptorPool> &&);

	// if async is true - update descriptors with updateAfterBind flag
	// 			   false - without updateAfterBindFlag
	virtual bool writeDescriptors(const QueuePassHandle &, DescriptorPool *pool, bool async) const;

	virtual void perform(const QueuePassHandle &, CommandBuffer &buf, const Callback<void()> &,
			bool writeBarriers = false);

protected:
	using core::RenderPass::init;

	bool initGraphicsPass(Device &dev, QueuePassData &);
	bool initComputePass(Device &dev, QueuePassData &);
	bool initTransferPass(Device &dev, QueuePassData &);
	bool initGenericPass(Device &dev, QueuePassData &);

	bool initDescriptors(Device &dev, const QueuePassData &, Data &);

	Vector<VkAttachmentDescription> _attachmentDescriptions;
	Vector<VkAttachmentDescription> _attachmentDescriptionsAlternative;
	Vector<VkAttachmentReference> _attachmentReferences;
	Vector<uint32_t> _preservedAttachments;
	Vector<VkSubpassDependency> _subpassDependencies;
	Vector<VkSubpassDescription> _subpasses;
	Set<const core::AttachmentPassData *> _variableAttachments;
	Data *_data = nullptr;

	Vector<VkClearValue> _clearValues;

	Mutex _descriptorPoolMutex;
	Vector<Vector<Rc<DescriptorPool>>> _descriptorPools;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_ */
