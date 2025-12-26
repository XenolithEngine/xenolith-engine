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

#include "MaterialAppBar.h"
#include "MaterialButton.h"
#include "XLInputListener.h"
#include "XLAction.h"
#include "XLDynamicStateSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool AppBar::init(AppBarLayout layout, const SurfaceStyle &style) {
	if (!Surface::init(style)) {
		return false;
	}

	_layout = layout;

	_inputListener = addSystem(Rc<InputListener>::create());
	_inputListener->addTouchRecognizer(
			[this](const GestureData &) -> bool { return isSwallowTouches(); });
	_inputListener->addPressRecognizer([this](const GesturePress &press) -> bool {
		if (_barCallback) {
			if (press.event == GestureEvent::Ended) {
				_barCallback();
			}
			return true;
		}
		return false;
	}, TimeInterval::milliseconds(425), true);
	_inputListener->setSwallowEvents(InputListener::EventMaskTouch);

	_actionMenuSourceListener = addSystem(
			Rc<DataListener<MenuSource>>::create(std::bind(&AppBar::layoutSubviews, this)));

	_navButton = addChild(Rc<Button>::create(NodeStyle::Text), ZOrder(1));
	_navButton->setTapCallback(std::bind(&AppBar::handleNavTapped, this));
	_navButton->setLeadingIconName(IconName::Navigation_menu_solid);
	_navButton->setIconSize(24.0f);
	_navButton->setNodeMask(Button::LeadingIcon);
	_navButton->setFollowContentSize(false);
	_navButton->setSwallowEvents(true);

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::TitleLarge));
	_label->setAnchorPoint(Anchor::MiddleLeft);

	_scissorNode = addChild(Rc<Node>::create());
	_scissorNode->setPosition(Vec2::ZERO);
	_scissorNode->setAnchorPoint(Anchor::BottomLeft);

	_iconsComposer = addChild(Rc<Node>::create(), ZOrder(1));
	_iconsComposer->setPosition(Vec2::ZERO);
	_iconsComposer->setAnchorPoint(Anchor::BottomLeft);
	_iconsComposer->setCascadeOpacityEnabled(true);

	updateDefaultHeight();

	_scissorComponent =
			addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
	_scissorComponent->enableScissor();

	return true;
}

void AppBar::handleContentSizeDirty() {
	Surface::handleContentSizeDirty();
	updateDefaultHeight();
	layoutSubviews();
}

void AppBar::setLayout(AppBarLayout layout) {
	if (_layout != layout) {
		_layout = layout;
		_contentSizeDirty = true;
	}
}

void AppBar::setTitle(StringView str) { _label->setString(str); }

StringView AppBar::getTitle() const { return _label->getString8(); }

void AppBar::setNavButtonIcon(IconName name, float progress) {
	_navButton->setLeadingIconName(name, progress);
	_contentSizeDirty = true;
}

IconName AppBar::getNavButtonIcon() const { return _navButton->getLeadingIconName(); }

void AppBar::setMaxActionIcons(size_t value) {
	_maxActionIcons = value;
	_contentSizeDirty = true;
}
size_t AppBar::getMaxActionIcons() const { return _maxActionIcons; }

void AppBar::setActionMenuSource(MenuSource *source) {
	if (_actionMenuSourceListener->getSubscription() != source) {
		_actionMenuSourceListener->setSubscription(source);
	}
}

void AppBar::replaceActionMenuSource(MenuSource *source, size_t maxIcons) {
	if (source == _actionMenuSourceListener->getSubscription()) {
		return;
	}

	if (maxIcons == maxOf<size_t>()) {
		maxIcons = source->getHintCount();
	}

	stopAllActionsByTag("replaceActionMenuSource"_tag);
	if (_prevComposer) {
		_prevComposer->removeFromParent();
		_prevComposer = nullptr;
	}

	_actionMenuSourceListener->setSubscription(source);
	_maxActionIcons = maxIcons;

	_prevComposer = _iconsComposer;
	float pos = -_prevComposer->getContentSize().height;
	_iconsComposer = _scissorNode->addChild(Rc<Node>::create(), ZOrder(1));
	_iconsComposer->setPosition(Vec2(0, pos));
	_iconsComposer->setAnchorPoint(Anchor::BottomLeft);
	_iconsComposer->setCascadeOpacityEnabled(true);

	float iconWidth = updateMenu(_iconsComposer, source, _maxActionIcons);
	if (iconWidth > _iconWidth) {
		_iconWidth = iconWidth;
		_contentSizeDirty = true;
	}

	_replaceProgress = 0.0f;
	updateProgress();

	runAction(Rc<ActionProgress>::create(0.15f,
					  [this](float p) {
		_replaceProgress = p;
		updateProgress();
	}, nullptr,
					  [this]() {
		_replaceProgress = 1.0f;
		updateProgress();
		_contentSizeDirty = true;
	}),
			"replaceActionMenuSource"_tag);
}

MenuSource *AppBar::getActionMenuSource() const {
	return _actionMenuSourceListener->getSubscription();
}

void AppBar::setBasicHeight(float value) {
	if (isnan(value)) {
		_basicHeight = value;
		_contentSizeDirty = true;
	} else {
		if (_basicHeight != value) {
			_basicHeight = value;
			_contentSizeDirty = true;
		}
	}
}

float AppBar::getBasicHeight() const { return _basicHeight; }

void AppBar::setNavCallback(Function<void()> &&cb) { _navCallback = sp::move(cb); }
const Function<void()> &AppBar::getNavCallback() const { return _navCallback; }

void AppBar::setSwallowTouches(bool value) {
	if (value) {
		_inputListener->setSwallowEvents(InputListener::EventMaskTouch);
	} else {
		_inputListener->clearSwallowEvents(InputListener::EventMaskTouch);
	}
}

bool AppBar::isSwallowTouches() const {
	return _inputListener->isSwallowAllEvents(InputListener::EventMaskTouch);
}

Button *AppBar::getNavNode() const { return _navButton; }

void AppBar::setBarCallback(Function<void()> &&cb) { _barCallback = sp::move(cb); }

const Function<void()> &AppBar::getBarCallback() const { return _barCallback; }

void AppBar::handleNavTapped() {
	if (_navCallback) {
		_navCallback();
	}
}

void AppBar::updateProgress() {
	if (_replaceProgress == 1.0f) {
		if (_prevComposer) {
			_prevComposer->removeFromParent();
			_prevComposer = nullptr;
		}
	}

	if (_iconsComposer) {
		_iconsComposer->setPositionY(
				progress(_iconsComposer->getContentSize().height, 0.0f, _replaceProgress));
	}
	if (_prevComposer) {
		_prevComposer->setPositionY(
				progress(0.0f, -_prevComposer->getContentSize().height, _replaceProgress));
		_prevComposer->setOpacity(1.0f - _replaceProgress);
	}
}

float AppBar::updateMenu(Node *composer, MenuSource *source, size_t maxIcons) {
	composer->removeAllChildren();
	composer->setContentSize(_contentSize);

	float baseline = getBaseLine();
	size_t iconsCount = 0;
	auto extMenuSource = Rc<MenuSource>::create();
	Vector<Button *> icons;
	bool hasExtMenu = false;

	if (source) {
		auto &menuItems = source->getItems();
		for (auto &item : menuItems) {
			if (item->getType() == MenuSourceItem::Type::Button) {
				auto btnSrc = dynamic_cast<MenuSourceButton *>(item.get());
				if (btnSrc->getNameIcon() != IconName::None) {
					if (iconsCount < maxIcons) {
						auto btn = composer->addChild(Rc<Button>::create(NodeStyle::Text),
								ZOrder(0), iconsCount);
						btn->setMenuSourceButton(btnSrc);
						btn->setIconSize(24.0f);
						btn->setFollowContentSize(false);
						btn->setSwallowEvents(true);
						btn->setNodeMask(Button::LeadingIcon);
						icons.push_back(btn);
						iconsCount++;
					} else {
						extMenuSource->addItem(item);
					}
				}
			}
		}
	}

	if (extMenuSource->count() > 0) {
		auto btn = Rc<Button>::create();
		auto source = Rc<MenuSourceButton>::create(StringView(),
				IconName::Navigation_more_vert_solid, move(extMenuSource));
		btn->setMenuSourceButton(move(source));
		btn->setFollowContentSize(false);
		btn->setSwallowEvents(true);
		btn->setNodeMask(Button::LeadingIcon);
		icons.push_back(btn);
		composer->addChild(move(btn), ZOrder(0), iconsCount);
		hasExtMenu = true;
	} else {
		hasExtMenu = false;
	}

	if (icons.size() > 0) {
		if (icons.back()->getLeadingIconName() == IconName::Navigation_more_vert_solid) {
			hasExtMenu = true;
		}
		auto pos =
				composer->getContentSize().width - 56 * (icons.size() - 1) - (hasExtMenu ? 8 : 36);
		for (auto &it : icons) {
			it->setContentSize(Size2(48, std::min(48.0f, getRealHeight())));
			it->setAnchorPoint(Vec2(0.5, 0.5));
			it->setPosition(Vec2(pos, baseline));
			pos += 56;
		}
		if (hasExtMenu) {
			icons.back()->setContentSize(Size2(24, std::min(48.0f, getRealHeight())));
			icons.back()->setPosition(Vec2(composer->getContentSize().width - 24, baseline));
		}
	}

	if (icons.size() > 0) {
		return (56 * (icons.size()) - (hasExtMenu ? 24 : 0));
	}
	return 0;
}

void AppBar::layoutSubviews() {
	_scissorNode->setContentSize(_contentSize);

	updateProgress();

	_iconsComposer->setContentSize(_scissorNode->getContentSize());
	auto iconWidth = updateMenu(_iconsComposer, _actionMenuSourceListener->getSubscription(),
			_maxActionIcons);
	if (_replaceProgress != 1.0f && _iconWidth != 0.0f) {
		_iconWidth = std::max(iconWidth, _iconWidth);
	} else {
		_iconWidth = iconWidth;
	}

	float baseline = getBaseLine();
	if (_navButton->getLeadingIconName() != IconName::Empty
			&& _navButton->getLeadingIconName() != IconName::None) {
		_navButton->setContentSize(Size2(48.0f, 48.0f));
		_navButton->setAnchorPoint(Anchor::Middle);
		_navButton->setPosition(Vec2(32.0f, baseline));
		_navButton->setVisible(true);
	} else {
		_navButton->setVisible(false);
	}

	float labelStart = (getNavButtonIcon() == IconName::None) ? 16.0f : 64.0f;
	float labelEnd = _contentSize.width - _iconWidth - 16.0f;

	_label->setMaxWidth(labelEnd - labelStart);

	switch (_layout) {
	case AppBarLayout::CenterAligned:
		_label->setAnchorPoint(Anchor::Middle);
		_label->setAlignment(Label::TextAlign::Center);
		_label->setPosition(Vec2((labelStart + labelEnd) / 2.0f, baseline));
		break;
	case AppBarLayout::Small:
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setAlignment(Label::TextAlign::Left);
		_label->setPosition((getNavButtonIcon() == IconName::None) ? Vec2(16.0f, baseline)
																   : Vec2(64.0f, baseline));
		break;
	case AppBarLayout::Minified:
		_label->setRole(TypescaleRole::TitleMedium);
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setAlignment(Label::TextAlign::Left);
		_label->setPosition((getNavButtonIcon() == IconName::None) ? Vec2(16.0f, baseline)
																   : Vec2(64.0f, baseline));
		break;
	default: break;
	}
}

float AppBar::getBaseLine() const {
	auto v = _defaultHeight;
	if (!isnan(_basicHeight)) {
		v = std::max(v, _basicHeight);
	}
	if (_contentSize.height > _basicHeight) {
		return _contentSize.height - v / 2;
	} else {
		return v / 2;
	}
}

float AppBar::getRealHeight() const {
	auto v = _defaultHeight;
	if (!isnan(_basicHeight)) {
		v = std::max(v, _basicHeight);
	}
	return v;
}

void AppBar::updateDefaultHeight() {
	switch (_layout) {
	case AppBarLayout::CenterAligned:
	case AppBarLayout::Small:
	case AppBarLayout::Medium:
	case AppBarLayout::Large: _defaultHeight = 56.0f; break;
	case AppBarLayout::Minified: _defaultHeight = 32.0f; break;
	}

	_minHeight = 0.0f;
	_maxHeight = getRealHeight();
}

} // namespace stappler::xenolith::material2d
