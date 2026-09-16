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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTMATERIAL_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTMATERIAL_H_

#include "XLSoftDevice.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

// Per-material persistent buffers are ordinary host allocations, and their "device address" is
// the host pointer - which is exactly what the flat vertex stage expects to dereference.
// Content is defined by the renderer subclass through getMaterialData/getMaterialSize, as in the
// Vulkan and WebGPU backends.
class SP_PUBLIC MaterialAttachment
	: public core::AttachmentTyped<core::AttachmentHandle, core::MaterialAttachment> {
public:
	virtual ~MaterialAttachment() = default;

	virtual Rc<core::BufferObject> allocateMaterialPersistentBuffer(
			NotNull<core::Material>) const override;

	virtual size_t getMaterialSize(NotNull<core::Material> m) const {
		return getMaterialData(m).size();
	}

	// device is bound on setCompiled
	virtual void setCompiled(core::Device &) override;

protected:
	mutable Device *_device = nullptr;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTMATERIAL_H_ */
