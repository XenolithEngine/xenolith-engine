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

// Mapping to VkFormat
enum class ImageFormat {
	Undefined = 0,
	R4G4_UNORM_PACK8 = 1,
	R4G4B4A4_UNORM_PACK16 = 2,
	B4G4R4A4_UNORM_PACK16 = 3,
	R5G6B5_UNORM_PACK16 = 4,
	B5G6R5_UNORM_PACK16 = 5,
	R5G5B5A1_UNORM_PACK16 = 6,
	B5G5R5A1_UNORM_PACK16 = 7,
	A1R5G5B5_UNORM_PACK16 = 8,
	R8_UNORM = 9,
	R8_SNORM = 10,
	R8_USCALED = 11,
	R8_SSCALED = 12,
	R8_UINT = 13,
	R8_SINT = 14,
	R8_SRGB = 15,
	R8G8_UNORM = 16,
	R8G8_SNORM = 17,
	R8G8_USCALED = 18,
	R8G8_SSCALED = 19,
	R8G8_UINT = 20,
	R8G8_SINT = 21,
	R8G8_SRGB = 22,
	R8G8B8_UNORM = 23,
	R8G8B8_SNORM = 24,
	R8G8B8_USCALED = 25,
	R8G8B8_SSCALED = 26,
	R8G8B8_UINT = 27,
	R8G8B8_SINT = 28,
	R8G8B8_SRGB = 29,
	B8G8R8_UNORM = 30,
	B8G8R8_SNORM = 31,
	B8G8R8_USCALED = 32,
	B8G8R8_SSCALED = 33,
	B8G8R8_UINT = 34,
	B8G8R8_SINT = 35,
	B8G8R8_SRGB = 36,
	R8G8B8A8_UNORM = 37,
	R8G8B8A8_SNORM = 38,
	R8G8B8A8_USCALED = 39,
	R8G8B8A8_SSCALED = 40,
	R8G8B8A8_UINT = 41,
	R8G8B8A8_SINT = 42,
	R8G8B8A8_SRGB = 43,
	B8G8R8A8_UNORM = 44,
	B8G8R8A8_SNORM = 45,
	B8G8R8A8_USCALED = 46,
	B8G8R8A8_SSCALED = 47,
	B8G8R8A8_UINT = 48,
	B8G8R8A8_SINT = 49,
	B8G8R8A8_SRGB = 50,
	A8B8G8R8_UNORM_PACK32 = 51,
	A8B8G8R8_SNORM_PACK32 = 52,
	A8B8G8R8_USCALED_PACK32 = 53,
	A8B8G8R8_SSCALED_PACK32 = 54,
	A8B8G8R8_UINT_PACK32 = 55,
	A8B8G8R8_SINT_PACK32 = 56,
	A8B8G8R8_SRGB_PACK32 = 57,
	A2R10G10B10_UNORM_PACK32 = 58,
	A2R10G10B10_SNORM_PACK32 = 59,
	A2R10G10B10_USCALED_PACK32 = 60,
	A2R10G10B10_SSCALED_PACK32 = 61,
	A2R10G10B10_UINT_PACK32 = 62,
	A2R10G10B10_SINT_PACK32 = 63,
	A2B10G10R10_UNORM_PACK32 = 64,
	A2B10G10R10_SNORM_PACK32 = 65,
	A2B10G10R10_USCALED_PACK32 = 66,
	A2B10G10R10_SSCALED_PACK32 = 67,
	A2B10G10R10_UINT_PACK32 = 68,
	A2B10G10R10_SINT_PACK32 = 69,
	R16_UNORM = 70,
	R16_SNORM = 71,
	R16_USCALED = 72,
	R16_SSCALED = 73,
	R16_UINT = 74,
	R16_SINT = 75,
	R16_SFLOAT = 76,
	R16G16_UNORM = 77,
	R16G16_SNORM = 78,
	R16G16_USCALED = 79,
	R16G16_SSCALED = 80,
	R16G16_UINT = 81,
	R16G16_SINT = 82,
	R16G16_SFLOAT = 83,
	R16G16B16_UNORM = 84,
	R16G16B16_SNORM = 85,
	R16G16B16_USCALED = 86,
	R16G16B16_SSCALED = 87,
	R16G16B16_UINT = 88,
	R16G16B16_SINT = 89,
	R16G16B16_SFLOAT = 90,
	R16G16B16A16_UNORM = 91,
	R16G16B16A16_SNORM = 92,
	R16G16B16A16_USCALED = 93,
	R16G16B16A16_SSCALED = 94,
	R16G16B16A16_UINT = 95,
	R16G16B16A16_SINT = 96,
	R16G16B16A16_SFLOAT = 97,
	R32_UINT = 98,
	R32_SINT = 99,
	R32_SFLOAT = 100,
	R32G32_UINT = 101,
	R32G32_SINT = 102,
	R32G32_SFLOAT = 103,
	R32G32B32_UINT = 104,
	R32G32B32_SINT = 105,
	R32G32B32_SFLOAT = 106,
	R32G32B32A32_UINT = 107,
	R32G32B32A32_SINT = 108,
	R32G32B32A32_SFLOAT = 109,
	R64_UINT = 110,
	R64_SINT = 111,
	R64_SFLOAT = 112,
	R64G64_UINT = 113,
	R64G64_SINT = 114,
	R64G64_SFLOAT = 115,
	R64G64B64_UINT = 116,
	R64G64B64_SINT = 117,
	R64G64B64_SFLOAT = 118,
	R64G64B64A64_UINT = 119,
	R64G64B64A64_SINT = 120,
	R64G64B64A64_SFLOAT = 121,
	B10G11R11_UFLOAT_PACK32 = 122,
	E5B9G9R9_UFLOAT_PACK32 = 123,
	D16_UNORM = 124,
	X8_D24_UNORM_PACK32 = 125,
	D32_SFLOAT = 126,
	S8_UINT = 127,
	D16_UNORM_S8_UINT = 128,
	D24_UNORM_S8_UINT = 129,
	D32_SFLOAT_S8_UINT = 130,
	BC1_RGB_UNORM_BLOCK = 131,
	BC1_RGB_SRGB_BLOCK = 132,
	BC1_RGBA_UNORM_BLOCK = 133,
	BC1_RGBA_SRGB_BLOCK = 134,
	BC2_UNORM_BLOCK = 135,
	BC2_SRGB_BLOCK = 136,
	BC3_UNORM_BLOCK = 137,
	BC3_SRGB_BLOCK = 138,
	BC4_UNORM_BLOCK = 139,
	BC4_SNORM_BLOCK = 140,
	BC5_UNORM_BLOCK = 141,
	BC5_SNORM_BLOCK = 142,
	BC6H_UFLOAT_BLOCK = 143,
	BC6H_SFLOAT_BLOCK = 144,
	BC7_UNORM_BLOCK = 145,
	BC7_SRGB_BLOCK = 146,
	ETC2_R8G8B8_UNORM_BLOCK = 147,
	ETC2_R8G8B8_SRGB_BLOCK = 148,
	ETC2_R8G8B8A1_UNORM_BLOCK = 149,
	ETC2_R8G8B8A1_SRGB_BLOCK = 150,
	ETC2_R8G8B8A8_UNORM_BLOCK = 151,
	ETC2_R8G8B8A8_SRGB_BLOCK = 152,
	EAC_R11_UNORM_BLOCK = 153,
	EAC_R11_SNORM_BLOCK = 154,
	EAC_R11G11_UNORM_BLOCK = 155,
	EAC_R11G11_SNORM_BLOCK = 156,
	ASTC_4x4_UNORM_BLOCK = 157,
	ASTC_4x4_SRGB_BLOCK = 158,
	ASTC_5x4_UNORM_BLOCK = 159,
	ASTC_5x4_SRGB_BLOCK = 160,
	ASTC_5x5_UNORM_BLOCK = 161,
	ASTC_5x5_SRGB_BLOCK = 162,
	ASTC_6x5_UNORM_BLOCK = 163,
	ASTC_6x5_SRGB_BLOCK = 164,
	ASTC_6x6_UNORM_BLOCK = 165,
	ASTC_6x6_SRGB_BLOCK = 166,
	ASTC_8x5_UNORM_BLOCK = 167,
	ASTC_8x5_SRGB_BLOCK = 168,
	ASTC_8x6_UNORM_BLOCK = 169,
	ASTC_8x6_SRGB_BLOCK = 170,
	ASTC_8x8_UNORM_BLOCK = 171,
	ASTC_8x8_SRGB_BLOCK = 172,
	ASTC_10x5_UNORM_BLOCK = 173,
	ASTC_10x5_SRGB_BLOCK = 174,
	ASTC_10x6_UNORM_BLOCK = 175,
	ASTC_10x6_SRGB_BLOCK = 176,
	ASTC_10x8_UNORM_BLOCK = 177,
	ASTC_10x8_SRGB_BLOCK = 178,
	ASTC_10x10_UNORM_BLOCK = 179,
	ASTC_10x10_SRGB_BLOCK = 180,
	ASTC_12x10_UNORM_BLOCK = 181,
	ASTC_12x10_SRGB_BLOCK = 182,
	ASTC_12x12_UNORM_BLOCK = 183,
	ASTC_12x12_SRGB_BLOCK = 184,
	G8B8G8R8_422_UNORM = 1'000'156'000,
	B8G8R8G8_422_UNORM = 1'000'156'001,
	G8_B8_R8_3PLANE_420_UNORM = 1'000'156'002,
	G8_B8R8_2PLANE_420_UNORM = 1'000'156'003,
	G8_B8_R8_3PLANE_422_UNORM = 1'000'156'004,
	G8_B8R8_2PLANE_422_UNORM = 1'000'156'005,
	G8_B8_R8_3PLANE_444_UNORM = 1'000'156'006,
	R10X6_UNORM_PACK16 = 1'000'156'007,
	R10X6G10X6_UNORM_2PACK16 = 1'000'156'008,
	R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1'000'156'009,
	G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1'000'156'010,
	B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1'000'156'011,
	G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1'000'156'012,
	G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1'000'156'013,
	G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1'000'156'014,
	G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1'000'156'015,
	G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1'000'156'016,
	R12X4_UNORM_PACK16 = 1'000'156'017,
	R12X4G12X4_UNORM_2PACK16 = 1'000'156'018,
	R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1'000'156'019,
	G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1'000'156'020,
	B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1'000'156'021,
	G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1'000'156'022,
	G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1'000'156'023,
	G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1'000'156'024,
	G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1'000'156'025,
	G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1'000'156'026,
	G16B16G16R16_422_UNORM = 1'000'156'027,
	B16G16R16G16_422_UNORM = 1'000'156'028,
	G16_B16_R16_3PLANE_420_UNORM = 1'000'156'029,
	G16_B16R16_2PLANE_420_UNORM = 1'000'156'030,
	G16_B16_R16_3PLANE_422_UNORM = 1'000'156'031,
	G16_B16R16_2PLANE_422_UNORM = 1'000'156'032,
	G16_B16_R16_3PLANE_444_UNORM = 1'000'156'033,
	PVRTC1_2BPP_UNORM_BLOCK_IMG = 1'000'054'000,
	PVRTC1_4BPP_UNORM_BLOCK_IMG = 1'000'054'001,
	PVRTC2_2BPP_UNORM_BLOCK_IMG = 1'000'054'002,
	PVRTC2_4BPP_UNORM_BLOCK_IMG = 1'000'054'003,
	PVRTC1_2BPP_SRGB_BLOCK_IMG = 1'000'054'004,
	PVRTC1_4BPP_SRGB_BLOCK_IMG = 1'000'054'005,
	PVRTC2_2BPP_SRGB_BLOCK_IMG = 1'000'054'006,
	PVRTC2_4BPP_SRGB_BLOCK_IMG = 1'000'054'007,
	ASTC_4x4_SFLOAT_BLOCK_EXT = 1'000'066'000,
	ASTC_5x4_SFLOAT_BLOCK_EXT = 1'000'066'001,
	ASTC_5x5_SFLOAT_BLOCK_EXT = 1'000'066'002,
	ASTC_6x5_SFLOAT_BLOCK_EXT = 1'000'066'003,
	ASTC_6x6_SFLOAT_BLOCK_EXT = 1'000'066'004,
	ASTC_8x5_SFLOAT_BLOCK_EXT = 1'000'066'005,
	ASTC_8x6_SFLOAT_BLOCK_EXT = 1'000'066'006,
	ASTC_8x8_SFLOAT_BLOCK_EXT = 1'000'066'007,
	ASTC_10x5_SFLOAT_BLOCK_EXT = 1'000'066'008,
	ASTC_10x6_SFLOAT_BLOCK_EXT = 1'000'066'009,
	ASTC_10x8_SFLOAT_BLOCK_EXT = 1'000'066'010,
	ASTC_10x10_SFLOAT_BLOCK_EXT = 1'000'066'011,
	ASTC_12x10_SFLOAT_BLOCK_EXT = 1'000'066'012,
	ASTC_12x12_SFLOAT_BLOCK_EXT = 1'000'066'013,
	G8_B8R8_2PLANE_444_UNORM_EXT = 1'000'330'000,
	G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT = 1'000'330'001,
	G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT = 1'000'330'002,
	G16_B16R16_2PLANE_444_UNORM_EXT = 1'000'330'003,
	A4R4G4B4_UNORM_PACK16_EXT = 1'000'340'000,
	A4B4G4R4_UNORM_PACK16_EXT = 1'000'340'001,
};

// VkColorSpaceKHR
enum class ColorSpace {
	SRGB_NONLINEAR_KHR = 0,
	DISPLAY_P3_NONLINEAR_EXT = 1'000'104'001,
	EXTENDED_SRGB_LINEAR_EXT = 1'000'104'002,
	DISPLAY_P3_LINEAR_EXT = 1'000'104'003,
	DCI_P3_NONLINEAR_EXT = 1'000'104'004,
	BT709_LINEAR_EXT = 1'000'104'005,
	BT709_NONLINEAR_EXT = 1'000'104'006,
	BT2020_LINEAR_EXT = 1'000'104'007,
	HDR10_ST2084_EXT = 1'000'104'008,
	DOLBYVISION_EXT = 1'000'104'009,
	HDR10_HLG_EXT = 1'000'104'010,
	ADOBERGB_LINEAR_EXT = 1'000'104'011,
	ADOBERGB_NONLINEAR_EXT = 1'000'104'012,
	PASS_THROUGH_EXT = 1'000'104'013,
	EXTENDED_SRGB_NONLINEAR_EXT = 1'000'104'014,
	DISPLAY_NATIVE_AMD = 1'000'213'000,
};

// VkCompositeAlphaFlagBitsKHR
enum class CompositeAlphaFlags : uint32_t {
	None = 0,
	Opaque = 0x0000'0001,
	Premultiplied = 0x0000'0002,
	Postmultiplied = 0x0000'0004,
	Inherit = 0x0000'0008,
};

SP_DEFINE_ENUM_AS_MASK(CompositeAlphaFlags)

enum class FullScreenExclusiveMode : uint32_t {
	Default = 0,
	Allowed = 1,
	Disallowed = 2,
	ApplicationControlled = 3,
};

SP_DEFINE_ENUM_AS_MASK(FullScreenExclusiveMode)

// mapping to VkImageTiling
enum class ImageTiling {
	Optimal = 0,
	Linear = 1,
};

// mapping to VkImageUsageFlagBits
enum class ImageUsage : uint32_t {
	None,
	TransferSrc = 0x0000'0001,
	TransferDst = 0x0000'0002,
	Sampled = 0x0000'0004,
	Storage = 0x0000'0008,
	ColorAttachment = 0x0000'0010,
	DepthStencilAttachment = 0x0000'0020,
	TransientAttachment = 0x0000'0040,
	InputAttachment = 0x0000'0080,
};

SP_DEFINE_ENUM_AS_MASK(ImageUsage);

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

enum class PresentMode : uint32_t {
	Unsupported,
	Immediate,
	FifoRelaxed,
	Fifo,
	Mailbox
};

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

enum class SurfaceTransformFlags : uint32_t {
	None,
	Identity = 0x0000'0001,
	Rotate90 = 0x0000'0002,
	Rotate180 = 0x0000'0004,
	Rotate270 = 0x0000'0008,
	Mirror = 0x0000'0010,
	MirrorRotate90 = 0x0000'0020,
	MirrorRotate180 = 0x0000'0040,
	MirrorRotate270 = 0x0000'0080,
	Inherit = 0x0000'0100,
	PreRotated = 0x0100'0000,
	TransformMask = 0x0000'01FF,
};

SP_DEFINE_ENUM_AS_MASK(SurfaceTransformFlags)

inline SurfaceTransformFlags getPureTransform(SurfaceTransformFlags flags) {
	return flags & SurfaceTransformFlags::TransformMask;
}

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

// Cross-OS window state flags
// Some OS/WN can support only some subset of this flags
enum class WindowState : uint64_t {
	None = 0,

	// Window is in modal state
	Modal = 1LLU << 0,

	// Window is sticked to stay in place between virtual desktops
	Sticky = 1LLU << 1,

	// Window is maximized vertically
	MaximizedVert = 1LLU << 2,

	// Window is maximized horizontally
	MaximizedHorz = 1LLU << 3,

	// Window is maximized (both flags should be set)
	Maximized = MaximizedVert | MaximizedHorz,

	// XCB: _NET_WM_STATE_SHADED
	Shaded = 1LLU << 4,

	// XCB: _NET_WM_STATE_SKIP_TASKBAR
	SkipTaskbar = 1LLU << 5,

	// XCB: _NET_WM_STATE_SKIP_PAGER
	SkipPager = 1LLU << 6,

	// Window was minimized or moved to background
	Minimized = 1LLU << 7,
	Background = Minimized,

	// Window in fullscreen state
	Fullscreen = 1LLU << 8,

	// Window is above most other windows
	Above = 1LLU << 9,

	// Window is below most other windows
	Below = 1LLU << 10,

	// Window demands user attention
	DemandsAttention = 1LLU << 11,

	// Window have input focus
	Focused = 1LLU << 12,

	// Window in live resize process
	Resizing = 1LLU << 13,

	// Pointer now within window
	Pointer = 1LLU << 14,

	// Block any close request for a window
	// CloseGuard will prevent OS WM to close window on it's side.
	// If CloseGuard is enabled, when WM or user tries to close window, it remains open and you will receive
	// WindowState notification with CloseRequest flag set.
	// When CloseRequest flag is set, ypu should commit this request with enableState(WindowState::CloseRequest)
	// to close window or discard it with disableState(WindowState::CloseRequest) to remove this flag and re-enable CloseGuard
	// Also, when CloseRequest flag is set, next `close` call or next WM close action will close this window
	//
	// Note that on Android, Back Gesture is not a subject for CloseGuard, use
	// WindowLayerFlag::BackButtonHandler and Predictive Back Gesture to achive similar user experience
	// (https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture)
	CloseGuard = 1LLU << 15,

	// WM asked user to close this window
	CloseRequest = 1LLU << 16,

	// Window is available for input events
	Enabled = 1LLU << 17,

	// Android decoration handling
	// This flags mapped to WindowInsetController class methods
	DecorationStatusBarVisible = 1LLU << 18,
	DecorationNavigationVisible = 1LLU << 19,
	DecorationStatusBarLight = 1LLU << 20,
	DecorationNavigationLight = 1LLU << 21,
	DecorationShowBySwipe = 1LLU << 22,
	DecorationState = DecorationStatusBarVisible | DecorationNavigationVisible
			| DecorationStatusBarLight | DecorationNavigationLight | DecorationShowBySwipe,

	// Extra space here

	// Values for allowed window actions
	AllowedWindowMenu = 1LLU << 45,
	AllowedMove = 1LLU << 46,
	AllowedResize = 1LLU << 47,
	AllowedMinimize = 1LLU << 48,
	AllowedShade = 1LLU << 49,
	AllowedStick = 1LLU << 50,
	AllowedMaximizeVert = 1LLU << 51,
	AllowedMaximizeHorz = 1LLU << 52,
	AllowedClose = 1LLU << 53,
	AllowedFullscreen = 1LLU << 54,
	AllowedActionsMask = AllowedMove | AllowedResize | AllowedMinimize | AllowedShade | AllowedStick
			| AllowedMaximizeVert | AllowedMaximizeHorz | AllowedClose | AllowedFullscreen,

	// Window tiling constraints flag
	TiledLeft = 1LLU << 55,
	TiledRight = 1LLU << 56,
	TiledTop = 1LLU << 57,
	TiledBottom = 1LLU << 58,

	TiledTopLeft = TiledLeft | TiledTop,
	TiledTopRight = TiledRight | TiledTop,
	TiledBottomLeft = TiledLeft | TiledBottom,
	TiledBottomRight = TiledRight | TiledBottom,

	ConstrainedLeft = 1LLU << 59,
	ConstrainedRight = 1LLU << 60,
	ConstrainedTop = 1LLU << 61,
	ConstrainedBottom = 1LLU << 62,

	TilingMask = TiledLeft | TiledRight | TiledTop | TiledBottom | ConstrainedLeft
			| ConstrainedRight | ConstrainedTop | ConstrainedBottom,

	All = Modal | Sticky | Maximized | Shaded | SkipTaskbar | Minimized | Fullscreen | Above | Below
			| DemandsAttention | Focused | Resizing | Pointer | CloseGuard | CloseRequest | Enabled
			| DecorationState | AllowedActionsMask | TilingMask,
};

SP_DEFINE_ENUM_AS_MASK(WindowState)

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

SP_PUBLIC const CallbackStream &operator<<(const CallbackStream &, WindowState);

inline std::ostream &operator<<(std::ostream &stream, WindowState f) {
	memory::makeCallback(stream) << f;
	return stream;
}


} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREENUM_H_ */
