/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_BACKEND_VK_XLVKMESHCOMPILER_H_
#define XENOLITH_BACKEND_VK_XLVKMESHCOMPILER_H_

#include "XLCoreMesh.h"
#include "XLVkQueuePass.h"
#include "XLVkAllocator.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class MeshCompilerAttachment;

class SP_PUBLIC MeshCompiler : public core::Queue {
public:
	using MeshAttachment = core::MeshAttachment;
	using MeshInputData = core::MeshInputData;
	using MeshIndex = core::MeshIndex;

	virtual ~MeshCompiler();

	bool init();

	bool inProgress(const MeshAttachment *) const;
	void setInProgress(const MeshAttachment *);
	void dropInProgress(const MeshAttachment *);

	bool hasRequest(const MeshAttachment *) const;
	void appendRequest(const MeshAttachment *, Rc<MeshInputData> &&, Vector<Rc<core::DependencyEvent>> &&deps);
	void clearRequests();

	Rc<FrameRequest> makeRequest(Rc<MeshInputData> &&, Vector<Rc<core::DependencyEvent>> &&deps);
	void runMeshCompilationFrame(core::Loop &loop, Rc<MeshInputData> &&req,
			Vector<Rc<core::DependencyEvent>> &&deps);

protected:
	using Queue::init;

	struct MeshRequest {
		Set<Rc<MeshIndex>> toAdd;
		Set<Rc<MeshIndex>> toRemove;
		Vector<Rc<core::DependencyEvent>> deps;
	};

	const AttachmentData *_attachment = nullptr;
	Set<const MeshAttachment *> _inProgress;
	Map<const MeshAttachment *, MeshRequest> _requests;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMESHCOMPILER_H_ */
