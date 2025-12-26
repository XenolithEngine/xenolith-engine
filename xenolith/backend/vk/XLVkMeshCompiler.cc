/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLVkMeshCompiler.h"

#include "XLCoreMesh.h"
#include "XLVkAllocator.h"
#include "XLVkTransferQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class MeshCompilerAttachment : public core::GenericAttachment {
public:
	virtual ~MeshCompilerAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class MeshCompilerAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~MeshCompilerAttachmentHandle();

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	virtual const Rc<core::MeshInputData> &getInputData() const { return _inputData; }
	virtual const Rc<core::MeshSet> &getMeshSet() const { return _originSet; }

protected:
	Rc<core::MeshInputData> _inputData;
	Rc<core::MeshSet> _originSet;
};

class MeshCompilerPass : public QueuePass {
public:
	virtual ~MeshCompilerPass();

	virtual bool init(QueuePassBuilder &, const AttachmentData *);

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *getMeshAttachment() const { return _meshAttachment; }

protected:
	using QueuePass::init;

	const AttachmentData *_meshAttachment = nullptr;
};

class MeshCompilerPassHandle : public QueuePassHandle {
public:
	virtual ~MeshCompilerPassHandle();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful) override;

	virtual core::QueueFlags getQueueOps() const override;

protected:
	virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;
	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool,
			Rc<core::Fence> &&) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

	bool loadPersistent(core::MeshIndex *);

	Rc<core::MeshSet> _outputData;
	MeshCompilerAttachmentHandle *_meshAttachment;
};

MeshCompiler::~MeshCompiler() { }

bool MeshCompiler::init() {
	using namespace core;

	Queue::Builder builder("MeshCompiler");

	auto attachment =
			builder.addAttachemnt("", [](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.defineAsOutput();
		return Rc<MeshCompilerAttachment>::create(attachmentBuilder);
	});

	builder.addPass("MeshPass", PassType::Transfer, RenderOrdering(0),
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<MeshCompilerPass>::create(passBuilder, attachment);
	});

	if (core::Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

bool MeshCompiler::inProgress(const MeshAttachment *a) const {
	auto it = _inProgress.find(a);
	if (it != _inProgress.end()) {
		return true;
	}
	return false;
}

void MeshCompiler::setInProgress(const MeshAttachment *a) { _inProgress.emplace(a); }

void MeshCompiler::dropInProgress(const MeshAttachment *a) { _inProgress.erase(a); }

bool MeshCompiler::hasRequest(const MeshAttachment *a) const {
	auto it = _requests.find(a);
	if (it != _requests.end()) {
		return true;
	}
	return false;
}

void MeshCompiler::appendRequest(const MeshAttachment *a, Rc<MeshInputData> &&req,
		Vector<Rc<core::DependencyEvent>> &&deps) {
	auto it = _requests.find(a);
	if (it == _requests.end()) {
		it = _requests.emplace(a, MeshRequest()).first;
	}

	for (auto &rem : req->meshesToRemove) {
		auto m = it->second.toAdd.find(rem);
		if (m != it->second.toAdd.end()) {
			it->second.toAdd.erase(m);
		}

		it->second.toRemove.emplace(rem);
	}

	for (auto &m : req->meshesToAdd) {
		auto iit = it->second.toAdd.find(m);
		if (iit == it->second.toAdd.end()) {
			it->second.toAdd.emplace(m);
		}
		auto v = it->second.toRemove.find(m);
		if (v != it->second.toRemove.end()) {
			it->second.toRemove.erase(v);
		}
	}

	if (it->second.deps.empty()) {
		it->second.deps = sp::move(deps);
	} else {
		for (auto &iit : deps) { it->second.deps.emplace_back(move(iit)); }
	}
}

void MeshCompiler::clearRequests() { _requests.clear(); }

auto MeshCompiler::makeRequest(Rc<core::MeshInputData> &&input,
		Vector<Rc<core::DependencyEvent>> &&deps) -> Rc<FrameRequest> {
	auto req = Rc<FrameRequest>::create(this);
	req->addInput(_attachment, move(input));
	req->addSignalDependencies(sp::move(deps));
	return req;
}

void MeshCompiler::runMeshCompilationFrame(core::Loop &loop, Rc<MeshInputData> &&req,
		Vector<Rc<core::DependencyEvent>> &&deps) {
	auto targetAttachment = req->attachment;

	auto h = loop.makeFrame(makeRequest(sp::move(req), sp::move(deps)), 0);
	h->setCompleteCallback([this, targetAttachment](FrameHandle &handle) {
		auto reqIt = _requests.find(targetAttachment);
		if (reqIt != _requests.end()) {
			if (handle.getLoop()->isRunning()) {
				auto deps = sp::move(reqIt->second.deps);
				Rc<MeshInputData> req = Rc<MeshInputData>::alloc();
				req->attachment = targetAttachment;
				req->meshesToAdd.reserve(reqIt->second.toAdd.size());
				for (auto &m : reqIt->second.toAdd) { req->meshesToAdd.emplace_back(m); }
				req->meshesToRemove.reserve(reqIt->second.toRemove.size());
				for (auto &m : reqIt->second.toRemove) { req->meshesToRemove.emplace_back(m); }
				_requests.erase(reqIt);

				runMeshCompilationFrame(*handle.getLoop(), sp::move(req), sp::move(deps));
			} else {
				clearRequests();
				dropInProgress(targetAttachment);
			}
		} else {
			dropInProgress(targetAttachment);
		}
	});
	h->update(true);
}

MeshCompilerAttachment::~MeshCompilerAttachment() { }

auto MeshCompilerAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MeshCompilerAttachmentHandle>::create(this, handle);
}

MeshCompilerAttachmentHandle::~MeshCompilerAttachmentHandle() { }

bool MeshCompilerAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	return true;
}

void MeshCompilerAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	auto d = data.cast<core::MeshInputData>();
	if (!d || q.isFinalized()) {
		cb(false);
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle, bool success) {
		handle.performOnGlThread([this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle) {
			_inputData = d;
			_originSet = _inputData->attachment->getMeshes();
			cb(true);
		}, this, true, "MeshCompilerAttachmentHandle::submitInput");
	});
}

MeshCompilerPass::~MeshCompilerPass() { }

bool MeshCompilerPass::init(QueuePassBuilder &passBuilder, const AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	_queueOps = core::QueueFlags::Transfer;
	_meshAttachment = attachment;
	return true;
}

auto MeshCompilerPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<MeshCompilerPassHandle>::create(*this, handle);
}

MeshCompilerPassHandle::~MeshCompilerPassHandle() { }

bool MeshCompilerPassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(
				static_cast<MeshCompilerPass *>(_queuePass.get())->getMeshAttachment())) {
		_meshAttachment = static_cast<MeshCompilerAttachmentHandle *>(a->handle.get());
	}

	return QueuePassHandle::prepare(frame, sp::move(cb));
}

void MeshCompilerPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);
}

core::QueueFlags MeshCompilerPassHandle::getQueueOps() const {
	return QueuePassHandle::getQueueOps();
}

Vector<const core::CommandBuffer *> MeshCompilerPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto &allocator = _device->getAllocator();
	auto memPool = static_cast<DeviceFrameHandle &>(handle).getMemPool(this);

	auto input = _meshAttachment->getInputData();
	auto prev = _meshAttachment->getMeshSet();

	core::QueueFlags ops = core::QueueFlags::None;
	for (auto &it : input->attachment->getRenderPasses()) {
		ops |= static_cast<vk::QueuePass *>(it->pass.get())->getQueueOps();
	}

	auto q = _device->getQueueFamily(ops);
	if (!q) {
		return Vector<const core::CommandBuffer *>();
	}

	auto indexes = _meshAttachment->getMeshSet()->getIndexes();

	do {
		auto it = indexes.begin();
		while (it != indexes.end()) {
			auto iit = std::find(input->meshesToAdd.begin(), input->meshesToAdd.end(), it->index);
			if (iit != input->meshesToAdd.end()) {
				input->meshesToAdd.erase(iit);
			}

			iit = std::find(input->meshesToRemove.begin(), input->meshesToRemove.end(), it->index);
			if (iit != input->meshesToRemove.end()) {
				it = indexes.erase(it);
			} else {
				++it;
			}
		}
	} while (0);

	for (auto &it : input->meshesToAdd) {
		indexes.emplace_back(
				core::MeshSet::Index{maxOf<VkDeviceSize>(), maxOf<VkDeviceSize>(), it});
	}

	uint64_t indexBufferSize = 0;
	uint64_t vertexBufferSize = 0;

	for (auto &it : indexes) {
		indexBufferSize += it.index->getIndexBufferData()->size;
		vertexBufferSize += it.index->getVertexBufferData()->size;
	}

	BufferInfo vertexBufferInfo;
	BufferInfo indexBufferInfo;

	if (prev) {
		vertexBufferInfo = prev->getVertexBuffer()->getInfo();
		indexBufferInfo = prev->getIndexBuffer()->getInfo();
	} else {
		vertexBufferInfo = *indexes.front().index->getVertexBufferData();
		indexBufferInfo = *indexes.front().index->getIndexBufferData();
	}

	vertexBufferInfo.size = vertexBufferSize;
	indexBufferInfo.size = indexBufferSize;

	auto vertexBuffer = allocator->spawnPersistent(AllocationUsage::DeviceLocal, vertexBufferInfo);
	auto indexBuffer = allocator->spawnPersistent(AllocationUsage::DeviceLocal, indexBufferInfo);

	auto loadBuffer = [](const core::BufferData *bufferData, Buffer *buf) {
		if (!bufferData->data.empty()) {
			buf->setData(bufferData->data);
		} else {
			buf->map([&](uint8_t *ptr, VkDeviceSize size) { bufferData->writeData(ptr, size); });
		}
		return buf;
	};

	auto writeBufferCopy = [&memPool, &loadBuffer](CommandBuffer &buf,
								   const core::BufferData *bufferData, Buffer *targetBuffer,
								   VkDeviceSize targetOffset, VkDeviceSize originOffset,
								   Buffer *originBuffer) -> VkDeviceSize {
		Buffer *sourceBuffer = nullptr;
		VkDeviceSize sourceOffset = 0;
		VkDeviceSize targetSize = bufferData->size;

		if (originBuffer && originOffset != maxOf<VkDeviceSize>()) {
			sourceBuffer = originBuffer;
			sourceOffset = originOffset;
		} else {
			auto resourceBuffer = bufferData->buffer.get();
			if (!resourceBuffer) {
				auto tmp = memPool->spawn(AllocationUsage::HostTransitionSource, bufferData);
				resourceBuffer = loadBuffer(bufferData, tmp);
			}

			if (resourceBuffer) {
				sourceBuffer = static_cast<Buffer *>(resourceBuffer);
				sourceOffset = 0;
			}
		}

		if (sourceBuffer) {
			buf.cmdCopyBuffer(sourceBuffer, targetBuffer, sourceOffset, targetOffset, targetSize);
			return targetSize;
		}
		return 0;
	};

	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors),
			[&, this](CommandBuffer &buf) {
		uint64_t targetIndexOffset = 0;
		uint64_t targetVertexOffset = 0;

		Buffer *prevIndexBuffer = nullptr;
		Buffer *prevVertexBuffer = nullptr;

		if (_pool->getFamilyIdx() == q->index) {
			prevIndexBuffer = static_cast<Buffer *>(prev->getIndexBuffer().get());
			prevVertexBuffer = static_cast<Buffer *>(prev->getVertexBuffer().get());
		}

		for (auto &it : indexes) {
			if (_pool->getFamilyIdx() != q->index) {
				if (!loadPersistent(it.index)) {
					continue;
				}
			}

			auto indexBufferSize = writeBufferCopy(buf, it.index->getIndexBufferData(), indexBuffer,
					targetIndexOffset, it.indexOffset, prevIndexBuffer);
			if (indexBufferSize > 0) {
				it.indexOffset = targetIndexOffset;
				targetIndexOffset += indexBufferSize;
			} else {
				it.indexOffset = maxOf<VkDeviceSize>();
			}

			auto vertexBufferSize = writeBufferCopy(buf, it.index->getVertexBufferData(),
					vertexBuffer, targetVertexOffset, it.vertexOffset, prevVertexBuffer);
			if (vertexBufferSize > 0) {
				it.vertexOffset = targetIndexOffset;
				targetIndexOffset += vertexBufferSize;
			} else {
				it.vertexOffset = maxOf<VkDeviceSize>();
			}
		}
		return true;
	});

	if (buf) {
		_outputData = Rc<core::MeshSet>::create(sp::move(indexes), indexBuffer, vertexBuffer);

		return Vector<const core::CommandBuffer *>{buf};
	}
	return Vector<const core::CommandBuffer *>();
}

void MeshCompilerPassHandle::doSubmitted(FrameHandle &frame, Function<void(bool)> &&func,
		bool success, Rc<core::Fence> &&fence) {
	if (success) {
		_meshAttachment->getInputData()->attachment->setMeshes(sp::move(_outputData));
	}

	QueuePassHandle::doSubmitted(frame, sp::move(func), success, sp::move(fence));
	frame.signalDependencies(success);
}

void MeshCompilerPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func,
		bool success) {
	QueuePassHandle::doComplete(queue, sp::move(func), success);
}

bool MeshCompilerPassHandle::loadPersistent(core::MeshIndex *index) {
	if (!index->isCompiled()) {
		auto res = Rc<TransferResource>::create(_device->getAllocator(), index);
		if (res->initialize(AllocationUsage::HostTransitionSource) && res->compile()) {
			return true;
		}
		return false;
	}
	return false;
}

} // namespace stappler::xenolith::vk
