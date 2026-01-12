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

#ifndef XENOLITH_CORE_XLCOREENUM_H_
#define XENOLITH_CORE_XLCOREENUM_H_

#include "SPCommon.h" // IWYU pragma: keep
#include "SPStringView.h"

#include <sprt/runtime/window/surface_info.h>
#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

enum class FrameRenderPassState {
	Initial,
	Ready,
	ResourcesAcquired,
	Prepared,
	Submission,
	Submitted,
	Complete,
	Finalized,
};

enum class FrameAttachmentState {
	Initial,
	Setup,
	InputRequired,
	Ready,
	ResourcesPending,
	ResourcesAcquired,
	Detached, // resource ownership transferred out of Frame
	Complete,
	ResourcesReleased,
	Finalized,
};

enum class AttachmentType {
	Image,
	Buffer,
	Generic
};

// VkPipelineStageFlagBits
enum class PipelineStage : uint32_t {
	None,
	TopOfPipe = 0x0000'0001,
	DrawIndirect = 0x0000'0002,
	VertexInput = 0x0000'0004,
	VertexShader = 0x0000'0008,
	TesselationControl = 0x0000'0010,
	TesselationEvaluation = 0x0000'0020,
	GeometryShader = 0x0000'0040,
	FragmentShader = 0x0000'0080,
	EarlyFragmentTest = 0x0000'0100,
	LateFragmentTest = 0x0000'0200,
	ColorAttachmentOutput = 0x0000'0400,
	ComputeShader = 0x0000'0800,
	Transfer = 0x0000'1000,
	BottomOfPipe = 0x0000'2000,
	Host = 0x0000'4000,
	AllGraphics = 0x0000'8000,
	AllCommands = 0x0001'0000,
	TransformFeedback = 0x0100'0000,
	ConditionalRendering = 0x0004'0000,
	AccelerationStructureBuild = 0x0200'0000,
	RayTracingShader = 0x0020'0000,
	ShadingRateImage = 0x0040'0000,
	TaskShader = 0x0008'0000,
	MeshShader = 0x0010'0000,
	FragmentDensityProcess = 0x0080'0000,
	CommandPreprocess = 0x0002'0000,
};

SP_DEFINE_ENUM_AS_MASK(PipelineStage);

// VkAccessFlag
enum class AccessType : uint32_t {
	None,
	IndirectCommandRead = 0x0000'0001,
	IndexRead = 0x0000'0002,
	VertexAttributeRead = 0x0000'0004,
	UniformRead = 0x0000'0008,
	InputAttachmantRead = 0x0000'0010,
	ShaderRead = 0x0000'0020,
	ShaderWrite = 0x0000'0040,
	ColorAttachmentRead = 0x0000'0080,
	ColorAttachmentWrite = 0x0000'0100,
	DepthStencilAttachmentRead = 0x0000'0200,
	DepthStencilAttachmentWrite = 0x0000'0400,
	TransferRead = 0x0000'0800,
	TransferWrite = 0x0000'1000,
	HostRead = 0x0000'2000,
	HostWrite = 0x0000'4000,
	MemoryRead = 0x0000'8000,
	MemoryWrite = 0x0001'0000,
	TransformFeedbackWrite = 0x0200'0000,
	TransformFeedbackCounterRead = 0x0400'0000,
	TransformFeedbackCounterWrite = 0x0800'0000,
	ConditionalRenderingRead = 0x0010'0000,
	ColorAttachmentReadNonCoherent = 0x0008'0000,
	AccelerationStructureRead = 0x0020'0000,
	AccelerationStructureWrite = 0x0040'0000,
	ShadingRateImageRead = 0x0080'0000,
	FragmentDensityMapRead = 0x0100'0000,
	CommandPreprocessRead = 0x0002'0000,
	CommandPreprocessWrite = 0x0004'0000,
};

SP_DEFINE_ENUM_AS_MASK(AccessType);

// read-write operations on attachment within passes
enum class AttachmentOps : uint32_t {
	Undefined,
	ReadColor = 1,
	ReadStencil = 2,
	WritesColor = 4,
	WritesStencil = 8
};

SP_DEFINE_ENUM_AS_MASK(AttachmentOps);

// VkAttachmentLoadOp
enum class AttachmentLoadOp {
	Load = 0,
	Clear = 1,
	DontCare = 2,
};

// VkAttachmentStoreOp
enum class AttachmentStoreOp {
	Store = 0,
	DontCare = 1,
};

// Attachment usage within subpasses
enum class AttachmentUsage : uint32_t {
	None,
	Input = 1,
	Output = 2,
	InputOutput = Input | Output,
	Resolve = 4,
	DepthStencil = 8,
	InputDepthStencil = Input | DepthStencil
};

SP_DEFINE_ENUM_AS_MASK(AttachmentUsage);

// VkDescriptorType
enum class DescriptorType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	StorageImage = 3,
	UniformTexelBuffer = 4,
	StorageTexelBuffer = 5,
	UniformBuffer = 6,
	StorageBuffer = 7,
	UniformBufferDynamic = 8,
	StorageBufferDynamic = 9,
	InputAttachment = 10,
	Attachment = maxOf<uint32_t>() - 1,
	Unknown = maxOf<uint32_t>()
};

// mapping to VkShaderStageFlagBits
enum class ProgramStage : uint32_t {
	None,
	Vertex = 0x0000'0001,
	TesselationControl = 0x0000'0002,
	TesselationEvaluation = 0x0000'0004,
	Geometry = 0x0000'0008,
	Fragment = 0x0000'0010,
	Compute = 0x0000'0020,
	RayGen = 0x0000'0100,
	AnyHit = 0x0000'0200,
	ClosestHit = 0x0000'0400,
	MissHit = 0x0000'0800,
	Intersection = 0x0000'1000,
	Callable = 0x0000'2000,
	Task = 0x0000'0040,
	Mesh = 0x0000'0080,
};

SP_DEFINE_ENUM_AS_MASK(ProgramStage);

// mapping to VkImageLayout
enum class AttachmentLayout : uint32_t {
	Undefined = 0,
	General = 1,
	ColorAttachmentOptimal = 2,
	DepthStencilAttachmentOptimal = 3,
	DepthStencilReadOnlyOptimal = 4,
	ShaderReadOnlyOptimal = 5,
	TransferSrcOptimal = 6,
	TransferDstOptimal = 7,
	Preinitialized = 8,
	DepthReadOnlyStencilAttachmentOptimal = 1'000'117'000,
	DepthAttachmentStencilReadOnlyOptimal = 1'000'117'001,
	DepthAttachmentOptimal = 1'000'241'000,
	DepthReadOnlyOptimal = 1'000'241'001,
	StencilAttachmentOptimal = 1'000'241'002,
	StencilReadOnlyOptimal = 1'000'241'003,
	PresentSrc = 1'000'001'002,
	Ignored = maxOf<uint32_t>()
};

enum class PassType {
	Graphics,
	Compute,
	Transfer,
	Generic
};

enum class DynamicState : uint32_t {
	None,
	Viewport = 1,
	Scissor = 2,

	Default = Viewport | Scissor,
};

SP_DEFINE_ENUM_AS_MASK(DynamicState)

// Mapping to VkBufferCreateFlagBits
enum class BufferFlags : uint32_t {
	None,
	SparceBinding = 0x0000'0001,
	SparceResidency = 0x0000'0002,
	SparceAliased = 0x0000'0004,
	Protected = 0x0000'0008,
};

SP_DEFINE_ENUM_AS_MASK(BufferFlags)


// Mapping to VkBufferUsageFlagBits
enum class BufferUsage : uint32_t {
	None,
	TransferSrc = 0x0000'0001,
	TransferDst = 0x0000'0002,
	UniformTexelBuffer = 0x0000'0004,
	StorageTexelBuffer = 0x0000'0008,
	UniformBuffer = 0x0000'0010,
	StorageBuffer = 0x0000'0020,
	IndexBuffer = 0x0000'0040,
	VertexBuffer = 0x0000'0080,
	IndirectBuffer = 0x0000'0100,
	ShaderDeviceAddress = 0x0002'0000,

	TransformFeedback = 0x0000'0800,
	TransformFeedbackCounter = 0x0000'1000,
	ConditionalRendering = 0x0000'0200,
	AccelerationStructureBuildInputReadOnly = 0x0008'0000,
	AccelerationStructureStorage = 0x0010'0000,
	ShaderBindingTable = 0x0000'0400,
};

SP_DEFINE_ENUM_AS_MASK(BufferUsage)


// Mapping to VkImageCreateFlagBits
enum class ImageFlags : uint32_t {
	None,
	SparceBinding = 0x0000'0001,
	SparceResidency = 0x0000'0002,
	SparceAliased = 0x0000'0004,
	MutableFormat = 0x0000'0008,
	CubeCompatible = 0x0000'0010,
	Alias = 0x0000'0400,
	SplitInstanceBindRegions = 0x0000'0040,
	Array2dCompatible = 0x0000'0020,
	BlockTexelViewCompatible = 0x0000'0080,
	ExtendedUsage = 0x0000'0100,
	Protected = 0x0000'0800,
	Disjoint = 0x0000'0200,
};

SP_DEFINE_ENUM_AS_MASK(ImageFlags)


// Mapping to VkSampleCountFlagBits
enum class SampleCount : uint32_t {
	None,
	X1 = 0x0000'0001,
	X2 = 0x0000'0002,
	X4 = 0x0000'0004,
	X8 = 0x0000'0008,
	X16 = 0x0000'0010,
	X32 = 0x0000'0020,
	X64 = 0x0000'0040,
};

SP_DEFINE_ENUM_AS_MASK(SampleCount)


// Mapping to VkImageType
enum class ImageType {
	Image1D = 0,
	Image2D = 1,
	Image3D = 2,
};

// Mapping to VkImageViewType
enum class ImageViewType {
	ImageView1D = 0,
	ImageView2D = 1,
	ImageView3D = 2,
	ImageViewCube = 3,
	ImageView1DArray = 4,
	ImageView2DArray = 5,
	ImageViewCubeArray = 6,
};

using sprt::window::ImageFormat;
using sprt::window::ColorSpace;
using sprt::window::CompositeAlphaFlags;
using sprt::window::FullScreenExclusiveMode;
using sprt::window::ImageUsage;

// mapping to VkImageTiling
enum class ImageTiling {
	Optimal = 0,
	Linear = 1,
};

// VkImageAspectFlagBits
enum class ImageAspects : uint32_t {
	None,
	Color = 0x0000'0001,
	Depth = 0x0000'0002,
	Stencil = 0x0000'0004,
	Metadata = 0x0000'0008,
	Plane0 = 0x0000'0010,
	Plane1 = 0x0000'0020,
	Plane2 = 0x0000'0040,
};

SP_DEFINE_ENUM_AS_MASK(ImageAspects);

using sprt::window::PresentMode;

enum class AttachmentStorageType {
	// Implementation-defined transient memory storage (if supported)
	Transient,

	// Attachment data stored in per-frame memory
	FrameStateless,

	// Attachment stored in independent, but persistent memory
	ObjectStateless,

	// Attachment has a persistent state
	Stateful,
};

enum class ImageHints : uint32_t {
	None = 0,
	Opaque = 1 << 0,
	FixedSize = 1 << 1,
	DoNotCache = 1 << 2,
	ReadOnly = 1 << 3,
	Static = FixedSize | DoNotCache | ReadOnly
};

SP_DEFINE_ENUM_AS_MASK(ImageHints);

// VkComponentSwizzle
enum class ComponentMapping : uint32_t {
	Identity = 0,
	Zero = 1,
	One = 2,
	R = 3,
	G = 4,
	B = 5,
	A = 6,
};

// VkFilter
enum class Filter {
	Nearest = 0,
	Linear = 1,
	Cubic = 1'000'015'000
};

// VkSamplerMipmapMode
enum class SamplerMipmapMode {
	Nearest = 0,
	Linear = 1,
};

// VkSamplerAddressMode
enum class SamplerAddressMode {
	Repeat = 0,
	MirroredRepeat = 1,
	ClampToEdge = 2,
	ClampToBorder = 3,
};

// VkCompareOp
enum class CompareOp : uint8_t {
	Never = 0,
	Less = 1,
	Equal = 2,
	LessOrEqual = 3,
	Greater = 4,
	NotEqual = 5,
	GreaterOrEqual = 6,
	Always = 7,
};

enum class BlendFactor : uint8_t {
	Zero = 0,
	One = 1,
	SrcColor = 2,
	OneMinusSrcColor = 3,
	DstColor = 4,
	OneMinusDstColor = 5,
	SrcAlpha = 6,
	OneMinusSrcAlpha = 7,
	DstAlpha = 8,
	OneMinusDstAlpha = 9,
};

enum class BlendOp : uint8_t {
	Add = 0,
	Subtract = 1,
	ReverseSubtract = 2,
	Min = 3,
	Max = 4,
};

enum class ColorComponentFlags : uint32_t {
	R = 0x0000'0001,
	G = 0x0000'0002,
	B = 0x0000'0004,
	A = 0x0000'0008,
	All = 0x0000'000F
};

SP_DEFINE_ENUM_AS_MASK(ColorComponentFlags)

enum class StencilOp : uint8_t {
	Keep = 0,
	Zero = 1,
	Replace = 2,
	IncrementAndClamp = 3,
	DecrementAndClamp = 4,
	Invert = 5,
	InvertAndWrap = 6,
	DecrementAndWrap = 7,
};

using sprt::window::SurfaceTransformFlags;
using sprt::window::getPureTransform;

enum class RenderingLevel {
	Default,
	Solid,
	Surface,
	Transparent
};

enum class ObjectType {
	Unknown,
	Buffer,
	BufferView,
	CommandPool,
	DescriptorPool,
	DescriptorSetLayout,
	Event,
	Fence,
	Framebuffer,
	Image,
	ImageView,
	Pipeline,
	PipelineCache,
	PipelineLayout,
	QueryPool,
	RenderPass,
	Sampler,
	Semaphore,
	ShaderModule,
	DeviceMemory,
	Surface,
	Swapchain
};

enum class PixelFormat {
	Unknown,
	A, // single-channel color
	IA, // dual-channel color
	RGB,
	RGBA,
	D, // depth
	DS, // depth-stencil
	S // stencil
};

// VkQueryType
enum class QueryType : uint32_t {
	Occlusion = 0,
	PipelineStatistics = 1,
	Timestamp = 2,
};

// VkQueryPipelineStatisticFlagBits
enum class QueryPipelineStatisticFlags : uint32_t {
	None = 0,
	InputAssemblyVertices = 0x0000'0001,
	InputAssemblyPrimitives = 0x0000'0002,
	VertexShaderInvocations = 0x0000'0004,
	GeometryShaderInvocations = 0x0000'0008,
	GeometryShaderPrimitives = 0x0000'0010,
	ClippingInvocations = 0x0000'0020,
	ClippingPrimitives = 0x0000'0040,
	FragmentShaderInvocations = 0x0000'0080,
	TesselationControlShaderPatches = 0x0000'0100,
	TesselationEvaluationShaderInvocations = 0x0000'0200,
	ComputeShaderInvocations = 0x0000'0400,
};

SP_DEFINE_ENUM_AS_MASK(QueryPipelineStatisticFlags)

// VkQueueFlagBits
enum class QueueFlags : uint32_t {
	None,
	Graphics = 1 << 0,
	Compute = 1 << 1,
	Transfer = 1 << 2,
	SparceBinding = 1 << 3,
	Protected = 1 << 4,
	VideoDecode = 1 << 5,
	VideoEncode = 1 << 6,
	Present = 0x8000'0000,
};

SP_DEFINE_ENUM_AS_MASK(QueueFlags)

enum class DeviceIdleFlags : uint32_t {
	None,
	PreQueue = 1 << 0,
	PreDevice = 1 << 1,
	PostQueue = 1 << 2,
	PostDevice = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(DeviceIdleFlags)

/** Flags for GPU resource descriptors capabilities */
enum class DescriptorFlags : uint32_t {
	None = 0,
	UpdateAfterBind = 0x0000'0001,
	UpdateWhilePending = 0x0000'0002,
	PartiallyBound = 0x0000'0004,
	VariableDescriptorCount = 0x0000'0008, // not implemented for now

	// Extra engine flags
	RuntimeDescriptorArray = 0x0800'0000,
	PredefinedCount = 0x1000'0000,
	DynamicIndexing = 0x2000'0000,
	NonUniformIndexing = 0x4000'0000,
	NonUniformIndexingNative = 0x8000'0000U,
};

SP_DEFINE_ENUM_AS_MASK(DescriptorFlags)

/** View tiling constraints flags. If flag is set - window enge is constrained, if not set - resizable  */
enum class ViewConstraints : uint32_t {
	None,
	Top = 1 << 0,
	Left = 1 << 1,
	Bottom = 1 << 2,
	Right = 1 << 3,

	Vertical = Top | Bottom,
	Horizontal = Left | Right,
	All = Vertical | Horizontal
};

SP_DEFINE_ENUM_AS_MASK(ViewConstraints)

using sprt::window::WindowState;

static inline ViewConstraints getViewConstraints(WindowState state) {
	ViewConstraints ct = ViewConstraints::None;
	if (hasFlag(state, WindowState::TiledLeft)) {
		ct |= ViewConstraints::Left;
	}
	if (hasFlag(state, WindowState::TiledTop)) {
		ct |= ViewConstraints::Top;
	}
	if (hasFlag(state, WindowState::TiledRight)) {
		ct |= ViewConstraints::Right;
	}
	if (hasFlag(state, WindowState::TiledBottom)) {
		ct |= ViewConstraints::Bottom;
	}
	return ct;
}

enum class FenceType {
	Default,
	Swapchain,
};

enum class SemaphoreType {
	Default,
	Timeline,
};

SP_PUBLIC PipelineStage getStagesForQueue(QueueFlags);

SP_PUBLIC StringView getDescriptorTypeName(DescriptorType);
SP_PUBLIC void getProgramStageDescription(const CallbackStream &, ProgramStage fmt);

inline const CallbackStream &operator<<(const CallbackStream &stream, DescriptorType t) {
	stream << xenolith::core::getDescriptorTypeName(t);
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, DescriptorType t) {
	stream << xenolith::core::getDescriptorTypeName(t);
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, ProgramStage t) {
	getProgramStageDescription(memory::makeCallback(stream), t);
	return stream;
}

} // namespace stappler::xenolith::core

namespace sprt::window {

inline std::ostream &operator<<(std::ostream &stream, WindowState f) {
	STAPPLER_VERSIONIZED_NAMESPACE::memory::makeCallback(stream) << f;
	return stream;
}

} // namespace sprt::window

#endif /* XENOLITH_CORE_XLCOREENUM_H_ */
