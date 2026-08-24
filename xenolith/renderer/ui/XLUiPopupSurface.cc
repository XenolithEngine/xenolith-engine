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
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

Rc<SubWindow> openPopupSurface(NotNull<AppWindow> window,
		const sprt::window::WindowPlacement &placement, PopupSurfaceConfig &&config) {
	auto director = window->getDirector();
	auto scene = director ? director->getScene() : nullptr;
	auto parentContent = scene ? scene->getContent() : nullptr;

	// The overlay path places the panel from the parent's height, and it is read HERE rather than
	// in the builder: on the native path the builder runs in another scene entirely.
	const float parentHeight = parentContent ? parentContent->getContentSize().height : 0.0f;

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
			[config = config, parentHeight](
					NotNull<SubWindow> surface) mutable -> Rc<basic2d::SceneLayout2d> {
		auto layout = Rc<basic2d::SceneLayout2d>::create();
		layout->setName(config.layoutName.empty() ? StringView("popup-layout")
												  : StringView(config.layoutName));

		const bool native = surface->isNative();

		if (native && (!config.stylesheet.empty() || !config.stylesheetSource.empty())) {
			// A native popup is a scene of its own and the parent window's sheet does not reach it.
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
		}
		if (!config.panelClass.empty()) {
			panel->addStyleClass(config.panelClass);
		}

		/* A SceneLayout2d paints nothing, so the surface IS this panel - and it has to be
		RenderingLevel::Solid: opaque geometry is drawn first and writes depth, while the surface
		pass only TESTS against it, so a panel left at the default level cannot cover the labels of
		whatever is underneath it on the overlay path. */
		panel->setRenderingLevel(RenderingLevel::Solid);
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
			tap has to be measured against, and a menu's handler needs the chain that content made. */
			auto listener = layout->addSystem(Rc<InputListener>::create());
			listener->addTouchRecognizer([surface = surface.get(), panel, cb = config.onOutsideTap](
												 const GestureData &data) {
				if (data.event == GestureEvent::Began && !panel->isTouched(data.location())) {
					if (cb) {
						cb(surface, panel);
					} else {
						surface->dismiss();
					}
				}
				return true;
			});
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
