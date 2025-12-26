/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLMATERIALCOMPILER_H_
#define XENOLITH_BACKEND_VK_XLMATERIALCOMPILER_H_

#include "XLVkQueuePass.h" // IWYU pragma: keep
#include "XLVkAllocator.h" // IWYU pragma: keep
#include "XLCoreAttachment.h"
#include "XLCoreMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC MaterialCompiler : public core::Queue {
public:
	using MaterialAttachment = core::MaterialAttachment;
	using MaterialInputData = core::MaterialInputData;
	using MaterialId = core::MaterialId;
	using Material = core::Material;

	virtual ~MaterialCompiler();

	bool init();

	bool inProgress(const MaterialAttachment *) const;
	void setInProgress(const MaterialAttachment *);
	void dropInProgress(const MaterialAttachment *);

	bool hasRequest(const MaterialAttachment *) const;
	void appendRequest(const MaterialAttachment *, Rc<MaterialInputData> &&,
			Vector<Rc<core::DependencyEvent>> &&deps);
	void appendRequest(const MaterialAttachment *, Rc<MaterialInputData> &&,
			SpanView<Rc<core::DependencyEvent>> deps);
	void clearRequests();

	Rc<FrameRequest> makeRequest(Rc<MaterialInputData> &&,
			Vector<Rc<core::DependencyEvent>> &&deps);
	Rc<FrameRequest> makeRequest(Rc<MaterialInputData> &&,
			SpanView<Rc<core::DependencyEvent>> deps);
	void runMaterialCompilationFrame(core::Loop &loop, Rc<MaterialInputData> &&req,
			Vector<Rc<core::DependencyEvent>> &&deps);
	void runMaterialCompilationFrame(core::Loop &loop, Rc<MaterialInputData> &&req,
			SpanView<Rc<core::DependencyEvent>> deps);

protected:
	using core::Queue::init;

	struct MaterialRequest {
		Map<MaterialId, Rc<Material>> materials;
		Set<MaterialId> dynamic;
		Set<MaterialId> remove;
		Vector<Rc<core::DependencyEvent>> deps;
		Function<void()> callback;
	};

	const AttachmentData *_attachment = nullptr;
	Set<const MaterialAttachment *> _inProgress;
	Map<const MaterialAttachment *, MaterialRequest> _requests;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLMATERIALCOMPILER_H_ */
