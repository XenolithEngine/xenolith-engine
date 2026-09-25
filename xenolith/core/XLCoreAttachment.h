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

#ifndef XENOLITH_CORE_XLCOREATTACHMENT_H_
#define XENOLITH_CORE_XLCOREATTACHMENT_H_

#include "XLCoreEnum.h"
#include "XLCoreQueueData.h"
#include "XLCoreDescriptorInfo.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class RenderClientChannel;

/** DependencyEvent используется для синхронизации данных на стороне GPU

Поставщик DependencyEvent создаёт или изменяет данные на стороне GPU, а
потребитель ожидает готовности этих данных.

Поставщик создаёт DependencyEvent для своего набора очередей рендеринга
и отправляет его с помощью FrameRequest::addSignalDependency при запуске 
работы. Также, можно связывать событие с обновлением материала, меша или
динамического изображения.

Потребитель использует AttachmentInputData::waitDependencies для передачи
событий в составе входящих данных для своего кадра. При работе с кадром,
вложение использует FrameHandle::waitForDependencies для ожидания событий.
Ожидание проходит асинхронно, кадр подготавливает вложения по мере поступления
событий.

Событие может завершиться с ошибкой. В таком случае, связанные кадры
потребителя также должны завершиться с ошибкой.
*/
class SP_PUBLIC DependencyEvent final : public Ref {
public:
	using QueueSet = mem_std::Set<Rc<Queue>>;

	static uint32_t GetNextId();

	// Set the high-bit mask added to every subsequently-generated id (0 = server/local, 0x80000000 =
	// remote client). Call once at app init before any DependencyEvent is created.
	static void SetIdGenerationMask(uint32_t mask);

	virtual ~DependencyEvent();

	DependencyEvent(QueueSet &&, StringView);
	DependencyEvent(InitializerList<Rc<Queue>> &&, StringView);

	uint32_t getId() const { return _id; }

	bool signal(Queue *, bool);

	// Safe to poll from any thread: it reads a flag the signalling thread publishes, not the queue
	// set itself (basic2d::Sprite::visitDraw checks it from the app thread).
	bool isSignaled() const;
	bool isSuccessful() const;

	void addQueue(Rc<Queue> &&);

	/* Stamps the hand-over to the queue that will signal the event, splitting its life into the
	wait to be sent and the queue's work (both logged by `XL_DEP_ACCOUNT=1` when it fires).
	Idempotent: the first stamp stands. */
	void markSent();

	// Register a callback fired exactly once, when the event becomes fully signalled. It runs on the
	// signalling thread (typically the GPU loop) and must hop threads for non-thread-safe work. The
	// remote server uses it to drop a fired client-mirrored dependency from its registry.
	void setSignalCallback(Function<void()> &&);

protected:
	uint32_t _id = GetNextId();
	uint64_t _clock = sp::platform::clock(ClockType::Monotonic);
	uint64_t _sentClock = 0; // see markSent; zero means nobody said
	QueueSet _queues;
	StringView _tag;
	bool _success = true;
	// Mirrors "_queues is empty", published for readers off the signalling thread. An event built
	// with no queues starts out signalled - that is how a client-side mirror event (nothing signals
	// it locally, the server gates on its own copy) reads as already satisfied.
	sprt::atomic<bool> _signaled;
	Function<void()> _signalCallback;
};

// dummy class for attachment input
struct SP_PUBLIC AttachmentInputData : public Ref {
	virtual ~AttachmentInputData() = default;

	Vector<Rc<DependencyEvent>> waitDependencies;

	// Serialization for the remote render session (see XLCoreFrameRequestProxy.h). Each concrete
	// input owns its wire format (e.g. basic2d serializes its command list). The defaults mean
	// "no wire format": serialize writes nothing and reports false, deserialize fails.
	//
	// `identityNamespace` separates the data identities of one peer from every other's: the
	// server passes the session id, and an input that carries identities moves them into it, so
	// that damage tracking never matches one client's data against another's (or the server's own,
	// which live in namespace 0).
	virtual bool serialize(const Callback<void(BytesView)> &) const { return false; }
	virtual bool deserialize(BytesView, Vector<uint32_t> *remoteDeps = nullptr,
			uint64_t identityNamespace = 0) {
		return false;
	}
};

class SP_PUBLIC Attachment : public NamedRef {
public:
	using FrameQueue = core::FrameQueue;
	using RenderQueue = core::Queue;
	using PassData = core::QueuePassData;
	using AttachmentData = core::AttachmentData;
	using AttachmentHandle = core::AttachmentHandle;
	using AttachmentBuilder = core::AttachmentBuilder;

	using FrameHandleCallback = Function<Rc<AttachmentHandle>(Attachment &, const FrameQueue &)>;

	virtual ~Attachment() = default;

	virtual bool init(AttachmentBuilder &builder);
	virtual void clear();

	virtual StringView getName() const override;

	uint64_t getId() const;
	AttachmentUsage getUsage() const;
	bool isTransient() const;

	void setFrameHandleCallback(FrameHandleCallback &&);

	// Run input callback for frame and handle
	void acquireInput(FrameQueue &, AttachmentHandle &, Function<void(bool)> &&);

	bool validateInput(const AttachmentInputData *) const;

	// Mint an empty input-data object of the concrete type this attachment consumes, so a remote
	// server can deserialize a wire blob into it (see AttachmentInputData::deserialize). Default null:
	// only input attachments that participate in the remote render session override this.
	// `windowId` travels with the input because the render pass that later reports a DrawStat runs
	// on another thread, and the channel serves every window.
	virtual Rc<AttachmentInputData> makeInputData(NotNull<RenderClientChannel>,
			uint64_t windowId) const {
		return nullptr;
	}

	virtual bool isCompatible(const ImageInfo &) const { return false; }

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &);

	virtual Vector<const PassData *> getRenderPasses() const;
	virtual const PassData *getFirstRenderPass() const;
	virtual const PassData *getLastRenderPass() const;
	virtual const PassData *getNextRenderPass(const PassData *) const;
	virtual const PassData *getPrevRenderPass(const PassData *) const;

	const AttachmentData *getData() const { return _data; }

	// The render queue this attachment belongs to, or null before the queue took ownership.
	// getData() lives in that queue's pool, so holding an attachment across a thread hop or a frame
	// requires keeping the queue alive, not just the attachment.
	Queue *getQueue() const;

	virtual void setCompiled(Device &);

protected:
	const AttachmentData *_data = nullptr;

	FrameHandleCallback _frameHandleCallback;
};

class SP_PUBLIC BufferAttachment : public Attachment {
public:
	virtual ~BufferAttachment() = default;

	virtual bool init(AttachmentBuilder &builder, const BufferInfo &);

	// init as static buffer
	// static buffer should be pre-initialized and valid until attachment's queue exists
	// or be a part of queue's internal resource (`builder.addBuffer()`)
	virtual bool init(AttachmentBuilder &builder, const BufferData *);
	virtual bool init(AttachmentBuilder &builder, Vector<const BufferData *> &&);

	virtual void clear() override;

	const BufferInfo &getInfo() const { return _info; }

	virtual bool isStatic() const { return !_staticBuffers.empty(); }

	virtual Vector<BufferObject *> getStaticBuffers() const;

protected:
	using Attachment::init;

	BufferInfo _info;
	Vector<const BufferData *> _staticBuffers;
};

class SP_PUBLIC ImageAttachment : public Attachment {
public:
	virtual ~ImageAttachment() = default;

	struct AttachmentInfo {
		AttachmentLayout initialLayout = AttachmentLayout::Ignored;
		AttachmentLayout finalLayout = AttachmentLayout::Ignored;
		bool clearOnLoad = false;
		Color4F clearColor = Color4F::BLACK;
		ColorMode colorMode;
	};

	virtual bool init(AttachmentBuilder &builder, const ImageInfo &, AttachmentInfo &&);

	// init as static image
	// static image should be pre-initialized and valid until attachment's queue exists
	// or be a part of queue's internal resource (`builder.addImage()`)
	virtual bool init(AttachmentBuilder &builder, const ImageData *, AttachmentInfo &&);

	virtual const ImageInfo &getImageInfo() const { return _imageInfo; }
	virtual bool shouldClearOnLoad() const { return _attachmentInfo.clearOnLoad; }
	virtual Color4F getClearColor() const { return _attachmentInfo.clearColor; }
	virtual ColorMode getColorMode() const { return _attachmentInfo.colorMode; }

	virtual AttachmentLayout getInitialLayout() const { return _attachmentInfo.initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _attachmentInfo.finalLayout; }

	virtual bool isStatic() const { return _staticImage != nullptr; }

	virtual ImageObject *getStaticImage() const { return _staticImage->image; }
	virtual ImageStorage *getStaticImageStorage() const { return _staticImageStorage; }

	virtual void addImageUsage(ImageUsage);

	virtual bool isCompatible(const ImageInfo &) const override;

	virtual ImageViewInfo getImageViewInfo(const ImageInfoData &info,
			const AttachmentPassData &) const;
	virtual Vector<ImageViewInfo> getImageViews(const ImageInfoData &info) const;

	virtual void setCompiled(Device &) override;

protected:
	using Attachment::init;

	ImageInfo _imageInfo;
	AttachmentInfo _attachmentInfo;
	const ImageData *_staticImage = nullptr;
	Rc<ImageStorage> _staticImageStorage;
};

class SP_PUBLIC GenericAttachment : public Attachment {
public:
	virtual ~GenericAttachment() = default;

	virtual bool init(AttachmentBuilder &builder) override;

protected:
	using Attachment::init;
};

class SP_PUBLIC AttachmentHandle : public Ref {
public:
	using PassHandle = core::QueuePassHandle;
	using FrameQueue = core::FrameQueue;
	using FrameHandle = core::FrameHandle;
	using Attachment = core::Attachment;

	virtual ~AttachmentHandle() = default;

	virtual bool init(const Rc<Attachment> &, const FrameQueue &);
	virtual bool init(Attachment &, const FrameQueue &);

	virtual void setQueueData(FrameAttachmentData *);

	virtual FrameAttachmentData *getQueueData() const { return _queueData; }

	virtual bool isAvailable(const FrameQueue &) const { return true; }

	// returns true for immediate setup, false if setup job was scheduled
	virtual bool setup(FrameQueue &, Function<void(bool)> &&);

	virtual void finalize(FrameQueue &, bool successful);

	virtual bool isInput() const;
	virtual bool isOutput() const;

	virtual const Rc<Attachment> &getAttachment() const { return _attachment; }

	virtual void submitInput(FrameQueue &, Rc<AttachmentInputData> &&, Function<void(bool)> &&cb);

	virtual uint32_t enumerateDirtyDescriptors(const PassHandle &, const PipelineDescriptor &,
			const DescriptorBinding &, const Callback<void(uint32_t)> &) const;

	virtual void enumerateAttachmentObjects(
			const Callback<void(Object *, const SubresourceRangeInfo &)> &);

	StringView getName() const { return _attachment->getName(); }

	AttachmentInputData *getInput() const { return _input; }

protected:
	Rc<AttachmentInputData> _input;
	Rc<Attachment> _attachment;
	FrameAttachmentData *_queueData = nullptr;
};

/* Typed attachment base: creates HandleType in makeFrameHandle.
 *
 * Prefer this over overriding makeFrameHandle by hand: inside a derived member function an
 * unqualified handle name resolves to the base AttachmentHandle alias and silently creates the
 * base handle. HandleType must be complete at the point of class instantiation.
 */
template <typename HandleType, typename BaseAttachment = Attachment>
class AttachmentTyped : public BaseAttachment {
public:
	virtual ~AttachmentTyped() = default;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &queue) override {
		if (this->_frameHandleCallback) {
			return this->_frameHandleCallback(*this, queue);
		}
		return Rc<HandleType>::create(*this, queue);
	}
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREATTACHMENT_H_ */
