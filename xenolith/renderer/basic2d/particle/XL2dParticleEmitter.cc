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

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool ParticleEmitter::init(NotNull<ParticleSystem> s) {
	if (!Sprite::init()) {
		return false;
	}

	_system = s;
	return true;
}

bool ParticleEmitter::init(NotNull<ParticleSystem> s, StringView texName) {
	if (!Sprite::init(texName)) {
		return false;
	}

	_system = s;
	return true;
}

bool ParticleEmitter::init(NotNull<ParticleSystem> s, Rc<Texture> &&tex) {
	if (!Sprite::init(move(tex))) {
		return false;
	}

	_system = s;
	return true;
}

void ParticleEmitter::handleEnter(Scene *scene) {
	Sprite::handleEnter(scene);
	_actionRenderLock = runAction(Rc<RenderContinuously>::create());
}

void ParticleEmitter::handleExit() {
	stopAction(_actionRenderLock);
	_actionRenderLock = nullptr;
	Sprite::handleExit();
}

void ParticleEmitter::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	auto data = _vertexes.pop();
	Mat4 newMV;
	if (_normalized) {
		auto &modelTransform = frame.modelTransformStack.back();
		newMV.m[12] = floorf(modelTransform.m[12]);
		newMV.m[13] = floorf(modelTransform.m[13]);
		newMV.m[14] = floorf(modelTransform.m[14]);
	} else {
		newMV = frame.modelTransformStack.back();
	}

	auto targetTransform = frame.viewProjectionStack.back() * newMV;

	FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

	auto particleSystem = _system->pop();

	auto cmdInfo = buildCmdInfo(frame);
	auto materialIndex = cmdInfo.material;

	auto transform = handle->commands->pushParticleEmitter(_system->getId(), targetTransform,
			move(cmdInfo), _commandFlags);

	handle->particleEmitters.emplace(_system->getId(),
			ParticleSystemRenderInfo{
				move(particleSystem),
				materialIndex,
				_maxFramesPerCall,
				transform,
			});
}

} // namespace stappler::xenolith::basic2d
