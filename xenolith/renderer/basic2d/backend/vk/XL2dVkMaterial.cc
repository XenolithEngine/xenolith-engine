/**
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

#include "XL2dVkMaterial.h"
#include "XLCoreAttachment.h"
#include "XLCoreEnum.h"
#include "XLCoreMaterial.h"
#include "XLVkAllocator.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

auto MaterialAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MaterialAttachmentHandle>::create(this, handle);
}

void MaterialAttachment::setCompiled(core::Device &cdev) {
	if (!_pool) {
		auto &dev = static_cast<Device &>(cdev);

		_pool = Rc<DeviceMemoryPool>::create(dev.getAllocator(), false);
	}

	core::MaterialAttachment::setCompiled(cdev);
}

Bytes MaterialAttachment::getMaterialData(NotNull<core::Material> m) const {
	Bytes ret;
	ret.resize(getMaterialSize(m));

	auto &images = m->getImages();
	if (!images.empty()) {
		MaterialData material;
		auto &image = images.front();
		material.samplerImageIdx = image.descriptor | (image.sampler << 16);
		material.setIdx = image.set;
		material.flags = 0;
		material.atlasIdx = 0;
		if (image.image->atlas) {
			if (auto &buffer = image.image->atlas->getBuffer()) {
				material.flags |= XL_GLSL_MATERIAL_FLAG_HAS_ATLAS_INDEX;
				material.atlasIdx = buffer->getDescriptor();

				auto indexSize = buffer->getSize()
						/ (image.image->atlas->getObjectSize() + sizeof(uint32_t) * 2);
				auto pow2index = std::countr_zero(indexSize);

				material.flags |= (pow2index << XL_GLSL_MATERIAL_FLAG_ATLAS_POW2_INDEX_BIT_OFFSET);
				if (buffer->getDeviceAddress() != 0) {
					material.flags |= XL_GLSL_MATERIAL_FLAG_ATLAS_IS_BDA;
				}
			}
		}
		memcpy(ret.data(), &material, sizeof(MaterialData));
	}
	return ret;
}

Rc<core::BufferObject> MaterialAttachment::allocateMaterialPersistentBuffer(
		NotNull<core::Material> m) const {
	Rc<Buffer> buf;
	auto lastPass = getLastRenderPass();
	if (!lastPass) {
		log::source().warn("MaterialAttachment", "Attachment '", _data->key,
				"' was not attached to any RenderPass");
		buf = _pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress, getMaterialSize(m)));
	} else {
		buf = _pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::ShaderDeviceAddress, getMaterialSize(m),
						lastPass->type));
	}
	setMaterialBuffer(m, buf);
	return buf;
}

size_t MaterialAttachment::getMaterialSize(NotNull<core::Material>) const {
	return sizeof(MaterialData);
}

bool MaterialAttachmentHandle::init(const Rc<Attachment> &a, const FrameQueue &handle) {
	if (core::AttachmentHandle::init(a, handle)) {
		return true;
	}
	return false;
}

const MaterialAttachment *MaterialAttachmentHandle::getMaterialAttachment() const {
	return static_cast<MaterialAttachment *>(_attachment.get());
}

const Rc<core::MaterialSet> MaterialAttachmentHandle::getSet() const {
	if (!_materials) {
		_materials = getMaterialAttachment()->getMaterials();
	}
	return _materials;
}

} // namespace stappler::xenolith::basic2d::vk
