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

#ifndef XENOLITH_BACKEND_VK_XLVKRENDERQUEUECOMPILER_H_
#define XENOLITH_BACKEND_VK_XLVKRENDERQUEUECOMPILER_H_

#include "XLVkQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class TransferQueue;
class MaterialCompiler;
class RenderQueueAttachment;

struct SP_PUBLIC RenderQueueInput : public core::AttachmentInputData {
	Rc<core::Queue> queue;
};

class SP_PUBLIC RenderQueueCompiler : public core::Queue {
public:
	virtual ~RenderQueueCompiler() = default;

	bool init(Device &, TransferQueue *, MaterialCompiler *);

	Rc<FrameRequest> makeRequest(Rc<RenderQueueInput> &&);

	TransferQueue *getTransferQueue() const { return _transfer; }
	MaterialCompiler *getMaterialCompiler() const { return _materialCompiler; }

protected:
	using core::Queue::init;

	TransferQueue *_transfer = nullptr;
	MaterialCompiler *_materialCompiler = nullptr;
	const AttachmentData *_attachment;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKRENDERQUEUECOMPILER_H_ */
