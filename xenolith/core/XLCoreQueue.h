/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_CORE_XLCOREQUEUE_H_
#define XENOLITH_CORE_XLCOREQUEUE_H_

#include "XLCoreQueueData.h"
#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

/* RenderQueue/RenderGraph implementation notes:
 *
 * 	- RenderQueue
 * 		- Attachment - global/per-queue data
 * 			- AttachmentDescriptor - per-pass attachment data
 * 				- AttachmentRef - per-subpass attachment data
 * 			- AttachmentHandle - per-frame attachment data
 * 		- RenderPass
 * 			- AttachmentDescriptor - pass attachments
 * 			- RenderSubpass
 * 				- AttachmentRef - subpass attachments
 * 			- RenderSubpassDependency - dependency between subpasses
 * 			- RenderPassHandle - per-frame pass data
 */

class SP_PUBLIC Queue : public NamedRef {
public:
	using FrameRequest = core::FrameRequest;
	using FrameQueue = core::FrameQueue;
	using AttachmentHandle = core::AttachmentHandle;
	using FrameHandle = core::FrameHandle;
	using AttachmentData = core::AttachmentData;
	using AttachmentBuilder = core::AttachmentBuilder;

	class Builder;

	Queue();
	virtual ~Queue();

	virtual bool init(Builder &&);

	bool isCompiled() const;

	// mark queue as compiled for device with specific finalization
	void setCompiled(Device &, Function<void()> &&);

	bool isCompatible(const ImageInfo &) const;

	virtual StringView getName() const override;

	FrameRenderPassState getDefaultSyncPassState() const;

	const HashTable<ProgramData *> &getPrograms() const;
	const HashTable<QueuePassData *> &getPasses() const;
	const HashTable<GraphicPipelineData *> &getGraphicPipelines() const;
	const HashTable<ComputePipelineData *> &getComputePipelines() const;
	const HashTable<AttachmentData *> &getAttachments() const;
	const HashTable<TextureSetLayoutData *> &getTextureSetLayouts() const;
	const HashTable<Rc<Resource>> &getLinkedResources() const;
	Rc<Resource> getInternalResource() const;

	const memory::vector<AttachmentData *> &getInputAttachments() const;
	const memory::vector<AttachmentData *> &getOutputAttachments() const;

	template <typename T>
	auto getInputAttachment() const -> const T *;

	template <typename T>
	auto getOutputAttachment() const -> const T *;

	const Attachment *getInputAttachment(std::type_index name) const;
	const Attachment *getOutputAttachment(std::type_index name) const;

	const QueuePassData *getPass(StringView) const;
	const ProgramData *getProgram(StringView) const;
	const GraphicPipelineData *getGraphicPipeline(StringView) const;
	const ComputePipelineData *getComputePipeline(StringView) const;
	const AttachmentData *getAttachment(StringView) const;

	Vector<AttachmentData *> getOutput() const;
	Vector<AttachmentData *> getOutput(AttachmentType) const;
	AttachmentData *getPresentImageOutput() const;
	AttachmentData *getTransferImageOutput() const;

	// get next frame order dumber for this queue
	uint64_t incrementOrder();

	// Prepare queue to be used on target device
	bool prepare(Device &);

	void beginFrame(FrameRequest &);
	void endFrame(FrameRequest &);

	void attachFrame(FrameHandle *);
	void detachFrame(FrameHandle *);

	void describe(const Callback<void(StringView)> &);

protected:
	QueueData *_data = nullptr;
};

class SP_PUBLIC AttachmentBuilder final {
public:
	void setType(AttachmentType type);

	void defineAsInput(
			AttachmentOps ops = AttachmentOps::WritesColor | AttachmentOps::WritesStencil);
	void defineAsOutput(AttachmentOps ops = AttachmentOps::ReadColor | AttachmentOps::ReadStencil,
			FrameRenderPassState pass = FrameRenderPassState::Submitted);
	void defineAsOutput(FrameRenderPassState pass);

	void setInputAcquisitionCallback(AttachmentData::InputAcquisitionCallback &&);
	void setInputSubmissionCallback(AttachmentData::InputSubmissionCallback &&);
	void setInputValidationCallback(AttachmentData::InputValidationCallback &&);

	const AttachmentData *getAttachmentData() const { return _data; }

protected:
	friend class Queue::Builder;

	AttachmentBuilder(AttachmentData *);

	AttachmentData *_data = nullptr;
};

class SP_PUBLIC AttachmentPassBuilder final {
public:
	void setAttachmentOps(AttachmentOps);
	void setInitialLayout(AttachmentLayout);
	void setFinalLayout(AttachmentLayout);

	void setLoadOp(AttachmentLoadOp);
	void setStoreOp(AttachmentStoreOp);
	void setStencilLoadOp(AttachmentLoadOp);
	void setStencilStoreOp(AttachmentStoreOp);

	void setColorMode(const ColorMode &);
	void setDependency(const AttachmentDependencyInfo &);

protected:
	friend class QueuePassBuilder;

	AttachmentPassBuilder(AttachmentPassData *);

	AttachmentPassData *_data = nullptr;
};

class SP_PUBLIC DescriptorSetBuilder final {
public:
	// add single descriptor
	// compiler CAN inspect shaders to modify descriptors count, if descriptor is actually an array
	// if descriptor array size defined by spec constant - use addDescriptorArray instead
	// note: UpdateAfterBind flag is set up by default
	bool addDescriptor(const AttachmentPassData *, DescriptorType = DescriptorType::Unknown,
			AttachmentLayout = AttachmentLayout::Ignored);

	// add single descriptor with dedicated flags
	// Note: this will override default UpdateAfterBind flag
	// Please, specify it manually, if you have no specific intent to disable it
	bool addDescriptor(const AttachmentPassData *, DescriptorFlags,
			DescriptorType = DescriptorType::Unknown, AttachmentLayout = AttachmentLayout::Ignored);

	// add descriptor array with predefined descriptors count
	// compiler can not modify size of this array
	bool addDescriptorArray(const AttachmentPassData *, uint32_t count,
			DescriptorType = DescriptorType::Unknown, AttachmentLayout = AttachmentLayout::Ignored);

	// add descriptor array with predefined descriptors count with dedicated flags
	// compiler can not modify size of this array
	// Note: this will override default UpdateAfterBind flag
	// Please, specify it manually, if you have no specific intent to disable it
	bool addDescriptorArray(const AttachmentPassData *, uint32_t count, DescriptorFlags,
			DescriptorType = DescriptorType::Unknown, AttachmentLayout = AttachmentLayout::Ignored);

protected:
	friend class PipelineLayoutBuilder;

	DescriptorSetBuilder(DescriptorSetData *);

	DescriptorSetData *_data = nullptr;
};

class SP_PUBLIC PipelineLayoutBuilder final {
public:
	bool addSet(const Callback<void(DescriptorSetBuilder &)> &);
	void setTextureSetLayout(const TextureSetLayoutData *);

	const PipelineFamilyData *addPipelineFamily(StringView);

protected:
	friend class QueuePassBuilder;

	PipelineLayoutBuilder(PipelineLayoutData *);

	PipelineLayoutData *_data = nullptr;
};

class SP_PUBLIC SubpassBuilder final {
public:
	bool addColor(const AttachmentPassData *, AttachmentDependencyInfo,
			AttachmentLayout = AttachmentLayout::Ignored, AttachmentOps = AttachmentOps::Undefined,
			BlendInfo = BlendInfo());
	bool addColor(const AttachmentPassData *, AttachmentDependencyInfo, BlendInfo);
	bool addInput(const AttachmentPassData *, AttachmentDependencyInfo,
			AttachmentLayout = AttachmentLayout::Ignored, AttachmentOps = AttachmentOps::Undefined);

	bool addResolve(const AttachmentPassData *color, const AttachmentPassData *resolve,
			AttachmentDependencyInfo colorDep, AttachmentDependencyInfo resolveDep);

	bool setDepthStencil(const AttachmentPassData *, AttachmentDependencyInfo,
			AttachmentLayout = AttachmentLayout::Ignored, AttachmentOps = AttachmentOps::Undefined);

	template <typename... Args>
	const GraphicPipelineData *addGraphicPipeline(StringView key, const PipelineFamilyData *family,
			Args &&...args) {
		if (auto p = emplacePipeline(key, family)) {
			if (setPipelineOptions(*p, std::forward<Args>(args)...)) {
				finalizePipeline(p);
				return p;
			}
			erasePipeline(p);
		}
		return nullptr;
	}

	const ComputePipelineData *addComputePipeline(StringView key, const PipelineFamilyData *family,
			SpecializationInfo &&);

	void setPrepareCallback(memory::function<void(FrameQueue &, const SubpassData &)> &&);

	void setCommandsCallback(
			memory::function<void(FrameQueue &, const SubpassData &, CommandBuffer &)> &&);

protected:
	friend class QueuePassBuilder;

	GraphicPipelineData *emplacePipeline(StringView key, const PipelineFamilyData *);
	void finalizePipeline(GraphicPipelineData *);
	void erasePipeline(GraphicPipelineData *);

	bool setPipelineOption(GraphicPipelineData &f, DynamicState);
	bool setPipelineOption(GraphicPipelineData &f, const Vector<SpecializationInfo> &);
	bool setPipelineOption(GraphicPipelineData &f, const PipelineMaterialInfo &);

	template <typename T>
	bool setPipelineOptions(GraphicPipelineData &f, T &&t) {
		return setPipelineOption(f, std::forward<T>(t));
	}

	template <typename T, typename... Args>
	bool setPipelineOptions(GraphicPipelineData &f, T &&t, Args &&...args) {
		if (!setPipelineOption(f, std::forward<T>(t))) {
			return false;
		}
		return setPipelineOptions(f, std::forward<Args>(args)...);
	}

	SubpassBuilder(SubpassData *);

	SubpassData *_data;
};

class SP_PUBLIC QueuePassBuilder final {
public:
	const PipelineLayoutData *addDescriptorLayout(StringView,
			const Callback<void(PipelineLayoutBuilder &)> &);
	const PipelineLayoutData *addDescriptorLayout(const Callback<void(PipelineLayoutBuilder &)> &);

	const SubpassData *addSubpass(const Callback<void(SubpassBuilder &)> &);

	bool addSubpassDependency(const SubpassData *src, PipelineStage srcStage, AccessType srcAccess,
			const SubpassData *dst, PipelineStage dstStage, AccessType dstAccess,
			bool byRegion = true);

	const AttachmentPassData *addAttachment(const AttachmentData *);
	const AttachmentPassData *addAttachment(const AttachmentData *,
			const AttachmentDependencyInfo &);
	const AttachmentPassData *addAttachment(const AttachmentData *,
			const Callback<void(AttachmentPassBuilder &)> &);

	void setAvailabilityChecker(
			memory::function<bool(const FrameQueue &, const QueuePassData &)> &&);

	void addSubmittedCallback(
			memory::function<void(FrameQueue &, const QueuePassData &, bool success)> &&);
	void addCompleteCallback(
			memory::function<void(FrameQueue &, const QueuePassData &, bool success)> &&);

	void setAcquireTimestamps(uint32_t);

	StringView getName() const;

protected:
	friend class Queue::Builder;
	friend class QueuePass;

	QueuePassData *getData() const { return _data; }

	QueuePassBuilder(QueuePassData *);

	QueuePassData *_data = nullptr;
};

class SP_PUBLIC Queue::Builder final {
public:
	Builder(StringView);
	~Builder();

	void setDefaultSyncPassState(FrameRenderPassState);

	const AttachmentData *addAttachemnt(StringView name,
			const Callback<Rc<Attachment>(AttachmentBuilder &)> &);

	const QueuePassData *addPass(StringView name, PassType, RenderOrdering,
			const Callback<Rc<QueuePass>(QueuePassBuilder &)> &);

	// add program, copy all data
	const ProgramData *addProgram(StringView key, SpanView<uint32_t>,
			const ProgramInfo * = nullptr);

	// add program, take shader data by ref, data should exists for all resource lifetime
	const ProgramData *addProgramByRef(StringView key, SpanView<uint32_t>,
			const ProgramInfo * = nullptr);

	// add program, data will be acquired with callback when needed
	const ProgramData *addProgram(StringView key,
			const memory::function<void(Device &, const ProgramData::DataCallback &)> &,
			const ProgramInfo * = nullptr);

	const TextureSetLayoutData *addTextureSetLayout(StringView key, SpanView<SamplerInfo>,
			uint32_t images = config::MaxTextureSetImages,
			uint32_t buffers = config::MaxBufferArrayObjects,
			uint32_t imagesIndexed = config::MaxTextureSetImagesIndexed,
			uint32_t buffersIndexed = config::MaxBufferArrayObjectsIndexed);

	// external resources, that should be compiled when added
	void addLinkedResource(const Rc<Resource> &);

	void setBeginCallback(Function<void(FrameRequest &)> &&);
	void setEndCallback(Function<void(FrameRequest &)> &&);

	void setAttachCallback(Function<void(const FrameHandle *)> &&);
	void setDetachCallback(Function<void(const FrameHandle *)> &&);

	const BufferData *addBufferByRef(StringView key, BufferInfo &&, BytesView data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData *addBuffer(StringView key, BufferInfo &&, const FileInfo &data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData *addBuffer(StringView key, BufferInfo &&, BytesView data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData *addBuffer(StringView key, BufferInfo &&,
			const memory::function<void(uint8_t *, uint64_t, const BufferData::DataCallback &)> &cb,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);

	const ImageData *addBitmapImageByRef(StringView key, ImageInfo &&, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);
	const ImageData *addBitmapImage(StringView key, ImageInfo &&img, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);
	const ImageData *addEncodedImageByRef(StringView key, ImageInfo &&, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);
	const ImageData *addEncodedImage(StringView key, ImageInfo &&img, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);
	const ImageData *addImage(StringView key, ImageInfo &&img, const FileInfo &data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);
	const ImageData *addImage(StringView key, ImageInfo &&img,
			const memory::function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &cb,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			AccessType = AccessType::ShaderRead);

	const ImageViewData *addImageView(const ImageData *, ImageViewInfo &&);

protected:
	friend class Queue;

	QueueData *_data = nullptr;
	Resource::Builder _internalResource;
};


template <typename T>
inline auto Queue::getInputAttachment() const -> const T * {
	if (auto c = getInputAttachment(std::type_index(typeid(T)))) {
		return static_cast<const T *>(c);
	}

	return nullptr;
}

template <typename T>
inline auto Queue::getOutputAttachment() const -> const T * {
	if (auto c = getOutputAttachment(std::type_index(typeid(T)))) {
		return static_cast<const T *>(c);
	}

	return nullptr;
}

} // namespace stappler::xenolith::core

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEQUEUE_H_ */
