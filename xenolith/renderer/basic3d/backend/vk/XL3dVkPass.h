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
meshes into one vertex and one index buffer, with a buffer of draws, one of views, one of their
point lights and one of the materials of a schema the draws use, each once, on a worker;
the textures the draws use become the array's slots, each sampled by the pass's sampler: the
layered ones (array layers) slots of the second, sampler2DArray, array. */
class SP_PUBLIC Scene3dInputHandle : public xvk::ImageAttachmentHandle {
public:
	struct DrawRecord {
		uint32_t pipeline = MeshPipeline;
		uint32_t drawIndex = 0;
		uint32_t indexCount = 0;
		uint32_t firstIndex = 0;
		int32_t vertexOffset = 0;

		// a mesh of a loaded pack draws from the pack's buffers, null for the frame's
		Rc<core::BufferObject> vertexes;
		Rc<core::BufferObject> indexes;

		// its pipeline is translucent: drawn in the second subpass
		bool translucent = false;

		// where its material of a schema is in the frame's material buffer
		uint32_t materialOffset = 0;
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
	bool hasTranslucent() const { return _translucentDraws > 0; }

	SpanView<ViewRecord> getViews() const { return _views; }
	SpanView<DrawRecord> getDraws() const { return _draws; }
	SpanView<Rc<core::ImageObject>> getImages() const { return _images; }
	SpanView<Rc<core::ImageObject>> getImageArrays() const { return _imageArrays; }

	const Rc<xvk::Buffer> &getVertexes() const { return _vertexes; }
	const Rc<xvk::Buffer> &getIndexes() const { return _indexes; }
	const Rc<xvk::Buffer> &getDrawData() const { return _drawData; }
	const Rc<xvk::Buffer> &getViewData() const { return _viewData; }
	const Rc<xvk::Buffer> &getLightData() const { return _lightData; }
	const Rc<xvk::Buffer> &getMaterialData() const { return _materialData; }

	void setTextures(Vector<Rc<core::ImageView>> &&, Rc<core::ImageView> &&empty,
			Vector<Rc<core::ImageView>> &&arrays, Rc<core::ImageView> &&emptyArray,
			Rc<core::Sampler> &&);

protected:
	bool pack(xvk::DeviceMemoryPool *, const FrameContextHandle3d &);

	// the slot of a texture, in the layered array for one with array layers; `overflow` set when
	// its array is full
	uint32_t acquireTexture(const Rc<Texture> &, bool &overflow);

	Vector<ViewRecord> _views;
	Vector<DrawRecord> _draws;
	uint32_t _translucentDraws = 0;
	Vector<Rc<core::ImageObject>> _images;
	Vector<Rc<core::ImageObject>> _imageArrays;

	Rc<xvk::Buffer> _vertexes;
	Rc<xvk::Buffer> _indexes;
	Rc<xvk::Buffer> _drawData;
	Rc<xvk::Buffer> _viewData;
	Rc<xvk::Buffer> _lightData;
	Rc<xvk::Buffer> _materialData;

	Vector<Rc<core::ImageView>> _textureViews;
	Rc<core::ImageView> _emptyView;
	Vector<Rc<core::ImageView>> _arrayViews;
	Rc<core::ImageView> _emptyArrayView;
	Rc<core::Sampler> _sampler;
};

class SP_PUBLIC Scene3dInputAttachment
: public core::AttachmentTyped<Scene3dInputHandle, core::GenericAttachment> {
public:
	virtual ~Scene3dInputAttachment() = default;

	// which of a material's pipeline indexes the pass has, which of them are translucent and the
	// schema of the material each reads, set once by the pass as it is made
	void setPipelines(Vector<bool> &&, Vector<bool> &&translucent,
			Vector<const MaterialSchema3d *> &&materials);

	// whether a material with this pipeline is drawn by the mesh pipeline: it picks the mesh
	// pipeline, or one the pass does not have
	bool isDrawnAsMesh(uint32_t pipeline) const;

	// whether a material with this pipeline draws in the second subpass
	bool isTranslucent(uint32_t pipeline) const;

	// the schema of the material a pipeline reads, null for none or for one drawn as a mesh
	const MaterialSchema3d *getMaterialSchema(uint32_t pipeline) const;

protected:
	Vector<bool> _pipelines;
	Vector<bool> _translucent;
	Vector<const MaterialSchema3d *> _materials;
};

// Makes the views of the frame's textures before the descriptors are written
class SP_PUBLIC Pass3dHandle : public xvk::QueuePassHandle {
public:
	virtual ~Pass3dHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
};

/* The 3d pass: before basic2d's pass in the same queue, into the same Output, with a depth
attachment of its own. The mesh pipeline draws a material by default, the app's pipelines those
that pick them; all of them test depth Less and cull back faces (counter-clockwise front faces,
which the camera's flipped clip y keeps on the framebuffer).

Two subpasses: the opaque pipelines draw in the first, writing depth; the translucent ones in the
second, over them, with the depth read only - tested, and read as an input attachment. They have
a layout of their own with the depth at binding 2, so the first subpass binds no descriptor of the
depth it writes. */
class SP_PUBLIC Pass3d : public core::QueuePassTyped<Pass3dHandle, xvk::QueuePass> {
public:
	static constexpr StringView MeshPipelineName = "Basic3dMesh";
	static constexpr StringView DepthAttachmentName = "Depth3d";

	struct PipelineProgram {
		String name;
		const core::ProgramData *vert = nullptr;
		const core::ProgramData *frag = nullptr;
		bool translucent = false;
		const MaterialSchema3d *material = nullptr;
	};

	// adds the pass, its depth and its input to a queue whose Output basic2d has made
	static bool addToQueue(core::Loop *, core::Queue::Builder &, Extent2 extent,
			SpanView<Pipeline3dInfo> = SpanView<Pipeline3dInfo>());

	virtual ~Pass3d() = default;

	// the first of `pipelines` is the mesh pipeline, the rest are the app's in their order
	virtual bool init(core::QueuePassBuilder &, const core::AttachmentData *output,
			const core::AttachmentData *depth, const core::AttachmentData *input,
			Vector<PipelineProgram> &&pipelines);

	const core::AttachmentData *getInput() const { return _input; }
	SpanView<String> getPipelineNames() const { return _pipelineNames; }

	// a material's pipeline the pass does not have is drawn by the mesh pipeline, with one warning
	void warnUnknownPipeline(uint32_t);

	// trilinear, repeating, anisotropic; BASIC3D_NOMIPS=1 samples level 0 alone (a test hook)
	const Rc<core::Sampler> &acquireSampler(xvk::Device &);

	// the queue's empty image seen as a layered texture, for the layered array's free slots
	const Rc<core::ImageView> &acquireEmptyArrayView(xvk::Device &, const Rc<core::ImageObject> &);

protected:
	using xvk::QueuePass::init;

	const core::AttachmentData *_input = nullptr;
	Vector<String> _pipelineNames;
	sprt::atomic<bool> _unknownPipelineWarned = false;
	Rc<core::Sampler> _sampler;
	Rc<core::ImageView> _emptyArrayView;
};

} // namespace stappler::xenolith::basic3d::vk

#endif /* XENOLITH_RENDERER_BASIC3D_BACKEND_VK_XL3DVKPASS_H_ */
