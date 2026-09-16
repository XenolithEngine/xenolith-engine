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

#include "css/WatchCssRecursiveLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"
#include "SPFilesystem.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static String cssFor(StringView color) {
	StringStream out;
	// only the CHILD is styled from CSS; the parent keeps its constructor color
	out << ".rchild { background-color: " << color << "; }\n";
	return out.str();
}

} // namespace

void WatchCssRecursiveLayout::writeCss(StringView color) {
	auto text = cssFor(color);
	filesystem::write(FileInfo(_cssPath, FileCategory::Custom),
			BytesView((const uint8_t *)text.data(), text.size()));
}

bool WatchCssRecursiveLayout::init() {
	_cssPath = ::getenv("XL_WATCH_CSS_FILE") ? String(::getenv("XL_WATCH_CSS_FILE"))
											 : String("/tmp/xl-watch-css-recursive-test.css");

	// seed the file BEFORE the StyleSystem reads it
	writeCss("#e53935"); // red

	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(FileInfo(_cssPath, FileCategory::Custom));

	// parent carries the ONE recursive resolver; it styles the child through the frame stack
	_parent = addChild(Rc<Layer>::create(Color::Grey_400), ZOrder(1));
	_parent->setType("rparent");
	_parent->addSystem(Rc<ui::StyleResolver>::create(true));

	// child has NO resolver of its own - it depends entirely on the parent's recursive resolver
	_child = _parent->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_child->setType("rchild");
	_child->addStyleClass("rchild");

	// t=0.6s: rewrite the file red -> green. The reload bumps the source version; the recursive
	// resolver must mark its subtree dirty so the (geometry-unchanged) child re-fires and re-resolves.
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] {
		writeCss("#43a047"); // green
		log::source().warn("WatchCssRecursiveTest", "rewrote ", _cssPath,
				" -> green; child should turn green via the recursive resolver");
	}));

	return true;
}

void WatchCssRecursiveLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	_parent->setAnchorPoint(Vec2(0.5f, 0.5f));
	_parent->setContentSize(Size2(240.0f, 240.0f));
	_parent->setPosition(Vec2(cs.width * 0.5f, cs.height * 0.5f));

	// centered inside the parent (parent-local coordinates)
	_child->setAnchorPoint(Vec2(0.5f, 0.5f));
	_child->setContentSize(Size2(120.0f, 120.0f));
	_child->setPosition(Vec2(120.0f, 120.0f));
}

} // namespace stappler::xenolith::app
