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

#include "XLUiPopupSurface.h"
#include "XLUiSubWindowSession.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XL2dSceneLayout.h"
#include "XL2dSceneContent.h"
#include "XLInputListener.h"
#include "XLFocusGroup.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The stylesheet in force for `node` (see PopupSurfaceConfig::styleSource). Nearest wins, and the
// node itself counts.
static Rc<StyleSheet> PopupSurface_sheetForNode(Node *node) {
	for (auto n = node; n; n = n->getParent()) {
		if (auto system = n->getSystemByType<StyleSystem>()) {
			if (auto sheet = system->getStyleSheet()) {
				return Rc<StyleSheet>(sheet);
			}
		}
	}
	return nullptr;
}

Rc<SubWindow> openPopupSurface(NotNull<AppWindow> window,
		const sprt::window::WindowPlacement &placement, PopupSurfaceConfig &&config) {
	auto director = window->getDirector();
	auto scene = director ? director->getScene() : nullptr;
	auto parentContent = scene ? scene->getContent() : nullptr;

	// Read here: on the native path the builder runs in another scene.
	const float parentHeight = parentContent ? parentContent->getContentSize().height : 0.0f;

	/* Resolved here for the same reason: `styleSource` lives in this scene. The parsed sheet is
	shared, not re-read. Skipped when a sheet was named, so `:root` stays on the surface's own. */
	Rc<StyleSheet> inheritedSheet;
	if (config.stylesheet.empty() && config.stylesheetSource.empty()) {
		Node *source = config.styleSource;
		if (!source) {
			if (auto content2d = dynamic_cast<basic2d::SceneContent2d *>(parentContent)) {
				source = content2d->getTopLayout();
			}
		}
		if (!source) {
			source = parentContent;
		}
		inheritedSheet = PopupSurface_sheetForNode(source);
	}

	// Cleared before the builder copies the config, so no raw pointer outlives this call.
	config.styleSource = nullptr;

	SubWindow::Config surfaceConfig;
	surfaceConfig.type = sprt::window::WindowType::Popup;
	surfaceConfig.flags = config.flags;
	surfaceConfig.placement = placement;
	surfaceConfig.size = config.size;
	surfaceConfig.title = config.title.empty() ? StringView("Popup") : StringView(config.title);
	surfaceConfig.idPrefix =
			config.idPrefix.empty() ? StringView("popup") : StringView(config.idPrefix);
	surfaceConfig.preferNative = config.preferNative;

	if (config.onClose) {
		surfaceConfig.onClose = [cb = config.onClose](NotNull<SubWindow>) { cb(); };
	}

	/* Captured by copy, not moved: on the native path this runs after the opener may be gone, and
	SubWindow::Config holds title and id prefix as StringViews into the config above. */
	surfaceConfig.content =
			[config = config, parentHeight, inheritedSheet](
					NotNull<SubWindow> surface) mutable -> Rc<basic2d::SceneLayout2d> {
		auto layout = Rc<basic2d::SceneLayout2d>::create();
		layout->setName(config.layoutName.empty() ? StringView("popup-layout")
												  : StringView(config.layoutName));

		const bool native = surface->isNative();

		if (!config.stylesheet.empty() || !config.stylesheetSource.empty()) {
			StyleSystem *style = nullptr;
			if (!config.stylesheet.empty()) {
				style = layout->addSystem(Rc<StyleSystem>::create(
						FileInfo{config.stylesheet, config.stylesheetCategory}));
			}
			if (!config.stylesheetSource.empty()) {
				if (style) {
					style->addStyle(config.stylesheetSource);
				} else {
					layout->addSystem(Rc<StyleSystem>::create(StringView(config.stylesheetSource)));
				}
			}
			layout->addSystem(Rc<StyleResolver>::create(true));
		} else if (native && inheritedSheet) {
			/* Nothing was named: share the opener's sheet object. Native only; the overlay layout
			is already in that sheet's scope. A live reload is not followed: ui::StyleSystem builds
			a new sheet, and the surface keeps the one it opened with. */
			layout->addSystem(Rc<StyleSystem>::create(Rc<StyleSheet>(inheritedSheet)));
			layout->addSystem(Rc<StyleResolver>::create(true));
		}

		const auto size = config.size;

		auto panel = layout->addChild(
				config.makePanel ? config.makePanel(surface, size) : Rc<Panel>::create());
		if (!panel) {
			return layout;
		}

		// Lets a caller get its typed panel back (see SubWindow::getPanel).
		surface->_panel = panel;

		if (!config.panelName.empty()) {
			panel->setName(config.panelName);
		}
		if (!config.panelType.empty()) {
			panel->setType(config.panelType);

			// A custom type needs the surface appliers registered under that name, or
			// `background-color` lands on the node's tint and multiplies the fallback. Idempotent;
			// richer appliers already registered for the type are kept.
			Panel::registerStyleAppliers(config.panelType);
		}
		if (!config.panelClass.empty()) {
			panel->addStyleClass(config.panelClass);
		}

		// A SceneLayout2d paints nothing, so the panel is the surface. Default rendering level: the
		// overlay path already lifts the layout onto RenderingLevel::Overlay.
		panel->setPathColor(config.fallbackColor, false);
		panel->setAnchorPoint(Anchor::TopLeft);
		panel->setContentSize(Size2(float(size.width), float(size.height)));

		if (native) {
			// Fill the extent the window system settled on, which may differ from the request.
			panel->setPosition(Vec2(0.0f, float(size.height)));
			layout->setContentSizeDirtyCallback([layout = layout.get(), panel] {
				const auto s = layout->getContentSize();
				if (s.width > 0.0f && s.height > 0.0f) {
					panel->setPosition(Vec2(0.0f, s.height));
					panel->setContentSize(s);
				}
			});
		} else {
			// Overlay path: pushOverlay stretches the layout over the parent with a bottom-left
			// origin, so the panel goes at the resolved placement (Y-down from the top).
			panel->addStyleClass("overlay");
			const auto rect = surface->getOverlayRect();
			panel->setPosition(Vec2(float(rect.x), parentHeight - float(rect.y)));
		}

		if (config.content) {
			config.content(surface, panel);
		}

		if (!native) {
			/* An overlay has no window system dismissal, so the full-parent layout listens for a
			press outside the panel. Installed after the content, which the outside test and a
			menu's handler depend on.

			The press is kept off the scene below by an exclusive focus group, not by swallowing:
			a swallowing listener owns the whole gesture and eats the End of taps inside the panel,
			while declining lets a covered text field capture the press. The group excludes outside
			listeners without owning the gesture (as basic2d::OverlaySurface does). It must be added
			before the listener, which otherwise records no group. Touch only, Propagate, since the
			content may have groups of its own (ui::MenuSystem, ui::FormSystem). */
			auto focus = layout->addSystem(Rc<FocusGroup>::create());
			focus->setEventMask(FocusGroup::EventMask(EventMaskTouch));
			focus->setFlags(FocusGroup::Flags::Exclusive | FocusGroup::Flags::Propagate);

			auto listener = layout->addSystem(Rc<InputListener>::create());
			listener->addTouchRecognizer([surface = surface.get(), panel, cb = config.onOutsideTap](
												 const GestureData &data) {
				// The panel's own widgets own gestures that start on them.
				if (panel->isTouched(data.location())) {
					return false;
				}

				if (data.event == GestureEvent::Began) {
					if (cb) {
						cb(surface, panel);
					} else {
						surface->dismiss();
					}
				}
				return true;
			});
			// Only presses outside the panel reach here; hide them from the surface's content too.
			listener->setSwallowEvents(EventMaskTouch);
		}

		return layout;
	};

	// Through the session, which drops a live tooltip before the popup opens.
	if (auto session = SubWindowSession::get(window)) {
		return session->openPopup(sp::move(surfaceConfig));
	}
	return SubWindow::open(window, sp::move(surfaceConfig));
}

} // namespace stappler::xenolith::ui
