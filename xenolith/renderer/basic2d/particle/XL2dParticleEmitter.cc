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

#include "XL2dParticleEmitter.h"
#include "XL2dParticleSystem.h"
#include "XL2dSprite.h"
#include "XLAction.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

static sprt::atomic<uint64_t> s_particleEmitterId = 1;

bool ParticleFeedbackReceiver::init(AppThread *app) {
	_application = app;
	return _application != nullptr;
}

void ParticleFeedbackReceiver::deliverFeedback(const ParticleFeedback &feedback) {
	_application->performOnAppThread([this, feedback] {
		if (!_attached) {
			return;
		}
		// Frames of one queue may complete out of order: totals take every frame, the latest
		// state stays the latest
		_totalBirths += feedback.births;
		_totalSteps += feedback.steps;
		if (feedback.sequence >= _feedback.sequence) {
			_feedback = feedback;
		}
	}, this);
}

void ParticleFeedbackReceiver::deliverSnapshot(uint32_t id, ParticleSnapshot &&snapshot) {
	_application->performOnAppThread([this, id, snapshot = sp::move(snapshot)]() mutable {
		if (!_attached) {
			return;
		}
		for (auto it = _snapshots.begin(); it != _snapshots.end(); ++it) {
			if (it->id == id) {
				auto callback = sp::move(it->callback);
				_snapshots.erase(it);
				snapshot.success = true;
				callback(sp::move(snapshot));
				return;
			}
		}
		// A second frame carrying the same request is late: the first one answered it
	}, this);
}

void ParticleFeedbackReceiver::attach() { _attached = true; }

void ParticleFeedbackReceiver::detach() {
	_attached = false;
	auto snapshots = sp::move(_snapshots);
	_snapshots.clear();
	for (auto &it : snapshots) { it.callback(ParticleSnapshot()); }
}

uint32_t ParticleFeedbackReceiver::requestSnapshot(uint32_t count, SnapshotCallback &&cb) {
	auto id = _nextSnapshotId++;
	_snapshots.emplace_back(SnapshotRequest{id, count, sp::move(cb)});
	return id;
}

const ParticleFeedbackReceiver::SnapshotRequest *
ParticleFeedbackReceiver::getPendingSnapshot() const {
	return _snapshots.empty() ? nullptr : &_snapshots.front();
}

bool ParticleEmitter::init(NotNull<ParticleSystem> s) {
	if (!Sprite::init()) {
		return false;
	}

	_system = s;
	_emitterId = s_particleEmitterId.fetch_add(1);

	// The emitter has no CPU geometry at all (particles are simulated on the GPU) and they can
	// fly outside contentSize, so damage tracking can neither version nor bound it.
	addCommandFlags(CommandFlags::AlwaysDirty | CommandFlags::UnknownBounds);
	return true;
}

bool ParticleEmitter::init(NotNull<ParticleSystem> s, StringView texName) {
	if (!Sprite::init(texName)) {
		return false;
	}

	_system = s;
	_emitterId = s_particleEmitterId.fetch_add(1);

	// The emitter has no CPU geometry at all (particles are simulated on the GPU) and they can
	// fly outside contentSize, so damage tracking can neither version nor bound it.
	addCommandFlags(CommandFlags::AlwaysDirty | CommandFlags::UnknownBounds);
	return true;
}

bool ParticleEmitter::init(NotNull<ParticleSystem> s, Rc<Texture> &&tex) {
	if (!Sprite::init(move(tex))) {
		return false;
	}

	_system = s;
	_emitterId = s_particleEmitterId.fetch_add(1);

	// The emitter has no CPU geometry at all (particles are simulated on the GPU) and they can
	// fly outside contentSize, so damage tracking can neither version nor bound it.
	addCommandFlags(CommandFlags::AlwaysDirty | CommandFlags::UnknownBounds);
	return true;
}

void ParticleEmitter::setFrameGrid(uint32_t h, uint32_t v) {
	_frameGrid = UVec2(sprt::max(h, uint32_t(1)), sprt::max(v, uint32_t(1)));
}

RenderingLevel ParticleEmitter::getRealRenderingLevel() const {
	auto level = Sprite::getRealRenderingLevel();
	if (_renderingLevel == RenderingLevel::Default && level < RenderingLevel::Transparent) {
		return RenderingLevel::Transparent;
	}
	return level;
}

void ParticleEmitter::handleEnter(Scene *scene) {
	Sprite::handleEnter(scene);
	_actionRenderLock = runAction(Rc<RenderContinuously>::create());
	if (_feedbackEnabled) {
		updateFeedback();
	}
}

void ParticleEmitter::handleExit() {
	if (_feedback) {
		_feedback->detach();
	}
	stopAction(_actionRenderLock);
	_actionRenderLock = nullptr;
	Sprite::handleExit();
}

void ParticleEmitter::setFeedbackEnabled(bool value) {
	if (value == _feedbackEnabled) {
		return;
	}

	_feedbackEnabled = value;
	if (_feedbackEnabled) {
		if (_running) {
			updateFeedback();
		}
	} else if (_feedback) {
		_feedback->detach();
		_feedback = nullptr;
	}
}

void ParticleEmitter::updateFeedback() {
	if (!_feedback) {
		auto app = _director ? _director->getApplication() : nullptr;
		if (!app) {
			return;
		}
		_feedback = Rc<ParticleFeedbackReceiver>::create(app);
	}
	_feedback->attach();
}

const ParticleFeedback &ParticleEmitter::getFeedback() const {
	static ParticleFeedback s_empty;
	return _feedback ? _feedback->getFeedback() : s_empty;
}

uint64_t ParticleEmitter::getFeedbackTotalBirths() const {
	return _feedback ? _feedback->getTotalBirths() : 0;
}

uint64_t ParticleEmitter::getFeedbackTotalSteps() const {
	return _feedback ? _feedback->getTotalSteps() : 0;
}

void ParticleEmitter::requestSnapshot(uint32_t count, Function<void(ParticleSnapshot &&)> &&cb) {
	setFeedbackEnabled(true);
	if (!_feedback || !_running) {
		cb(ParticleSnapshot());
		return;
	}
	_feedback->requestSnapshot(count, sp::move(cb));
}

void ParticleEmitter::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	auto data = _vertexes.pop();

	FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

	auto particleSystem = _system->pop();
	auto &modelTransform = frame.modelTransformStack.back();

	Mat4 targetTransform;
	Mat4 nodeToScene;
	if (hasFlag(ParticleSystemFlags(particleSystem->data.flags),
				ParticleSystemFlags::LocalCoords)) {
		Mat4 newMV;
		if (_normalized) {
			newMV.m[12] = floorf(modelTransform.m[12]);
			newMV.m[13] = floorf(modelTransform.m[13]);
			newMV.m[14] = floorf(modelTransform.m[14]);
		} else {
			newMV = modelTransform;
		}
		targetTransform = frame.viewProjectionStack.back() * newMV;
	} else {
		// Scene space is the dp space of the scene content: the model transform ends in pixels
		Mat4 content;
		if (_scene && _scene->getContent()) {
			content = _scene->getContent()->getModelTransform();
		}
		targetTransform = frame.viewProjectionStack.back() * content;
		nodeToScene = content.getInversed() * modelTransform;
	}

	auto cmdInfo = buildCmdInfo(frame);
	auto materialIndex = cmdInfo.material;

	auto transform = handle->commands->pushParticleEmitter(_emitterId, targetTransform,
			move(cmdInfo), _commandFlags);

	Size2 defaultSize;
	if (_texture) {
		auto extent = _texture->getExtent();
		auto &rect = getTextureRect();
		defaultSize = Size2(extent.width * rect.size.width, extent.height * rect.size.height);
	}

	ParticleSystemRenderInfo info{
		move(particleSystem),
		materialIndex,
		_maxFramesPerCall,
		transform,
		0,
		defaultSize,
		nodeToScene,
		getTextureRect(),
		_frameGrid,
		_displayedColor,
		_feedback,
	};

	if (_feedback) {
		// Sent with every frame until one of them answers it
		if (auto request = _feedback->getPendingSnapshot()) {
			info.snapshotId = request->id;
			info.snapshotCount = sprt::min(request->count, info.system->data.count);
		}
	}

	handle->particleEmitters.emplace(_emitterId, move(info));
}

} // namespace stappler::xenolith::basic2d
