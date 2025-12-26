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

#include "XL2dSceneContent.h"
#include "XL2dFrameContext.h"
#include "XL2dSceneLayout.h"
#include "XL2dSceneLight.h"
#include "XL2dScene.h" // IWYU pragma: keep
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

SceneContent2d::~SceneContent2d() { }

bool SceneContent2d::init() {
	if (!SceneContent::init()) {
		return false;
	}

	_2dContext = Rc<FrameContext2d>::create();
	_frameContext = _2dContext;

	return true;
}

void SceneContent2d::handleEnter(Scene *scene) {
	SceneContent::handleEnter(scene);

	for (auto &it : _lights) { it->onEnter(scene); }
}

void SceneContent2d::handleExit() {
	for (auto &it : _lights) { it->onExit(); }

	SceneContent::handleExit();
}

void SceneContent2d::handleContentSizeDirty() {
	SceneContent::handleContentSizeDirty();

	for (auto &node : _layouts) { updateLayoutNode(node); }

	for (auto &overlay : _overlays) { updateLayoutNode(overlay); }
}

void SceneContent2d::replaceLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (_layouts.empty()) {
		pushLayout(node);
		return;
	}

	updateLayoutNode(node);

	ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
	for (auto n : _layouts) {
		n->setLocalZOrder(zIndex);
		zIndex++;
	}

	_layouts.push_back(node);
	addChild(node, zIndex);

	for (auto &it : _layouts) {
		if (it != node) {
			it->handlePopTransitionBegan(this, true);
		} else {
			it->handlePush(this, true);
		}
	}

	auto fn = [this, node] {
		for (auto &it : _layouts) {
			if (it != node) {
				it->handlePop(this, true);
			} else {
				it->handlePushTransitionEnded(this, true);
			}
		}
		replaceNodes();
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(enter, fn));
	} else {
		fn();
	}
}

void SceneContent2d::pushLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	updateLayoutNode(node);
	pushNodeInternal(node, nullptr);
}

void SceneContent2d::replaceTopLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (!_layouts.empty()) {
		auto back = _layouts.back();
		_layouts.pop_back();
		back->handlePopTransitionBegan(this, false);

		// just push node, then silently remove previous

		pushNodeInternal(node, [this, back] {
			eraseLayout(back);
			back->handlePop(this, false);
		});
	}
}

void SceneContent2d::popLayout(SceneLayout2d *node) {
	auto it = std::find(_layouts.begin(), _layouts.end(), node);
	if (it == _layouts.end()) {
		return;
	}

	auto linkId = node->retain();
	_layouts.erase(it);

	node->handlePopTransitionBegan(this, false);
	if (!_layouts.empty()) {
		_layouts.back()->handleForegroundTransitionBegan(this, node);
	}

	auto fn = [this, node, linkId] {
		eraseLayout(node);
		node->handlePop(this, false);
		if (!_layouts.empty()) {
			_visitNotification.emplace_back(
					[this, l = _layouts.back(), node = Rc<SceneLayout2d>(node)] {
				if (!_layouts.empty() && _layouts.back() == l) {
					l->handleForeground(this, node);
				}
			});
		}
		node->release(linkId);
	};

	if (auto exit = node->makeExitTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(exit), fn));
	} else {
		fn();
	}
}

void SceneContent2d::pushNodeInternal(SceneLayout2d *node, Function<void()> &&cb) {
	if (!_layouts.empty()) {
		ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
		for (auto &n : _layouts) {
			n->setLocalZOrder(zIndex);
			zIndex++;
		}
	}

	_layouts.push_back(node);

	updateLayoutNode(node);
	addChild(node, ZOrder(-1));

	if (_layouts.size() > 1) {
		_layouts.at(_layouts.size() - 2)->handleBackground(this, node);
	}
	node->handlePush(this, false);

	auto fn = [this, node, cb = sp::move(cb)] {
		updateNodesVisibility();
		if (_layouts.size() > 1) {
			_layouts.at(_layouts.size() - 2)->handleBackgroundTransitionEnded(this, node);
		}
		node->handlePushTransitionEnded(this, false);
		if (cb) {
			cb();
		}
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(enter), move(fn)));
	} else {
		fn();
	}
}

void SceneContent2d::eraseLayout(SceneLayout2d *node) {
	node->removeFromParent();
	if (!_layouts.empty()) {
		ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
		for (auto n : _layouts) {
			n->setLocalZOrder(zIndex);
			n->setVisible(false);
			zIndex++;
		}
		updateNodesVisibility();
	}
}

void SceneContent2d::eraseOverlay(SceneLayout2d *l) {
	l->removeFromParent();
	if (!_overlays.empty()) {
		ZOrder zIndex = ZOrder(1);
		for (auto n : _overlays) {
			n->setLocalZOrder(zIndex);
			zIndex++;
		}
		updateNodesVisibility();
	}
}

void SceneContent2d::replaceNodes() {
	if (!_layouts.empty()) {
		auto vec = _layouts;
		for (auto &node : vec) {
			if (node != _layouts.back()) {
				node->removeFromParent();
			}
		}

		_layouts.erase(_layouts.begin(), _layouts.begin() + _layouts.size() - 1);
	}
}

void SceneContent2d::updateNodesVisibility() {
	for (size_t i = 0; i < _layouts.size(); i++) {
		if (i == _layouts.size() - 1) {
			_layouts.at(i)->setVisible(true);
		} else {
			_layouts.at(i)->setVisible(false);
		}
	}

	if (!_layouts.empty()) {
		auto status = _layouts.back()->getDecorationStatus();
		switch (status) {
		case DecorationStatus::DontCare: break;
		case DecorationStatus::Visible: showViewDecoration(); break;
		case DecorationStatus::Hidden: hideViewDecoration(); break;
		}
	}
}

SceneLayout2d *SceneContent2d::getTopLayout() {
	if (!_layouts.empty()) {
		return _layouts.back();
	} else {
		return nullptr;
	}
}

SceneLayout2d *SceneContent2d::getPrevLayout() {
	if (_layouts.size() > 1) {
		return *(_layouts.end() - 2);
	}
	return nullptr;
}

bool SceneContent2d::popTopLayout() {
	if (!_overlays.empty()) {
		popOverlay(_overlays.back());
		return true;
	}
	if (_layouts.size() > 1) {
		popLayout(_layouts.back());
		return true;
	}
	return false;
}

bool SceneContent2d::isActive() const { return !_layouts.empty(); }

bool SceneContent2d::handleBackButton() {
	if (_layouts.empty()) {
		return false;
	} else {
		if (!_overlays.empty()) {
			if (!_overlays.back()->handleBackButton()) {
				if (!popTopLayout()) {
					return false;
				} else {
					return true;
				}
			} else {
				return true;
			}
		}
		if (!_layouts.back()->handleBackButton()) {
			if (!popTopLayout()) {
				return false;
			}
		}
		return true;
	}
}

size_t SceneContent2d::getLayoutsCount() const { return _layouts.size(); }

const Vector<Rc<SceneLayout2d>> &SceneContent2d::getLayouts() const { return _layouts; }

const Vector<Rc<SceneLayout2d>> &SceneContent2d::getOverlays() const { return _overlays; }

bool SceneContent2d::pushOverlay(SceneLayout2d *l) {
	if (!l || l->isRunning()) {
		return false;
	}

	updateLayoutNode(l);

	ZOrder zIndex = ZOrder(_overlays.size() + 1);

	_overlays.push_back(l);

	addChild(l, zIndex);

	l->handlePush(this, false);

	auto fn = [this, l] { l->handlePushTransitionEnded(this, false); };

	if (auto enter = l->makeEnterTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(enter), move(fn)), "ContentLayer.Transition"_tag);
	} else {
		fn();
	}

	return true;
}

bool SceneContent2d::popOverlay(SceneLayout2d *l) {
	auto it = std::find(_overlays.begin(), _overlays.end(), l);
	if (it == _overlays.end()) {
		return false;
	}

	auto linkId = l->retain();
	_overlays.erase(it);
	l->handlePopTransitionBegan(this, false);

	auto fn = [this, l, linkId] {
		eraseOverlay(l);
		l->handlePop(this, false);
		l->release(linkId);
	};

	if (auto exit = l->makeExitTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(exit), move(fn)));
	} else {
		fn();
	}

	return true;
}

void SceneContent2d::updateLayoutNode(SceneLayout2d *node) {
	auto mask = node->getDecorationMask();

	Padding padding = getDecorationPadding();

	Vec2 pos = Vec2::ZERO;
	Size2 size = _contentSize;
	Padding effectiveDecorations;

	if (node->getTargetContentSize() != Size2::ZERO) {
		size.width = std::min(size.width, node->getTargetContentSize().width);
		size.height = std::min(size.height, node->getTargetContentSize().height);
	}

	if ((mask & DecorationMask::Top) != DecorationMask::None) {
		size.height -= padding.top;
		effectiveDecorations.top = padding.top;
	}
	if ((mask & DecorationMask::Right) != DecorationMask::None) {
		size.width -= padding.right;
		effectiveDecorations.right = padding.right;
	}
	if ((mask & DecorationMask::Left) != DecorationMask::None) {
		size.width -= padding.left;
		pos.x += padding.left;
		effectiveDecorations.left = padding.left;
	}
	if ((mask & DecorationMask::Bottom) != DecorationMask::None) {
		size.height -= padding.bottom;
		pos.y += padding.bottom;
		effectiveDecorations.bottom = padding.bottom;
	}

	node->setAnchorPoint(Anchor::BottomLeft);
	node->setDecorationPadding(effectiveDecorations);
	node->setPosition(pos);
	node->setContentSize(size);
}

void SceneContent2d::setDefaultLights() {
	auto color = Color4F::WHITE;
	color.a = 0.5f;

	// Свет сверху под углом, дающий удлиннение теней снизу
	auto light = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient, Vec2(0.0f, 0.3f),
			1.5f, color);

	// Свет строго сверху, дающий базовые тени
	auto ambient = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient,
			Vec2(0.0f, 0.0f), 1.5f, color);

	removeAllLights();

	// Базовый белый заполняющий свет
	setGlobalLight(Color4F::WHITE);

	addLight(move(light));
	addLight(move(ambient));
}

bool SceneContent2d::addLight(SceneLight *light, uint64_t tag, StringView name) {
	if (tag != InvalidTag) {
		if (_lightsByTag.find(tag) != _lightsByTag.end()) {
			log::source().warn("Scene", "Light with tag ", tag, " is already defined");
			return false;
		}
	}
	if (!name.empty()) {
		if (_lightsByName.find(name) != _lightsByName.end()) {
			log::source().warn("Scene", "Light with name ", name, " is already defined");
			return false;
		}
	}

	if (light->getScene()) {
		log::source().warn("Scene", "Light is already on scene");
		return false;
	}

	switch (light->getType()) {
	case SceneLightType::Ambient:
		if (_lightsAmbientCount >= config::MaxAmbientLights) {
			log::source().warn("Scene", "Too many ambient lights");
			return false;
		}
		break;
	case SceneLightType::Direct:
		if (_lightsDirectCount >= config::MaxDirectLights) {
			log::source().warn("Scene", "Too many direct lights");
			return false;
		}
		break;
	}

	_lights.emplace_back(light);

	switch (light->getType()) {
	case SceneLightType::Ambient: ++_lightsAmbientCount; break;
	case SceneLightType::Direct: ++_lightsDirectCount; break;
	}

	if (tag != InvalidTag) {
		light->setTag(tag);
		_lightsByTag.emplace(tag, light);
	}

	if (!name.empty()) {
		light->setName(name);
		_lightsByName.emplace(light->getName(), light);
	}

	if (_running) {
		light->onEnter(_scene);
	} else {
		light->_scene = _scene;
	}

	return true;
}

SceneLight *SceneContent2d::getLightByTag(uint64_t tag) const {
	auto it = _lightsByTag.find(tag);
	if (it != _lightsByTag.end()) {
		return it->second;
	}
	return nullptr;
}

SceneLight *SceneContent2d::getLightByName(StringView name) const {
	auto it = _lightsByName.find(name);
	if (it != _lightsByName.end()) {
		return it->second;
	}
	return nullptr;
}

void SceneContent2d::removeLight(SceneLight *light) {
	if (light->getScene() != _scene) {
		return;
	}

	auto it = _lights.begin();
	while (it != _lights.end()) {
		if (*it == light) {
			removeLight(it);
			return;
		}
		++it;
	}
}

void SceneContent2d::removeLightByTag(uint64_t tag) {
	if (auto l = getLightByTag(tag)) {
		removeLight(l);
	}
}

void SceneContent2d::removeLightByName(StringView name) {
	if (auto l = getLightByName(name)) {
		removeLight(l);
	}
}

void SceneContent2d::removeAllLights() {
	auto it = _lights.begin();
	while (it != _lights.end()) { it = removeLight(it); }
}

void SceneContent2d::removeAllLightsByType(SceneLightType type) {
	auto it = _lights.begin();
	while (it != _lights.end()) {
		if ((*it)->getType() == type) {
			it = removeLight(it);
		} else {
			++it;
		}
	}
}

void SceneContent2d::setGlobalLight(const Color4F &color) { _globalLight = color; }

const Color4F &SceneContent2d::getGlobalLight() const { return _globalLight; }

bool SceneContent2d::visitGeometry(FrameInfo &info, NodeVisitFlags parentFlags) {
	if (_visible) {
		auto tmp = sp::move(_visitNotification);
		_visitNotification.clear();

		for (auto &it : tmp) { it(); }
	}

	return SceneContent::visitGeometry(info, parentFlags);
}

void SceneContent2d::draw(FrameInfo &info, NodeVisitFlags flags) {
	SceneContent::draw(info, flags);

	FrameContextHandle2d *ctx = reinterpret_cast<FrameContextHandle2d *>(info.currentContext);

	auto &constraints = _scene->getFrameConstraints();

	Size2 scaledExtent(constraints.extent.width / constraints.density,
			constraints.extent.height / constraints.density);

	ctx->lights.sceneDensity = constraints.density;
	ctx->lights.shadowDensity = _shadowDensity;
	ctx->lights.globalColor = _globalLight;

	for (auto &it : _lights) {
		switch (it->getType()) {
		case SceneLightType::Ambient:
			ctx->lights.addAmbientLight(it->getNormal(), it->getColor(), it->isSoftShadow());
			break;
		case SceneLightType::Direct:
			ctx->lights.addDirectLight(it->getNormal(), it->getColor(), it->getData());
			break;
		}
	}

	auto &theme = _director->getApplication()->getThemeInfo();

	auto window = _director->getWindow();
	if (hasFlag(window->getInfo()->flags,
				WindowCreationFlags::UserSpaceDecorations)) {
		ctx->decorations.drawUserShadows = hasFlag(_director->getWindow()->getInfo()->capabilities,
				WindowCapabilities::UserShadowsRequired);
		ctx->decorations.borderRadius = theme.decorations.borderRadius;
		ctx->decorations.shadowRadius = theme.decorations.shadowWidth;
		ctx->decorations.shadowValue =
				hasFlag(_director->getWindow()->getWindowState(), WindowState::Focused)
				? theme.decorations.shadowMaxValue
				: theme.decorations.shadowMinValue;
		ctx->decorations.shadowOffset = theme.decorations.shadowOffset;
		ctx->decorations.viewConstraints =
				core::getViewConstraints(_director->getWindow()->getWindowState());
	}
}

Vector<Rc<SceneLight>>::iterator SceneContent2d::removeLight(
		Vector<Rc<SceneLight>>::iterator itVec) {
	if ((*itVec)->isRunning()) {
		(*itVec)->onExit();
	}

	if (!(*itVec)->getName().empty()) {
		auto it = _lightsByName.find((*itVec)->getName());
		if (it != _lightsByName.end()) {
			_lightsByName.erase(it);
		}
	}

	if ((*itVec)->getTag() != InvalidTag) {
		auto it = _lightsByTag.find((*itVec)->getTag());
		if (it != _lightsByTag.end()) {
			_lightsByTag.erase(it);
		}
	}

	switch ((*itVec)->getType()) {
	case SceneLightType::Ambient: --_lightsAmbientCount; break;
	case SceneLightType::Direct: --_lightsDirectCount; break;
	}

	return _lights.erase(itVec);
}

} // namespace stappler::xenolith::basic2d
