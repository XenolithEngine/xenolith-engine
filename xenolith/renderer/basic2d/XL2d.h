/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2D_H_
#define XENOLITH_RENDERER_BASIC2D_XL2D_H_

#include "XLCommon.h"
#include "XLCoreMaterial.h"
#include "XLNodeInfo.h"
#include "XL2dConfig.h"

#include "glsl/include/XL2dGlslVertexData.h"
#include "glsl/include/XL2dGlslShadowData.h"
#include "glsl/include/XL2dGlslSdfData.h"
#include "glsl/include/XL2dGlslParticle.h"

#include <sprt/runtime/thread/qtimeline.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

using glsl::VertexConstantData;
using glsl::PSDFConstantData;

using glsl::Vertex;
using glsl::MaterialData;
using glsl::TransformData;
using glsl::ShadowData;

using glsl::DataAtlasIndex;
using glsl::AmbientLightData;
using glsl::DirectLightData;

using glsl::Sdf2DObjectData;
using glsl::Circle2DIndex;
using glsl::Triangle2DIndex;
using glsl::Rect2DIndex;
using glsl::RoundedRect2DIndex;
using glsl::Polygon2DIndex;

using stappler::font::Autofit;

/* Which of the two 2d render graphs a queue is.
 *
 * It lives here, and not inside Scene2d, because it is written into core::QueueData::typeTag by the
 * backend pass makers -- the pass IS what decides the shape, so having it set there is what keeps a
 * hand-built queue (one that never went through Scene2d::buildQueue) from being untagged. Scene2d
 * re-exports it as Scene2d::QueueType.
 *
 * The values are explicit and start at 1 because they now travel: 0 is `typeTag` unset, and a value
 * here may not be renumbered while a client may be matching against a server's.
 */
enum class QueueType : uint32_t {
	// full-featured queue: shadows, pseudo-SDF, particles, depth buffer, post-processing
	Default = 1,

	// lightweight queue: no shadows, no particles, no depth buffer, no post-processing
	// (Vulkan only; the WebGPU, Metal, GLES and Software queues are already of this shape)
	Flat = 2,
};
using core::SamplerIndex;

using glsl::ParticleIndirectCommand;
using glsl::ParticleFloatParam;
using glsl::ParticleVec2Param;
using glsl::ParticleEmissionPoints;
using glsl::ParticleEmitterData;
using glsl::ParticleData;
using glsl::ParticleConstantData;
using glsl::ParticleFeedback;
using glsl::FrameClipperData;

struct Triangle {
	Vertex a;
	Vertex b;
	Vertex c;
};

struct Quad {
	Vertex tl;
	Vertex bl;
	Vertex tr;
	Vertex br;
};

struct VertexSpan {
	core::MaterialId material = 0;
	uint32_t indexCount = 0;
	uint32_t instanceCount = 0;
	uint32_t firstIndex = 0;
	uint32_t vertexOffset = 0;
	uint32_t firstInstance = 0;
	StateId state = 0;
	uint32_t gradientOffset = 0;
	uint32_t gradientCount = 0;
	float outlineOffset = 0;
	uint64_t particleSystemId = 0;
};

struct alignas(16) VertexData : public Ref {
	Vector<Vertex> data;
	Vector<uint32_t> indexes;
	DataIdentity identity;

	bool getBounds(Rect &out) const;
};

struct InstanceVertexData {
	SpanView<TransformData> instances;
	Rc<VertexData> data;
	uint32_t fillIndexes = 0;
	uint32_t strokeIndexes = 0;
	uint32_t sdfIndexes = 0;
};

enum class SdfShape {
	Circle2D,
	Rect2D,
	RoundedRect2D,
	Triangle2D,
	Polygon2D,
};

struct SdfPrimitive2D {
	Vec2 origin;
};

struct SdfCircle2D : SdfPrimitive2D {
	float radius;
};

struct SdfRect2D : SdfPrimitive2D {
	Size2 size;
};

struct SdfRoundedRect2D : SdfPrimitive2D {
	Size2 size;
	Vec4 radius;
};

struct SdfTriangle2D : SdfPrimitive2D {
	Vec2 a;
	Vec2 b;
	Vec2 c;
};

struct SdfPolygon2D {
	SpanView<Vec2> points;
};

struct SdfPrimitive2DHeader {
	SdfShape type;
	BytesView bytes;
};

struct ImagePlacementResult {
	// position in requested view coordinate space
	Rect viewRect;

	// position in image coordinate space
	Rect imageRect;

	// normalized texture rect
	Rect textureRect;

	Size2 imageFragmentSize;
	float scale = 1.0f;
};

struct ImagePlacementInfo {
	Rect textureRect = Rect(0.0f, 0.0f, 1.0f, 1.0f); // normalized
	Autofit autofit = Autofit::None;
	Vec2 autofitPos = Vec2(0.5f, 0.5f);

	ImagePlacementResult resolve(const Size2 &content, const Size2 &texSize);
};

class SP_PUBLIC DeferredVertexResult : public Ref {
public:
	static constexpr sprt::qtimeline::value_type SignalValue = 1;

	enum Flags : uint32_t {
		None = 0,
		Immutable = 1 << 0,
	};

	virtual ~DeferredVertexResult() { }

	virtual bool acquireResult(const Callback<void(SpanView<InstanceVertexData>, Flags)> &) = 0;

	bool isReady() const { return _timeline.try_wait(SignalValue); }
	bool isWaitOnReady() const { return _waitOnReady; }

#if XL_FRAME_ACCOUNT
	/* WHAT THE TASK ITSELF SPENT, stamped by the task before it raises the signal.

	It has to be recorded here, by the producer, and it cannot be derived by the consumer: this
	result is made on a worker thread the consumer never sees, and the interval between handing the
	task out and taking the result back is queue latency plus work plus however long the result sat
	ready before anybody asked. Only the task knows which part of that was work.

	READ IT AS A SUM ACROSS THREADS, never as a share of the frame. Several of these run at once, so
	the total may legitimately exceed the frame it belongs to - which is the opposite of the wait
	below, and why the two are reported as separate categories rather than as parts of one whole.

	Plain, not atomic: written once by the task before the timeline signal, read after that signal
	has been observed. The signal is the barrier. */
	void setWorkTime(uint64_t ns) { _workTime = ns; }

	/* TAKEN, not read, and that is the whole correctness of the number.

	A result outlives the frame that produced it - a sprite whose content did not change re-pushes
	the SAME result every frame and the consumer takes it again each time. A plain getter would
	therefore report the tesselation cost on every steady frame forever, which is precisely the
	claim this account exists to refute. Taking it once attributes the work to the frame that
	actually paid for it, and every later frame correctly reports zero. */
	uint64_t takeWorkTime() {
		auto ret = _workTime;
		_workTime = 0;
		return ret;
	}
#endif

protected:
	bool _waitOnReady = true;
	mutable sprt::qtimeline _timeline;

#if XL_FRAME_ACCOUNT
	uint64_t _workTime = 0;
#endif
};

SP_DEFINE_ENUM_AS_MASK(DeferredVertexResult::Flags)

struct SP_PUBLIC ShadowLightInput {
	Color4F globalColor = Color4F::BLACK;
	uint32_t ambientLightCount = 0;
	uint32_t directLightCount = 0;
	float sceneDensity = 1.0f;
	float shadowDensity = 1.0f;
	float luminosity = nan();
	float padding0 = 0.0f;
	AmbientLightData ambientLights[config::MaxAmbientLights];
	DirectLightData directLights[config::MaxDirectLights];

	bool addAmbientLight(const Vec4 &, const Color4F &, bool softShadow);
	bool addDirectLight(const Vec4 &, const Color4F &, const Vec4 &);

	Extent2 getShadowExtent(Size2 frameSize) const;
	Size2 getShadowSize(Size2 frameSize) const;
};

struct SP_PUBLIC WindowDecorationsInput {
	float borderRadius = 0.0f;
	float shadowRadius = 0.0f;
	float shadowValue = 0.0f;
	Vec2 shadowOffset;
	ViewConstraints viewConstraints = ViewConstraints::None;
	bool drawUserShadows = false;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2D_H_ */
