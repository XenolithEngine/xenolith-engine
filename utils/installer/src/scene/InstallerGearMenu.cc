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

#include "InstallerGearMenu.h"
#include "InstallerDialogs.h"
#include "InstallerProjects.h"
#include "InstallerSceneContent.h"
#include "InstallerStrings.h"
#include "InstallerDoctor.h"

#include "XL2dLayer.h"
#include "XL2dLayerRounded.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLInputListener.h"
#include "XLUiButton.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// Popup card geometry. Unlike the rest of the shell this is not CSS-driven: the menu is a
// free-floating panel outside the flex tree, positioned against the window corner.
constexpr Size2 kGearCardSize(260.0f, 180.0f);
constexpr float kGearItemHeight = 40.0f;

} // namespace

void showGearMenu(NotNull<AppWindow> parent, InstallerController *controller) {
	auto *content = getSceneContent(parent);
	if (!content) {
		return;
	}

	auto overlay = Rc<basic2d::SceneLayout2d>::create();
	overlay->setName("gear-overlay");
	overlay->addSystem(Rc<ui::StyleResolver>::create(true));

	auto dim = overlay->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.45f)));
	dim->setName("gear-dim");

	auto card =
			overlay->addChild(Rc<basic2d::LayerRounded>::create(Color4F::WHITE, 8.0f), ZOrder(1));
	card->setName("gear-card");
	card->setPathColor(Color4B(0x23, 0x23, 0x23, 0xff), true);
	card->setColor(Color4F::WHITE, true);
	card->setRenderingLevel(RenderingLevel::Solid);
	card->setContentSize(kGearCardSize);

	// Every item closes the menu before acting: an item that opens a dialog would otherwise
	// present it into the same modal slot and leave the menu behind it.
	float itemTop = kGearCardSize.height - 20.0f;
	auto addItem = [&](StringView label, Function<void()> &&cb) {
		auto btn = card->addChild(Rc<ui::Button>::create(
				[content, overlay = overlay.get(), cb = sp::move(cb)]() mutable {
			content->dismissOverlay(overlay);
			cb();
		}));
		btn->addStyleClass("gear-item");
		btn->setString(label);
		btn->setContentSize(Size2(kGearCardSize.width - 40.0f, 36.0f));
		btn->setAnchorPoint(Anchor::MiddleTop);
		btn->setPosition(Vec2(kGearCardSize.width / 2.0f, itemTop));
		itemTop -= kGearItemHeight;
	};

	addItem(strings::gearOpenDataDir(), [parent, controller] {
		if (controller) {
			controller->openFolder(parent, controller->layout().data);
		}
	});
	addItem(strings::gearStorage(),
			[parent, controller] { showStorageDialog(parent, controller); });
	addItem(strings::gearSettings(), [parent] { showSettingsDialog(parent); });
	addItem(strings::gearDoctor(), [parent, controller] { showDoctorDialog(parent, controller); });

	// Same as confirm dialog: dim must NOT claim touches that land on the card, or menu
	// buttons never receive Activated (swallow-all ate every click on the panel).
	auto listener = dim->addSystem(Rc<InputListener>::create());
	listener->setSwallowAllEvents();
	listener->setTouchFilter(
			[card](const InputEvent &event, const InputListener::DefaultEventFilter &def) {
		if (event.data.hasLocation() && card && card->isTouched(event.currentLocation)) {
			return false;
		}
		return def(event);
	});
	listener->addTapRecognizer([content, overlay = overlay.get()](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			content->dismissOverlay(overlay);
		}
		return true;
	}, InputTapInfo(1, true));

	content->presentOverlay(overlay);

	const Size2 full = overlay->getContentSize();
	dim->setAnchorPoint(Anchor::BottomLeft);
	dim->setPosition(Vec2::ZERO);
	dim->setContentSize(full);
	card->setAnchorPoint(Anchor::TopRight);
	card->setPosition(Vec2(full.width - 24.0f, full.height - 56.0f));
}

} // namespace stappler::xenolith::installer
