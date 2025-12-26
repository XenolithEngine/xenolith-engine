/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XL2dVectorCanvas.h"
#include "SPFilepath.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

struct VectorCanvasPathOutput {
	Mat4 transform;
	Size2 targetSize;
	Color4F color;
	VertexData *vertexes = nullptr;
	uint32_t material = 0;
	uint32_t objects = 0;
	const VectorCanvasConfig *config = nullptr;
};

struct VectorCanvasPathDrawer : VectorCanvasConfig {
	const VectorPath *path = nullptr;

	uint32_t draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform, VertexData *,
			bool cache, uint32_t &fillIndexes, uint32_t &strokeIndexes, uint32_t &sdfIndexes);
};

struct VectorCanvasCacheData {
	Rc<VertexData> data;
	uint32_t fillIndexes = 0;
	uint32_t strokeIndexes = 0;
	uint32_t sdfIndexes = 0;

	String name;

	float quality = 1.0f;
	float scale = 1.0f;
	geom::Tesselator::RelocateRule relocateRule = geom::Tesselator::RelocateRule::Auto;
	vg::DrawFlags style = vg::DrawFlags::Fill;

	bool operator<(const VectorCanvasCacheData &other) const {
		if (style != other.style) {
			return toInt(style) < toInt(other.style);
		} else if (name != other.name) {
			return name < other.name;
		} else if (quality != other.quality) {
			return quality < other.quality;
		} else if (relocateRule != other.relocateRule) {
			return toInt(relocateRule) < toInt(other.relocateRule);
		} else {
			return scale < other.scale;
		}
	}
};

struct VectorCanvasCache {
	static Mutex s_cacheMutex;
	static VectorCanvasCache *s_instance;

	static void retain();
	static void release();

	static const VectorCanvasCacheData *getCacheData(const VectorCanvasCacheData &);
	static const VectorCanvasCacheData *setCacheData(VectorCanvasCacheData &&);

	VectorCanvasCache();
	~VectorCanvasCache();

	uint32_t refCount = 0;
	Set<VectorCanvasCacheData> cacheData;
};

VectorCanvasCache *VectorCanvasCache::s_instance = nullptr;
Mutex VectorCanvasCache::s_cacheMutex;

struct VectorCanvas::Data : memory::AllocPool {
	memory::pool_t *pool = nullptr;
	memory::pool_t *transactionPool = nullptr;
	bool isOwned = true;
	bool deferred = false;

	VectorCanvasPathDrawer pathDrawer;

	Mat4 transform = Mat4::IDENTITY;
	Vector<Mat4> states;

	TimeInterval subAccum;

	Rc<VectorImageData> image;

	Vector<InstanceVertexData> *out = nullptr;
	std::forward_list<Vector<TransformData>> *instances = nullptr;
	Map<String, VectorCanvasResult::ObjectRef> *objects = nullptr;

	Data(memory::pool_t *p, bool deferred);
	~Data();

	void save();
	void restore();

	void applyTransform(const Mat4 &t);

	void draw(const VectorPath &, StringView id, StringView cache, const Color4F &color);
	void draw(const VectorPath &, StringView id, StringView cache, const Mat4 &,
			const Color4F &color);

	void doDraw(const VectorPath &, StringView id, StringView cache, const Color4F &color);

	void writeCacheData(const VectorPath &p, InstanceVertexData *out,
			const VectorCanvasCacheData &source);
};

static void VectorCanvasPathDrawer_pushVertex(void *ptr, uint32_t idx, const Vec2 &pt,
		float vertexValue, const Vec2 &norm) {
	auto out = reinterpret_cast<VectorCanvasPathOutput *>(ptr);
	if (size_t(idx) >= out->vertexes->data.size()) {
		out->vertexes->data.resize(idx + 1);
	}

	out->vertexes->data[idx] = Vertex{Vec4(pt, 0.0f, 1.0f),
		Vec4(out->color.r, out->color.g, out->color.b, out->color.a * vertexValue),
		out->transform.transformPoint(pt), out->material, 0};
}

static void VectorCanvasPathDrawer_pushSdf(void *ptr, uint32_t idx, const Vec2 &pt,
		float vertexValue, const Vec2 &norm) {
	auto out = reinterpret_cast<VectorCanvasPathOutput *>(ptr);
	if (size_t(idx) >= out->vertexes->data.size()) {
		out->vertexes->data.resize(idx + 1);
	}

	out->vertexes->data[idx] = Vertex{Vec4(pt, 0.0f, 1.0f),
		Vec4(out->color.r, norm.x, norm.y, vertexValue),
		Vec2(out->config->sdfBoundaryInset, out->config->sdfBoundaryOffset), out->material, 0};
}

static void VectorCanvasPathDrawer_pushTriangle(void *ptr, uint32_t pt[3]) {
	auto out = reinterpret_cast<VectorCanvasPathOutput *>(ptr);
	out->vertexes->indexes.emplace_back(pt[0]);
	out->vertexes->indexes.emplace_back(pt[1]);
	out->vertexes->indexes.emplace_back(pt[2]);
	++out->objects;
}

Rc<VectorCanvas> VectorCanvas::getInstance(bool deferred) {
	static thread_local Rc<VectorCanvas> tl_instance = nullptr;
	if (!tl_instance) {
		tl_instance = Rc<VectorCanvas>::create(deferred);
		thread::ThreadInfo::addCleanup([value = &tl_instance] {
			// deallocate thread interface
			*value = nullptr;
		});
	}
	return tl_instance;
}

VectorCanvas::~VectorCanvas() {
	if (_data && _data->isOwned) {
		auto p = _data->pool;
		_data->~Data();
		_data = nullptr;
		memory::pool::destroy(p);
	}
}

bool VectorCanvas::init(bool deferred) {
	auto p = memory::pool::create_tagged("xenolith::VectorCanvas");
	memory::context ctx(p);
	_data = new (p) Data(p, deferred);
	if (_data) {
		return true;
	}
	return false;
}

void VectorCanvas::setConfig(const VectorCanvasConfig &config) {
	static_cast<VectorCanvasConfig &>(_data->pathDrawer) = config;
}

const VectorCanvasConfig &VectorCanvas::getConfig() const { return _data->pathDrawer; }

Rc<VectorCanvasResult> VectorCanvas::draw(const VectorCanvasConfig &config,
		Rc<VectorImageData> &&image) {
	setConfig(config);
	return draw(move(image));
}

Rc<VectorCanvasResult> VectorCanvas::draw(Rc<VectorImageData> &&image) {
	auto ret = Rc<VectorCanvasResult>::alloc();
	_data->out = &ret->data;
	_data->instances = &ret->instances;
	_data->objects = &ret->objects;
	_data->image = move(image);
	ret->config = static_cast<const VectorCanvasConfig &>(_data->pathDrawer);

	auto imageSize = _data->image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(_data->pathDrawer.targetSize.width / imageSize.width,
			_data->pathDrawer.targetSize.height / imageSize.height, 1.0f);

	ret->targetTransform = t;

	auto &m = _data->image->getViewBoxTransform();
	if (!m.isIdentity()) {
		t *= m;
	}

	bool isIdentity = t.isIdentity();

	if (!isIdentity) {
		_data->save();
		_data->applyTransform(t);
	}

	_data->image->draw([&, this](const VectorPath &path, StringView id, StringView cacheId,
							   const Mat4 &pos, const Color4F &color) {
		if (ret->config.instancedMode == VectorInstancedMode::Aggressive) {
			auto it = _data->objects->find(id);
			if (it != _data->objects->end()) {
				// add new object instance

				auto matTransform = path.getTransform() * pos;
				bool hasTransform = !matTransform.isIdentity();

				if (hasTransform) {
					_data->save();
					_data->applyTransform(matTransform);
				}

				auto &inst = it->second.instances->emplace_back(TransformData(_data->transform));
				inst.instanceColor = color;
				_data->out->at(it->second.dataIndex).instances = *it->second.instances;

				if (hasTransform) {
					_data->restore();
				}

				return;
			}
		}

		if (pos.isIdentity()) {
			_data->draw(path, id, cacheId, color);
		} else {
			_data->draw(path, id, cacheId, pos, color);
		}
	});

	if (!isIdentity) {
		_data->restore();
	}

	if (!_data->out->empty() && _data->out->back().data->data.empty()) {
		_data->out->pop_back();
	}

	_data->out = nullptr;
	_data->image = nullptr;
	ret->updateColor(ret->config.color);
	return ret;
}

VectorCanvas::Data::Data(memory::pool_t *p, bool deferred) : pool(p), deferred(deferred) {
	transactionPool = memory::pool::create(pool);

	VectorCanvasCache::retain();
}

VectorCanvas::Data::~Data() {
	VectorCanvasCache::release();

	memory::pool::destroy(transactionPool);
}

void VectorCanvas::Data::save() { states.push_back(transform); }

void VectorCanvas::Data::restore() {
	if (!states.empty()) {
		transform = states.back();
		states.pop_back();
	}
}

void VectorCanvas::Data::applyTransform(const Mat4 &t) { transform *= t; }

void VectorCanvas::Data::draw(const VectorPath &path, StringView id, StringView cache,
		const Color4F &color) {
	bool hasTransform = !path.getTransform().isIdentity();
	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path, id, cache, color);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::draw(const VectorPath &path, StringView id, StringView cache,
		const Mat4 &mat, const Color4F &color) {
	auto matTransform = path.getTransform() * mat;
	bool hasTransform = !matTransform.isIdentity();

	if (hasTransform) {
		save();
		applyTransform(matTransform);
	}

	doDraw(path, id, cache, color);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::doDraw(const VectorPath &path, StringView id, StringView cache,
		const Color4F &color) {
	// Add new output slot or use already allocated and empty
	InstanceVertexData *outData = nullptr;
	if (out->empty() || !out->back().data->data.empty()) {
		out->emplace_back(InstanceVertexData{SpanView<TransformData>(), Rc<VertexData>::alloc()});
	}

	outData = &out->back();

	memory::perform_clear([&] {
		if (!deferred && !cache.empty()) {
			auto style = path.getStyle();
			float quality = pathDrawer.quality;

			Vec3 scaleVec;
			transform.getScale(&scaleVec);
			float scale = std::max(scaleVec.x, scaleVec.y);

			VectorCanvasCacheData data{nullptr, 0, 0, 0, cache.str<Interface>(), quality, scale,
				pathDrawer.relocateRule, style};

			if (auto it = VectorCanvasCache::getCacheData(data)) {
				if (!it->data->indexes.empty()) {
					writeCacheData(path, outData, *it);

					auto &it = instances->emplace_front(Vector<TransformData>());
					auto &instObj = it.emplace_back(TransformData(transform));
					instObj.instanceColor = color;
					outData->instances = it;
				}
				return;
			}

			data.data = Rc<VertexData>::alloc();

			auto ret = pathDrawer.draw(transactionPool, path, transform, data.data, true,
					data.fillIndexes, data.strokeIndexes, data.sdfIndexes);
			if (ret != 0) {
				if (auto it = VectorCanvasCache::setCacheData(move(data))) {
					writeCacheData(path, outData, *it);

					auto &inst = instances->emplace_front(Vector<TransformData>());
					auto &instObj = inst.emplace_back(TransformData(transform));
					instObj.instanceColor = color;
					outData->instances = inst;

					if (pathDrawer.instancedMode == VectorInstancedMode::Aggressive) {
						objects->emplace(id.str<Interface>(),
								VectorCanvasResult::ObjectRef{&inst, uint32_t(out->size() - 1)});
					}
				}
			} else {
				outData->data->data.clear();
				outData->data->indexes.clear();
			}
		} else {
			auto ret = pathDrawer.draw(transactionPool, path, transform, outData->data, false,
					outData->fillIndexes, outData->strokeIndexes, outData->sdfIndexes);
			if (ret == 0) {
				outData->data->data.clear();
				outData->data->indexes.clear();
			} else {
				auto &inst = instances->emplace_front(Vector<TransformData>());
				auto &instObj = inst.emplace_back(TransformData(transform));
				instObj.instanceColor = color;
				outData->instances = inst;

				if (pathDrawer.instancedMode == VectorInstancedMode::Aggressive) {
					objects->emplace(id.str<Interface>(),
							VectorCanvasResult::ObjectRef{&inst, uint32_t(out->size() - 1)});
				}
			}
		}
	}, transactionPool);
}

void VectorCanvas::Data::writeCacheData(const VectorPath &p, InstanceVertexData *out,
		const VectorCanvasCacheData &source) {
	auto fillColor = Color4F(p.getFillColor());
	auto strokeColor = Color4F(p.getStrokeColor());

	Vec4 fillVec = fillColor;
	Vec4 strokeVec = strokeColor;

	out->data->indexes = source.data->indexes;
	out->data->data = source.data->data;
	for (auto &it : out->data->data) {
		if (it.material == pathDrawer.fillMaterial) {
			it.color = it.color * fillVec;
		} else if (it.material == pathDrawer.strokeMaterial) {
			it.color = it.color * strokeVec;
		}
	}
	out->fillIndexes = source.fillIndexes;
	out->strokeIndexes = source.strokeIndexes;
}

uint32_t VectorCanvasPathDrawer::draw(memory::pool_t *pool, const VectorPath &p,
		const Mat4 &transform, VertexData *out, bool cache, uint32_t &fillIndexes,
		uint32_t &strokeIndexes, uint32_t &sdfIndexes) {
	bool success = true;
	path = &p;

	float approxScale = 1.0f;
	auto style = path->getStyle();

	Rc<geom::Tesselator> fillTess = ((style & geom::DrawFlags::Fill) != geom::DrawFlags::None)
			? Rc<geom::Tesselator>::create(pool)
			: nullptr;
	Rc<geom::Tesselator> strokeTess = ((style & geom::DrawFlags::Stroke) != geom::DrawFlags::None)
			? Rc<geom::Tesselator>::create(pool)
			: nullptr;
	Rc<geom::Tesselator> sdfTess =
			(fillTess
					&& ((style & geom::DrawFlags::PseudoSdf) != geom::DrawFlags::None
							|| forcePseudoSdf))
			? Rc<geom::Tesselator>::create(pool)
			: nullptr;

	Vec3 scale;
	transform.getScale(&scale);
	approxScale = std::max(scale.x, scale.y);

	geom::LineDrawer line(approxScale * quality, Rc<geom::Tesselator>(fillTess),
			Rc<geom::Tesselator>(strokeTess), Rc<geom::Tesselator>(sdfTess),
			path->getStrokeWidth());

	auto d = path->getPoints().data();

	for (auto &it : path->getCommands()) {
		switch (it) {
		case vg::Command::MoveTo:
			line.drawBegin(d[0].p.x, d[0].p.y);
			++d;
			break;
		case vg::Command::LineTo:
			line.drawLine(d[0].p.x, d[0].p.y);
			++d;
			break;
		case vg::Command::QuadTo:
			line.drawQuadBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y);
			d += 2;
			break;
		case vg::Command::CubicTo:
			line.drawCubicBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y, d[2].p.x, d[2].p.y);
			d += 3;
			break;
		case vg::Command::ArcTo:
			line.drawArc(d[0].p.x, d[0].p.y, d[2].f.v, d[2].f.a, d[2].f.b, d[1].p.x, d[1].p.y);
			d += 3;
			break;
		case vg::Command::ClosePath: line.drawClose(true); break;
		default: break;
		}
	}
	line.drawClose(false);

	VectorCanvasPathOutput target{transform, targetSize, Color4F::WHITE, out};
	target.transform.scale(1.0f / targetSize.width, 1.0f / targetSize.height, 1.0f);

	target.transform =
			Mat4(textureFlippedX ? -1.0f : 1.0, 0.0f, 0.0f, textureFlippedY ? 1.0f : -1.0f,
					textureFlippedX ? 1.0f : 0.0f, textureFlippedY ? 0.0f : 1.0f)
			* target.transform;

	target.config = this;

	geom::TessResult result;
	result.target = &target;
	result.pushVertex = VectorCanvasPathDrawer_pushVertex;
	result.pushTriangle = VectorCanvasPathDrawer_pushTriangle;

	if (fillTess) {
		// draw antialias outline only if stroke is transparent enough
		// for cached image, always draw antialias, because user can change color and opacity
		if (path->isAntialiased()
				&& (path->getStyle() == vg::DrawFlags::Fill || path->getStrokeOpacity() < 96
						|| cache)) {
			fillTess->setBoundariesTransform(boundaryInset / approxScale,
					boundaryOffset / approxScale);
			fillTess->setRelocateRule(relocateRule);
		}
		fillTess->setContentScale(approxScale);
		fillTess->setWindingRule(path->getWindingRule());
		if (!fillTess->prepare(result)) {
			success = false;
		}
	}

	if (strokeTess) {
		if (path->isAntialiased()) {
			strokeTess->setBoundariesTransform(boundaryInset / approxScale,
					boundaryOffset / approxScale);
		}

		strokeTess->setRelocateRule(relocateRule);
		strokeTess->setContentScale(approxScale);
		strokeTess->setWindingRule(vg::Winding::NonZero);
		if (!strokeTess->prepare(result)) {
			success = false;
		}
	}

	if (sdfTess) {
		sdfTess->setBoundariesTransform(sdfBoundaryInset / approxScale,
				sdfBoundaryOffset / approxScale);
		sdfTess->setRelocateRule(geom::Tesselator::RelocateRule::DistanceField);
		sdfTess->setContentScale(approxScale);
		sdfTess->setWindingRule(path->getWindingRule());
		if (!sdfTess->prepare(result)) {
			success = false;
		}
	}

	out->data.resize(result.nvertexes);
	out->indexes.reserve(result.nfaces * 3);

	auto objects = target.objects;

	if (fillTess) {
		target.material = fillMaterial;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = Color4F(path->getFillColor());
		}
		fillTess->write(result);
		fillIndexes = (target.objects - objects) * 3;
	}

	objects = target.objects;

	if (strokeTess) {
		target.material = strokeMaterial;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = Color4F(path->getStrokeColor());
		}
		strokeTess->write(result);
		strokeIndexes = (target.objects - objects) * 3;
	}

	objects = target.objects;

	if (sdfTess) {
		result.pushVertex = VectorCanvasPathDrawer_pushSdf;
		target.material = sdfMaterial;
		target.color = Color4F::WHITE;
		sdfTess->write(result);
		sdfIndexes = (target.objects - objects) * 3;
	}

	if (!success) {
		if (verbose) {
			log::source().error("VectorCanvasPathDrawer", "Failed path:\n", path->toString(true));
		}
	}

	return target.objects;
}


void VectorCanvasCache::retain() {
	std::unique_lock<Mutex> lock(s_cacheMutex);

	if (!s_instance) {
		s_instance = new VectorCanvasCache();
	}
	++s_instance->refCount;
}

void VectorCanvasCache::release() {
	std::unique_lock<Mutex> lock(s_cacheMutex);

	if (s_instance) {
		if (s_instance->refCount == 1) {
			delete s_instance;
			s_instance = nullptr;
		} else {
			--s_instance->refCount;
		}
	}
}

const VectorCanvasCacheData *VectorCanvasCache::getCacheData(const VectorCanvasCacheData &data) {
	std::unique_lock<Mutex> lock(s_cacheMutex);
	if (!s_instance) {
		return nullptr;
	}

	auto it = s_instance->cacheData.find(data);
	if (it != s_instance->cacheData.end()) {
		return &*it;
	}

	return nullptr;
}

const VectorCanvasCacheData *VectorCanvasCache::setCacheData(VectorCanvasCacheData &&data) {
	std::unique_lock<Mutex> lock(s_cacheMutex);
	if (!s_instance) {
		return nullptr;
	}

	auto it = s_instance->cacheData.find(data);
	if (it == s_instance->cacheData.end()) {
		it = s_instance->cacheData.emplace(move(data)).first;
	}
	return &*it;
}

VectorCanvasCache::VectorCanvasCache() {
	auto path = FileInfo("vector_cache.cbor", FileCategory::AppCache);

	if (filesystem::exists(path)) {
		auto val = data::readFile<Interface>(path);
		for (auto &it : val.asArray()) {
			if (it.getInteger("version") != 2) {
				continue;
			}

			VectorCanvasCacheData data;
			data.name = it.getString("name");
			data.quality = it.getDouble("quality");
			data.scale = it.getDouble("scale");
			data.relocateRule = geom::Tesselator::RelocateRule(it.getInteger("rule"));
			data.style = geom::DrawFlags(it.getInteger("style"));
			data.fillIndexes = uint32_t(it.getInteger("fill"));
			data.strokeIndexes = uint32_t(it.getInteger("stroke"));
			data.sdfIndexes = uint32_t(it.getInteger("sdf"));

			auto &vertexes = it.getBytes("vertexes");
			auto &indexes = it.getBytes("indexes");

			data.data = Rc<VertexData>::alloc();
			data.data->data.assign(reinterpret_cast<Vertex *>(vertexes.data()),
					reinterpret_cast<Vertex *>(vertexes.data() + vertexes.size()));
			data.data->indexes.assign(reinterpret_cast<uint32_t *>(indexes.data()),
					reinterpret_cast<uint32_t *>(indexes.data() + indexes.size()));

			cacheData.emplace(move(data));
		}
	}
}

VectorCanvasCache::~VectorCanvasCache() {
	Value val;
	for (auto &it : cacheData) {
		if (!it.data) {
			continue;
		}

		Value data;
		data.setString(it.name, "name");
		data.setDouble(it.quality, "quality");
		data.setDouble(it.scale, "scale");
		data.setInteger(toInt(it.relocateRule), "rule");
		data.setInteger(toInt(it.style), "style");
		data.setInteger(it.fillIndexes, "fill");
		data.setInteger(it.strokeIndexes, "stroke");
		data.setInteger(it.sdfIndexes, "sdf");
		data.setInteger(2, "version");

		data.setBytes(BytesView(reinterpret_cast<uint8_t *>(it.data->data.data()),
							  it.data->data.size() * sizeof(Vertex)),
				"vertexes");
		data.setBytes(BytesView(reinterpret_cast<uint8_t *>(it.data->indexes.data()),
							  it.data->indexes.size() * sizeof(uint32_t)),
				"indexes");

		val.addValue(move(data));
	}

	if (!val.empty()) {
		auto path = FileInfo("vector_cache.cbor", FileCategory::AppCache);

		filesystem::remove(path);
		data::save(val, path, data::EncodeFormat::CborCompressed);
	}
}

void VectorCanvasResult::updateColor(const Color4F &color) {
	auto copyData = [](const VertexData *data) {
		auto ret = Rc<VertexData>::alloc();
		ret->data.resize(data->data.size());
		ret->indexes.resize(data->indexes.size());
		memcpy(ret->data.data(), data->data.data(), data->data.size() * sizeof(Vertex));
		memcpy(ret->indexes.data(), data->indexes.data(), data->indexes.size() * sizeof(uint32_t));
		return ret;
	};

	mut.clear();
	mut.reserve(data.size());
	for (auto &it : data) {
		auto &iit = mut.emplace_back(InstanceVertexData{it.instances, copyData(it.data),
			it.fillIndexes, it.strokeIndexes, it.sdfIndexes});
		if (iit.sdfIndexes > 0) {
			for (auto &vertex : iit.data->data) {
				if (vertex.material != config::VGPseudoSdfMaterial) {
					if (vertex.color.z > 1.0f) {
						log::source().error("VectorCanvasResult", "vertex transform error");
					}
					vertex.color = vertex.color * color;
				}
			}
		} else {
			for (auto &vertex : iit.data->data) { vertex.color = vertex.color * color; }
		}
	}

	config.color = color;
}

VectorCanvasDeferredResult::~VectorCanvasDeferredResult() {
	if (_future) {
		delete _future;
		_future = nullptr;
	}
}

bool VectorCanvasDeferredResult::init(std::future<Rc<VectorCanvasResult>> &&future,
		bool waitOnReady) {
	_future = new std::future<Rc<VectorCanvasResult>>(sp::move(future));
	_waitOnReady = waitOnReady;
	return true;
}

bool VectorCanvasDeferredResult::acquireResult(
		const Callback<void(SpanView<InstanceVertexData>, Flags)> &cb) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		_result = _future->get();
		delete _future;
		_future = nullptr;
		DeferredVertexResult::handleReady();
	}
	cb(_result->mut, None);
	return true;
}

void VectorCanvasDeferredResult::handleReady(Rc<VectorCanvasResult> &&res) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		delete _future;
		_future = nullptr;
	}
	if (res && (!_result || res.get() != _result.get())) {
		_result = move(res);
		DeferredVertexResult::handleReady();
	}
}

Rc<VectorCanvasResult> VectorCanvasDeferredResult::getResult() const {
	std::unique_lock<Mutex> lock(_mutex);
	return _result;
}

void VectorCanvasDeferredResult::updateColor(const Color4F &color) {
	getResult(); // ensure rendering was complete

	std::unique_lock<Mutex> lock(_mutex);
	if (_result) {
		lock.unlock();
		_result->updateColor(color);
	}
}

} // namespace stappler::xenolith::basic2d
