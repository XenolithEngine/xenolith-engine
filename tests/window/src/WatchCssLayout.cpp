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

#include "WatchCssLayout.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"
#include "SPFilesystem.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static String cssFor(StringView color) {
	StringStream out;
	out << ".wsw { background-color: " << color << "; }\n";
	return out.str();
}

} // namespace

void WatchCssLayout::writeCss(StringView color) {
	auto text = cssFor(color);
	filesystem::write(FileInfo(_cssPath, FileCategory::Custom),
			BytesView((const uint8_t *)text.data(), text.size()));
}

bool WatchCssLayout::init() {
	_cssPath = ::getenv("XL_WATCH_CSS_FILE") ? String(::getenv("XL_WATCH_CSS_FILE"))
											 : String("/tmp/xl-watch-css-test.css");

	// seed the file BEFORE the StyleSystem reads it
	writeCss("#e53935"); // red

	if (!SceneLayout2d::init()) {
		return false;
	}

	// stylesheet loaded from the file -> this is the source the watch reloads
	addSystem(Rc<ui::StyleSystem>::create(FileInfo(_cssPath, FileCategory::Custom)));

	_swatch = addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_swatch->addStyleClass("wsw");
	_swatch->addSystem(Rc<ui::StyleResolver>::create());

	// baseline: the sheet was just loaded from the (red) file
	{
		auto c = ui::StyleResolver::resolveStyleForNode(_swatch).background().backgroundColor;
		log::source().warn("WatchCssTest", "initial resolved rgb(", int(c.r), ",", int(c.g), ",",
				int(c.b), ") (expected red 229,57,53)");
	}

	// t=0.6s: rewrite the file (red -> green) on the director thread. The file watch
	// registered by StyleSystem::handleEnter fires on this same thread, rebuilds the sheet
	// and invalidates the subtree -> the swatch turns green (see the 1.8s screenshot).
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] {
		writeCss("#43a047"); // green
		log::source().warn("WatchCssTest", "rewrote ", _cssPath,
				" -> green; watch should reload the sheet");
	}));

	return true;
}

void WatchCssLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	const auto cs = getContentSize();
	_swatch->setAnchorPoint(Vec2(0.5f, 0.5f));
	_swatch->setContentSize(Size2(160.0f, 160.0f));
	_swatch->setPosition(Vec2(cs.width * 0.5f, cs.height * 0.5f));
}

} // namespace stappler::xenolith::app
