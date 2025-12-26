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

#include "MaterialTabBar.h"
#include "MaterialStyleContainer.h"
#include "XLFontLocale.h"
#include "XLInputListener.h"
#include "XLFontLocale.h"
#include "XLAction.h"
#include "XLActionEase.h"
#include "XLDirector.h"
#include "XLAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

constexpr static float TAB_MIN_WIDTH = 72.0f;
constexpr static float TAB_MAX_WIDTH = 264.0f;

void TabBarButton::initialize(const TabButtonCallback &cb, TabBar::ButtonStyle style, bool swallow,
		bool wrapped) {
	_tabButtonCallback = cb;
	setSwallowEvents(swallow);

	_tabStyle = style;
	_wrapped = wrapped;

	_labelText->setLocaleEnabled(true);
	_labelText->setTextTransform(font::TextTransform::Uppercase);
}

bool TabBarButton::init(MenuSourceButton *btn, const TabButtonCallback &cb,
		TabBar::ButtonStyle style, bool swallow, bool wrapped) {
	if (!Button::init(NodeStyle::Text, ColorRole::Secondary)) {
		return false;
	}

	setShapeStyle(ShapeStyle::None);
	setTapCallback(std::bind(&TabBarButton::onTabButton, this));

	initialize(cb, style, swallow, wrapped);
	setMenuSourceButton(btn);
	_followContentSize = false;
	return true;
}

bool TabBarButton::init(MenuSource *source, const TabButtonCallback &cb, TabBar::ButtonStyle style,
		bool swallow, bool wrapped) {
	if (!Button::init(NodeStyle::Text, ColorRole::Secondary)) {
		return false;
	}

	initialize(cb, style, swallow, wrapped);

	//_labelText->setString("SystemMore"_locale);
	_leadingIcon->setIconName(IconName::Navigation_more_vert_solid);
	_floatingMenuSource = source;
	_followContentSize = false;
	return true;
}

void TabBarButton::onTabButton() {
	if (_tabButtonCallback) {
		_tabButtonCallback(this, _menuButtonListener->getSubscription());
	}
}

void TabBarButton::layoutContent() {
	_labelValue->setVisible(false);
	_trailingIcon->setVisible(false);

	switch (_tabStyle) {
	case TabBar::ButtonStyle::Icon:
		_labelText->setVisible(false);
		_leadingIcon->setVisible(true);
		_leadingIcon->setAnchorPoint(Vec2(0.5f, 0.5f));
		_leadingIcon->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
		break;
	case TabBar::ButtonStyle::Title:
		_leadingIcon->setVisible(false);
		_labelText->setVisible(true);
		_labelText->setAlignment(font::TextAlign::Center);
		_labelText->setFontWeight(
				_selected ? font::FontWeight::SemiBold : font::FontWeight::Regular);
		_labelText->setFontSize(14);
		_labelText->setWidth(_contentSize.width - (_wrapped ? 32.0f : 16.0f)
				- (_floatingMenuSource ? 16.0f : 0.0f));
		_labelText->tryUpdateLabel();
		if (_labelText->getLinesCount() > 1) {
			_labelText->setFontSize(12);
		}
		if (_floatingMenuSource) {
			_leadingIcon->setVisible(true);
			_leadingIcon->setIconName(IconName::Navigation_arrow_drop_down_solid);
			_leadingIcon->setAnchorPoint(Vec2(1.0f, 0.5f));
			_labelText->setAnchorPoint(Vec2(0.5f, 0.5f));
			_labelText->setPosition(
					Vec2((_contentSize.width) / 2.0f - 8.0f, _contentSize.height / 2.0f));
			_leadingIcon->setPosition(
					Vec2(_labelText->getPosition().x + _labelText->getContentSize().width / 2.0f,
							_contentSize.height / 2.0f));
			_leadingIcon->setAnchorPoint(Vec2(0.0f, 0.5f));
		} else {
			_labelText->setAnchorPoint(Vec2(0.5f, 0.5f));
			_labelText->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
		}
		break;
	case TabBar::ButtonStyle::TitleIcon:
		_leadingIcon->setVisible(true);
		_labelText->setVisible(true);
		_labelText->setAlignment(font::TextAlign::Center);

		_leadingIcon->setAnchorPoint(Vec2(0.5f, 0.5f));
		_leadingIcon->setPosition(
				Vec2(_contentSize.width / 2.0f, (_contentSize.height / 2.0f - 3.0f) + 13.0f));
		_labelText->setFontWeight(
				_selected ? font::FontWeight::SemiBold : font::FontWeight::Regular);
		_labelText->setFontSize(12);
		_labelText->setWidth(_contentSize.width - (_wrapped ? 30.0f : 14.0f));
		_labelText->setAnchorPoint(Vec2(0.5f, 0.5f));
		_labelText->setPosition(
				Vec2(_contentSize.width / 2.0f, (_contentSize.height / 2.0f - 3.0f) - 13.0f));
		_labelText->setMaxLines(1);
		_labelText->setAdjustValue(4);
		break;
	}
}

bool TabBar::init(MenuSource *source, ButtonStyle button, BarStyle bar, Alignment align) {
	if (!Surface::init(SurfaceStyle(NodeStyle::Filled))) {
		return false;
	}

	_alignment = align;
	_buttonStyle = button;
	_barStyle = bar;

	_source = addSystem(Rc<DataListener<MenuSource>>::create(
			[this](SubscriptionFlags flags) { onMenuSource(); }));
	_source->setSubscription(source);

	if (_source && _source->getSubscription()) {
		size_t idx = 0;
		for (auto &it : _source->getSubscription()->getItems()) {
			if (auto btn = dynamic_cast<MenuSourceButton *>(it.get())) {
				if (btn->isSelected()) {
					_selectedIndex = idx;
					break;
				}
				++idx;
			}
		}
	}

	_scroll = addChild(Rc<ScrollView>::create(ScrollView::Horizontal));
	_scroll->setController(Rc<ScrollController>::create());
	_scroll->setOverscrollVisible(false);
	_scroll->setIndicatorVisible(false);
	_scroll->setScrollCallback(std::bind(&TabBar::onScrollPosition, this));

	_layer = _scroll->getRoot()->addChild(Rc<Layer>::create(), ZOrder(1));
	_layer->setAnchorPoint(Vec2(0.0f, 0.0f));
	_layer->setVisible(false);

	_left = addChild(Rc<IconSprite>::create(IconName::Navigation_chevron_left_solid));
	auto leftListener = _left->addSystem(Rc<InputListener>::create());
	leftListener->addTouchRecognizer([](const GestureData &) -> bool { return true; },
			InputListener::makeButtonMask({InputMouseButton::Touch}));
	leftListener->setSwallowEvents(InputListener::EventMaskTouch);

	_right = addChild(Rc<IconSprite>::create(IconName::Navigation_chevron_right_solid));
	auto rightListener = _right->addSystem(Rc<InputListener>::create());
	rightListener->addTouchRecognizer([](const GestureData &) -> bool { return true; },
			InputListener::makeButtonMask({InputMouseButton::Touch}));
	rightListener->setSwallowEvents(InputListener::EventMaskTouch);

	return true;
}

void TabBar::handleContentSizeDirty() {
	struct ItemData {
		float width = 0.0f;
		MenuSourceButton *button = nullptr;
		bool primary = true;
		bool wrapped = false;
	};

	Surface::handleContentSizeDirty();

	if (_buttonCount == 0) {
		auto c = _scroll->getController();
		c->clear();
		return;
	}

	float width = 0.0f;
	Vector<ItemData> itemWidth;
	itemWidth.reserve(_buttonCount);

	if (_source && _source->getSubscription()) {
		auto &items = _source->getSubscription()->getItems();
		for (auto &item : items) {
			if (auto btn = dynamic_cast<MenuSourceButton *>(item.get())) {
				bool wrapped = false;
				auto w = getItemSize(btn->getName(), false, btn->isSelected());
				if (w > TAB_MAX_WIDTH) {
					wrapped = true;
					w = TAB_MAX_WIDTH - 60.0f;
				}
				width += w;
				itemWidth.push_back(ItemData{w, btn, true, wrapped});
			}
		}
	}

	if (_contentSize.width == 0.0f) {
		return;
	}

	float extraWidth = TAB_MIN_WIDTH;
	Rc<MenuSource> extraSource;
	if ((_barStyle == BarStyle::Layout) && width > _contentSize.width) {
		width = 0.0f;
		ItemData *prev = nullptr;
		for (auto &it : itemWidth) {
			if (extraSource != nullptr) {
				extraSource->addItem(it.button);
				it.primary = false;
			} else {
				if (width + it.width > _contentSize.width) {
					extraSource = Rc<MenuSource>::create();
					extraSource->addItem(prev->button);
					extraSource->addItem(it.button);
					prev->primary = false;
					it.primary = false;
					extraWidth = getItemSize("SystemMore"_locale, true);
					width += extraWidth;
					width -= prev->width;
				} else {
					width += it.width;
					prev = &it;
				}
			}
		}
	}

	float scale = 1.0f;
	if (_alignment == Alignment::Justify) {
		if (width < _contentSize.width) {
			scale = _contentSize.width / width;
		}
		width = _contentSize.width;
	}

	auto pos = _scroll->getScrollRelativePosition();
	auto c = _scroll->getController();
	c->clear();

	_positions.clear();

	float currentWidth = 0.0f;
	for (auto &it : itemWidth) {
		if (it.primary) {
			c->addItem(std::bind(&TabBar::onItem, this, it.button, it.wrapped), it.width * scale);
			_positions.emplace_back(currentWidth, it.width * scale);
			currentWidth += it.width * scale;
		} else {
			_positions.emplace_back(nan(), nan());
		}
	}

	if (extraSource) {
		_extra = extraSource;
		c->addItem(std::bind(&TabBar::onItem, this, nullptr, false), extraWidth * scale);
	}

	_scrollWidth = width;
	if (_contentSize.width >= _scrollWidth) {
		_left->setVisible(false);
		_right->setVisible(false);

		_scroll->setContentSize(Size2(_scrollWidth, _contentSize.height));
		switch (_alignment) {
		case Alignment::Right:
			_scroll->setAnchorPoint(Vec2(1.0f, 0.5f));
			_scroll->setPosition(Vec2(_contentSize.width, _contentSize.height / 2.0f));
			break;
		case Alignment::Center:
		case Alignment::Justify:
			_scroll->setAnchorPoint(Vec2(0.5f, 0.5f));
			_scroll->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
			break;
		case Alignment::Left:
			_scroll->setAnchorPoint(Vec2(0.0f, 0.5f));
			_scroll->setPosition(Vec2(0.0f, _contentSize.height / 2.0f));
			break;
		default: break;
		}

		_scroll->updateScrollBounds();
		_scroll->setScrollPosition(0.0f);
	} else {
		_left->setVisible(true);
		_right->setVisible(true);

		_left->setAnchorPoint(Vec2(0.5f, 0.5f));
		_left->setPosition(Vec2(16.0f, _contentSize.height / 2.0f));

		_right->setAnchorPoint(Vec2(0.5f, 0.5f));
		_right->setPosition(Vec2(_contentSize.width - 16.0f, _contentSize.height / 2.0f));

		_scroll->setAnchorPoint(Vec2(0.5f, 0.5f));
		_scroll->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
		_scroll->setContentSize(Size2(_contentSize.width - 64.0f, _contentSize.height));
		_scroll->setScrollRelativePosition(pos);
		_scroll->updateScrollBounds();
		_scroll->getController()->onScrollPosition(true);

		onScrollPositionProgress(0.5f);
	}

	setSelectedTabIndex(_selectedIndex);
}

void TabBar::setMenuSource(MenuSource *source) {
	if (_source->getSubscription() != source) {
		_source->setSubscription(source);
		_selectedIndex = maxOf<size_t>();

		if (_source && _source->getSubscription()) {
			auto &items = _source->getSubscription()->getItems();
			size_t idx = 0;
			for (auto &it : items) {
				if (auto btn = dynamic_cast<MenuSourceButton *>(it.get())) {
					if (btn->isSelected()) {
						_selectedIndex = idx;
						break;
					}
					++idx;
				}
			}
		}
	}
}

MenuSource *TabBar::getMenuSource() const { return _source->getSubscription(); }

void TabBar::setAccentColor(const ColorRole &color) {
	if (_accentColor != color) {
		_accentColor = color;
	}
}
ColorRole TabBar::getAccentColor() const { return _accentColor; }

void TabBar::setButtonStyle(const ButtonStyle &btn) {
	if (_buttonStyle != btn) {
		_buttonStyle = btn;
		_contentSizeDirty = true;
	}
}
const TabBar::ButtonStyle &TabBar::getButtonStyle() const { return _buttonStyle; }

void TabBar::setBarStyle(const BarStyle &bar) {
	if (_barStyle != bar) {
		_barStyle = bar;
		_contentSizeDirty = true;
	}
}
const TabBar::BarStyle &TabBar::getBarStyle() const { return _barStyle; }

void TabBar::setAlignment(const Alignment &a) {
	if (_alignment != a) {
		_alignment = a;
		_contentSizeDirty = true;
	}
}
const TabBar::Alignment &TabBar::getAlignment() const { return _alignment; }

void TabBar::onMenuSource() {
	_buttonCount = 0;
	auto &items = _source->getSubscription()->getItems();
	for (auto &item : items) {
		if (dynamic_cast<MenuSourceButton *>(item.get())) {
			++_buttonCount;
		}
	}
	_contentSizeDirty = true;
}

float TabBar::getItemSize(const String &name, bool extended, bool selected) const {
	auto fc = _director->getApplication()->getExtension<font::FontController>();
	if (_buttonStyle == ButtonStyle::Icon) {
		return TAB_MIN_WIDTH;
	} else if (_buttonStyle == ButtonStyle::Title) {
		auto desc = TypescaleLabel::getTypescaleRoleStyle(TypescaleRole::BodyLarge, _inputDensity);
		desc.font.fontWeight = selected ? font::FontWeight::SemiBold : font::FontWeight::Regular;

		float width = Label::getStringWidth(fc, desc, name, true) + 32.0f;
		if (extended) {
			width += 16.0f;
		}
		return std::max(width, TAB_MIN_WIDTH);
	} else {
		auto desc = TypescaleLabel::getTypescaleRoleStyle(TypescaleRole::BodyLarge, _inputDensity);
		desc.font.fontSize = font::FontSize(12);
		desc.font.fontWeight = selected ? font::FontWeight::SemiBold : font::FontWeight::Regular;

		float width = Label::getStringWidth(fc, desc, name, true);
		if (width > 72.0f) {
			desc.font.fontSize = font::FontSize(8);
			width = ceilf(Label::getStringWidth(fc, desc, name, true));
		}
		return std::max(width + 16.0f, TAB_MIN_WIDTH);
	}
}

Rc<Node> TabBar::onItem(MenuSourceButton *btn, bool wrapped) {
	Rc<TabBarButton> ret = nullptr;
	if (btn) {
		ret = Rc<TabBarButton>::create(btn, [this](Button *b, MenuSourceButton *btn) {
			onTabButton(b, btn);
		}, _buttonStyle, _barStyle == BarStyle::Layout, wrapped);
	} else {
		ret = Rc<TabBarButton>::create(_extra, [this](Button *b, MenuSourceButton *btn) {
			onTabButton(b, btn);
		}, _buttonStyle, _barStyle == BarStyle::Layout, wrapped);
	}

	/*if (ret) {
		ret->setTextColor(_textColor);
		ret->setSelectedColor(_selectedColor);
	}*/

	return ret;
}

void TabBar::onTabButton(Button *b, MenuSourceButton *btn) {
	auto &items = _source->getSubscription()->getItems();
	for (auto &it : items) {
		auto b = dynamic_cast<MenuSourceButton *>(it.get());
		b->setSelected(false);
	}
	btn->setSelected(true);
	size_t idx = 0;
	for (auto &it : items) {
		if (btn == it) {
			break;
		}
		++idx;
	}
	if (idx != _selectedIndex) {
		auto &cb = btn->getCallback();
		if (cb) {
			cb(b, btn);
		}
	}
	if (idx < size_t(items.size())) {
		setSelectedTabIndex(idx);
	} else {
		setSelectedTabIndex(maxOf<size_t>());
	}
}

void TabBar::onScrollPosition() {
	if (_scrollWidth > _contentSize.width) {
		onScrollPositionProgress(_scroll->getScrollRelativePosition());
	}
}

void TabBar::onScrollPositionProgress(float pos) {
	if (pos <= 0.01f) {
		_left->setOpacity(64);
		_right->setOpacity(222);
	} else if (pos >= 0.99f) {
		_left->setOpacity(222);
		_right->setOpacity(64);
	} else {
		_left->setOpacity(222);
		_right->setOpacity(222);
	}
}

void TabBar::setSelectedTabIndex(size_t idx) {
	if (idx == maxOf<size_t>() || idx >= _positions.size()) {
		_layer->setVisible(false);
	} else {
		auto pos = _positions[idx];

		if (_selectedIndex == maxOf<size_t>()) {
			_layer->setVisible(true);
			_layer->setOpacity(1.0f);

			_layer->setPosition(Vec2(pos.first, 0.0f));
			_layer->setContentSize(Size2(pos.second, 2.0f));

			_layer->stopAllActionsByTag("TabBarAction"_tag);
			auto spawn = Rc<EaseActionTyped>::create(Rc<FadeTo>::create(0.15f, 1.0f),
					EaseActionTyped::Type::QuartEaseOut);
			_layer->runAction(spawn, "TabBarAction"_tag);
		} else {
			_layer->setVisible(true);
			_layer->setOpacity(1.0f);
			_layer->stopAllActionsByTag("TabBarAction"_tag);
			auto spawn = Rc<EaseActionTyped>::create(
					Rc<Spawn>::create(Rc<MoveTo>::create(0.35f, Vec2(pos.first, 0.0f)),
							Rc<ResizeTo>::create(0.35f, Size2(pos.second, 2.0f)),
							Rc<FadeTo>::create(0.15f, 1.0f)),
					EaseActionTyped::Type::QuartEaseOut);
			_layer->runAction(spawn, "TabBarAction"_tag);

			float scrollPos = _scroll->getScrollPosition();
			float scrollSize = _scroll->getScrollSize();

			if (scrollPos > pos.first) {
				if (idx == 0) {
					onScrollPositionProgress(0.0f);
					_scroll->runAdjustPosition(pos.first);
				} else {
					_scroll->runAdjustPosition(pos.first - _positions[idx - 1].second / 2.0f);
				}
			} else if (scrollPos + scrollSize < pos.first + pos.second) {
				if (idx + 1 == _positions.size()) {
					onScrollPositionProgress(1.0f);
					_scroll->runAdjustPosition(pos.first + pos.second - _scroll->getScrollSize());
				} else {
					_scroll->runAdjustPosition(pos.first + pos.second - _scroll->getScrollSize()
							+ _positions[idx + 1].second / 2.0f);
				}
			}
		}
	}
	_selectedIndex = idx;
}

void TabBar::setSelectedIndex(size_t nidx) {
	if (_source) {
		auto &items = _source->getSubscription()->getItems();

		size_t idx = 0;
		for (auto &it : items) {
			if (auto b = dynamic_cast<MenuSourceButton *>(it.get())) {
				if (idx == nidx) {
					b->setSelected(true);
					setSelectedTabIndex(idx);
				} else {
					b->setSelected(false);
				}
				++idx;
			}
		}
	}
}

size_t TabBar::getSelectedIndex() const { return _selectedIndex; }

void TabBar::setProgress(float prog) {
	if (_layer->getActionByTag("TabBarAction"_tag) == nullptr) {
		if (prog > 0.0f) {
			if (_selectedIndex + 1 < _positions.size()) {
				auto &origin = _positions[_selectedIndex];
				auto &next = _positions[_selectedIndex + 1];
				_layer->setPositionX(progress(origin.first, next.first, prog));
				_layer->setContentSize(Size2(progress(origin.second, next.second, prog),
						_layer->getContentSize().height));
			}
		} else if (prog < 0.0f) {
			prog = -prog;
			if (_selectedIndex > 0) {
				auto &origin = _positions[_selectedIndex];
				auto &next = _positions[_selectedIndex - 1];
				_layer->setPositionX(progress(origin.first, next.first, prog));
				_layer->setContentSize(Size2(progress(origin.second, next.second, prog),
						_layer->getContentSize().height));
			}
		}
	}
}

void TabBar::applyStyle(StyleContainer *c, const SurfaceStyleData &style) {
	Surface::applyStyle(c, style);

	if (auto scheme = c->getScheme(style.schemeTag)) {
		auto c = scheme->get(_accentColor);
		_layer->setColor(c, false);

		_left->setColor(style.colorOn);
		_right->setColor(style.colorOn);
	}
}

} // namespace stappler::xenolith::material2d
