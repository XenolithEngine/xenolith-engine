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

#include "XL2dSceneLayout.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

SceneLayout2d::~SceneLayout2d() { }

void SceneLayout2d::setDecorationMask(DecorationMask mask) {
	if (_decorationMask != mask) {
		_decorationMask = mask;
		if (_sceneContent) {
			_sceneContent->updateLayoutNode(this);
		}
	}
}

void SceneLayout2d::setDecorationPadding(Padding padding) {
	if (_decorationPadding != padding) {
		_decorationPadding = padding;
		_contentSizeDirty = true;
	}
}

void SceneLayout2d::setTargetContentSize(const Size2 &size) { _targetContentSize = size; }

bool SceneLayout2d::handleBackButton() {
	if (_backButtonCallback) {
		return _backButtonCallback();
	} else if (_sceneContent) {
		if (_sceneContent->getLayoutsCount() >= 2 && _sceneContent->getTopLayout() == this) {
			_sceneContent->popLayout(this);
			return true;
		}
	}
	return false;
}

void SceneLayout2d::setBackButtonCallback(const BackButtonCallback &cb) {
	_backButtonCallback = cb;
}
const SceneLayout2d::BackButtonCallback &SceneLayout2d::getBackButtonCallback() const {
	return _backButtonCallback;
}

void SceneLayout2d::handlePush(SceneContent2d *l, bool replace) {
	_sceneContent = l;
	_inTransition = true;
}
void SceneLayout2d::handlePushTransitionEnded(SceneContent2d *l, bool replace) {
	_sceneContent = l;
	_inTransition = false;
	_contentSizeDirty = true;
}

void SceneLayout2d::handlePopTransitionBegan(SceneContent2d *l, bool replace) {
	_inTransition = true;
}
void SceneLayout2d::handlePop(SceneContent2d *l, bool replace) {
	_inTransition = false;
	_contentSizeDirty = true;
	_sceneContent = nullptr;
}

void SceneLayout2d::handleBackground(SceneContent2d *l, SceneLayout2d *overlay) {
	_inTransition = true;
}
void SceneLayout2d::handleBackgroundTransitionEnded(SceneContent2d *l, SceneLayout2d *overlay) {
	_inTransition = false;
	_contentSizeDirty = true;
}

void SceneLayout2d::handleForegroundTransitionBegan(SceneContent2d *l, SceneLayout2d *overlay) {
	_inTransition = true;
}
void SceneLayout2d::handleForeground(SceneContent2d *l, SceneLayout2d *overlay) {
	_inTransition = false;
	_contentSizeDirty = true;
}

Rc<SceneLayout2d::Transition> SceneLayout2d::makeEnterTransition(SceneContent2d *) const {
	return nullptr;
}
Rc<SceneLayout2d::Transition> SceneLayout2d::makeExitTransition(SceneContent2d *) const {
	return nullptr;
}

bool SceneLayout2d::hasBackButtonAction() const { return _backButtonCallback != nullptr; }

void SceneLayout2d::setLayoutName(StringView name) { _name = name.str<Interface>(); }

StringView SceneLayout2d::getLayoutName() const { return _name; }

bool SceneLayout2d::pop() {
	if (auto c = getSceneContent()) {
		c->popLayout(this);
		return true;
	}
	return false;
}

} // namespace stappler::xenolith::basic2d
