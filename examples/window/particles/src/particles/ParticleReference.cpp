/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "particles/ParticleReference.h"
#include "glsl/include/XL2dGlslParticleSim.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// The extra data buffer the simulation reads through the four functions below, by 4-byte words
static SpanView<uint32_t> s_referenceWords;

} // namespace stappler::xenolith::examples

// XL2dGlslParticleSim.h declares these for its C++ users; the engine itself calls nothing that needs
// them, so the example defines them once, over the buffer of the simulation in progress
namespace STAPPLER_VERSIONIZED stappler::glsl {

using xenolith::examples::s_referenceWords;

uint particleEmissionPointCount() { return s_referenceWords.empty() ? 0 : s_referenceWords[0]; }

vec2 particleEmissionPoint(uint index) {
	// ParticleEmissionPoints is 8 bytes, then vec2 x count
	vec2 ret;
	::__sprt_memcpy(&ret, s_referenceWords.data() + 2 + index * 2, sizeof(vec2));
	return ret;
}

uint particleExtraUint(uint word) { return s_referenceWords[word]; }

float particleExtraFloat(uint word) {
	float ret;
	::__sprt_memcpy(&ret, s_referenceWords.data() + word, sizeof(float));
	return ret;
}

} // namespace stappler::glsl

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

bool simulateParticleReference(const ParticleReferenceInput &input, uint64_t steps, uint32_t count,
		Vector<basic2d::ParticleData> &particles) {
	auto s = input.system.get();
	if (!s || !s->hasSeed || s->data.count == 0) {
		return false;
	}

	// As ParticlePersistentData::writeUploads builds the emitter and extra data buffers
	auto data = s->data;
	if (data.sizeValue == 0.0f) {
		basic2d::ParticleSystemData::writeParticleSize(data, input.defaultSize);
	}

	Vector<uint32_t> words;
	words.resize((s->getExtraDataSize() + 3) / 4);
	s->writeExtraData(reinterpret_cast<uint8_t *>(words.data()), words.size() * 4, data);

	// As ParticlePersistentData_initParticles does with a seed
	particles.clear();
	particles.resize(data.count);
	::__sprt_memset(particles.data(), 0, particles.size() * sizeof(basic2d::ParticleData));
	for (uint32_t i = 0; i < data.count; ++i) {
		glsl::particleSeedRng(particles[i].rng, s->seed, i);
	}

	// As ParticlePass::recordCommandBuffer fills the frame, with every step in one frame
	glsl::ParticleFrameData frame;
	::__sprt_memset(&frame, 0, sizeof(frame));
	frame.framesInGen = glsl::particleCycleFrames(data);
	frame.genframe = 0;
	frame.cycle = 0;
	frame.nframes = uint32_t(steps);
	frame.seed = s->seed;
	frame.dt = data.dt;

	auto &m = input.nodeToScene.m;
	frame.transformX = Vec4(m[0], m[4], m[12], 0.0f);
	frame.transformY = Vec4(m[1], m[5], m[13], 0.0f);
	frame.transformRotation = sprt::atan2(m[1], m[0]);
	frame.transformScale = sprt::sqrt(sprt::fabs(m[0] * m[5] - m[4] * m[1]));

	s_referenceWords = words;

	glsl::ParticleUpdateCounters counters{0, 0};
	for (uint32_t i = 0; i < data.count; ++i) {
		glsl::particleUpdate(particles[i], data, frame, i, counters);
	}

	s_referenceWords = SpanView<uint32_t>();

	particles.resize(sprt::min(count, data.count));
	return true;
}

} // namespace stappler::xenolith::examples
