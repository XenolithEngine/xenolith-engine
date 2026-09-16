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

#include "XLCommon.h"

#include "window/GeometryLayout.h"
#include "window/SecondaryWindow.h"
#include "app/ExampleScene.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

static constexpr auto kGeometryWindowId = StringView("geometry-second");

namespace {

Value encodeGeometryValue(const sprt::window::WindowGeometry &g) {
	Value ret;
	ret.setInteger(int64_t(g.rect.x), "x");
	ret.setInteger(int64_t(g.rect.y), "y");
	ret.setInteger(int64_t(g.rect.width), "width");
	ret.setInteger(int64_t(g.rect.height), "height");
	ret.setBool(g.hasPosition, "hasPosition");
	return ret;
}

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool GeometryLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_report = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_report->setFontSize(16);
	_report->setColor(Color::White);
	_report->setAnchorPoint(Anchor::TopLeft);
	_report->setAlignment(font::TextAlign::Left);

	return true;
}

void GeometryLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_report) {
		_report->setPosition(Vec2(24.0f, getWorkTop() - 24.0f));
		_report->setWidth(sprt::max(_contentSize.width - 48.0f, 0.0f));

		StringStream text;
		if (auto server = _director ? _director->getRenderServer() : nullptr) {
			auto &g = server->getWindowGeometry();
			text << "window geometry\n";
			if (g.hasPosition) {
				text << "  position: " << g.rect.x << ", " << g.rect.y << "\n";
			} else {
				text << "  position: unknown on this platform\n";
			}
			text << "  size (logical): " << g.rect.width << " x " << g.rect.height << "\n";
			// The pixel size and the scale are FrameConstraints' answer, not the geometry's - shown
			// side by side here precisely because they are different questions.
			auto &c = server->getConstraints();
			text << "  surface (px):   " << c.extent.width << " x " << c.extent.height << "\n";
			text << "  density:        " << c.density << "\n";
		}
		_report->setString(text.str());
	}
}

void GeometryLayout::handleExit() {
	// The second window is this layout's, not the app's: leaving it open would outlive the test.
	if (_secondWindow) {
		SecondaryWindow::close(_secondWindow);
		_secondWindow = nullptr;
	}
	TestLayout::handleExit();
}

AppWindow *GeometryLayout::getAppWindow() const {
	auto server = _director ? _director->getRenderServer() : nullptr;
	return server ? static_cast<AppWindow *>(server) : nullptr;
}

Value GeometryLayout::encodeState() const {
	Value ret;

	if (auto server = _director ? _director->getRenderServer() : nullptr) {
		ret.setValue(encodeGeometryValue(server->getWindowGeometry()), "geometry");
		ret.setBool(hasFlag(server->getCapabilities(),
							sprt::window::WindowCapabilities::WindowPosition),
				"positionCapability");
	}

	// The notification half. Counted in the scene, because that is where the hook is.
	if (auto scene = dynamic_cast<ExampleScene *>(_director ? _director->getScene() : nullptr)) {
		ret.setInteger(int64_t(scene->getGeometryChangeCount()), "changes");
		ret.setValue(encodeGeometryValue(scene->getLastGeometry()), "lastNotified");
	}

	ret.setValue(encodeSecondary(), "second");
	return ret;
}

Value GeometryLayout::encodeSecondary() const {
	Value ret;
	ret.setBool(_secondWindow != nullptr, "open");
	ret.setInteger(int64_t(_requestedOrigin.x), "requestedX");
	ret.setInteger(int64_t(_requestedOrigin.y), "requestedY");
	ret.setInteger(int64_t(_requestedSize.width), "requestedWidth");
	ret.setInteger(int64_t(_requestedSize.height), "requestedHeight");

	// Read from the SECOND window's own channel: this is the round trip that matters - a rect that
	// went in through WindowInfo::rect coming back out through the protocol.
	auto window = _secondWindow ? _secondWindow->getWindow() : nullptr;
	if (window) {
		ret.setValue(encodeGeometryValue(window->getWindowGeometry()), "geometry");
	}
	return ret;
}

void GeometryLayout::registerCommands() {
	addCommand("state", "Report this window's geometry, the notification counter and the second "
					   "window's readback",
			[this](Value &&) { return encodeState(); });

	addCommand("open-second",
			"Open a second Root window at the requested position: {x, y, width, height}",
			[this](Value &&args) {
		if (_secondWindow) {
			return ackValue(false);
		}

		const Value &req = args;
		if (req.hasValue("x")) {
			_requestedOrigin.x = int32_t(req.getInteger("x"));
		}
		if (req.hasValue("y")) {
			_requestedOrigin.y = int32_t(req.getInteger("y"));
		}
		if (req.hasValue("width")) {
			_requestedSize.width = uint32_t(req.getInteger("width"));
		}
		if (req.hasValue("height")) {
			_requestedSize.height = uint32_t(req.getInteger("height"));
		}

		auto window = getAppWindow();
		if (!window) {
			return ackValue(false);
		}

		_secondWindow = SecondaryWindow::open(window, kGeometryWindowId, _requestedSize,
				[](StringView) -> Rc<basic2d::SceneLayout2d> {
			return Rc<basic2d::SceneLayout2d>::create();
		}, nullptr, nullptr, _requestedOrigin);

		return ackValue(_secondWindow != nullptr);
	});

	addCommand("close-second", "Close the second window", [this](Value &&) {
		if (!_secondWindow) {
			return ackValue(false);
		}
		SecondaryWindow::close(_secondWindow);
		_secondWindow = nullptr;
		return ackValue(true);
	});

	addCommand("reset-counter", "Zero the geometry-notification counter", [this](Value &&) {
		if (auto scene =
						dynamic_cast<ExampleScene *>(_director ? _director->getScene() : nullptr)) {
			scene->resetGeometryCounter();
			return ackValue(true);
		}
		return ackValue(false);
	});
}

} // namespace stappler::xenolith::app
