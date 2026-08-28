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

// Ids are for logs and for the window backend's own bookkeeping only — nothing is looked up by
// them. App thread, so a plain counter is enough.
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

	// A tooltip is an overlay even where subwindows exist (showTooltip sets preferNative=false):
	// a native tip costs a swapchain for a few hundred milliseconds and takes hover away from the
	// node it describes. The caller can still ask for a native one explicitly.
	const bool wantNative = config.preferNative && parent->getContext() && parent->getInfo()
			&& platformSupportsSubwindows(parent);

	if (wantNative) {
		if (ret->openNative(parent, sp::move(config))) {
			return ret;
		}
		// Fall through: a refused native window is still better served by an overlay than by
		// nothing, and the caller cannot tell the difference anyway. `config` is untouched — see
		// openNative, which moves nothing until it can no longer fail.
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

	// Nothing was materialized. Answer the opener now — it may already have put up a backdrop.
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

	// The handle rides INSIDE the scene provider, which is how what-to-show travels with the
	// window request. That makes a reference cycle — SubWindow -> WindowSceneInfo -> closure ->
	// SubWindow — and that is deliberate: it is what keeps the surface alive when the opener drops
	// its Rc immediately, which most callers do. handleClosed() breaks it, and every teardown path
	// reaches handleClosed() through AppWindow::end().
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

	if (hasFlag(parentInfo->flags, WindowCreationFlags::UserSpaceDecorations)) {
		info->flags |= WindowCreationFlags::UserSpaceDecorations;
	}

	info->appData = _sceneInfo;

	ctx->createWindow(sp::move(info), [self](Status st, StringView id) mutable {
		if (!sprt::status::isSuccessful(st)) {
			// Context::createWindow already handed the payload back to this thread and its close
			// callback ran, so the surface has already been retired.
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

	// Resolve the placement the same way the window backends do, rather than dropping the surface
	// at the raw anchor point: `anchor`, `gravity`, `offset` and the flip/slide adjustments are the
	// whole reason WindowPlacement exists, and a caller that gets them honoured natively but
	// ignored here has to branch on the platform after all. The scene IS the work area for an
	// overlay — there is nothing outside it to slide against.
	//
	// It is resolved BEFORE the content builder for the same reason it is published: a caller that
	// re-derives this arithmetic is a caller that will get the Y flip or the density wrong.
	const auto contentSize = content->getContentSize();
	const auto workArea = IRect(0, 0, int32_t(std::lround(contentSize.width)),
			int32_t(std::lround(contentSize.height)));
	const auto placed =
			sprt::window::computeWindowPlacement(config.placement, config.size, workArea, workArea);
	_overlayRect = placed;

	// The builder runs AFTER the placement is resolved, so it can read getOverlayRect(): everything
	// but a tip is pushed as a full-parent overlay, and then the builder - not the push - is what
	// decides where the visible box of a menu or a palette actually sits.
	auto layout = config.content ? config.content(this) : nullptr;
	if (!layout) {
		return false;
	}

	// computeWindowPlacement answers in the same Y-down space it was asked in; scene nodes are Y-up.
	const float yUp = contentSize.height - float(placed.y);
	layout->setAnchorPoint(Anchor::TopLeft);
	layout->setPosition(Vec2(float(placed.x), yUp));

	_overlayIsTip = config.type == WindowType::Tooltip;

	// A modal dialog that could not become a real window still has to behave like one. There is no
	// second window here, so ContextController's _modalBlocks cannot help: the block is a node that
	// covers the parent's content and swallows pointer and key events before they reach it.
	//
	// This is a different mechanism from the native path with the same observable behaviour, and
	// it is the only one available on Android and wasm. Note WindowState::Enabled is NOT cleared
	// here — there is no OS window to clear it on.
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
		// A key recognizer needs an explicit key mask or it refuses to arm — so name every key
		// rather than leave the mask empty and silently swallow nothing.
		InputKeyMask allKeys;
		allKeys.set();
		listener->addKeyRecognizer([](const GestureData &) { return true; },
				InputKeyInfo(sp::move(allKeys)));
		listener->setSwallowEvents(EventMaskTouch | EventMaskKeyboard);

		// Just below the overlay the dialog itself is pushed as.
		_backdrop = content->addChild(backdrop, ZOrder(0));
	}

	if (_overlayIsTip) {
		// pushOverlay's updateLayoutNode would force full-parent size and a BottomLeft origin,
		// which is exactly wrong for a hint anchored at a point.
		//
		// Only name it if the builder did not: a tip's name is its CSS id and the hook tools look
		// it up by, so a content builder that named its own root has said something deliberate and
		// overwriting it would make every custom hint indistinguishable from the stock one.
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

	// Dropping _sceneInfo below breaks the cycle described in openNative, and the reference it
	// releases may well be the last one — so hold ourselves across the whole method.
	auto guard = Rc<SubWindow>(this);

	// Move out before invoking: a callback that opens the next surface is normal, and it must not
	// be able to reenter this one.
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
