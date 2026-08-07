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

#include "InstallerDialogs.h"

#include "InstallerSceneContent.h"
#include "InstallerStrings.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dLayerRounded.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLAction.h"
#include "XLAppWindow.h"
#include "XLInputListener.h"
#include "XLUiButton.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// Tauri UI Kit `.dialog`: 380 wide, padding 20, radius 10, border #424242 / fill #232323.
constexpr float kDialogWidth = 380.0f;
constexpr float kDialogHeight = 172.0f;
constexpr float kDialogRadius = 10.0f;
constexpr float kBorderPx = 2.0f;

constexpr Color4B kDialogBorderColor(0x42, 0x42, 0x42, 0xff);
constexpr Color4B kDialogFillColor(0x23, 0x23, 0x23, 0xff);

struct DialogHandle : public Ref {
	Function<void()> onConfirm;
	Function<void()> dismiss;
};

void syncCardChrome(Node *card) {
	if (!card) {
		return;
	}
	const Size2 sz = card->getContentSize();
	const float w = sz.width > 0.0f ? sz.width : kDialogWidth;
	const float h = sz.height > 0.0f ? sz.height : kDialogHeight;
	for (auto &child : card->getChildren()) {
		if (child->getName() == "confirm-border") {
			child->setAnchorPoint(Anchor::BottomLeft);
			child->setPosition(Vec2::ZERO);
			child->setContentSize(Size2(w, h));
		} else if (child->getName() == "confirm-bg") {
			child->setAnchorPoint(Anchor::BottomLeft);
			child->setPosition(Vec2(kBorderPx, kBorderPx));
			child->setContentSize(Size2(w - kBorderPx * 2.0f, h - kBorderPx * 2.0f));
		} else if (child->getName() == "confirm-body") {
			child->setAnchorPoint(Anchor::BottomLeft);
			child->setPosition(Vec2::ZERO);
			child->setContentSize(Size2(w, h));
		}
	}
}

void layoutOverlay(basic2d::SceneLayout2d *overlay) {
	if (!overlay) {
		return;
	}
	const Size2 full = overlay->getContentSize();
	for (auto &child : overlay->getChildren()) {
		if (child->getName() == "confirm-backdrop") {
			child->setAnchorPoint(Anchor::BottomLeft);
			child->setPosition(Vec2::ZERO);
			child->setContentSize(full);
		} else if (child->getName() == "confirm-card") {
			child->setAnchorPoint(Anchor::Middle);
			child->setPosition(Vec2(full.width * 0.5f, full.height * 0.5f));
			child->setContentSize(Size2(kDialogWidth, kDialogHeight));
			child->setLocalZOrder(ZOrder(1));
			syncCardChrome(child.get());
		}
	}
}

Rc<basic2d::SceneLayout2d> buildConfirmOverlay(StringView title, StringView message,
		StringView confirmLabel, ConfirmTone tone, Rc<DialogHandle> handle) {
	auto overlay = Rc<basic2d::SceneLayout2d>::create();
	overlay->setName("confirm-overlay");
	// Same pattern as InstallerLayout: one recursive resolver so button.primary / :hover /
	// ghost outline come from resources/style.css — not hand-painted LayerRounded plates.
	overlay->addSystem(Rc<ui::StyleResolver>::create(true));

	auto dim = overlay->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.55f)));
	dim->setName("confirm-backdrop");
	dim->addStyleClass("confirm-dim");

	auto card = overlay->addChild(Rc<Node>::create());
	card->setName("confirm-card");
	card->addStyleClass("confirm-card");
	card->setLocalZOrder(ZOrder(1));

	// CSS subset has no box border on a plain Node — only this chrome stays in code.
	// Must be Solid: antialiased LayerRounded otherwise resolves to Transparent and paints
	// AFTER ui::Button Surface chrome / Label Surface titles (empty card + text-only buttons).
	// Node color must stay WHITE — pathColor is the paint; color×path would crush #232323 to near-black.
	auto border = card->addChild(Rc<basic2d::LayerRounded>::create(Color4F::WHITE, kDialogRadius));
	border->setName("confirm-border");
	border->setPathColor(kDialogBorderColor, true);
	border->setColor(Color4F::WHITE, true);
	border->setRenderingLevel(RenderingLevel::Solid);
	border->setLocalZOrder(ZOrder(0));

	auto bg = card->addChild(
			Rc<basic2d::LayerRounded>::create(Color4F::WHITE, kDialogRadius - kBorderPx));
	bg->setName("confirm-bg");
	bg->setPathColor(kDialogFillColor, true);
	bg->setColor(Color4F::WHITE, true);
	bg->setRenderingLevel(RenderingLevel::Solid);
	bg->setLocalZOrder(ZOrder(0));

	auto body = card->addChild(Rc<Node>::create());
	body->setName("confirm-body");
	body->addStyleClass("confirm-body");
	body->setLocalZOrder(ZOrder(1));

	auto titleText = body->addChild(Rc<basic2d::Label>::create());
	titleText->setType("label");
	titleText->addStyleClass("confirm-title");
	titleText->setString(title);
	titleText->setRenderingLevel(RenderingLevel::Transparent);

	auto msgText = body->addChild(Rc<basic2d::Label>::create());
	msgText->setType("label");
	msgText->addStyleClass("confirm-message");
	msgText->setString(message);
	msgText->setRenderingLevel(RenderingLevel::Transparent);

	auto actions = body->addChild(Rc<Node>::create());
	actions->setName("confirm-actions");
	actions->addStyleClass("confirm-actions");

	auto cancel = actions->addChild(Rc<ui::Button>::create([handle]() {
		if (handle->dismiss) {
			handle->dismiss();
		}
	}));
	cancel->setName("confirm-cancel");
	cancel->addStyleClass("ghost");
	cancel->setString(strings::actionCancel());

	auto confirm = actions->addChild(Rc<ui::Button>::create([handle]() {
		if (handle->onConfirm) {
			handle->onConfirm();
		}
		if (handle->dismiss) {
			handle->dismiss();
		}
	}));
	confirm->setName("confirm-ok");
	confirm->addStyleClass(tone == ConfirmTone::Danger ? "danger" : "primary");
	confirm->setString(confirmLabel);

	auto listener = dim->addSystem(Rc<InputListener>::create());
	listener->setSwallowAllEvents();
	listener->setTouchFilter(
			[card](const InputEvent &event, const InputListener::DefaultEventFilter &def) {
		if (event.data.hasLocation() && card && card->isTouched(event.currentLocation)) {
			return false;
		}
		return def(event);
	});
	listener->addTapRecognizer([handle](const GestureTap &tap) {
		if (tap.event != GestureEvent::Activated) {
			return true;
		}
		if (handle->dismiss) {
			handle->dismiss();
		}
		return true;
	}, InputTapInfo(1, true));

	return overlay;
}

} // namespace

void showConfirmDialog(NotNull<AppWindow> parent, StringView title, StringView message,
		StringView confirmLabel, ConfirmTone tone, Function<void()> &&onConfirm) {
	auto *content = getSceneContent(parent);
	if (!content) {
		return;
	}

	auto handle = Rc<DialogHandle>::create();
	handle->onConfirm = sp::move(onConfirm);

	auto overlay = buildConfirmOverlay(title, message, confirmLabel, tone, handle);
	// `content` owns the overlay while it is presented and drops it on dismiss; dismissOverlay
	// ignores a stale handle whose dialog was already replaced, so the strong ref here cannot
	// close somebody else's dialog.
	handle->dismiss = [content, overlay]() {
		content->stopActionByTag("ConfirmRelayout"_tag);
		content->dismissOverlay(overlay);
	};

	// Never hide/restore loading chrome from dialogs — that race left "Loading catalogue…"
	// stuck forever after onboarding confirm dismissed.
	content->presentOverlay(overlay);

	layoutOverlay(overlay.get());

	// Font metrics / flex measure often settle one frame late — re-sync card chrome only.
	content->runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.0f),
							   [overlay]() {
		layoutOverlay(overlay.get());
	}, Rc<DelayTime>::create(0.05f), [overlay]() { layoutOverlay(overlay.get()); }),
			"ConfirmRelayout"_tag);
}

} // namespace stappler::xenolith::installer
