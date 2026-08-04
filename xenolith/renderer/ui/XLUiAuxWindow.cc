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

#include "XLUiAuxWindow.h"

#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLContext.h"
#include "XLDirector.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// App-thread only (see AuxWindow docs), so plain statics are enough.
static Map<String, AuxWindow::ContentBuilder> s_auxBuilders;
static Map<String, Rc<basic2d::SceneLayout2d>> s_overlayLayouts;
static uint32_t s_auxCounter = 0;

static constexpr ZOrder kTipZOrder = ZOrder(10'000);

static String nextAuxId(StringView prefix) {
	return toString(prefix.empty() ? StringView("aux") : prefix, "-", ++s_auxCounter);
}

bool AuxWindow::platformSupportsSubwindows(NotNull<AppWindow> parent) {
	return hasFlag(parent->getCapabilities(), sprt::window::WindowCapabilities::Subwindows);
}

AuxWindow::ContentBuilder AuxWindow::takeContentBuilder(StringView id) {
	auto it = s_auxBuilders.find(id);
	if (it == s_auxBuilders.end()) {
		return nullptr;
	}
	auto builder = sprt::move(it->second);
	s_auxBuilders.erase(it);
	return builder;
}

bool AuxWindow::hasOverlay(StringView id) {
	auto it = s_overlayLayouts.find(id);
	return it != s_overlayLayouts.end() && it->second && it->second->getParent() != nullptr;
}

bool AuxWindow::dismissOverlay(StringView id) {
	auto it = s_overlayLayouts.find(id);
	if (it == s_overlayLayouts.end()) {
		return false;
	}
	auto layout = sprt::move(it->second);
	s_overlayLayouts.erase(it);
	if (layout) {
		layout->removeFromParent();
	}
	return true;
}

static basic2d::SceneContent2d *contentForParent(NotNull<AppWindow> parent) {
	auto director = parent->getDirector();
	auto scene = director ? director->getScene() : nullptr;
	return scene ? dynamic_cast<basic2d::SceneContent2d *>(scene->getContent()) : nullptr;
}

// In-scene materialization. `asTip` keeps the measured size and anchors it at the placement point
// (pushOverlay's updateLayoutNode would force full-parent size and BottomLeft origin instead).
static bool openAsOverlay(NotNull<AppWindow> parent, AuxWindow::OpenRequest &&req, StringView id,
		bool asTip) {
	auto content = contentForParent(parent);
	if (!content) {
		log::source().warn("AuxWindow", "in-scene fallback needs SceneContent2d; id=", id);
		return false;
	}

	auto builder = sprt::move(req.builder);
	auto layout = builder ? builder(id) : nullptr;
	if (!layout) {
		return false;
	}

	// Placement is Y-down from the parent content top-left, scene nodes are Y-up.
	const float yUp = content->getContentSize().height - float(req.placement.anchorRect.y);
	layout->setAnchorPoint(Anchor::TopLeft);
	layout->setPosition(Vec2(float(req.placement.anchorRect.x), yUp));

	if (asTip) {
		layout->setName("aux-tip");
		layout->setContentSize(Size2(float(req.size.width), float(req.size.height)));
		content->addChild(layout, kTipZOrder);
	} else if (!content->pushOverlay(layout)) {
		log::source().warn("AuxWindow", "pushOverlay failed id=", id);
		return false;
	}

	s_overlayLayouts.insert_or_assign(id.str<Interface>(), layout);
	return true;
}

String AuxWindow::open(NotNull<AppWindow> parent, OpenRequest &&req) {
	if (!req.builder) {
		log::source().error("AuxWindow", "open: ContentBuilder is required");
		return String();
	}
	if (req.type != sprt::window::WindowType::Popup
			&& req.type != sprt::window::WindowType::Tooltip) {
		log::source().error("AuxWindow", "open: only Popup/Tooltip are supported");
		return String();
	}

	const bool isTooltip = req.type == sprt::window::WindowType::Tooltip;
	auto id = nextAuxId(req.idPrefix.empty()
					? (isTooltip ? StringView("tooltip") : StringView("popup"))
					: req.idPrefix);

	// Tooltips are always in-scene: a native tip window costs a swapchain for a few hundred
	// milliseconds of hint, and it takes hover away from the node it describes.
	if (isTooltip || !platformSupportsSubwindows(parent)) {
		return openAsOverlay(parent, sprt::move(req), id, isTooltip) ? id : String();
	}

	s_auxBuilders.insert_or_assign(id, sprt::move(req.builder));

	auto info = Rc<sprt::window::WindowInfo>::create();
	info->id = id;
	info->title = req.title.empty() ? toString("aux ", sprt::window::getWindowTypeName(req.type))
								   : req.title.str<Interface>();
	info->type = req.type;
	info->parent = parent->getInfo() ? parent->getInfo()->id : String();
	info->rect = IRect(0, 0, int32_t(req.size.width), int32_t(req.size.height));
	info->placement = req.placement;
	info->flags = sprt::window::WindowCreationFlags::None;

	parent->getContext()->createWindow(sprt::move(info));
	return id;
}

String AuxWindow::openPopup(NotNull<AppWindow> parent, const sprt::window::WindowPlacement &placement,
		Extent2 size, ContentBuilder &&builder, StringView title) {
	OpenRequest req;
	req.type = sprt::window::WindowType::Popup;
	req.placement = placement;
	req.size = size;
	req.title = title;
	req.builder = sprt::move(builder);
	return open(parent, sprt::move(req));
}

String AuxWindow::showTooltip(NotNull<AppWindow> parent,
		const sprt::window::WindowPlacement &placement, Extent2 size, ContentBuilder &&builder,
		StringView title) {
	OpenRequest req;
	req.type = sprt::window::WindowType::Tooltip;
	req.placement = placement;
	req.size = size;
	req.title = title;
	req.builder = sprt::move(builder);
	return open(parent, sprt::move(req));
}

} // namespace stappler::xenolith::ui
