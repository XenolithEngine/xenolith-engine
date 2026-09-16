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

#include "render/FrameCaptureLayout.h"
#include "XLAppWindow.h"
#include "XL2dLayer.h"
#include "director/XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// Four flat colours in a fixed arrangement. One colour would not do: a copy taken from the wrong
// origin, with a flipped y or with the channels swapped still looks like a plausible rectangle,
// and only the arrangement tells those apart.
static constexpr Color4F s_quadrants[4] = {
	Color4F(0.85f, 0.15f, 0.15f, 1.0f), // top-left: red
	Color4F(0.15f, 0.75f, 0.25f, 1.0f), // top-right: green
	Color4F(0.15f, 0.35f, 0.90f, 1.0f), // bottom-left: blue
	Color4F(0.95f, 0.80f, 0.10f, 1.0f), // bottom-right: yellow
};

static StringView stateName(FrameCaptureTarget::State state) {
	switch (state) {
	case FrameCaptureTarget::State::Allocating: return StringView("allocating");
	case FrameCaptureTarget::State::Armed: return StringView("armed");
	case FrameCaptureTarget::State::Ready: return StringView("ready");
	case FrameCaptureTarget::State::Failed: return StringView("failed");
	}
	return StringView("unknown");
}

} // namespace

bool FrameCaptureLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_target = addChild(Rc<Node>::create(), ZOrder(1));
	_target->setAnchorPoint(Anchor::TopLeft);
	_target->setName("capture-target");

	for (uint32_t i = 0; i < 4; ++i) {
		auto quad =
				_target->addChild(Rc<basic2d::Layer>::create(s_quadrants[i]), ZOrder(int16_t(i)));
		quad->setAnchorPoint(Anchor::BottomLeft);
	}

	/* The overlay veil: the same rectangle as the target, drawn over it - and, because it is on the
	Overlay level, drawn only AFTER the frame has been copied out. So it is on the screen and not in
	the cutout, which is the entire claim being tested.

	setOverlay is called on the root ALONE. The two layers under it declare nothing, and are lifted
	with it; if inheritance were broken they would draw as ordinary content and land in the cutout. */
	_veil = addChild(Rc<Node>::create(), ZOrder(2));
	_veil->setAnchorPoint(Anchor::TopLeft);
	_veil->setName("capture-veil");
	_veil->setOverlay(true);
	_veil->setVisible(false);

	auto veilBack = _veil->addChild(Rc<basic2d::Layer>::create(Color4F(0.85f, 0.10f, 0.80f, 1.0f)),
			ZOrder(0));
	veilBack->setAnchorPoint(Anchor::BottomLeft);
	veilBack->setName("capture-veil-back");

	// One level deeper again: inheritance has to reach past the node that was marked, not just its
	// immediate children.
	auto veilInner = veilBack->addChild(
			Rc<basic2d::Layer>::create(Color4F(0.10f, 0.85f, 0.85f, 1.0f)), ZOrder(1));
	veilInner->setAnchorPoint(Anchor::Middle);
	veilInner->setName("capture-veil-inner");

	// Starts with no texture: the sprite exists from the first frame so that a capture landing in
	// it changes nothing about the scene graph - only what it samples.
	_mirror = addChild(Rc<basic2d::Sprite>::create(), ZOrder(1));
	_mirror->setAnchorPoint(Anchor::TopLeft);
	_mirror->setName("capture-mirror");
	// A captured frame carries no meaningful alpha - the compositor was told the surface is opaque,
	// so whatever the swapchain image holds in that channel is not a transparency the cutout should
	// inherit. Without this the mirror blends itself away to nothing.
	_mirror->setColorMode(core::ColorMode(core::ComponentMapping::R, core::ComponentMapping::G,
			core::ComponentMapping::B, core::ComponentMapping::One));

	// The capture is read out of what was PRESENTED, so the frame has to keep coming even when
	// nothing in the scene changed - otherwise an armed capture waits forever for a frame.
	runAction(Rc<RenderContinuously>::create());

	return true;
}

void FrameCaptureLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto work = getWorkSize();
	const float total = BlockWidth * 2.0f + Gap;
	const float left = sprt::max(0.0f, (work.width - total) / 2.0f);
	const float top = getWorkTop() - 40.0f;

	_target->setPosition(Vec2(left, top));
	_target->setContentSize(Size2(BlockWidth, BlockHeight));

	const auto half = Size2(BlockWidth / 2.0f, BlockHeight / 2.0f);
	auto quads = _target->getChildren();
	for (uint32_t i = 0; i < quads.size() && i < 4; ++i) {
		quads[i]->setContentSize(half);
		quads[i]->setPosition(Vec2((i % 2) ? half.width : 0.0f, (i < 2) ? half.height : 0.0f));
	}

	_veil->setPosition(Vec2(left, top));
	_veil->setContentSize(Size2(BlockWidth, BlockHeight));

	auto veilChildren = _veil->getChildren();
	if (!veilChildren.empty()) {
		auto back = veilChildren.front();
		back->setContentSize(Size2(BlockWidth, BlockHeight));
		back->setPosition(Vec2::ZERO);

		auto inner = back->getChildren();
		if (!inner.empty()) {
			inner.front()->setContentSize(Size2(BlockWidth / 2.0f, BlockHeight / 2.0f));
			inner.front()->setPosition(Vec2(BlockWidth / 2.0f, BlockHeight / 2.0f));
		}
	}

	_mirror->setPosition(Vec2(left + BlockWidth + Gap, top));
	_mirror->setContentSize(Size2(BlockWidth, BlockHeight));
}

AppWindow *FrameCaptureLayout::getAppWindow() const {
	auto server = _director ? _director->getRenderServer() : nullptr;
	return server ? static_cast<AppWindow *>(server) : nullptr;
}

Rect FrameCaptureLayout::getTargetRect() const {
	if (!_target) {
		return Rect::ZERO;
	}
	// The box in WORLD space, which on a HiDPI surface is NOT the world origin plus the content
	// size: the scene root is scaled by the density, so the origin comes out in surface pixels and
	// the size in logical units. FrameCapture::makeRegion does the projection and the y flip.
	return _target->getWorldBoundingBox();
}

Value FrameCaptureLayout::requestCapture() {
	Value ret;

	auto window = getAppWindow();
	auto capture = window ? window->getFrameCapture() : nullptr;
	if (!capture) {
		ret.setString("no frame capture on this window", "error");
		return ret;
	}

	if (!capture->isAvailable()) {
		ret.setBool(false, "available");
		ret.setString("this backend or surface cannot produce a cutout", "error");
		return ret;
	}

	_lastRegion = FrameCapture::makeRegion(getTargetRect(), _director->getGeneralProjection(),
			Extent2(_director->getFrameConstraints().extent.width,
					_director->getFrameConstraints().extent.height));

	// A target is immutable, so re-requesting is a NEW image rather than a refill; dropping the old
	// one here is what keeps a repeated request from piling images up.
	_mirror->setTexture(nullptr);
	_capture = capture->request(_lastRegion, [this](FrameCaptureTarget *target) {
		++_completions;
		if (target->getTexture()) {
			_mirror->setTexture(target->getTexture());
		}
	});

	++_requests;
	ret.setBool(_capture != nullptr, "ok");
	ret.setValue(encodeState(), "state");
	return ret;
}

Value FrameCaptureLayout::encodeState() const {
	Value ret;

	auto window = getAppWindow();
	auto capture = window ? window->getFrameCapture() : nullptr;
	ret.setBool(capture && capture->isAvailable(), "available");
	ret.setInteger(int64_t(_requests), "requests");
	ret.setInteger(int64_t(_completions), "completions");

	Value region;
	region.setInteger(int64_t(_lastRegion.x), "x");
	region.setInteger(int64_t(_lastRegion.y), "y");
	region.setInteger(int64_t(_lastRegion.width), "width");
	region.setInteger(int64_t(_lastRegion.height), "height");
	ret.setValue(sp::move(region), "region");

	if (_capture) {
		ret.setString(stateName(_capture->getState()), "target");
		ret.setBool(_capture->getTexture() != nullptr, "texture");
		if (auto tex = _capture->getTexture()) {
			auto extent = tex->getExtent();
			ret.setInteger(int64_t(extent.width), "textureWidth");
			ret.setInteger(int64_t(extent.height), "textureHeight");
		}
	} else {
		ret.setString("none", "target");
	}

	ret.setBool(_mirror && _mirror->getTexture() != nullptr, "mirror");
	ret.setBool(_veil && _veil->isVisible(), "overlay");
	ret.setBool(_veil && _veil->isOverlay(), "overlayLevel");
	return ret;
}

void FrameCaptureLayout::registerCommands() {
	addCommand("request", "Capture the coloured block and show it in the mirror beside it",
			[this](Value &&) { return requestCapture(); });

	addCommand("state", "Report the capture target's state, the resolved region and the mirror",
			[this](Value &&) { return encodeState(); });

	addCommand("overlay",
			"Raise or lower the veil covering the target: { visible, level } - `level` false draws "
			"it as ordinary content instead, which is the control for the overlay claim",
			[this](Value &&args) {
		if (_veil) {
			_veil->setVisible(args.isBool("visible") ? args.getBool("visible") : !_veil->isVisible());
			if (args.isBool("level")) {
				_veil->setOverlay(args.getBool("level"));
			}
		}
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
