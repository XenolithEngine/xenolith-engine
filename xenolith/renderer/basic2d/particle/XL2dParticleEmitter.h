/**
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

#ifndef XENOLITH_RENDERER_BASIC2D_PARTICLE_XL2DPARTICLEEMITTER_H_
#define XENOLITH_RENDERER_BASIC2D_PARTICLE_XL2DPARTICLEEMITTER_H_

#include "XL2dSprite.h"
#include "XL2dParticleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

// The quad mask shapes the particle fragment knows (xl_2d_particle.frag); the values are the
// shader's own switch over the mask push constant
enum class ParticleQuadMask : uint32_t {
	None = 0,
	Disc = 1,
	Ring = 2,
	Streak = 3,
};

// The blend modes the emitter's material asks the pass for. The particle draw writes straight
// color with the shape's alpha in its own channel, so `Alpha` is the sprite pair (the source
// scaled by its own alpha over the destination reduced by it) and `Add` keeps the source's own
// scaling but sums the destination unchanged - a fading particle adds less and nothing else,
// which no raw `One/One` pair would give over a straight-color draw.
enum class ParticleBlendMode : uint32_t {
	Alpha = 0,
	Add = 1,
};

class ParticleEmitter : public Sprite {
public:
	virtual ~ParticleEmitter() = default;

	virtual bool init(NotNull<ParticleSystem>);
	virtual bool init(NotNull<ParticleSystem>, StringView);
	virtual bool init(NotNull<ParticleSystem>, Rc<Texture> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void pushCommands(FrameInfo &, NodeVisitFlags flags) override;

	// Keys the particle buffers on the GPU: two emitters sharing a system simulate separately
	uint64_t getEmitterId() const { return _emitterId; }

	ParticleSystem *getParticleSystem() const { return _system; }

	// Animation frames as an h x v grid inside the texture rect, row 0 at the top of the image;
	// the system's animation frame curve picks the frame
	void setFrameGrid(uint32_t h, uint32_t v);
	UVec2 getFrameGrid() const { return _frameGrid; }

	// The quad mask the draw computes from its UV instead of sampling the texture; the two
	// numbers belong to the ring (band thickness and edge softness, both in UV units). The
	// shape also switches the emitter's material to the particle fragment's own pipeline
	void setQuadMask(ParticleQuadMask shape, float ringThickness = 0.0f, float ringSoftness = 0.0f);
	ParticleQuadMask getQuadMask() const { return _quadMask; }
	float getQuadMaskThickness() const { return _maskThickness; }
	float getQuadMaskSoftness() const { return _maskSoftness; }

	// How the draw lands on what is already there; `Alpha` is what a sprite does, `Add` sums
	// over it. The mode is a material state, not a simulation one: two emitters of one system
	// simulate together and may still differ here.
	void setBlendMode(ParticleBlendMode mode);
	ParticleBlendMode getBlendMode() const { return _blendMode; }

	// The mode's journey over a life, Trapcode Particular 2026's reception: a flagged particle
	// starts at the sprite pair and ends additive, the blend fraction the fraction of its
	// lifetime it has lived - alpha at birth, add at the last step, the one pair there is. The
	// flag is draw state beside the mode and the simulation never reads it, but the pass cannot
	// express it yet: the emitter draws through the shared sprite vertex stage and, for a
	// texture, the shared material fragment, and a per-particle blend weight has no seat in
	// either without forking them - the wall the quad mask met, which that stage answered with
	// the pass's own fragment. The state rides the emitter unread until such a shader pair
	// gives it a seat; setting it moves no material.
	void setBlendOverLife(bool);
	bool isBlendOverLife() const { return _blendOverLife; }

	// Particles carry their own alpha: without an explicit level the emitter is transparent
	virtual RenderingLevel getRealRenderingLevel() const override;

	// With feedback enabled the renderer reports the simulation after every frame; counters come
	// only from the feedback pipeline, enabled by XL_PARTICLE_FEEDBACK=1
	void setFeedbackEnabled(bool);
	bool isFeedbackEnabled() const { return _feedbackEnabled; }

	// The latest report; empty before the first frame or without feedback
	const ParticleFeedback &getFeedback() const;

	uint64_t getFeedbackTotalBirths() const;
	uint64_t getFeedbackTotalSteps() const;

	// The first `count` particles (at most the system's count) after the next rendered frame. Enables
	// feedback. The callback runs on the application thread once, with success = false if the
	// emitter leaves the scene first.
	void requestSnapshot(uint32_t count, Function<void(ParticleSnapshot &&)> &&);

protected:
	virtual MaterialInfo getMaterialInfo() const override;

	void updateFeedback();

	UVec2 _frameGrid = UVec2(1, 1);
	uint64_t _emitterId = 0;
	Rc<ParticleSystem> _system;
	uint32_t _maxFramesPerCall = 2;
	ParticleQuadMask _quadMask = ParticleQuadMask::None;
	float _maskThickness = 0.0f;
	float _maskSoftness = 0.0f;
	ParticleBlendMode _blendMode = ParticleBlendMode::Alpha;
	bool _blendOverLife = false;

	Action *_actionRenderLock = nullptr;
	bool _feedbackEnabled = false;
	Rc<ParticleFeedbackReceiver> _feedback; // exists while enabled and once on a scene

	// The node's origin in scene space as the last pushed frame saw it, for the velocity a
	// newborn with inheritVelocity inherits its share of; false until the first frame stands it
	Vec2 _previousOrigin = Vec2::ZERO;
	bool _originSeen = false;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_PARTICLE_XL2DPARTICLEEMITTER_H_ */
