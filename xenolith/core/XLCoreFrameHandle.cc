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

#include "XLCoreFrameHandle.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

static constexpr ClockType FrameClockType = ClockType::Monotonic;

#ifdef XL_FRAME_LOG
#define XL_FRAME_LOG_INFO \
	"[", _order, "] [", s_frameCount.load(), \
	"] [", sp::platform::clock(FrameClockType) - _timeStart, "] "
#else
#define XL_FRAME_LOG_INFO
#define XL_FRAME_LOG(...)
#endif

#ifndef XL_FRAME_PROFILE
#define XL_FRAME_PROFILE(fn, tag, max) \
	do { \
		fn; \
	} while (0);
#endif

static std::atomic<uint32_t> s_frameCount = 0;

static Mutex s_frameMutex;
static std::set<FrameHandle *> s_activeFrames;

FrameExternalTask::~FrameExternalTask() {
	if (_frame) {
		_frame->releaseTask(this, _success);
	}
}

void FrameExternalTask::invalidate() { _success = true; }

bool FrameExternalTask::init(FrameHandle &frame, uint32_t idx, Ref *ref, StringView tag) {
	_frame = &frame;
	_index = idx;
	_success = true;
	_tag = tag;
	_userdata = ref;
	return true;
}

uint32_t FrameHandle::GetActiveFramesCount() { return s_frameCount.load(); }

void FrameHandle::DescribeActiveFrames() {
	s_frameMutex.lock();
#if SP_REF_DEBUG
	auto hasFailed = false;
	for (auto &it : s_activeFrames) {
		if (!it->isValidFlag()) {
			hasFailed = true;
			break;
		}
	}

	if (hasFailed) {
		StringStream stream;
		stream << "\n";
		for (auto &it : s_activeFrames) {
			stream << "\tFrame: " << it->getOrder() << " refcount: " << it->getReferenceCount()
				   << "; success: " << it->isValidFlag() << "; backtrace:\n";
			it->foreachBacktrace([&](uint64_t id, Time time, const std::vector<std::string> &vec) {
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) { stream << "\t" << it << "\n"; }
			});
		}
		stappler::log::source().debug("FrameHandle", stream.str());
	}
#endif

	s_frameMutex.unlock();
}

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Destroy");

	_device = nullptr;

#if DEBUG
	s_frameMutex.lock();
	--s_frameCount;
	s_activeFrames.erase(this);
	s_frameMutex.unlock();
#endif

	if (_request) {
		_request->detachFrame();
		_request = nullptr;
	}
	_pool = nullptr;
}

bool FrameHandle::init(Loop &loop, Device &dev, Rc<FrameRequest> &&req, uint64_t gen) {
#if DEBUG
	s_frameMutex.lock();
	s_activeFrames.emplace(this);
	++s_frameCount;
	s_frameMutex.unlock();
#endif

	_loop = &loop;
	_device = &dev;
	_request = move(req);
	_persistentMappings = _request->isPersistentMapping();
	_pool = _request->getPool();
	_timeStart = sp::platform::clock(FrameClockType);
	if (!_request || !_request->getQueue()) {
		return false;
	}

	_gen = gen;
	_order = _request->getQueue()->incrementOrder();

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Init; ready: ");
	return setup();
}

void FrameHandle::update(bool init) {
	if (!_valid) {
		return;
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "update");

	auto tmp = _queues;
	for (auto &it : tmp) { it->update(); }
}

const Rc<Queue> &FrameHandle::getQueue() const { return _request->getQueue(); }

const FrameConstraints &FrameHandle::getFrameConstraints() const {
	return _request->getFrameConstraints();
}

const ImageInfoData *FrameHandle::getImageSpecialization(const ImageAttachment *a) const {
	return _request->getImageSpecialization(a);
}

const FrameOutputBinding *FrameHandle::getOutputBinding(const Attachment *a) const {
	return _request->getOutputBinding(a->getData());
}

const FrameOutputBinding *FrameHandle::getOutputBinding(const AttachmentData *a) const {
	return _request->getOutputBinding(a);
}

Rc<ImageStorage> FrameHandle::getRenderTarget(const Attachment *a) const {
	return _request->getRenderTarget(a->getData());
}

Rc<ImageStorage> FrameHandle::getRenderTarget(const AttachmentData *a) const {
	return _request->getRenderTarget(a);
}

const Vector<Rc<DependencyEvent>> &FrameHandle::getSignalDependencies() const {
	return _request->getSignalDependencies();
}

FrameQueue *FrameHandle::getFrameQueue(Queue *queue) const {
	for (auto &it : _queues) {
		if (it->getQueue() == queue) {
			return it;
		}
	}
	return nullptr;
}

Rc<FrameExternalTask> FrameHandle::acquireTask(Ref *ref, StringView tag) {
	auto task = Rc<FrameExternalTask>::alloc();
	task->init(*this, _tasksRequired.fetch_add(1), ref, tag);
	return task;
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(
			Rc<thread::Task>::create([this, cb = sp::move(cb)](const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [=, this](const thread::Task &, bool) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform,
		Function<void(FrameHandle &, bool)> &&complete, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create(
			[this, perform = sp::move(perform)](const thread::Task &) -> bool {
		return perform(*this);
	}, [=, this, complete = sp::move(complete)](const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1'000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate,
		StringView tag) {
	if (immediate && _loop->isOnThisThread()) {
		XL_FRAME_PROFILE(cb(*this), tag, 1'000);
	} else {
		auto linkId = retain();
		_loop->performOnThread([=, this, cb = sp::move(cb)]() {
			XL_FRAME_PROFILE(cb(*this);, tag, 1'000);
			XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
			release(linkId);
		}, ref);
	}
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&cb, Ref *ref,
		StringView tag) {
	auto task = acquireTask(ref, tag);
	_loop->performInQueue(
			Rc<thread::Task>::create([this, cb = sp::move(cb)](const thread::Task &) -> bool {
		return cb(*this);
	}, [this, tag = tag.str<Interface>(), task = task.get()](const thread::Task &, bool success) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (!success) {
			task->invalidate();
			log::source().error("FrameHandle", "Async task failed: ", tag);
		}
	}, task));
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&perform,
		Function<void(FrameHandle &, bool)> &&complete, Ref *ref, StringView tag) {
	auto task = acquireTask(ref, tag);
	_loop->performInQueue(Rc<thread::Task>::create(
			[this, perform = sp::move(perform)](
					const thread::Task &) -> bool { return perform(*this); },
			[this, complete = sp::move(complete), tag = tag.str<Interface>(),
					task = task.get()](const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1'000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (!success) {
			task->invalidate();
			log::source().error("FrameHandle", "Async task failed: ", tag);
		}
	},
			task));
}

bool FrameHandle::isValid() const {
	return _valid
			&& (!_request->getPresentationFrame() || _request->getPresentationFrame()->isValid());
}

bool FrameHandle::isPersistentMapping() const { return _persistentMappings; }

Rc<AttachmentInputData> FrameHandle::getInputData(const AttachmentData *attachment) {
	return _request->getInputData(attachment);
}

void FrameHandle::invalidate() {
	if (_loop->isOnThisThread()) {
		if (_valid) {
			auto id = retain();
			if (!_timeEnd) {
				_timeEnd = sp::platform::clock(FrameClockType);
			}

			_valid = false;
			_completed = true;

			HashMap<const AttachmentData *, FrameAttachmentData *> attachments;
			for (auto &it : _queues) {
				for (auto &iit : it->getAttachments()) {
					if (iit.second.handle->isOutput()) {
						attachments.emplace(iit.first, (FrameAttachmentData *)&iit.second);
					}
				}
				it->invalidate();
			}

			if (!_submitted) {
				_submitted = true;
				if (auto f = _request->getPresentationFrame()) {
					f->setSubmitted();
				}
			}

			if (_complete) {
				_complete(*this);
			}

			if (_request) {
				auto req = sp::move(_request);
				auto queues = sp::move(_queues);
				auto success = _valid;

				_request = nullptr;
				_queues.clear();

				req->finalize(*_loop, attachments, success);
				for (auto &it : queues) {
					req->signalDependencies(*_loop, it->getQueue(), success);
				}
			}
			release(id);
		}
	} else {
		_loop->performOnThread([this] { invalidate(); }, this);
	}
}

void FrameHandle::setCompleteCallback(Function<void(FrameHandle &)> &&cb) {
	_complete = sp::move(cb);
}

bool FrameHandle::setup() {
	_pool->perform([&, this] {
		auto q = Rc<FrameQueue>::create(_pool, _request->getQueue(), *this);
		q->setup();

		_queues.emplace_back(sp::move(q));
	});

	if (!_valid) {
		for (auto &it : _queues) { it->invalidate(); }
	}

	if (_request) {
		_request->attachFrame(this);
	}

	return true;
}

void FrameHandle::onQueueSubmitted(FrameQueue &queue) {
	++_queuesSubmitted;
	if (_queuesSubmitted == _queues.size()) {
		_submitted = true;
		if (auto f = _request->getPresentationFrame()) {
			f->setSubmitted();
		}
	}
}

void FrameHandle::onQueueComplete(FrameQueue &queue) {
	_submissionTime += queue.getSubmissionTime();
	_deviceTime += queue.getDeviceTime();
	++_queuesCompleted;
	tryComplete();
}

void FrameHandle::releaseTask(FrameExternalTask *task, bool success) {
	_loop->performOnThread([this, success, tag = task->getTag()]() {
		if (success) {
			++_tasksCompleted;
			tryComplete();
		} else {
			log::source().info("FrameHandle", "Task '", tag, "' failed, invalidate frame");
			invalidate();
		}
	}, this, true);
}

bool FrameHandle::onOutputAttachment(FrameAttachmentData &data) {
	return _request->onOutputReady(*_loop, data);
}

void FrameHandle::onOutputAttachmentInvalidated(FrameAttachmentData &data) {
	_request->onOutputInvalidated(*_loop, data);
}

void FrameHandle::waitForDependencies(const Vector<Rc<DependencyEvent>> &events,
		Function<void(FrameHandle &, bool)> &&cb) {
	auto linkId = retain();
	_loop->waitForDependencies(events, [this, cb = sp::move(cb), linkId](bool success) {
		cb(*this, success);
		release(linkId);
	});
}

void FrameHandle::waitForInput(FrameQueue &queue, AttachmentHandle &a, Function<void(bool)> &&cb) {
	_request->waitForInput(queue, a, sp::move(cb));
}

void FrameHandle::signalDependencies(bool success) {
	for (auto &it : _queues) { _request->signalDependencies(*_loop, it->getQueue(), _valid); }
}

void FrameHandle::onQueueInvalidated(FrameQueue &) {
	++_queuesCompleted;
	invalidate();
}

void FrameHandle::tryComplete() {
	if (_tasksCompleted == _tasksRequired.load() && _queuesCompleted == _queues.size()) {
		onComplete();
	}
}

void FrameHandle::onComplete() {
	if (!_completed && _valid) {
		_timeEnd = sp::platform::clock(FrameClockType);
		_completed = true;

		HashMap<const AttachmentData *, FrameAttachmentData *> attachments;
		for (auto &it : _queues) {
			for (auto &iit : it->getAttachments()) {
				if (iit.second.handle->isOutput()) {
					attachments.emplace(iit.first, (FrameAttachmentData *)&iit.second);
				}
			}
		}

		if (_complete) {
			_complete(*this);
		}

		_request->finalize(*_loop, attachments, _valid);

		for (auto &it : _queues) { _request->signalDependencies(*_loop, it->getQueue(), _valid); }
	}
}

} // namespace stappler::xenolith::core
