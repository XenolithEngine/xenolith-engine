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
	void updateFeedback();

	UVec2 _frameGrid = UVec2(1, 1);
	uint64_t _emitterId = 0;
	Rc<ParticleSystem> _system;
	uint32_t _maxFramesPerCall = 2;

	Action *_actionRenderLock = nullptr;
	bool _feedbackEnabled = false;
	Rc<ParticleFeedbackReceiver> _feedback; // exists while enabled and once on a scene
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_PARTICLE_XL2DPARTICLEEMITTER_H_ */
