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

#include "XLSceneContent.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLDynamicStateSystem.h"
#include "XLAppWindow.h"
#include "XLWindowDecorations.h"
#include "XLCloseGuardWidget.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

SceneContent::~SceneContent() { }

bool SceneContent::init() {
	if (!Node::init()) {
		return false;
	}

	_inputListener = addSystem(Rc<InputListener>::create());
	_inputListener->setPriority(-1);
	_inputListener->addKeyRecognizer([this](GestureData data) {
		if (data.event == GestureEvent::Ended) {
			if (!handleBackButton()) {
				// propagate back button to Window
				if (_director) {
					if (auto w = _director->getWindow()) {
						w->handleBackButton();
					}
				}
			}
			return true;
		}
		return data.event == GestureEvent::Began;
	}, InputKeyInfo{makeKeyMask(InputKeyCode::ESCAPE)});

	_inputListener->setWindowStateCallback([this](WindowState state, WindowState changes) -> bool {
		handleWindowStateChanged(state, changes);
		return true;
	});

	_inputListener->setLayerFlags(WindowLayerFlags::BackButtonHandler);

	_scissor = addSystem(Rc<DynamicStateSystem>::create());

	return true;
}

void SceneContent::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	if (_closeGuard && !_closeGuardRetained) {
		_director->getWindow()->enableState(WindowState::CloseGuard);
		_closeGuardRetained = true;
	}

	if (hasFlag(_director->getWindow()->getInfo()->flags,
				WindowCreationFlags::UserSpaceDecorations)) {
		if (!_userDecorations && _windowDecorationsConstructor) {
			_userDecorations = addChild(_windowDecorationsConstructor(this));
		}
	}

	if (_handlesViewDecoration) {
		if (_decorationVisible) {
			showViewDecoration();
		} else {
			hideViewDecoration();
		}
	}
}

void SceneContent::handleExit() {
	if (_closeGuard && _closeGuardRetained) {
		_director->getWindow()->disableState(WindowState::CloseGuard);
		_closeGuardRetained = false;
	}

	Node::handleExit();
}

void SceneContent::handleContentSizeDirty() { Node::handleContentSizeDirty(); }

SP_COVERAGE_TRIVIAL
bool SceneContent::handleBackButton() { return false; }

void SceneContent::setHandlesViewDecoration(bool value) {
	if (_handlesViewDecoration != value) {
		_handlesViewDecoration = value;
		if (_decorationVisible) {
			showViewDecoration();
		} else {
			hideViewDecoration();
		}
	}
}

void SceneContent::showViewDecoration() {
	if (_decorationVisible != true) {
		if (_running && _handlesViewDecoration) {
			//_director->getWindow()->setInsetDecorationVisible(true);
		}
		_decorationVisible = true;
	}
}

void SceneContent::hideViewDecoration() {
	if (_decorationVisible != false) {
		if (_running && _handlesViewDecoration) {
			//_director->getWindow()->setInsetDecorationVisible(false);
		}
		_decorationVisible = false;
	}
}

void SceneContent::setCloseGuardEnabled(bool value) {
	if (_closeGuard != value) {
		_closeGuard = value;
		if (_running) {
			if (_closeGuard && !_closeGuardRetained) {
				_director->getWindow()->enableState(WindowState::CloseGuard);
				_closeGuardRetained = true;
			} else if (!_closeGuard && _closeGuardRetained) {
				_director->getWindow()->disableState(WindowState::CloseGuard);
				_closeGuardRetained = false;
			}
		}
	}
}

bool SceneContent::isCloseGuardEnabled() const { return _closeGuard; }

Padding SceneContent::getDecorationPadding() const {
	if (!_running) {
		return Padding();
	}
	auto &c = _scene->getFrameConstraints();
	auto padding = c.contentPadding / c.density;
	if (_userDecorations && _userDecorations->isVisible()) {
		padding += _userDecorations->getPadding();
	}
	return padding;
}

void SceneContent::enableScissor() {
	_scissor->enableScissor();
	_scissor->setStateApplyMode(DynamicStateApplyMode::ApplyForAll);
}

void SceneContent::disableScissor() {
	_scissor->disableScissor();
	_scissor->setStateApplyMode(DynamicStateApplyMode::DoNotApply);
}

bool SceneContent::isScissorEnabled() const { return _scissor->isScissorEnabled(); }

void SceneContent::setWindowDecorationsContructor(WindowDecorationsCallback &&cb) {
	_windowDecorationsConstructor = sp::move(cb);

	if (_running
			&& hasFlag(_director->getWindow()->getInfo()->flags,
					WindowCreationFlags::UserSpaceDecorations)) {
		if (_windowDecorationsConstructor) {
			if (_userDecorations) {
				_userDecorations->removeFromParent(true);
				_userDecorations = nullptr;
			}

			if (auto d = _windowDecorationsConstructor(this)) {
				if (!d->isRunning()) {
					_userDecorations = addChild(d);
				} else {
					_userDecorations = d;
				}
			}
			_contentSizeDirty = true; // to place header correctly
		}
	}
}

void SceneContent::setCloseGuardWidgetContructor(CloseGuardWidgetCallback &&cb) {
	_closeGuardWidgetConstructor = sp::move(cb);
}

void SceneContent::handleBackgroundTransition(bool val) { }

void SceneContent::handleCloseRequest(bool value) {
	if (value) {
		if (_closeGuardWidget) {
			return;
		}

		if (_closeGuardWidgetConstructor) {
			auto w = _closeGuardWidgetConstructor(this);
			if (!w->isRunning()) {
				_closeGuardWidget = addChild(w);
			} else {
				_closeGuardWidget = w;
			}
			auto cb = _closeGuardWidget->addSystem(Rc<CallbackSystem>::create());
			cb->setExitCallback([this](CallbackSystem *) { _closeGuardWidget = nullptr; });
		}
	}
}

void SceneContent::handleWindowStateChanged(WindowState state, WindowState changes) {
	if (hasFlag(changes, WindowState::Background)) {
		handleBackgroundTransition(hasFlag(state, WindowState::Background));
	}
	if (hasFlag(changes, WindowState::CloseRequest)) {
		handleCloseRequest(hasFlag(state, WindowState::CloseRequest));
	}
}

} // namespace stappler::xenolith
