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

#include "widgets/TooltipLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto Region =
		Rect(60.0f, 60.0f, TooltipLayout::RegionWidth, TooltipLayout::RegionHeight);

static constexpr auto Plain = Rect(40.0f, 220.0f, 200.0f, 120.0f);
static constexpr auto Under = Rect(300.0f, 200.0f, 220.0f, 140.0f);
static constexpr auto Over = Rect(360.0f, 240.0f, 100.0f, 60.0f);
static constexpr auto Padded = Rect(60.0f, 60.0f, 120.0f, 80.0f);
static constexpr auto Disabled = Rect(320.0f, 40.0f, 180.0f, 100.0f);

} // namespace

bool TooltipLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_region = addBox(this, "region", Region, ZOrder(0), Color::BlueGrey_800);

	/* Declared HERE, in init(), which is the whole point of this one.

	Nothing is in a scene yet, so there is no content node to put a coordinator on. A hint used to
	be a listener that could acquire one the first time it was hovered; a component cannot notice
	anything, so the acquire is deferred to the node's entry. If that deferral broke, this hint - and
	every hint declared the way applications actually declare them - would silently never appear. */
	_plain = addBox(_region, "plain", Plain, ZOrder(1), Color::Teal_700);
	ui::setTooltip(_plain, "plain hint");

	_under = addBox(_region, "under", Under, ZOrder(1), Color::Indigo_500);
	ui::setTooltip(_under, "under hint");

	// A distinct ZOrder, not a bigger one by accident: sortAllChildren is unstable, so two children
	// at the same order permute between frames and "topmost" would mean nothing
	_over = addBox(_region, "over", Over, ZOrder(5), Color::Amber_600);
	ui::setTooltip(_over, "over hint");

	_padded = addBox(_region, "padded", Padded, ZOrder(1), Color::Green_600);
	ui::setTooltip(_padded,
			ui::TooltipInfo{
				.text = String("padded hint"),
				.hoverPadding = PadHover,
			});

	_disabled = addBox(_region, "disabled", Disabled, ZOrder(1), Color::Grey_600);
	ui::setTooltip(_disabled, "disabled hint");
	ui::setTooltipEnabled(_disabled, false);

	return true;
}

void TooltipLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_region) {
		_region->setPosition(Vec2(Region.origin.x, getWorkTop() - Region.size.height - 20.0f));
	}
}

Node *TooltipLayout::addBox(Node *parent, StringView name, const Rect &rect, ZOrder z,
		const Color4F &color) {
	auto node = parent->addChild(Rc<basic2d::Layer>::create(color), z);
	node->setName(name);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setPosition(rect.origin);
	node->setContentSize(rect.size);
	return node;
}

Node *TooltipLayout::findBox(StringView name) const {
	for (auto node : {_plain, _under, _over, _padded, _disabled, _region}) {
		if (node && node->getName() == name) {
			return node;
		}
	}
	return nullptr;
}

Value TooltipLayout::encodeState() const {
	Value ret;

	// findForNode, never acquireForNode: whether a coordinator exists at all is one of the answers
	auto tips = ui::TooltipSystem::findForNode(const_cast<TooltipLayout *>(this));
	ret.setBool(tips != nullptr, "hasSystem");
	if (tips) {
		auto name = [](Node *node) { return node ? node->getName() : StringView(); };
		ret.setString(name(tips->getHoveredTarget()), "hovered");
		ret.setString(name(tips->getPendingTarget()), "pending");
		ret.setString(name(tips->getCurrentTarget()), "shown");
		ret.setBool(tips->isVisible(), "visible");
		ret.setInteger(int64_t(tips->getHoverDelay().toMillis()), "delay");

		// Whether the one listener that resolves hover is there and live. A hint that never appears
		// looks the same whether the pointer was refused or never offered, and this is the
		// difference
		auto listener = tips->getHoverListener();
		ret.setBool(listener != nullptr, "hasListener");
		ret.setBool(listener && listener->isRunning(), "listenerRunning");

		// What the hint that is up SAYS, read out of the node it describes
		if (auto shown = tips->getCurrentTarget()) {
			if (auto comp = ui::getTooltip(shown)) {
				ret.setString(comp->info.text, "shownText");
			}
		}
	}

	Value rects;
	for (auto node : {_region, _plain, _under, _over, _padded, _disabled}) {
		if (!node) {
			continue;
		}
		Value item;
		const auto world = node->getWorldBoundingBox();
		item.setDouble(world.origin.x, "x");
		item.setDouble(world.origin.y, "y");
		item.setDouble(world.size.width, "width");
		item.setDouble(world.size.height, "height");
		rects.setValue(sp::move(item), node->getName());
	}
	ret.setValue(sp::move(rects), "rects");
	return ret;
}

void TooltipLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("state", "The coordinator, what is hovered, what is up, and every node's box",
			[this](Value &&) { return encodeState(); });

	addCommand("set-delay", "Set the scene's hover delay in milliseconds: {ms}",
			[this](Value &&args) {
		if (auto tips = ui::TooltipSystem::findForNode(this)) {
			tips->setHoverDelay(TimeInterval::milliseconds(uint64_t(args.getInteger("ms"))));
		}
		return encodeState();
	});

	addCommand("set-text", "Change a node's hint: {node, text}", [this](Value &&args) {
		if (auto node = findBox(args.getString("node"))) {
			ui::setTooltipText(node, args.getString("text"));
		}
		return encodeState();
	});

	addCommand("set-enabled", "Turn a node's hint on or off: {node, value}", [this](Value &&args) {
		if (auto node = findBox(args.getString("node"))) {
			ui::setTooltipEnabled(node, args.getBool("value"));
		}
		return encodeState();
	});

	addCommand("remove", "Take a node's hint away entirely: {node}", [this](Value &&args) {
		if (auto node = findBox(args.getString("node"))) {
			ui::removeTooltip(node);
		}
		return encodeState();
	});

	addCommand("move", "Slide a node by {node, dx, dy} - the pointer does not move",
			[this](Value &&args) {
		if (auto node = findBox(args.getString("node"))) {
			node->setPosition(node->getPosition().xy()
					+ Vec2(float(args.getDouble("dx")), float(args.getDouble("dy"))));
		}
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
