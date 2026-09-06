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

// The stylesheet in force for `node`, for a native surface's own scene to share - see
// PopupSurfaceConfig::styleSource. Nearest wins, and the node itself counts: putting the
// ui::StyleSystem on the SceneContent is as ordinary as putting it on the layout.
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

	// The overlay path places the panel from the parent's height, and it is read HERE rather than
	// in the builder: on the native path the builder runs in another scene entirely.
	const float parentHeight = parentContent ? parentContent->getContentSize().height : 0.0f;

	/* (1). Resolved HERE for the same reason: `styleSource` is a live node in THIS scene and the
	builder runs later, in the popup's. What crosses over is the parsed sheet, shared rather than
	re-read - so a dropdown costs no second parse of the application's CSS.

	Skipped when a sheet was named: a caller that said what the surface should look like has said
	it, and inheriting underneath would only make `:root` resolve somewhere else. */
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

	// Dropped before the builder below copies the config: the node has served its purpose, and a
	// raw pointer that survives into a lambda which may not run until the popup's scene exists is
	// exactly the dangling one the field's documentation promises never to keep.
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

	/* Captured BY COPY, not moved: on the native path this does not run until the popup's scene
	exists, by which time whatever opened the surface may be gone - and SubWindow::Config holds its
	title and id prefix as non-owning StringViews into the config above, which a move would leave
	pointing at nothing before openPopup ever reads them. */
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
			/* Nothing was named, so the surface inherits - the very sheet that styles whatever
			opened it, the object itself rather than another parse of the same CSS. Native only: on
			the overlay path the layout is already inside that sheet's scope, and a second system
			would just move `:root` down here for no reason.

			A live reload is not followed. ui::StyleSystem answers a changed file by building a NEW
			sheet, and this surface keeps the one it opened with - which for something that lives
			for as long as a dropdown is up is the right amount of machinery. */
			layout->addSystem(Rc<StyleSystem>::create(Rc<StyleSheet>(inheritedSheet)));
			layout->addSystem(Rc<StyleResolver>::create(true));
		}

		const auto size = config.size;

		auto panel = layout->addChild(
				config.makePanel ? config.makePanel(surface, size) : Rc<Panel>::create());
		if (!panel) {
			return layout;
		}

		// The surface remembers what it was built around, so a caller that asked for a typed panel
		// can have it back without guessing at this structure. See SubWindow::getPanel.
		surface->_panel = panel;

		if (!config.panelName.empty()) {
			panel->setName(config.panelName);
		}
		if (!config.panelType.empty()) {
			panel->setType(config.panelType);

			/* A type of its own needs the surface appliers registered UNDER that name.

			The type is what a stylesheet addresses (`menu { … }`), and with nothing registered for
			it the resolver reads the declarations and finds nobody to consume them: the fill keeps
			the fallback and `background-color` lands on the node's TINT instead, which then
			multiplies the fallback rather than replacing it. That is a menu asked to be #2b3038
			and drawn almost black. Idempotent, and a panel class that already registered richer
			appliers for this type keeps them. */
			Panel::registerStyleAppliers(config.panelType);
		}
		if (!config.panelClass.empty()) {
			panel->addStyleClass(config.panelClass);
		}

		/* A SceneLayout2d paints nothing, so the surface IS this panel. It keeps the DEFAULT
		rendering level: on the overlay path SubWindow::openOverlay lifts the whole layout onto
		RenderingLevel::Overlay, which is a pass of its own drawn after everything, and on the
		native path there is nothing behind the panel to be resolved against. */
		panel->setPathColor(config.fallbackColor, false);
		panel->setAnchorPoint(Anchor::TopLeft);
		panel->setContentSize(Size2(float(size.width), float(size.height)));

		if (native) {
			// The surface IS the panel: it fills whatever extent the window system settled on,
			// which is not necessarily the one that was asked for.
			panel->setPosition(Vec2(0.0f, float(size.height)));
			layout->setContentSizeDirtyCallback([layout = layout.get(), panel] {
				const auto s = layout->getContentSize();
				if (s.width > 0.0f && s.height > 0.0f) {
					panel->setPosition(Vec2(0.0f, s.height));
					panel->setContentSize(s);
				}
			});
		} else {
			/* The overlay path. SceneContent2d::pushOverlay stretches the layout it is given over
			the whole parent content and puts its origin at the bottom left - right for an overlay
			and wrong for a popup - so the panel is placed inside it, at the placement the surface
			already resolved. That rect is Y-down from the content's top; the scene is Y-up. */
			panel->addStyleClass("overlay");
			const auto rect = surface->getOverlayRect();
			panel->setPosition(Vec2(float(rect.x), parentHeight - float(rect.y)));
		}

		if (config.content) {
			config.content(surface, panel);
		}

		if (!native) {
			/* A native Popup is dismissed by the window system when the user clicks away from it.
			An overlay has no such contract, so the layout - which covers the whole parent content -
			listens for a press outside the panel and takes the surface down itself.

			Installed AFTER the content: whatever the content put on the panel is what the outside
			tap has to be measured against, and a menu's handler needs the chain that content made.

			WHAT KEEPS THE PRESS OFF THE SCENE UNDER THE OVERLAY IS THE FOCUS GROUP, not a
			swallowing listener, and the distinction is the whole of (5).

			A swallowed event makes its own listener the EXCLUSIVE owner of the rest of the
			gesture, so a layout-wide listener that answered "handled" to every press took the
			End along with it - and every tap inside an overlay popup died between its halves. A
			menu item could be pressed and never activated, a list row highlighted and never
			chosen, a colour swatch clicked and never picked; only the keyboard reached an
			overlay at all. Answering "not mine" instead hands the gesture back to the panel's own
			widgets, but it also lets the press fall through to whatever the overlay covers - and
			a text field down there captures it and steals the End just the same.

			An exclusive focus group is the one thing that separates the two: it removes every
			listener OUTSIDE the group from the dispatch without making any single listener the
			gesture's owner. basic2d::OverlaySurface does exactly this, for exactly this reason.

			The group goes on BEFORE the listener below it - systems on one node register in
			order, and a listener that registers first records no group. Touch only (the keyboard
			is not ours to claim), and Propagate, because the content is free to carry groups of
			its own: a ui::MenuSystem, a ui::FormSystem, an editor. */
			auto focus = layout->addSystem(Rc<FocusGroup>::create());
			focus->setEventMask(FocusGroup::EventMask(EventMaskTouch));
			focus->setFlags(FocusGroup::Flags::Exclusive | FocusGroup::Flags::Propagate);

			auto listener = layout->addSystem(Rc<InputListener>::create());
			listener->addTouchRecognizer([surface = surface.get(), panel, cb = config.onOutsideTap](
												 const GestureData &data) {
				// Not ours: the panel's own widgets own every gesture that starts on them.
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
			// Reached only for a press OUTSIDE the panel now. The group already hides it from
			// everything behind the overlay; this hides it from the surface's own content too.
			listener->setSwallowEvents(EventMaskTouch);
		}

		return layout;
	};

	// Through the session rather than SubWindow::open directly: it is what drops a live tooltip
	// before the popup takes over.
	if (auto session = SubWindowSession::get(window)) {
		return session->openPopup(sp::move(surfaceConfig));
	}
	return SubWindow::open(window, sp::move(surfaceConfig));
}

} // namespace stappler::xenolith::ui
