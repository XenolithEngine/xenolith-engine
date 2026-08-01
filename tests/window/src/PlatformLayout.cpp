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

#include "PlatformLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

// base grey, then per-platform overrides through the custom `platform` media feature. On the
// reference Linux build only the linux rule applies (green); the windows rule (red) must be filtered
// out. Swap the running platform to see the swatch colour follow it.
static constexpr auto s_platformCss = StringView(R"css(
.sw {
	background-color: #9e9e9e;
}
@media (platform: linux) {
	.sw { background-color: #43a047; }
}
@media (platform: windows) {
	.sw { background-color: #e53935; }
}
@media (platform: macos) {
	.sw { background-color: #1e88e5; }
}
@media (platform: android) {
	.sw { background-color: #43a047; }
}
@media (platform: web) {
	.sw { background-color: #8e24aa; }
}
)css");

} // namespace

bool PlatformLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_platformCss);

	_swatch = addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_swatch->addStyleClass("sw");
	_swatch->addSystem(Rc<ui::StyleResolver>::create());

	return true;
}

void PlatformLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	_swatch->setAnchorPoint(Vec2(0.5f, 0.5f));
	_swatch->setContentSize(Size2(200.0f, 200.0f));
	_swatch->setPosition(Vec2(cs.width * 0.5f, cs.height * 0.5f));
}

} // namespace stappler::xenolith::app
