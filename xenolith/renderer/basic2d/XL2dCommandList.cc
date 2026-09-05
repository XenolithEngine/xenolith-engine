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

bool CommandList::init(const Rc<sprt::PoolRef> &pool) {
	_pool = pool;
	return true;
}

void CommandList::pushVertexArray(Rc<VertexData> &&vert, const Mat4 &t, CmdInfo &&info,
		CommandFlags flags) {
	if (!vert) {
		log::warn("CommandList", "Pushing empty commands should be avoidable on node's side");
		return;
	}

	_pool->perform([&, this] {
		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray, flags);
		auto cmdData = reinterpret_cast<CmdVertexArray *>(cmd->data);

		// pool memory is 16-bytes aligned, no problems with Mat4
		auto p = new (memory::pool::palloc(_pool->getPool(), sizeof(InstanceVertexData)))
				InstanceVertexData();

		TransformData instance(t);

		// Copy data to pool
		p->instances = makeSpanView(&instance, 1).pdup(_pool->getPool());
		p->data = move(vert);

		cmdData->vertexes = makeSpanView(p, 1);

		while (!info.zPath.empty() && info.zPath.back() == ZOrder(0)) { info.zPath.pop_back(); }

		cmdData->zPath = info.zPath.pdup(_pool->getPool());
		cmdData->material = info.material;
		cmdData->state = info.state;
		cmdData->renderingLevel = info.renderingLevel;
		cmdData->depthValue = info.depthValue;
		cmdData->bounds = info.bounds;

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
		cmdData->bounds = info.bounds;

		addCommand(cmd);
	});
}

void CommandList::pushDeferredVertexResult(const Rc<DeferredVertexResult> &res, const Mat4 &viewT,
		const Mat4 &modelT, bool normalized, CmdInfo &&info, CommandFlags flags) {
	if (!res) {
		log::warn("CommandList", "Pushing empty commands should be avoidable on node's side");
		return;
	}

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
		cmdData->bounds = info.bounds;

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
		cmdData->bounds = info.bounds;

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

Rc<core::AttachmentInputData> makeFrameContextInput(NotNull<core::RenderClientChannel> client) {
	auto ret = Rc<FrameContextHandle2d>::alloc();
	ret->clock = sprt::platform::clock(sprt::platform::ClockType::Monotonic);
	ret->client = client;
	return ret;
}

// --- remote render-session wire format -------------------------------------------------------
//
// Compact host-order binary blob (same-build ABI: POD vertex/instance structs are memcpy'd). Layout:
//   clock, lights(POD), decorations(POD),
//   stateCount, [enabled, viewport, scissor]*,            (DrawStateValues.data Rc extension dropped)
//   cmdCount, [material, state, level, depth, zPath[], array[]]*
// where array = [fill, stroke, sdf, instances(TransformData)[], vertices(Vertex)[], indexes(u32)[]].
// Only immediate vertex commands are emitted: Deferred commands are resolved client-side and folded
// into immediate vertices; particle/group commands are skipped.

namespace {

struct BinWriter {
	Bytes buf;
	void raw(const void *p, size_t n) {
		if (!n) {
			return;
		}
		auto o = buf.size();
		buf.resize(o + n);
		memcpy(buf.data() + o, p, n);
	}
	template <typename T>
	void pod(const T &v) {
		raw(&v, sizeof(T));
	}
	void u32(uint32_t v) { raw(&v, sizeof(v)); }
	template <typename T>
	void podArray(const T *p, uint32_t n) {
		u32(n);
		raw(p, sizeof(T) * size_t(n));
	}
};

struct BinReader {
	BytesView v;
	size_t off = 0;
	bool ok = true;
	const uint8_t *take(size_t n) {
		if (!ok || off + n > v.size()) {
			ok = false;
			return nullptr;
		}
		auto p = v.data() + off;
		off += n;
		return p;
	}
	template <typename T>
	T pod() {
		T t{};
		if (auto p = take(sizeof(T))) {
			memcpy(&t, p, sizeof(T));
		}
		return t;
	}
	uint32_t u32() { return pod<uint32_t>(); }
	template <typename T>
	Vector<T> podArray() {
		auto n = u32();
		Vector<T> out;
		if (!ok || n == 0) {
			return out;
		}
		if (auto p = take(sizeof(T) * size_t(n))) {
			out.resize(n);
			sprt::memcpy(out.data(), p, sizeof(T) * size_t(n));
		}
		return out;
	}
};

// Apply a resolved Deferred command's view/model transform to one instance (mirrors
// VertexMaterialDynamicData::applyNormalized in the vk vertex pass) so the wire carries final
// pixel-space transforms.
static TransformData transformInstance(const TransformData &src, const Mat4 &view,
		const Mat4 &model, bool normalized) {
	TransformData inst = src;
	if (normalized) {
		auto modelTransform = model * src.transform;
		Mat4 newMV;
		newMV.m[12] = sprt::floor(modelTransform.m[12]);
		newMV.m[13] = sprt::floor(modelTransform.m[13]);
		newMV.m[14] = sprt::floor(modelTransform.m[14]);
		inst.transform = view * newMV;
	} else {
		inst.transform = view * model * src.transform;
	}
	return inst;
}

// Emit one immediate vertex command. For a resolved Deferred command pass its view/model transforms;
// for an immediate VertexArray pass nullptr (instances are written as-is).
static void writeVertexCommand(BinWriter &w, const CmdInfo &info,
		SpanView<InstanceVertexData> arrays, const Mat4 *view, const Mat4 *model, bool normalized) {
	w.u32(info.material);
	w.u32(uint32_t(info.state));
	w.u32(uint32_t(info.renderingLevel));
	w.pod(info.depthValue);
	w.podArray(info.zPath.data(), uint32_t(info.zPath.size()));
	w.u32(uint32_t(arrays.size()));
	for (auto &iv : arrays) {
		w.u32(iv.fillIndexes);
		w.u32(iv.strokeIndexes);
		w.u32(iv.sdfIndexes);
		if (view) {
			Vector<TransformData> transformed;
			if (iv.instances.empty()) {
				TransformData inst;
				inst.transform = Mat4::IDENTITY;
				transformed.emplace_back(transformInstance(inst, *view, *model, normalized));
			} else {
				transformed.reserve(iv.instances.size());
				for (auto &src : iv.instances) {
					transformed.emplace_back(transformInstance(src, *view, *model, normalized));
				}
			}
			w.podArray(transformed.data(), uint32_t(transformed.size()));
		} else {
			w.podArray(iv.instances.data(), uint32_t(iv.instances.size()));
		}
		if (iv.data) {
			w.podArray(iv.data->data.data(), uint32_t(iv.data->data.size()));
			w.podArray(iv.data->indexes.data(), uint32_t(iv.data->indexes.size()));
		} else {
			w.u32(0);
			w.u32(0);
		}
	}
}

} // namespace

bool FrameContextHandle2d::serialize(const Callback<void(BytesView)> &cb) const {
	BinWriter w;
	w.pod(clock);
	w.pod(lights);
	w.pod(decorations);

	// states: only the POD fields cross the wire (the Rc<Ref> extension is server-local)
	w.u32(uint32_t(states.size()));
	for (auto &s : states) {
		w.pod(s.enabled);
		w.pod(s.viewport);
		w.pod(s.scissor);
	}

	// commands: build into a side buffer first (Deferred resolves may add entries), then prefix count
	BinWriter cmds;
	uint32_t cmdCount = 0;
	uint32_t skippedParticles = 0;
	if (commands) {
		for (auto cmd = commands->getFirst(); cmd; cmd = cmd->next) {
			switch (cmd->type) {
			case CommandType::VertexArray: {
				auto d = reinterpret_cast<const CmdVertexArray *>(cmd->data);
				writeVertexCommand(cmds, *d, d->vertexes, nullptr, nullptr, false);
				++cmdCount;
				break;
			}
			case CommandType::Deferred: {
				auto d = reinterpret_cast<const CmdDeferred *>(cmd->data);
				if (d->deferred) {
					d->deferred->acquireResult(
							[&](SpanView<InstanceVertexData> v, DeferredVertexResult::Flags) {
						writeVertexCommand(cmds, *d, v, &d->viewTransform, &d->modelTransform,
								d->normalized);
						++cmdCount;
					});
				}
				break;
			}
			case CommandType::ParticleEmitter: ++skippedParticles; break;
			case CommandType::CommandGroup: break;
			}
		}
	}
	if (skippedParticles) {
		log::source().verbose("FrameContextHandle2d", "serialize: skipped ", skippedParticles,
				" particle command(s) (not transferred this stage)");
	}
	w.u32(cmdCount);
	w.raw(cmds.buf.data(), cmds.buf.size());

	// Remote font dependencies: ship the client-minted (high-bit masked) dependency ids so the server can
	// gate this frame on its own atlas-update events. The DependencyEvents themselves never cross the wire.
	BinWriter deps;
	uint32_t depCount = 0;
	for (auto &d : waitDependencies) {
		if (d && (d->getId() & 0x8000'0000u)) {
			deps.u32(d->getId());
			++depCount;
		}
	}
	w.u32(depCount);
	w.raw(deps.buf.data(), deps.buf.size());

	cb(BytesView(w.buf.data(), w.buf.size()));
	return true;
}

bool FrameContextHandle2d::deserialize(BytesView bytes, Vector<uint32_t> *remoteDeps) {
	BinReader r;
	r.v = bytes;

	clock = r.pod<uint64_t>();
	lights = r.pod<ShadowLightInput>();
	decorations = r.pod<WindowDecorationsInput>();

	auto stateCount = r.u32();
	states.clear();
	states.reserve(stateCount);
	for (uint32_t i = 0; i < stateCount && r.ok; ++i) {
		DrawStateValues s;
		s.enabled = r.pod<core::DynamicState>();
		s.viewport = r.pod<URect>();
		s.scissor = r.pod<URect>();
		states.emplace_back(s);
	}

	auto cmdCount = r.u32();
	if (cmdCount > 0 && r.ok) {
		commands = Rc<CommandList>::create(Rc<sprt::PoolRef>::alloc());
		for (uint32_t i = 0; i < cmdCount && r.ok; ++i) {
			auto material = r.u32();
			auto state = r.u32();
			auto level = r.u32();
			auto depth = r.pod<float>();
			auto zPath = r.podArray<ZOrder>();
			auto arrayCount = r.u32();

			struct ParsedArray {
				uint32_t fill = 0, stroke = 0, sdf = 0;
				Vector<TransformData> instances;
				Vector<Vertex> vertices;
				Vector<uint32_t> indexes;
			};
			Vector<ParsedArray> parsed;
			parsed.reserve(arrayCount);
			for (uint32_t a = 0; a < arrayCount && r.ok; ++a) {
				ParsedArray pa;
				pa.fill = r.u32();
				pa.stroke = r.u32();
				pa.sdf = r.u32();
				pa.instances = r.podArray<TransformData>();
				pa.vertices = r.podArray<Vertex>();
				pa.indexes = r.podArray<uint32_t>();
				parsed.emplace_back(sp::move(pa));
			}
			if (!r.ok) {
				break;
			}

			CmdInfo info;
			info.material = material;
			info.state = StateId(state);
			info.renderingLevel = RenderingLevel(level);
			info.depthValue = depth;
			info.zPath = makeSpanView(zPath);

			commands->pushVertexArray([&](memory::pool_t *p) -> SpanView<InstanceVertexData> {
				if (parsed.empty()) {
					return SpanView<InstanceVertexData>();
				}
				auto arr = reinterpret_cast<InstanceVertexData *>(
						memory::pool::palloc(p, sizeof(InstanceVertexData) * parsed.size()));
				for (size_t a = 0; a < parsed.size(); ++a) {
					auto iv = new (&arr[a]) InstanceVertexData();
					iv->fillIndexes = parsed[a].fill;
					iv->strokeIndexes = parsed[a].stroke;
					iv->sdfIndexes = parsed[a].sdf;
					iv->instances =
							makeSpanView(parsed[a].instances.data(), parsed[a].instances.size())
									.pdup(p);
					auto vd = Rc<VertexData>::alloc();
					vd->data = sp::move(parsed[a].vertices);
					vd->indexes = sp::move(parsed[a].indexes);
					iv->data = move(vd);
				}
				return makeSpanView(arr, parsed.size());
			}, sp::move(info));
		}
	}

	// Remote font dependency ids (reconciled to real, frame-gating events on the server by
	// RemoteRenderClient::handleFrameInput). Output into the caller-provided vector wired via
	// makeInputData; if absent (local path), the ids are still consumed from the stream but discarded.
	auto depCount = r.u32();
	if (remoteDeps) {
		remoteDeps->clear();
		remoteDeps->reserve(depCount);
	}
	for (uint32_t i = 0; i < depCount && r.ok; ++i) {
		auto id = r.u32();
		if (remoteDeps) {
			remoteDeps->emplace_back(id);
		}
	}

	return r.ok;
}

} // namespace stappler::xenolith::basic2d
