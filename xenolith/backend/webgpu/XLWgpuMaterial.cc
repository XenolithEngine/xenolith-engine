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

#include "XLWgpuMaterial.h"
#include "XLWgpuDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

void MaterialAttachment::setCompiled(core::Device &dev) {
	_device = static_cast<Device *>(&dev);

	core::MaterialAttachment::setCompiled(dev);
}

Rc<core::BufferObject> MaterialAttachment::allocateMaterialPersistentBuffer(
		NotNull<core::Material> m) const {
	if (!_device) {
		log::source().error("webgpu::MaterialAttachment",
				"allocateMaterialPersistentBuffer: attachment is not compiled");
		return nullptr;
	}

	auto size = getMaterialSize(m);
	if (size == 0) {
		return nullptr;
	}

	auto buf = Rc<Buffer>::create(*_device,
			core::BufferInfo(core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
					uint64_t(size), StringView("MaterialBuffer")));
	if (buf) {
		setMaterialBuffer(m, Rc<core::BufferObject>(buf));
	}
	return buf;
}

} // namespace stappler::xenolith::webgpu
