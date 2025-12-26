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

#ifndef XENOLITH_CORE_XLCOREQUEUEDATA_H_
#define XENOLITH_CORE_XLCOREQUEUEDATA_H_

#include "XLCorePipelineInfo.h"
#include "XLCoreResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Instance;
class Queue;
class QueuePass;
class QueuePassHandle;
class QueuePassBuilder;
class Device;
class Attachment;
class AttachmentHandle;
class AttachmentBuilder;
class RenderPass;
class Shader;
class Resource;
class ComputePipeline;
class GraphicPipeline;
class FrameRequest;
class FrameQueue;
class FrameCache;
class FrameHandle;
class FrameEmitter;
class ImageStorage;
class ImageView;
class Framebuffer;
class Semaphore;
class Loop;

struct SubpassData;
struct PipelineLayoutData;
struct DescriptorSetData;
struct AttachmentPassData;
struct AttachmentData;
struct QueuePassData;
struct QueueData;
struct PipelineFamilyInfo;

struct AttachmentInputData;

struct FramePassData;
struct FrameAttachmentData;
struct FrameSync;
struct FrameOutputBinding;

struct ImageData;
struct BufferData;

struct SP_PUBLIC ProgramDescriptorBinding {
	uint32_t set = 0;
	uint32_t descriptor = 0;
	DescriptorType type = DescriptorType::Unknown;
	uint32_t count = 0;
};

struct SP_PUBLIC ProgramPushConstantBlock {
	uint32_t offset = 0;
	uint32_t size = 0;
};

struct SP_PUBLIC ProgramEntryPointBlock {
	uint32_t id;
	memory::string name;
	uint32_t localX;
	uint32_t localY;
	uint32_t localZ;
};

struct SP_PUBLIC ProgramInfo : NamedMem {
	ProgramStage stage;
	memory::vector<ProgramDescriptorBinding> bindings;
	memory::vector<ProgramPushConstantBlock> constants;
	memory::vector<ProgramEntryPointBlock> entryPoints;
};

struct SP_PUBLIC ProgramData : ProgramInfo {
	using DataCallback = memory::callback<void(SpanView<uint32_t>)>;

	SpanView<uint32_t> data;

	// Useful for conditional-loading against device capabilities
	// Device can be null for shader code inspection
	memory::function<void(Device &, const DataCallback &)> callback = nullptr;
	Rc<Shader> program; // GL implementation-dependent object

	void inspect(SpanView<uint32_t>);
};

struct SpecializationConstant {
	using ValueCallback =
			memory::function<SpecializationConstant(const Device &, const PipelineLayoutData &)>;

	enum Type {
		Int,
		Float,
		Callback
	};

	Type type = Int;
	union {
		int intValue;
		float floatValue;
		ValueCallback *function;
	};

	SpecializationConstant(int val) : type(Int), intValue(val) { }
	SpecializationConstant(uint32_t val) : type(Int), intValue(val) { }
	SpecializationConstant(float val) : type(Float), floatValue(val) { }
	SpecializationConstant(ValueCallback &&cb) : type(Callback) {
		auto pool = memory::pool::acquire();
		memory::perform([&] { function = new (pool) ValueCallback(move(cb)); }, pool);
	}
};

struct SP_PUBLIC SpecializationInfo {
	const ProgramData *data = nullptr;
	memory::vector<SpecializationConstant> constants;

	SpecializationInfo() = default;
	SpecializationInfo(const ProgramData *program);
	SpecializationInfo(const ProgramData *program, SpanView<SpecializationConstant> constants);
};

struct SP_PUBLIC GraphicPipelineInfo : NamedMem {
	memory::vector<SpecializationInfo> shaders;
	DynamicState dynamicState = DynamicState::Default;
	PipelineMaterialInfo material;
	const SubpassData *subpass = nullptr;
	const PipelineLayoutData *layout = nullptr;
	const PipelineFamilyInfo *family = nullptr;

	bool isSolid() const;
};

struct SP_PUBLIC GraphicPipelineData : GraphicPipelineInfo {
	Rc<GraphicPipeline> pipeline; // GL implementation-dependent object
};

struct SP_PUBLIC ComputePipelineInfo : NamedMem {
	SpecializationInfo shader;
	const SubpassData *subpass = nullptr;
	const PipelineLayoutData *layout = nullptr;
	const PipelineFamilyInfo *family = nullptr;
};

struct SP_PUBLIC ComputePipelineData : ComputePipelineInfo {
	Rc<ComputePipeline> pipeline; // GL implementation-dependent object
};

struct SP_PUBLIC PipelineFamilyInfo : NamedMem {
	const PipelineLayoutData *layout = nullptr;
};

struct SP_PUBLIC PipelineFamilyData : PipelineFamilyInfo {
	memory::vector<const GraphicPipelineData *> graphicPipelines;
	memory::vector<const ComputePipelineData *> computePipelines;
};

struct SP_PUBLIC PipelineDescriptor : NamedMem {
	const DescriptorSetData *set = nullptr;
	const AttachmentPassData *attachment = nullptr;
	DescriptorType type = DescriptorType::Unknown;
	ProgramStage stages = ProgramStage::None;
	AttachmentLayout layout = AttachmentLayout::Ignored;
	uint32_t count = 1;
	uint32_t index = maxOf<uint32_t>();

	// note that UpdateAfterBind requested by default, engine uses it to optimize command buffer setup,
	// executing buffer write and descriptors write in separate threads
	DescriptorFlags requestFlags = DescriptorFlags::UpdateAfterBind; // what was requested
	DescriptorFlags deviceFlags = DescriptorFlags::None; // what device supports
	mutable uint64_t boundGeneration = 0;
};

struct SP_PUBLIC SubpassDependency {
	static constexpr uint32_t External = maxOf<uint32_t>();

	uint32_t srcSubpass;
	PipelineStage srcStage;
	AccessType srcAccess;
	uint32_t dstSubpass;
	PipelineStage dstStage;
	AccessType dstAccess;
	bool byRegion;

	uint64_t value() const { return uint64_t(srcSubpass) << 32 | uint64_t(dstSubpass); }
};

inline bool operator<(const SubpassDependency &l, const SubpassDependency &r) {
	return l.value() < r.value();
}
inline bool operator==(const SubpassDependency &l, const SubpassDependency &r) {
	return l.value() == r.value();
}
inline bool operator!=(const SubpassDependency &l, const SubpassDependency &r) {
	return l.value() != r.value();
}

struct SP_PUBLIC AttachmentDependencyInfo {
	// when and how attachment will be used for a first time within renderpass/subpass
	PipelineStage initialUsageStage = PipelineStage::None;
	AccessType initialAccessMask = AccessType::None;

	// when and how attachment will be used for a last time within renderpass/subpass
	PipelineStage finalUsageStage = PipelineStage::None;
	AccessType finalAccessMask = AccessType::None;

	// FrameRenderPassState, after which attachment can be used by next renderpass
	// Or Initial if no dependencies
	FrameRenderPassState requiredRenderPassState = FrameRenderPassState::Initial;

	// FrameRenderPassState that can be processed before attachment is acquired
	FrameRenderPassState lockedRenderPassState = FrameRenderPassState::Initial;

	static AttachmentDependencyInfo make(PipelineStage stage, AccessType type) {
		return AttachmentDependencyInfo{stage, type, stage, type};
	}
};

struct SP_PUBLIC AttachmentSubpassData : NamedMem {
	const AttachmentPassData *pass = nullptr;
	const SubpassData *subpass = nullptr;
	AttachmentLayout layout = AttachmentLayout::Ignored;
	AttachmentUsage usage = AttachmentUsage::None;
	AttachmentOps ops = AttachmentOps::Undefined;
	AttachmentDependencyInfo dependency;
	BlendInfo blendInfo;
};

struct SP_PUBLIC AttachmentPassData : NamedMem {
	const AttachmentData *attachment = nullptr;
	const QueuePassData *pass = nullptr;

	mutable uint32_t index = maxOf<uint32_t>();

	AttachmentOps ops = AttachmentOps::Undefined;

	// calculated initial layout
	// for first descriptor in execution chain - initial layout of queue's attachment or first usage layout
	// for others - final layout of previous descriptor in chain of execution
	AttachmentLayout initialLayout = AttachmentLayout::Ignored;

	// calculated final layout
	// for last descriptor in execution chain - final layout of queue's attachment or last usage layout
	// for others - last usage layout
	AttachmentLayout finalLayout = AttachmentLayout::Ignored;

	AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare;
	AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::DontCare;

	ColorMode colorMode;
	AttachmentDependencyInfo dependency;

	memory::vector<PipelineDescriptor *> descriptors;
	memory::vector<AttachmentSubpassData *> subpasses;
};

struct SP_PUBLIC AttachmentData : NamedMem {
	using InputAcquisitionCallback =
			memory::function<void(FrameQueue &, AttachmentHandle &, Function<void(bool)> &&)>;

	using InputSubmissionCallback = memory::function<void(FrameQueue &, AttachmentHandle &,
			AttachmentInputData *, Function<void(bool)> &&)>;

	using InputValidationCallback = memory::function<bool(const AttachmentInputData *)>;

	const QueueData *queue = nullptr;
	uint64_t id = 0;
	AttachmentOps ops = AttachmentOps::Undefined;
	AttachmentType type = AttachmentType::Image;
	AttachmentUsage usage = AttachmentUsage::None;
	FrameRenderPassState outputState = FrameRenderPassState::Submitted;
	memory::vector<AttachmentPassData *> passes;

	InputAcquisitionCallback inputAcquisitionCallback;
	InputSubmissionCallback inputSubmissionCallback;
	InputValidationCallback inputValidationCallback;

	Rc<Attachment> attachment;
	bool transient = false;
};

struct SP_PUBLIC DescriptorSetData : NamedMem {
	const PipelineLayoutData *layout = nullptr;
	uint32_t index = 0;
	memory::vector<PipelineDescriptor *> descriptors;
};

struct SP_PUBLIC PipelineLayoutData : NamedMem {
	const QueuePassData *pass = nullptr;
	uint32_t index = 0;

	const TextureSetLayoutData *textureSetLayout = nullptr;

	const PipelineFamilyData *defaultFamily = nullptr;

	memory::vector<DescriptorSetData *> sets;
	memory::vector<const PipelineFamilyData *> families;
	memory::vector<const GraphicPipelineData *> graphicPipelines;
	memory::vector<const ComputePipelineData *> computePipelines;
};

struct SP_PUBLIC SubpassData : NamedMem {
	SubpassData() = default;
	SubpassData(const SubpassData &) = default;
	SubpassData(SubpassData &&) = default;

	SubpassData &operator=(const SubpassData &) = delete;
	SubpassData &operator=(SubpassData &&) = delete;

	const QueuePassData *pass = nullptr;
	uint32_t index = 0;

	HashTable<GraphicPipelineData *> graphicPipelines;
	HashTable<ComputePipelineData *> computePipelines;

	memory::vector<const AttachmentSubpassData *> inputImages;
	memory::vector<const AttachmentSubpassData *> outputImages;
	memory::vector<const AttachmentSubpassData *> resolveImages;
	const AttachmentSubpassData *depthStencil = nullptr;
	mutable memory::vector<uint32_t> preserve;

	memory::function<void(FrameQueue &, const SubpassData &)> prepareCallback = nullptr;
	memory::function<void(FrameQueue &, const SubpassData &, CommandBuffer &)> commandsCallback =
			nullptr;
};

/** RenderOrdering defines order of execution for render passes between interdependent passes
 * if render passes is not interdependent, RenderOrdering can be used as an advice, or not used at all
 */
using RenderOrdering = ValueWrapper<uint32_t, class RenderOrderingFlag>;

static constexpr RenderOrdering RenderOrderingLowest = RenderOrdering::min();
static constexpr RenderOrdering RenderOrderingHighest = RenderOrdering::max();

struct SP_PUBLIC QueuePassRequirements {
	const QueuePassData *data = nullptr;
	FrameRenderPassState requiredState = FrameRenderPassState::Initial;
	FrameRenderPassState lockedState = FrameRenderPassState::Initial;

	QueuePassRequirements() = default;
	QueuePassRequirements(const QueuePassData &d, FrameRenderPassState required,
			FrameRenderPassState locked)
	: data(&d), requiredState(required), lockedState(locked) { }
};

struct SP_PUBLIC QueuePassDependency {
	const QueuePassData *source = nullptr;
	const QueuePassData *target = nullptr;
	memory::vector<const AttachmentData *> attachments;
	PipelineStage stageFlags = PipelineStage::None;
};

struct SP_PUBLIC QueuePassData : NamedMem {
	QueuePassData() = default;
	QueuePassData(const QueuePassData &) = default;
	QueuePassData(QueuePassData &&) = default;

	QueuePassData &operator=(const QueuePassData &) = delete;
	QueuePassData &operator=(QueuePassData &&) = delete;

	const QueueData *queue = nullptr;
	memory::vector<const AttachmentPassData *> attachments;
	memory::vector<const SubpassData *> subpasses;
	memory::vector<const PipelineLayoutData *> pipelineLayouts;
	memory::vector<SubpassDependency> dependencies;

	memory::vector<QueuePassDependency *> sourceQueueDependencies;
	memory::vector<QueuePassDependency *> targetQueueDependencies;

	memory::vector<QueuePassRequirements> required;

	PassType type = PassType::Graphics;
	RenderOrdering ordering = RenderOrderingLowest;
	bool hasUpdateAfterBind = false;
	uint32_t acquireTimestamps = 0;

	Rc<QueuePass> pass;
	Rc<RenderPass> impl;

	memory::function<bool(const FrameQueue &, const QueuePassData &)> checkAvailable;

	memory::vector<memory::function<void(FrameQueue &, const QueuePassData &, bool)>>
			submittedCallbacks;
	memory::vector<memory::function<void(FrameQueue &, const QueuePassData &, bool)>>
			completeCallbacks;
};

struct SP_PUBLIC QueueData : NamedMem {
	memory::pool_t *pool = nullptr;
	memory::vector<AttachmentData *> input;
	memory::vector<AttachmentData *> output;
	HashTable<AttachmentData *> attachments;
	HashTable<QueuePassData *> passes;
	HashTable<ProgramData *> programs;
	HashTable<GraphicPipelineData *> graphicPipelines;
	HashTable<ComputePipelineData *> computePipelines;
	HashTable<TextureSetLayoutData *> textureSets;
	HashTable<Rc<Resource>> linked;
	Function<void(FrameRequest &)> beginCallback;
	Function<void(FrameRequest &)> endCallback;
	Function<void(const FrameHandle *)> attachCallback;
	Function<void(const FrameHandle *)> detachCallback;
	Function<void()> releaseCallback;
	Rc<Resource> resource;
	bool compiled = false;
	uint64_t order = 0;
	Queue *queue = nullptr;
	FrameRenderPassState defaultSyncPassState = FrameRenderPassState::Submitted;

	memory::map<std::type_index, Attachment *> typedInput;
	memory::map<std::type_index, Attachment *> typedOutput;

	memory::vector<QueuePassDependency> passDependencies;

	const ImageData *emptyImage = nullptr;
	const ImageData *solidImage = nullptr;
	const BufferData *emptyBuffer = nullptr;

	void clear();
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREQUEUEDATA_H_ */
