/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_BACKEND_VK_XL3DVKPASS_H_
#define XENOLITH_RENDERER_BASIC3D_BACKEND_VK_XL3DVKPASS_H_

#include "XL3dFrameContext.h"
#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"
#include "XLVkObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d::vk {

namespace xvk = stappler::xenolith::vk;

/* The frame input of the 3d pass, and its texture descriptor array. submitInput packs every view's
meshes into one vertex and one index buffer, with a buffer of draws and one of views, on a worker;
the textures the draws use become the array's slots, each sampled by the pass's sampler. */
class SP_PUBLIC Scene3dInputHandle : public xvk::ImageAttachmentHandle {
public:
	struct DrawRecord {
		uint32_t drawIndex = 0;
		uint32_t indexCount = 0;
		uint32_t firstIndex = 0;
		int32_t vertexOffset = 0;
	};

	struct ViewRecord {
		Rect viewport; // the scene's pixels, y up
		uint32_t firstDraw = 0;
		uint32_t drawCount = 0;
	};

	virtual ~Scene3dInputHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	virtual bool writeDescriptor(const core::QueuePassHandle &,
			core::DescriptorImageInfo &) override;

	virtual uint32_t enumerateDirtyDescriptors(const core::QueuePassHandle &,
			const core::PipelineDescriptor &, const core::DescriptorBinding &,
			const Callback<void(uint32_t)> &) const override;

	virtual void enumerateAttachmentObjects(
			const Callback<void(core::Object *, const core::SubresourceRangeInfo &)> &) override { }

	bool empty() const { return _draws.empty(); }

	SpanView<ViewRecord> getViews() const { return _views; }
	SpanView<DrawRecord> getDraws() const { return _draws; }
	SpanView<Rc<core::ImageObject>> getImages() const { return _images; }

	const Rc<xvk::Buffer> &getVertexes() const { return _vertexes; }
	const Rc<xvk::Buffer> &getIndexes() const { return _indexes; }
	const Rc<xvk::Buffer> &getDrawData() const { return _drawData; }
	const Rc<xvk::Buffer> &getViewData() const { return _viewData; }

	void setTextures(Vector<Rc<core::ImageView>> &&, Rc<core::ImageView> &&empty,
			Rc<core::Sampler> &&);

protected:
	bool pack(xvk::DeviceMemoryPool *, const FrameContextHandle3d &);

	uint32_t acquireTexture(const Rc<Texture> &);

	Vector<ViewRecord> _views;
	Vector<DrawRecord> _draws;
	Vector<Rc<core::ImageObject>> _images;

	Rc<xvk::Buffer> _vertexes;
	Rc<xvk::Buffer> _indexes;
	Rc<xvk::Buffer> _drawData;
	Rc<xvk::Buffer> _viewData;

	Vector<Rc<core::ImageView>> _textureViews;
	Rc<core::ImageView> _emptyView;
	Rc<core::Sampler> _sampler;
};

class SP_PUBLIC Scene3dInputAttachment
: public core::AttachmentTyped<Scene3dInputHandle, core::GenericAttachment> {
public:
	virtual ~Scene3dInputAttachment() = default;
};

// Makes the views of the frame's textures before the descriptors are written
class SP_PUBLIC Pass3dHandle : public xvk::QueuePassHandle {
public:
	virtual ~Pass3dHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
};

/* The 3d pass: before basic2d's pass in the same queue, into the same Output, with a depth
attachment of its own. One pipeline draws every mesh: depth test Less, back faces culled
(counter-clockwise front faces, which the camera's flipped clip y keeps on the framebuffer). */
class SP_PUBLIC Pass3d : public core::QueuePassTyped<Pass3dHandle, xvk::QueuePass> {
public:
	static constexpr StringView MeshPipelineName = "Basic3dMesh";
	static constexpr StringView DepthAttachmentName = "Depth3d";

	// adds the pass, its depth and its input to a queue whose Output basic2d has made
	static bool addToQueue(core::Loop *, core::Queue::Builder &, Extent2 extent);

	virtual ~Pass3d() = default;

	virtual bool init(core::QueuePassBuilder &, const core::AttachmentData *output,
			const core::AttachmentData *depth, const core::AttachmentData *input,
			const core::ProgramData *vert, const core::ProgramData *frag);

	const core::AttachmentData *getInput() const { return _input; }

	// trilinear, repeating, anisotropic; BASIC3D_NOMIPS=1 samples level 0 alone (a test hook)
	const Rc<core::Sampler> &acquireSampler(xvk::Device &);

protected:
	using xvk::QueuePass::init;

	const core::AttachmentData *_input = nullptr;
	Rc<core::Sampler> _sampler;
};

} // namespace stappler::xenolith::basic3d::vk

#endif /* XENOLITH_RENDERER_BASIC3D_BACKEND_VK_XL3DVKPASS_H_ */
