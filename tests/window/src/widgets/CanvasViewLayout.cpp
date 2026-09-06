/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "widgets/CanvasViewLayout.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// The control's parts by the names the widget gives them. A test-only walk on purpose: the widget
// owes a caller the control and not a handle on each of its three pieces, and a stand that needs
// one is a stand, not an application.
static Node *CanvasViewLayout_find(Node *parent, StringView name) {
	if (!parent) {
		return nullptr;
	}
	for (auto &it : parent->getChildren()) {
		if (it->getName() == name) {
			return it;
		}
		if (auto found = CanvasViewLayout_find(it, name)) {
			return found;
		}
	}
	return nullptr;
}

// One at the origin, one up-and-right, one down-and-left. Two markers would not catch a sign error
// on either axis, and none of the three numbers is a multiple of another.
static const CanvasViewLayout::Marker s_markers[] = {
	{StringView("origin"), Vec2(0.0f, 0.0f), Size2(120.0f, 80.0f), Color4B(200, 90, 60, 255)},
	{StringView("far"), Vec2(430.0f, 270.0f), Size2(120.0f, 80.0f), Color4B(70, 150, 210, 255)},
	{StringView("near"), Vec2(-310.0f, -170.0f), Size2(120.0f, 80.0f), Color4B(120, 190, 110, 255)},
};

} // namespace

bool CanvasViewLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_canvas = addChild(Rc<ui::CanvasView>::create());
	_canvas->setAnchorPoint(Vec2(0.0f, 0.0f));
	_canvas->setPosition(Vec2(0.0f, 0.0f));
	_canvas->setName("canvas");

	// The stand keeps ONE listener and hands it to the widget, which is the arrangement the widget
	// exists to make possible - see its header on why it does not bring one of its own.
	auto listener = _canvas->addSystem(Rc<InputListener>::create());
	_canvas->attachGestures(listener);

	for (auto &m : s_markers) {
		auto node = _canvas->getWorld()->addChild(Rc<basic2d::Layer>::create(m.color));
		node->setAnchorPoint(Vec2(0.0f, 0.0f));
		node->setPosition(m.world);
		node->setContentSize(m.size);
		node->setName(m.name);
		_markers.emplace_back(m, node);
	}

	return true;
}

void CanvasViewLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_canvas) {
		_canvas->setContentSize(Size2(_contentSize.width, _contentSize.height - CaptionHeight));
	}
}

/* BOTH NUMBERS FOR THE SAME MARKER, and that is the whole design of this stand.

`math` is where `Viewport::toScreen` says the marker's world origin lands on the canvas. `node` is
where the marker's node actually is, read out of the live scene through the same conversion the
input path uses. They must be equal - and they are two different roads to the answer, so a widget
whose transform drifted from its own arithmetic fails here and nowhere else. */
Value CanvasViewLayout::encodeState() const {
	Value ret;

	const auto view = _canvas->getViewport();

	auto &v = ret.newDict("viewport");
	v.setDouble(view.offset.x, "x");
	v.setDouble(view.offset.y, "y");
	v.setDouble(view.zoom, "zoom");
	v.setDouble(view.screenSize.x, "width");
	v.setDouble(view.screenSize.y, "height");

	ret.setBool(_canvas->isClipped(), "clipped");

	/* THE FLOATING CONTROL, and the two button centres in SCENE coordinates rather than its own.

	A check that pressed the buttons through a socket shortcut would prove the callbacks and nothing
	about the control being reachable; the point of reporting where a pointer would land is that the
	check can press them with a real click, and a control laid out off the surface fails. */
	if (auto zoom = _canvas->getZoomControl()) {
		auto &z = ret.newDict("zoomControl");
		z.setBool(true, "enabled");
		z.setDouble(zoom->getPosition().x, "x");
		z.setDouble(zoom->getPosition().y, "y");
		z.setDouble(zoom->getContentSize().width, "width");
		z.setDouble(zoom->getContentSize().height, "height");

		auto encodeAt = [&](StringView name, Node *node) {
			if (!node) {
				return;
			}
			auto &b = z.newDict(name);
			const auto size = node->getContentSize();
			const auto at = node->convertToWorldSpace(Vec2(size.width * 0.5f, size.height * 0.5f));
			b.setDouble(at.x, "x");
			b.setDouble(at.y, "y");
		};
		encodeAt("minus", CanvasViewLayout_find(zoom, "canvas-zoom-out"));
		encodeAt("plus", CanvasViewLayout_find(zoom, "canvas-zoom-in"));

		if (auto label = dynamic_cast<basic2d::Label *>(
					CanvasViewLayout_find(zoom, "canvas-zoom-value"))) {
			z.setString(label->getString8(), "value");
		}
	} else {
		auto &z = ret.newDict("zoomControl");
		z.setBool(false, "enabled");
	}

	auto &markers = ret.newDict("markers");
	for (auto &it : _markers) {
		auto &m = markers.newDict(it.first.name);

		auto &world = m.newDict("world");
		world.setDouble(it.first.world.x, "x");
		world.setDouble(it.first.world.y, "y");

		const auto byMath = view.toScreen(it.first.world);
		auto &math = m.newDict("math");
		math.setDouble(byMath.x, "x");
		math.setDouble(byMath.y, "y");

		// The node's own origin, converted out of the world node and into the canvas - the road the
		// picture actually takes.
		const auto inScene = it.second->convertToWorldSpace(Vec2(0.0f, 0.0f));
		const auto inCanvas = _canvas->convertToNodeSpace(inScene);
		auto &node = m.newDict("node");
		node.setDouble(inCanvas.x, "x");
		node.setDouble(inCanvas.y, "y");
	}

	return ret;
}

void CanvasViewLayout::registerCommands() {
	addCommand("state", "The viewport, and every marker by two roads: the math and the live node",
			[this](Value &&) { return encodeState(); });

	addCommand("set-view", "Put the viewport: {x, y, zoom}", [this](Value &&args) {
		auto view = _canvas->getViewport();
		view.offset = Vec2(float(args.getDouble("x")), float(args.getDouble("y")));
		view.zoom = float(args.getDouble("zoom"));
		_canvas->setViewport(view);
		return encodeState();
	});

	addCommand("zoom-at", "Zoom about a canvas point: {x, y, factor}", [this](Value &&args) {
		_canvas->zoomAt(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))),
				float(args.getDouble("factor")));
		return encodeState();
	});

	// The notch, through the SAME constant the wheel goes through. A check that spelled 1.1 itself
	// would be asserting its own arithmetic; this asserts the widget's.
	addCommand("wheel", "Zoom as the wheel would: {x, y, notches}", [this](Value &&args) {
		const auto notches = float(args.getDouble("notches"));
		_canvas->zoomAt(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))),
				sprt::geom::wheelZoomRatio(notches, ui::CanvasView::ZoomStepRatio));
		return encodeState();
	});

	/* A SCENE POINT, ANSWERED IN BOTH SPACES, and it is what makes "no parallax" checkable.

	Parallax is not a number the widget reports; it is a property of two moments - the world point
	under the pointer before a drag and the one under it after. A check that wanted to compute that
	itself would need this node's transform, which is exactly what it must not have to reproduce.
	So it asks for the answer at a point, drags, and asks again. */
	addCommand("probe", "A scene point in the canvas's spaces: {x, y}", [this](Value &&args) {
		const auto scene = Vec2(float(args.getDouble("x")), float(args.getDouble("y")));
		const auto node = _canvas->convertToNodeSpace(scene);
		const auto world = _canvas->worldLocation(scene);

		Value ret;
		auto &n = ret.newDict("node");
		n.setDouble(node.x, "x");
		n.setDouble(node.y, "y");
		auto &w = ret.newDict("world");
		w.setDouble(world.x, "x");
		w.setDouble(world.y, "y");
		return ret;
	});

	addCommand("fit", "Frame every marker", [this](Value &&) {
		sprt::geom::Bounds bounds;
		for (auto &it : _markers) {
			bounds.add(Rect(it.first.world.x, it.first.world.y, it.first.size.width,
					it.first.size.height));
		}
		_canvas->fit(bounds);
		return encodeState();
	});

	addCommand("set-clipped", "Turn the scissor on or off: {clipped}", [this](Value &&args) {
		_canvas->setClipped(args.getBool("clipped"));

		auto ret = encodeState();
		// Read back off the SYSTEM rather than off the flag, so that "clipped" being true and the
		// scissor being off cannot pass unnoticed.
		if (auto scissor = _canvas->getSystemByType<DynamicStateSystem>()) {
			ret.setBool(scissor->isScissorEnabled(), "scissorEnabled");
		}
		return ret;
	});

	addCommand("limits", "The zoom range this canvas was built with", [this](Value &&) {
		Value ret;
		ret.setDouble(_canvas->getZoomLimits().min, "min");
		ret.setDouble(_canvas->getZoomLimits().max, "max");
		ret.setDouble(ui::CanvasView::ZoomStepRatio, "stepRatio");

		// What one detent of the wheel is worth in a Scroll event. Reported so the check can inject
		// a REAL event of exactly one notch without spelling the figure itself - the amount and the
		// division the widget makes by it are the pair that was wrong, and a check carrying its own
		// copy of one of them could not have seen it.
		ret.setDouble(sprt::window::InputScrollNotch, "notchAmount");
		return ret;
	});

	addCommand("zoom-control", "Turn the floating control on or off: {enabled}",
			[this](Value &&args) {
		if (args.hasValue("enabled")) {
			_canvas->setZoomControlEnabled(args.getBool("enabled"));
		}
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
