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

#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

void CmdSdfGroup2D::addCircle2D(Vec2 origin, float r) {
	auto p = data.get_allocator().getPool();

	auto circle = new (memory::pool::palloc(p, sizeof(SdfCircle2D))) SdfCircle2D;
	circle->origin = origin;
	circle->radius = r;

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Circle2D,
		BytesView(reinterpret_cast<const uint8_t *>(circle), sizeof(SdfCircle2D))});
}

void CmdSdfGroup2D::addRect2D(Rect r) {
	auto p = data.get_allocator().getPool();

	auto rect = new (memory::pool::palloc(p, sizeof(SdfRect2D))) SdfRect2D;
	rect->origin = Vec2(r.getMidX(), r.getMidY());
	rect->size = Size2(r.size / 2.0f);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Rect2D,
		BytesView(reinterpret_cast<const uint8_t *>(rect), sizeof(SdfRect2D))});
}

void CmdSdfGroup2D::addRoundedRect2D(Rect rect, float r) {
	auto p = data.get_allocator().getPool();

	auto roundedRect = new (memory::pool::palloc(p, sizeof(SdfRoundedRect2D))) SdfRoundedRect2D;
	roundedRect->origin = Vec2(rect.getMidX(), rect.getMidY());
	roundedRect->size = Size2(rect.size / 2.0f);
	roundedRect->radius = Vec4(r, r, r, r);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::RoundedRect2D,
		BytesView(reinterpret_cast<const uint8_t *>(roundedRect), sizeof(SdfRoundedRect2D))});
}

void CmdSdfGroup2D::addRoundedRect2D(Rect rect, Vec4 r) {
	auto p = data.get_allocator().getPool();

	auto roundedRect = new (memory::pool::palloc(p, sizeof(SdfRoundedRect2D))) SdfRoundedRect2D;
	roundedRect->origin = Vec2(rect.getMidX(), rect.getMidY());
	roundedRect->size = Size2(rect.size / 2.0f);
	roundedRect->radius = Vec4(r);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::RoundedRect2D,
		BytesView(reinterpret_cast<const uint8_t *>(roundedRect), sizeof(SdfRoundedRect2D))});
}

void CmdSdfGroup2D::addTriangle2D(Vec2 origin, Vec2 a, Vec2 b, Vec2 c) {
	auto p = data.get_allocator().getPool();

	auto triangle = new (memory::pool::palloc(p, sizeof(SdfTriangle2D))) SdfTriangle2D;
	triangle->origin = origin;
	triangle->a = a;
	triangle->b = b;
	triangle->c = c;

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Triangle2D,
		BytesView(reinterpret_cast<const uint8_t *>(triangle), sizeof(SdfTriangle2D))});
}

void CmdSdfGroup2D::addPolygon2D(SpanView<Vec2> view) {
	auto p = data.get_allocator().getPool();

	auto polygon = new (memory::pool::palloc(p, sizeof(SdfPolygon2D))) SdfPolygon2D;
	polygon->points = view.pdup(p);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Polygon2D,
		BytesView(reinterpret_cast<const uint8_t *>(polygon), sizeof(SdfPolygon2D))});
}

Command *Command::create(memory::pool_t *p, CommandType t, CommandFlags f) {
	auto commandSize = sizeof(Command);

	auto bytes = memory::pool::palloc(p, commandSize);
	auto c = new (bytes) Command;
	c->next = nullptr;
	c->type = t;
	c->flags = f;
	switch (t) {
	case CommandType::CommandGroup: c->data = nullptr; break;
	case CommandType::VertexArray:
		c->data = new (memory::pool::palloc(p, sizeof(CmdVertexArray), alignof(CmdVertexArray)))
				CmdVertexArray;
		break;
	case CommandType::Deferred:
		c->data = new (memory::pool::palloc(p, sizeof(CmdDeferred), alignof(CmdDeferred)))
				CmdDeferred;
		break;
	case CommandType::ParticleEmitter:
		c->data = new (memory::pool::palloc(p, sizeof(CmdParticleEmitter),
				alignof(CmdParticleEmitter))) CmdParticleEmitter;
		break;
	}
	return c;
}

void Command::release() {
	switch (type) {
	case CommandType::CommandGroup: break;
	case CommandType::VertexArray:
		if (CmdVertexArray *d = static_cast<CmdVertexArray *>(data)) {
			for (auto &it : d->vertexes) { const_cast<InstanceVertexData &>(it).data = nullptr; }
		}
		break;
	case CommandType::Deferred:
		if (CmdDeferred *d = static_cast<CmdDeferred *>(data)) {
			d->deferred = nullptr;
		}
		break;
	case CommandType::ParticleEmitter: break;
	}
}

CommandList::~CommandList() {
	if (!_first) {
		return;
	}

	memory::perform([&] {
		auto cmd = _first;
		do {
			cmd->release();
			cmd = cmd->next;
		} while (cmd);
	}, _pool->getPool());
}

bool CommandList::init(const Rc<PoolRef> &pool) {
	_pool = pool;
	return true;
}

void CommandList::pushVertexArray(Rc<VertexData> &&vert, const Mat4 &t, CmdInfo &&info,
		CommandFlags flags) {
	_pool->perform([&, this] {
		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray, flags);
		auto cmdData = reinterpret_cast<CmdVertexArray *>(cmd->data);

		// pool memory is 16-bytes aligned, no problems with Mat4
		auto p = new (memory::pool::palloc(_pool->getPool(), sizeof(InstanceVertexData)))
				InstanceVertexData();

		TransformData instance(t);

		p->instances = makeSpanView(&instance, 1).pdup(_pool->getPool());
		p->data = move(vert);

		cmdData->vertexes = makeSpanView(p, 1);

		while (!info.zPath.empty() && info.zPath.back() == ZOrder(0)) { info.zPath.pop_back(); }

		cmdData->zPath = info.zPath.pdup(_pool->getPool());
		cmdData->material = info.material;
		cmdData->state = info.state;
		cmdData->renderingLevel = info.renderingLevel;
		cmdData->depthValue = info.depthValue;

		addCommand(cmd);
	});
}

void CommandList::pushVertexArray(
		const Callback<SpanView<InstanceVertexData>(memory::pool_t *)> &cb, CmdInfo &&info,
		CommandFlags flags) {
	_pool->perform([&, this] {
		auto data = cb(_pool->getPool());
		if (data.empty()) {
			return;
		}

		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray, flags);
		auto cmdData = reinterpret_cast<CmdVertexArray *>(cmd->data);

		cmdData->vertexes = data;

		while (!info.zPath.empty() && info.zPath.back() == ZOrder(0)) { info.zPath.pop_back(); }

		cmdData->zPath = info.zPath.pdup(_pool->getPool());
		cmdData->material = info.material;
		cmdData->state = info.state;
		cmdData->renderingLevel = info.renderingLevel;
		cmdData->depthValue = info.depthValue;

		addCommand(cmd);
	});
}

void CommandList::pushDeferredVertexResult(const Rc<DeferredVertexResult> &res, const Mat4 &viewT,
		const Mat4 &modelT, bool normalized, CmdInfo &&info, CommandFlags flags) {
	_pool->perform([&, this] {
		auto cmd = Command::create(_pool->getPool(), CommandType::Deferred, flags);
		auto cmdData = reinterpret_cast<CmdDeferred *>(cmd->data);

		cmdData->deferred = res;
		cmdData->viewTransform = viewT;
		cmdData->modelTransform = modelT;
		cmdData->normalized = normalized;

		while (!info.zPath.empty() && info.zPath.back() == ZOrder(0)) { info.zPath.pop_back(); }

		cmdData->zPath = info.zPath.pdup(_pool->getPool());
		cmdData->material = info.material;
		cmdData->state = info.state;
		cmdData->renderingLevel = info.renderingLevel;
		cmdData->depthValue = info.depthValue;

		addCommand(cmd);
	});
}

uint32_t CommandList::pushParticleEmitter(uint64_t id, const Mat4 &t, CmdInfo &&info,
		CommandFlags flags) {
	uint32_t ret = 0;
	_pool->perform([&, this] {
		auto cmd = Command::create(_pool->getPool(), CommandType::ParticleEmitter, flags);
		auto cmdData = reinterpret_cast<CmdParticleEmitter *>(cmd->data);

		cmdData->transform = t;
		cmdData->id = id;

		while (!info.zPath.empty() && info.zPath.back() == ZOrder(0)) { info.zPath.pop_back(); }

		cmdData->zPath = info.zPath.pdup(_pool->getPool());
		cmdData->material = info.material;
		cmdData->state = info.state;
		cmdData->renderingLevel = info.renderingLevel;
		cmdData->depthValue = info.depthValue;

		// note: prefix increment, NOT suffix
		ret = cmdData->transformIndex = ++_preallocatedTransforms;

		addCommand(cmd);
	});
	return ret;
}

void CommandList::addCommand(Command *cmd) {
	if (!_last) {
		_first = cmd;
	} else {
		_last->next = cmd;
	}
	_last = cmd;
	++_size;
}

FrameContextHandle2d::~FrameContextHandle2d() { particleEmitters.clear(); }

} // namespace stappler::xenolith::basic2d
