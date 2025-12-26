/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "MaterialSceneContent.h"
#include "MaterialLabel.h"
#include "MaterialButton.h"
#include "MaterialEasing.h"
#include "MaterialNavigationDrawer.h"
#include "XLInputListener.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class Snackbar : public Node {
public:
	virtual ~Snackbar() { }

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void setSnackbarData(SnackbarData &&);

	const SnackbarData &getData() const;

	void clear();

	void hide(Function<void()> &&cb);
	void show(SnackbarData &&);

	void onHidden();
	void onButton();

protected:
	SnackbarData _data;
	Surface *_surface = nullptr;
	TypescaleLabel *_label = nullptr;
	Button *_button = nullptr;
	InputListener *_listener = nullptr;
	bool _scheduledUpdate = false;
};

bool Snackbar::init() {
	if (!Node::init()) {
		return false;
	}

	setAnchorPoint(Anchor::MiddleBottom);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addTouchRecognizer([this](const GestureData &data) -> bool {
		if (data.event == GestureEvent::Began) {
			stopAllActions();
			runAction(Rc<Sequence>::create(_data.delayTime,
					std::bind(&Snackbar::hide, this, nullptr)));
		}
		return true;
	});
	_listener->setSwallowEvents(InputListener::EventMaskTouch);

	_surface = addChild(Rc<Surface>::create(
			SurfaceStyle(NodeStyle::Filled, Elevation::Level5, ColorRole::OnSurfaceVariant)));

	_label = _surface->addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodyLarge), ZOrder(1));
	_label->setLocaleEnabled(true);
	_label->setAnchorPoint(Anchor::MiddleLeft);

	_button = _surface->addChild(Rc<Button>::create(NodeStyle::Text), ZOrder(1));
	_button->setTapCallback([this] { onButton(); });
	_button->setAnchorPoint(Anchor::MiddleRight);
	_button->setVisible(false);
	_button->setSwallowEvents(true);

	return true;
}

void Snackbar::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_surface->setContentSize(_contentSize);

	_button->setPosition(Vec2(_contentSize.width - 8.0f, _contentSize.height / 2.0f));
	_button->setContentSize(Size2(_button->getContentSize().width, _contentSize.height));

	_label->setPosition(Vec2(24.0f, _contentSize.height / 2.0f));
}

void Snackbar::setSnackbarData(SnackbarData &&data) {
	_data = move(data);

	if (!_data.buttonText.empty() && _data.buttonCallback) {
		_button->setVisible(true);
		_button->setLeadingIconName(_data.buttonIcon);
		_button->setText(string::toupper<Interface>(_data.buttonText));
		_button->setBlendColor(_data.buttonColor, _data.buttonBlendValue);
		_label->setWidth(_contentSize.width - 48.0f - _button->getContentSize().width);
	} else {
		_button->setVisible(false);
		_label->setWidth(_contentSize.width - 48.0f);
	}

	_label->setString(_data.text);
	_label->setBlendColor(_data.textColor, _data.textBlendValue);
	_label->tryUpdateLabel();

	setContentSize(Size2(_contentSize.width, _label->getContentSize().height + 32.0f));
	setPosition(Vec2(_position.x, -_contentSize.height));
	if (!_data.text.empty() || !_data.buttonText.empty()) {
		setVisible(true);
		setOpacity(1.0f);
		runAction(Rc<Sequence>::create(
				makeEasing(Rc<MoveTo>::create(0.25f, Vec2(_position.x, 0)), EasingType::Standard),
				_data.delayTime, std::bind(&Snackbar::hide, this, nullptr)));
	}
}

const SnackbarData &Snackbar::getData() const { return _data; }

void Snackbar::clear() { setSnackbarData(SnackbarData("")); }

void Snackbar::hide(Function<void()> &&cb) {
	if (!cb) {
		runAction(Rc<Sequence>::create(
				makeEasing(Rc<MoveTo>::create(0.25f, Vec2(_position.x, -_contentSize.height)),
						EasingType::Standard),
				std::bind(&Snackbar::onHidden, this)));
	} else {
		runAction(Rc<Sequence>::create(
				makeEasing(Rc<MoveTo>::create(0.25f, Vec2(_position.x, -_contentSize.height)),
						EasingType::Standard),
				sp::move(cb)));
	}
}

void Snackbar::show(SnackbarData &&data) {
	stopAllActions();
	if (!isVisible()) {
		setSnackbarData(move(data));
	} else {
		_scheduledUpdate = true;
		hide([this, data = move(data)] {
			_scheduledUpdate = false;
			setSnackbarData(move(const_cast<SnackbarData &>(data)));
		});
	}
}

void Snackbar::onHidden() {
	stopAllActions();
	setVisible(false);
	setPosition(Vec2(_position.x, -_contentSize.height));
	_button->setVisible(false);
	_label->setString("");
}

void Snackbar::onButton() {
	if (_data.buttonCallback) {
		_data.buttonCallback();
		_data.buttonCallback = nullptr;
	}
	if (!_scheduledUpdate) {
		stopAllActions();
		runAction(Rc<Sequence>::create(0.35f, std::bind(&Snackbar::hide, this, nullptr)));
	}
}


SceneContent::~SceneContent() { }

bool SceneContent::init() {
	if (!SceneContent2d::init()) {
		return false;
	}

	_snackbarRoot = addChild(Rc<Node>::create());

	_snackbar = _snackbarRoot->addChild(Rc<Snackbar>::create(), ZOrderMax - ZOrder(2));
	_snackbar->setVisible(false);

	_navigation = addChild(Rc<NavigationDrawer>::create(), ZOrderMax - ZOrder(3));

	return true;
}

void SceneContent::handleContentSizeDirty() {
	SceneContent2d::handleContentSizeDirty();

	_snackbarRoot->setPosition(Vec2::ZERO);
	_snackbarRoot->setContentSize(_contentSize);

	_snackbar->onHidden();
	_snackbar->setContentSize(Size2(std::min(_contentSize.width, 536.0f), 48.0f));
	_snackbar->setPosition(Vec2(_contentSize.width / 2, -48.0f));

	_navigation->setPosition(Vec2::ZERO);
	_navigation->setContentSize(_contentSize);
}

bool SceneContent::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto maxDepth = getMaxDepthIndex();

	_snackbar->setDepthIndex(maxDepth);
	_navigation->setDepthIndex(maxDepth);

	return SceneContent2d::visitDraw(frame, parentFlags);
}

void SceneContent::showSnackbar(SnackbarData &&data) { _snackbar->show(move(data)); }
const String &SceneContent::getSnackbarString() const { return _snackbar->getData().text; }
void SceneContent::clearSnackbar() { _snackbar->clear(); }

bool SceneContent::isNavigationAvailable() const { return _navigation->isEnabled(); }

void SceneContent::setNavigationEnabled(bool value) { _navigation->setEnabled(value); }

void SceneContent::setNavigationMenuSource(MenuSource *source) {
	_navigation->setMenuSource(source);
}

void SceneContent::setNavigationStyle(const SurfaceStyle &style) { _navigation->setStyle(style); }

void SceneContent::openNavigation() { _navigation->show(); }

void SceneContent::closeNavigation() { _navigation->hide(); }

float SceneContent::getMaxDepthIndex() const {
	float maxIndex = _depthIndex;
	for (auto &it : _children) {
		if (it != _snackbar && it != _snackbar && it->isVisible()) {
			maxIndex = std::max(it->getMaxDepthIndex(), maxIndex);
		}
	}
	return maxIndex;
}

bool SceneContent::handleBackButton() {
	if (_navigation->isNodeVisible()) {
		_navigation->hide();
		return true;
	}
	return SceneContent2d::handleBackButton();
}

void SceneContent::updateNodesVisibility() {
	SceneContent2d::updateNodesVisibility();

	if (!_layouts.empty()) {
		_snackbarRoot->setLocalZOrder(_layouts.back()->getLocalZOrder());
	}
}

void SceneContent::handleBackgroundTransition(bool value) {
	SceneContent2d::handleBackgroundTransition(value);

	_contentSizeDirty = true;
}

} // namespace stappler::xenolith::material2d
