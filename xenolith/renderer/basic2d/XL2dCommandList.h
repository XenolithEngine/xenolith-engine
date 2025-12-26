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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DCOMMANDLIST_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DCOMMANDLIST_H_

#include "XL2dVertexArray.h"
#include "XL2dParticleSystem.h"
#include "XLNodeInfo.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class CommandType : uint16_t {
	CommandGroup,
	VertexArray,
	ParticleEmitter,
	Deferred,
};

struct SP_PUBLIC CmdInfo {
	SpanView<ZOrder> zPath;
	core::MaterialId material = 0;
	StateId state = StateIdNone;
	RenderingLevel renderingLevel = RenderingLevel::Solid;
	float depthValue = 0.0f;
};

struct SP_PUBLIC CmdVertexArray : CmdInfo {
	SpanView<InstanceVertexData> vertexes;
};

struct SP_PUBLIC CmdParticleEmitter : CmdInfo {
	Mat4 transform;
	uint64_t id = 0;
	uint32_t transformIndex = 0;
};

struct SP_PUBLIC CmdDeferred : CmdInfo {
	Rc<DeferredVertexResult> deferred;
	Mat4 viewTransform;
	Mat4 modelTransform;
	bool normalized = false;
};

struct SP_PUBLIC CmdSdfGroup2D {
	Mat4 modelTransform;
	StateId state = 0;
	float value = 0.0f;
	float opacity = 1.0f;

	memory::vector<SdfPrimitive2DHeader> data;

	void addCircle2D(Vec2 origin, float r);
	void addRect2D(Rect rect);
	void addRoundedRect2D(Rect rect, float r);
	void addRoundedRect2D(Rect rect, Vec4 r);
	void addTriangle2D(Vec2 origin, Vec2 a, Vec2 b, Vec2 c);
	void addPolygon2D(SpanView<Vec2>);
};

struct SP_PUBLIC Command {
	static Command *create(memory::pool_t *, CommandType t, CommandFlags = CommandFlags::None);

	void release();

	Command *next;
	CommandType type;
	CommandFlags flags = CommandFlags::None;
	void *data;
};

class SP_PUBLIC CommandList : public Ref {
public:
	virtual ~CommandList();
	bool init(const Rc<PoolRef> &);

	void pushVertexArray(Rc<VertexData> &&, const Mat4 &, CmdInfo &&info,
			CommandFlags = CommandFlags::None);

	// data should be preallocated from frame's pool
	void pushVertexArray(const Callback<SpanView<InstanceVertexData>(memory::pool_t *)> &,
			CmdInfo &&info, CommandFlags = CommandFlags::None);

	void pushDeferredVertexResult(const Rc<DeferredVertexResult> &, const Mat4 &view,
			const Mat4 &model, bool normalized, CmdInfo &&info, CommandFlags = CommandFlags::None);

	uint32_t pushParticleEmitter(uint64_t id, const Mat4 &, CmdInfo &&info,
			CommandFlags = CommandFlags::None);

	const Command *getFirst() const { return _first; }
	const Command *getLast() const { return _last; }

	bool empty() const { return _first == nullptr; }

	size_t size() const { return _size; }

	uint32_t getPredefinedTransforms() const { return _preallocatedTransforms; }

protected:
	void addCommand(Command *);

	Rc<PoolRef> _pool;
	Command *_first = nullptr;
	Command *_last = nullptr;
	size_t _size = 0;
	uint32_t _preallocatedTransforms = 0;
};

struct SP_PUBLIC FrameContextHandle2d : public FrameContextHandle {
	virtual ~FrameContextHandle2d();

	ShadowLightInput lights;
	WindowDecorationsInput decorations;
	Rc<CommandList> commands;

	memory::map<uint64_t, ParticleSystemRenderInfo> particleEmitters;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DCOMMANDLIST_H_ */
