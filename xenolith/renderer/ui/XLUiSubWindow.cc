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

#include "XLUiSubWindow.h"
#include "XLUiSubWindowScene.h"

#include "XL2dLayer.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLAppThread.h"
#include "XLContext.h"
#include "XLDirector.h"
#include "XLScene.h"

#include <cmath>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Overlay-backed tips are parented directly rather than pushed, so they need a z of their own,
// above whatever the scene already draws.
static constexpr ZOrder kTipZOrder = ZOrder(10'000);

// Ids are for logs and the window backend's bookkeeping only. App thread, so a plain counter.
static uint32_t s_subWindowCounter = 0;

static String nextSubWindowId(StringView prefix, sprt::window::WindowType type) {
	if (prefix.empty()) {
		prefix = sprt::window::getWindowTypeName(type);
	}
	return toString(prefix, "-", ++s_subWindowCounter);
}

static basic2d::SceneContent2d *contentForWindow(AppWindow *w) {
	auto director = w ? w->getDirector() : nullptr;
	auto scene = director ? director->getScene() : nullptr;
	return scene ? dynamic_cast<basic2d::SceneContent2d *>(scene->getContent()) : nullptr;
}

/* Window logical points per point of scene content space. `surfaceDensity` (the display's) is what
the window system scales by, so window points are pixels over it. `density` also includes the
application's `WindowInfo::density` (`--density`) and is what the scene is laid out in. The spaces
differ by `WindowInfo::density`, usually 1. Read from the scene's frame constraints. */
static float placementPointScale(const Node *inScene) {
	auto scene = inScene->getScene();
	if (!scene) {
		return 1.0f;
	}

	const auto &c = scene->getFrameConstraints();
	if (c.density <= 0.0f || c.surfaceDensity <= 0.0f) {
		// No constraints yet: assume 1.
		return 1.0f;
	}
	return c.density / c.surfaceDensity;
}

IRect placementAnchorRect(NotNull<Node> anchor) {
	auto scene = anchor->getScene();
	auto content = scene ? scene->getContent() : nullptr;
	if (!content) {
		return IRect();
	}

	const auto k = placementPointScale(anchor);

	// Four corners and both conversions; see the header.
	const auto size = anchor->getContentSize();
	const Vec2 corners[4] = {
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2::ZERO)),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(size.width, 0.0f))),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(0.0f, size.height))),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(size.width, size.height))),
	};

	Vec2 low = corners[0];
	Vec2 high = corners[0];
	for (auto &it : corners) {
		low.x = sprt::min(low.x, it.x);
		low.y = sprt::min(low.y, it.y);
		high.x = sprt::max(high.x, it.x);
		high.y = sprt::max(high.y, it.y);
	}

	// Scene nodes are Y-up; WindowPlacement is Y-down from the content's top-left. The flip swaps
	// which edge is "top", so the rect is built from the flipped extremes. Flip in content space,
	// then scale: the scale is uniform about the same top-left origin.
	const float topYDown = content->getContentSize().height - high.y;

	return IRect(int32_t(std::lround(low.x * k)), int32_t(std::lround(topYDown * k)),
			uint32_t(std::lround((high.x - low.x) * k)),
			uint32_t(std::lround((high.y - low.y) * k)));
}

IRect placementAnchorPoint(NotNull<Node> inScene, const Vec2 &worldLocation) {
	auto scene = inScene->getScene();
	auto content = scene ? scene->getContent() : nullptr;
	if (!content) {
		return IRect();
	}

	// A point has no corners, but still needs the conversion into content space (undoing the
	// scene's density scale) and the scale into window points.
	const auto at = content->convertToNodeSpace(worldLocation);
	const auto k = placementPointScale(inScene);

	return IRect(int32_t(std::lround(at.x * k)),
			int32_t(std::lround((content->getContentSize().height - at.y) * k)), 0, 0);
}

SubWindow::~SubWindow() { }

bool SubWindow::platformSupportsSubwindows(NotNull<AppWindow> parent) {
	return hasFlag(parent->getCapabilities(), sprt::window::WindowCapabilities::Subwindows);
}

Rc<SubWindow> SubWindow::open(NotNull<AppWindow> parent, Config &&config) {
	if (!config.content && !config.scene) {
		log::source().error("SubWindow", "open: either content or scene builder is required");
		return nullptr;
	}
	if (parent->isInCloseRequest()) {
		return nullptr;
	}

	auto ret = Rc<SubWindow>::alloc();
	ret->_parent = parent;
	ret->_type = config.type;
	ret->_id = nextSubWindowId(config.idPrefix, config.type);
	ret->_onClose = sp::move(config.onClose);

	// A tooltip is an overlay even where subwindows exist (showTooltip sets preferNative=false);
	// a caller can still ask for a native one explicitly.
	const bool wantNative = config.preferNative && parent->getContext() && parent->getInfo()
			&& platformSupportsSubwindows(parent);

	if (wantNative) {
		if (ret->openNative(parent, sp::move(config))) {
			return ret;
		}
		// Fall through to an overlay. `config` is untouched: openNative moves nothing until it can
		// no longer fail.
	}

	if (!config.content) {
		log::source().error("SubWindow",
				"open: a scene-only surface needs native subwindow support");
		ret->handleClosed();
		return nullptr;
	}

	if (ret->openOverlay(parent, sp::move(config))) {
		return ret;
	}

	// Nothing was materialized. Notify the opener now; it may already have put up a backdrop.
	ret->handleClosed();
	return nullptr;
}

bool SubWindow::openNative(NotNull<AppWindow> parent, Config &&config) {
	auto ctx = parent->getContext();
	auto parentInfo = parent->getInfo();
	if (!ctx || !parentInfo) {
		// Nothing has been moved out of `config` yet, so open() can still fall back to an overlay.
		return false;
	}

	// The handle rides inside the scene provider, forming a deliberate cycle
	// (SubWindow -> WindowSceneInfo -> closure -> SubWindow) that keeps the surface alive after the
	// opener drops its Rc. handleClosed() breaks it; all teardown reaches it via AppWindow::end().
	auto self = Rc<SubWindow>(this);

	_sceneInfo = Rc<WindowSceneInfo>::create(
			[self, builder = sp::move(config.content), sceneBuilder = sp::move(config.scene)](
					NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
					const core::FrameConstraints &c) mutable -> Rc<Scene> {
		if (sceneBuilder) {
			return sceneBuilder(self, app, window, c);
		}
		return Rc<SubWindowScene>::create(app, window, c, self, sp::move(builder));
	},
			[self](NotNull<WindowSceneInfo>) { self->handleClosed(); });

	if (config.queue) {
		_sceneInfo->setQueue(sp::move(config.queue));
	}

	auto info = Rc<sprt::window::WindowInfo>::create();
	info->id = _id;
	info->title = config.title.empty()
			? toString("aux ", sprt::window::getWindowTypeName(config.type))
			: config.title.str<Interface>();
	info->type = config.type;
	info->parent = parentInfo->id;
	info->rect = IRect(0, 0, int32_t(config.size.width), int32_t(config.size.height));
	info->minExtent = config.minExtent;
	info->maxExtent = config.maxExtent;
	info->placement = config.placement;
	info->flags = config.flags;

	/* Popups and tips are chrome of their parent and inherit its user-space decorations. Dialogs
	and Utility windows are real windows the WM places and the user closes by the frame, so they get
	system decorations regardless of the parent. `config.flags` can only add, never remove. */
	const bool anchored = config.type == WindowType::Popup || config.type == WindowType::Tooltip;
	if (anchored && hasFlag(parentInfo->flags, WindowCreationFlags::UserSpaceDecorations)) {
		info->flags |= WindowCreationFlags::UserSpaceDecorations;
	}

	info->appData = _sceneInfo;

	ctx->createWindow(sp::move(info), [self](Status st, StringView id) mutable {
		if (!sprt::status::isSuccessful(st)) {
			// Context::createWindow already returned the payload and ran the close callback, so the
			// surface is already retired.
			return;
		}
		// Adopt the id the window system actually settled on: a collision renames it.
		self->_id = id.str<Interface>();
	});

	return true;
}

bool SubWindow::openOverlay(NotNull<AppWindow> parent, Config &&config) {
	auto content = contentForWindow(parent);
	if (!content) {
		log::source().warn("SubWindow", "in-scene fallback needs a SceneContent2d; id=", _id);
		return false;
	}

	const auto contentSize = content->getContentSize();
	const auto workArea = IRect(0, 0, int32_t(std::lround(contentSize.width)),
			int32_t(std::lround(contentSize.height)));

	auto placement = config.placement;
	if (const auto k = placementPointScale(content); k > 0.0f && k != 1.0f) {
		const auto toContent = [k](int32_t v) { return int32_t(std::lround(float(v) / k)); };
		placement.anchorRect =
				IRect(toContent(placement.anchorRect.x), toContent(placement.anchorRect.y),
						uint32_t(toContent(int32_t(placement.anchorRect.width))),
						uint32_t(toContent(int32_t(placement.anchorRect.height))));
		placement.offset = IVec2(toContent(placement.offset.x), toContent(placement.offset.y));
	}

	const auto placed =
			sprt::window::computeWindowPlacement(placement, config.size, workArea, workArea);
	_overlayRect = placed;

	// Everything but a tip is pushed as a full-parent overlay, so the builder positions the visible
	// box from getOverlayRect().
	auto layout = config.content ? config.content(this) : nullptr;
	if (!layout) {
		return false;
	}

	/* The overlay must draw on top of the scene, not be depth-tested against it. A high ZOrder only
	orders within a level; RenderingLevel::Overlay is a separate pass drawn last at zero depth.
	setOverlay is inherited by the whole subtree and cannot be escaped from inside, so one call
	covers the popup. Window decorations (ZOrder::max() - 1) and the drag ghost (ZOrder::max() - 16)
	are on the overlay too and stay above. */
	layout->setOverlay(true);

	// computeWindowPlacement answers in Y-down space; scene nodes are Y-up.
	const float yUp = contentSize.height - float(placed.y);
	layout->setAnchorPoint(Anchor::TopLeft);
	layout->setPosition(Vec2(float(placed.x), yUp));

	_overlayIsTip = config.type == WindowType::Tooltip;

	// An overlay modal dialog has no second window for ContextController's _modalBlocks, so a
	// backdrop covers the parent's content and swallows pointer and key events. The only mechanism
	// on Android and wasm; WindowState::Enabled is not cleared, there being no OS window.
	if (config.type == WindowType::Dialog && hasFlag(config.flags, WindowCreationFlags::Modal)) {
		auto backdrop = Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.32f));
		backdrop->setName("modal-backdrop");
		backdrop->setAnchorPoint(Anchor::BottomLeft);
		backdrop->setPosition(Vec2::ZERO);
		backdrop->setContentSize(content->getContentSize());

		auto listener = backdrop->addSystem(Rc<InputListener>::create());
		// A recognizer is what actually claims the event; the swallow mask alone would let the
		// press fall through to whatever is underneath.
		listener->addTouchRecognizer([](const GestureData &) { return true; });
		// A key recognizer with an empty key mask refuses to arm, so name every key.
		InputKeyMask allKeys;
		allKeys.set();
		listener->addKeyRecognizer([](const GestureData &) { return true; },
				InputKeyInfo(sp::move(allKeys)));
		listener->setSwallowEvents(EventMaskTouch | EventMaskKeyboard);

		// Just below the overlay the dialog itself is pushed as.
		_backdrop = content->addChild(backdrop, ZOrder(0));
	}

	if (_overlayIsTip) {
		// Not pushOverlay: its updateLayoutNode forces full-parent size and a BottomLeft origin.
		//
		// Only name it if the builder did not: the name is the tip's CSS id and how hook tools find
		// it, so a builder-chosen name is kept.
		if (layout->getName().empty()) {
			layout->setName("aux-tip");
		}
		layout->setContentSize(Size2(float(config.size.width), float(config.size.height)));
		content->addChild(layout, kTipZOrder);
	} else if (!content->pushOverlay(layout)) {
		log::source().warn("SubWindow", "pushOverlay failed id=", _id);
		return false;
	}

	_layout = sp::move(layout);
	return true;
}

bool SubWindow::isOpen() const {
	if (_sceneInfo) {
		return _sceneInfo->getWindow() != nullptr;
	}
	return _layout && _layout->getParent() != nullptr;
}

AppWindow *SubWindow::getWindow() const { return _sceneInfo ? _sceneInfo->getWindow() : nullptr; }

StringView SubWindow::getId() const { return _id; }

void SubWindow::dismiss() {
	if (_sceneInfo) {
		// The window's own teardown fires the close callback, which lands in handleClosed().
		if (auto window = _sceneInfo->getWindow()) {
			window->hide();
			return;
		}
		handleClosed();
		return;
	}

	if (_layout) {
		auto layout = sp::move(_layout);
		_layout = nullptr;
		_panel = nullptr;
		layout->removeFromParent();
	}
	handleClosed();
}

void SubWindow::handleClosed() {
	if (_closeFired) {
		return;
	}
	_closeFired = true;
	_parent = nullptr;

	// Dropping _sceneInfo breaks the cycle from openNative and may release the last reference, so
	// hold ourselves across the whole method.
	auto guard = Rc<SubWindow>(this);

	// Move out before invoking: a callback may open the next surface and must not reenter this one.
	auto cb = sp::move(_onClose);
	_onClose = nullptr;
	if (cb) {
		cb(this);
	}

	if (_backdrop) {
		auto backdrop = sp::move(_backdrop);
		_backdrop = nullptr;
		backdrop->removeFromParent();
	}

	_sceneInfo = nullptr;
	_layout = nullptr;
	_panel = nullptr;
}

Rc<SubWindow> SubWindow::openPopup(NotNull<AppWindow> parent, const WindowPlacement &placement,
		Extent2 size, ContentBuilder &&builder, StringView title) {
	Config config;
	config.type = WindowType::Popup;
	config.placement = placement;
	config.size = size;
	config.title = title;
	config.content = sp::move(builder);
	return open(parent, sp::move(config));
}

Rc<SubWindow> SubWindow::openDialog(NotNull<AppWindow> parent, Extent2 size,
		ContentBuilder &&builder, bool modal, StringView title) {
	Config config;
	config.type = WindowType::Dialog;
	config.size = size;
	config.title = title;
	config.content = sp::move(builder);
	config.flags = sprt::window::WindowCreationFlags::AllowClose
			| sprt::window::WindowCreationFlags::AllowMove;
	if (modal) {
		config.flags |= sprt::window::WindowCreationFlags::Modal;
	}
	return open(parent, sp::move(config));
}

Rc<SubWindow> SubWindow::openUtility(NotNull<AppWindow> parent, Extent2 size,
		ContentBuilder &&builder, StringView title) {
	Config config;
	config.type = WindowType::Utility;
	config.size = size;
	config.title = title;
	config.content = sp::move(builder);
	config.flags = sprt::window::WindowCreationFlags::AllowClose
			| sprt::window::WindowCreationFlags::AllowMove;
	return open(parent, sp::move(config));
}

Rc<SubWindow> SubWindow::showTooltip(NotNull<AppWindow> parent, const WindowPlacement &placement,
		Extent2 size, ContentBuilder &&builder, StringView title) {
	Config config;
	config.type = WindowType::Tooltip;
	config.placement = placement;
	config.size = size;
	config.title = title;
	config.content = sp::move(builder);
	config.preferNative = false;
	return open(parent, sp::move(config));
}

} // namespace stappler::xenolith::ui
