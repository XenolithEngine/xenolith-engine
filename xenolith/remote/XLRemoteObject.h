/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_REMOTE_XLREMOTEOBJECT_H_
#define XENOLITH_REMOTE_XLREMOTEOBJECT_H_

#include <sprt/runtime/window/interface.h>

#include "XLCommon.h"
#include "XLCoreObject.h"
#include "XLCoreRenderSession.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Thin client-side gAPI handles.
//
// On the server a compiled core::Queue/core::Resource holds backend (vk::*) objects whose native
// handles are process-local and cannot cross the wire. The client mirror replaces each with one of
// the thin handles below: a subclass of the matching core:: gAPI base that carries only the
// server-assigned object id (stored in ObjectData::handle) plus the relevant info struct. These are
// constructed WITHOUT a core::Device (Object::init is bypassed; device/callback stay null so the
// destructor's invalidate() is a no-op), and they perform no GPU work -- they exist so the client's
// queue/resource graph is structurally complete and can reference objects by id in later stages.

// Read the server object id back from any handle minted here (0 if none).
SP_PUBLIC uint64_t getRemoteObjectId(const core::Object &);

class SP_PUBLIC Image : public core::ImageObject {
public:
	virtual ~Image();
	bool init(uint64_t id, const core::ImageInfoData &);
};

class SP_PUBLIC Buffer : public core::BufferObject {
public:
	virtual ~Buffer();
	bool init(uint64_t id, const core::BufferInfo &);
};

class SP_PUBLIC ImageView : public core::ImageView {
public:
	virtual ~ImageView();
	bool init(uint64_t id, const Rc<core::ImageObject> &image, const core::ImageViewInfo &);
};

class SP_PUBLIC Sampler : public core::Sampler {
public:
	virtual ~Sampler();
	bool init(uint64_t id, const core::SamplerInfo &);
};

class SP_PUBLIC Shader : public core::Shader {
public:
	virtual ~Shader();
	bool init(uint64_t id, core::ProgramStage stage);
};

class SP_PUBLIC GraphicPipeline : public core::GraphicPipeline {
public:
	virtual ~GraphicPipeline();
	bool init(uint64_t id);
};

class SP_PUBLIC ComputePipeline : public core::ComputePipeline {
public:
	virtual ~ComputePipeline();
	bool init(uint64_t id, uint32_t localX, uint32_t localY, uint32_t localZ);
};

class SP_PUBLIC RenderPass : public core::RenderPass {
public:
	virtual ~RenderPass();
	bool init(uint64_t id, core::PassType type, uint64_t index);
};

class SP_PUBLIC TextureSetLayout : public core::TextureSetLayout {
public:
	virtual ~TextureSetLayout();
	bool init(uint64_t id, uint32_t imageCount, uint32_t samplersCount);
};

// Server-side object id table. Assigns a monotonic uint64 id to each distinct gAPI object during
// encode and keeps it alive so the client can later reference objects by id (the server resolves the
// id back to the real object). id 0 == null.
class SP_PUBLIC ObjectRegistry : public Ref {
public:
	struct SharedQueueInfo {
		Rc<core::Queue> queue;
		HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> materials;
	};

	struct SharedWindowInfo {
		core::RenderServerChannel *window = nullptr;
		Vector<uint64_t> queues;
	};

	virtual ~ObjectRegistry();

	void shareWindow(core::RenderServerChannel *, SpanView<core::Queue *>,
			const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &materials);

	uint64_t share(core::RenderServerChannel *);
	uint64_t share(core::Queue *);
	uint64_t share(core::Object *);
	uint64_t share(const Rc<core::Object> &o) { return share(o.get()); }

	uint64_t attachMaterials(NotNull<core::MaterialSet>);

	uint64_t get(core::RenderServerChannel *) const;
	uint64_t get(core::Queue *) const;
	uint64_t get(core::Object *) const;

	void drop(core::RenderServerChannel *);
	void drop(core::Queue *);
	void drop(core::Object *);

	core::Object *resolveObject(uint64_t) const;
	const SharedQueueInfo *resolveQueue(uint64_t) const;
	core::RenderServerChannel *resolveWindow(uint64_t) const;

	size_t size() const { return _objectById.size(); }
	void clear();

	uint64_t allocateId() { return _next++; }

	const Map<uint64_t, SharedWindowInfo> &getWindows() const { return _windowById; }

protected:
	uint64_t _next = 1;
	Map<core::Object *, uint64_t> _objectByPtr;
	Map<uint64_t, Rc<core::Object>> _objectById;

	Map<core::Queue *, uint64_t> _queueByPtr;
	Map<uint64_t, SharedQueueInfo> _queueById;

	Map<core::RenderServerChannel *, uint64_t> _windowByPtr;
	Map<uint64_t, SharedWindowInfo> _windowById;
};

// Client-side factory: mints a thin handle for a server object id (info comes from the wire) and
// caches id -> handle so repeated references in the same stream resolve to the SAME handle. The
// reverse map is exposed for the frame stage (client -> server id-referenced commands). id 0 == null.
class SP_PUBLIC ObjectFactory : public Ref {
public:
	virtual ~ObjectFactory() = default;

	core::Queue *makeQueue(uint64_t id, core::Queue &, BytesView);

	core::ImageObject *makeImage(uint64_t id, const core::ImageInfoData &);
	core::BufferObject *makeBuffer(uint64_t id, const core::BufferInfo &);
	core::ImageView *makeImageView(uint64_t id, const Rc<core::ImageObject> &,
			const core::ImageViewInfo &);
	core::Sampler *makeSampler(uint64_t id, const core::SamplerInfo &);
	core::Shader *makeShader(uint64_t id, core::ProgramStage);
	core::GraphicPipeline *makeGraphicPipeline(uint64_t id);
	core::ComputePipeline *makeComputePipeline(uint64_t id, uint32_t x, uint32_t y, uint32_t z);
	core::RenderPass *makeRenderPass(uint64_t id, core::PassType, uint64_t index);
	core::TextureSetLayout *makeTextureSetLayout(uint64_t id, uint32_t imageCount,
			uint32_t samplersCount);

	core::Object *resolveObject(uint64_t) const;
	core::Queue *resolveQueue(uint64_t id) const;

	// Map a gAPI image object id back to the resource ImageData that wraps it. Material images on the
	// mirror reference an image by the object id; FrameContext::getMaterialInfo needs the ImageData
	// (`it.image->image->getIndex()`), so the resource decode registers id -> ImageData here.
	void registerImageData(uint64_t id, const core::ImageData *d) {
		if (id && d) {
			_imageDataById.emplace(id, d);
		}
	}
	const core::ImageData *resolveImageData(uint64_t id) const {
		auto it = _imageDataById.find(id);
		return (it != _imageDataById.end()) ? it->second : nullptr;
	}

	size_t size() const { return _objectById.size(); }
	void clear();

protected:
	Map<uint64_t, Rc<core::Object>> _objectById;
	Map<uint64_t, Rc<core::Queue>> _queueById;
	Map<uint64_t, const core::ImageData *> _imageDataById;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEOBJECT_H_ */
