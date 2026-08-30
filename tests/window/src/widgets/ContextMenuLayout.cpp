/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "widgets/ContextMenuLayout.h"
#include "XL2dLayer.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// The menu's own scene does not inherit the application's sheet, so the popup carries this one.
// Only the surface is styled: what the check reads is the item NAMES, which are node ids.
static constexpr auto s_menuCss = StringView(R"css(
menu               { background-color: #23262c; border-radius: 6px; }
menu-item > label  { color: #e8eaed; font-size: 14px; }
)css");

// Where the regions sit inside the layout's work area. Anchored bottom-left, so every rect here is
// its own origin in the layout's space.
static constexpr auto Region =
		Rect(60.0f, 60.0f, ContextMenuLayout::RegionWidth, ContextMenuLayout::RegionHeight);

// All four are INSIDE the region, so each one is also a test that the topmost target answers
static constexpr auto Under = Rect(40.0f, 240.0f, 220.0f, 140.0f);
static constexpr auto Over = Rect(120.0f, 280.0f, 120.0f, 70.0f);
static constexpr auto Hidden = Rect(320.0f, 280.0f, 160.0f, 100.0f);
static constexpr auto Blocked = Rect(40.0f, 60.0f, 200.0f, 120.0f);
static constexpr auto Swallow = Rect(320.0f, 60.0f, 200.0f, 120.0f);

} // namespace

bool ContextMenuLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_region = addRegion("region", Region, ZOrder(0), Color::BlueGrey_800);
	_under = _region->addChild(Rc<basic2d::Layer>::create(Color::Teal_700), ZOrder(1));
	_under->setName("under");
	_under->setAnchorPoint(Anchor::BottomLeft);
	_under->setPosition(Under.origin);
	_under->setContentSize(Under.size);

	// A distinct ZOrder, not a bigger one by accident: sortAllChildren is unstable, so two children
	// at the same order permute between frames and "topmost" would mean nothing
	_over = _region->addChild(Rc<basic2d::Layer>::create(Color::Amber_600), ZOrder(5));
	_over->setName("over");
	_over->setAnchorPoint(Anchor::BottomLeft);
	_over->setPosition(Over.origin);
	_over->setContentSize(Over.size);

	_hidden = _region->addChild(Rc<basic2d::Layer>::create(Color::Red_500), ZOrder(5));
	_hidden->setName("hidden");
	_hidden->setAnchorPoint(Anchor::BottomLeft);
	_hidden->setPosition(Hidden.origin);
	_hidden->setContentSize(Hidden.size);
	_hidden->setVisible(false);

	_blocked = _region->addChild(Rc<basic2d::Layer>::create(Color::Grey_600), ZOrder(3));
	_blocked->setName("blocked");
	_blocked->setAnchorPoint(Anchor::BottomLeft);
	_blocked->setPosition(Blocked.origin);
	_blocked->setContentSize(Blocked.size);

	_swallow = _region->addChild(Rc<basic2d::Layer>::create(Color::Indigo_500), ZOrder(3));
	_swallow->setName("swallow");
	_swallow->setAnchorPoint(Anchor::BottomLeft);
	_swallow->setPosition(Swallow.origin);
	_swallow->setContentSize(Swallow.size);

	/* The other way to keep a context menu away: take the press.

	This widget has no target at all - the context menu has never heard of it - and stops the menu
	by swallowing the right button, which turns the coordinator's tap into a Cancel. That is the
	mechanism a widget with a gesture of its own uses; the empty target above is the one a widget
	with no menu uses. */
	/* An ordinary widget's click, which is what a dismissal has to be spent instead of.

	The region counts left taps the way any button would, so "the click closed the menu and stopped
	there" is told from "the click closed the menu and also pressed what was under it" - the second
	is what makes a menu feel like it swallowed a command. */
	auto regionListener = _region->addSystem(Rc<InputListener>::create());
	regionListener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			++_regionTaps;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseLeft}), 1});

	auto listener = _swallow->addSystem(Rc<InputListener>::create());
	listener->setSwallowEvent(InputEventName::Begin);
	listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			++_swallowTaps;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseRight}), 1});

	return true;
}

void ContextMenuLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// Pinned to the work area's top-left, so every expected number is derived from the constants
	// above and a window resize does not move them
	if (_region) {
		_region->setPosition(Vec2(Region.origin.x, getWorkTop() - Region.size.height - 20.0f));
	}
}

void ContextMenuLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);

	// NOT in init(): a node has no scene until it is added to one, and acquireForNode installs the
	// system on the scene's content node
	_menus = ui::ContextMenuSystem::acquireForNode(this);
	if (!_menus) {
		return;
	}

	_menus->setMenuConfigCallback([](ui::MenuConfig &config) {
		config.stylesheetSource = s_menuCss.str<Interface>();
		config.idPrefix = String("ctx");
	});

	// The region answers with different items for its two halves. `req.location` is in the
	// REGION's space, so the halves are its own, not the window's
	ui::setContextMenu(_region, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
		++_builderCalls;
		_lastRequest = req.location;
		_lastFromTouch = req.fromTouch;
		_lastTarget = String("region");
		return makeMenu(req.location.x < RegionWidth / 2.0f ? "left" : "right", 2);
	});

	ui::setContextMenu(_under, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
		++_builderCalls;
		_lastRequest = req.location;
		_lastFromTouch = req.fromTouch;
		_lastTarget = String("under");
		return makeMenu("under", 1);
	});

	ui::setContextMenu(_over, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
		++_builderCalls;
		_lastRequest = req.location;
		_lastFromTouch = req.fromTouch;
		_lastTarget = String("over");
		return makeMenu("over", 1);
	});

	ui::setContextMenu(_hidden, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
		++_builderCalls;
		_lastTarget = String("hidden");
		return makeMenu("hidden", 1);
	});

	// Offers nothing, and that is the point: it must stop the region's menu instead of letting it
	// through
	ui::setContextMenu(_blocked, [this](const ui::ContextMenuRequest &) -> Rc<ui::MenuSource> {
		++_builderCalls;
		_lastTarget = String("blocked");
		return nullptr;
	});
}

Rc<ui::MenuSource> ContextMenuLayout::makeMenu(StringView prefix, size_t count) {
	auto source = Rc<ui::MenuSource>::create();
	for (size_t i = 1; i <= count; ++i) {
		auto name = string::toString<Interface>(prefix, "-", i);
		source->addButton(name, name, [this, name](NotNull<ui::MenuSourceButton>) {
			++_activations;
			_lastItem = name;
		});
	}
	return source;
}

Node *ContextMenuLayout::addRegion(StringView name, const Rect &rect, ZOrder z,
		const Color4F &color) {
	auto node = addChild(Rc<basic2d::Layer>::create(color), z);
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setPosition(rect.origin);
	node->setContentSize(rect.size);
	return node;
}

Value ContextMenuLayout::encodeRect(const Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}

	// Where to aim a synthetic pointer. Derived by the ENGINE rather than by the script summing
	// positions down the tree: the anchors along that chain are not all the same
	const auto world = node->getWorldBoundingBox();
	ret.setDouble(world.origin.x, "x");
	ret.setDouble(world.origin.y, "y");
	ret.setDouble(world.size.width, "width");
	ret.setDouble(world.size.height, "height");
	ret.setBool(node->isVisible(), "visible");
	return ret;
}

Value ContextMenuLayout::encodeState() const {
	Value ret;

	ret.setInteger(int64_t(_menus ? _menus->getTargetCount() : 0), "targetCount");

	// Whether the coordinator's own listener is there and live. A menu that never opens looks the
	// same whether the press was refused or never offered, and this is the difference
	if (_menus) {
		auto listener = _menus->getListener();
		ret.setBool(listener != nullptr, "hasListener");
		ret.setBool(listener && listener->isEnabled(), "listenerEnabled");
		ret.setBool(listener && listener->isRunning(), "listenerRunning");
	}
	ret.setBool(_menus && _menus->isMenuOpen(), "menuOpen");

	// Which target ANSWERED, which is not the same as which one had a menu: a refusal is an answer
	if (_menus) {
		if (auto target = _menus->getCurrentTarget()) {
			if (auto owner = target->getOwner()) {
				ret.setString(owner->getName(), "currentTarget");
			}
		}
	}

	ret.setString(_lastTarget, "lastBuilder");
	ret.setString(_lastItem, "lastItem");
	ret.setInteger(int64_t(_builderCalls), "builderCalls");
	ret.setInteger(int64_t(_opens), "opens");
	ret.setInteger(int64_t(_activations), "activations");
	ret.setInteger(int64_t(_swallowTaps), "swallowTaps");
	ret.setInteger(int64_t(_regionTaps), "regionTaps");
	ret.setBool(_lastFromTouch, "lastFromTouch");

	if (_lastRequest.isValid()) {
		Value pt;
		pt.setDouble(_lastRequest.x, "x");
		pt.setDouble(_lastRequest.y, "y");
		ret.setValue(sp::move(pt), "lastRequest");
	}

	Value rects;
	rects.setValue(encodeRect(_region), "region");
	rects.setValue(encodeRect(_under), "under");
	rects.setValue(encodeRect(_over), "over");
	rects.setValue(encodeRect(_hidden), "hidden");
	rects.setValue(encodeRect(_blocked), "blocked");
	rects.setValue(encodeRect(_swallow), "swallow");
	ret.setValue(sp::move(rects), "rects");
	return ret;
}

void ContextMenuLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("state", "Roster size, the open menu, the last request and the counters",
			[this](Value &&) { return encodeState(); });

	addCommand("open-at", "Open the menu at a world point: {x, y, touch} - the pointer's own call",
			[this](Value &&args) {
		const Value &in = args;
		bool ok = false;
		if (_menus) {
			ok = _menus->openAt(Vec2(float(in.getDouble("x")), float(in.getDouble("y"))),
					in.getBool("touch"));
		}
		auto ret = encodeState();
		ret.setBool(ok, "ok");
		if (ok) {
			++_opens;
		}
		return ret;
	});

	addCommand("close", "Take down whatever menu is open", [this](Value &&) {
		if (_menus) {
			_menus->close();
		}
		return encodeState();
	});

	addCommand("reset", "Zero the counters and forget the last request", [this](Value &&) {
		_builderCalls = 0;
		_opens = 0;
		_activations = 0;
		_swallowTaps = 0;
		_regionTaps = 0;
		_lastItem.clear();
		_lastTarget.clear();
		_lastRequest = Vec2::INVALID;
		_lastFromTouch = false;
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
