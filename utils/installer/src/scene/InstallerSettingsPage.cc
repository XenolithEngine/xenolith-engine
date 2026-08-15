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

#include "InstallerSettingsPage.h"
#include "InstallerAppController.h"
#include "InstallerStrings.h"

#include "XLUiSubWindow.h"
#include "XLUiWindowFrame.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"
#include "XLUiTextInput.h"
#include "XLUiCheckbox.h"
#include "XLUiButton.h"
#include "XLUiBadge.h"
#include "XLUiPanel.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h"
#include "XLEventListener.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XL2dSceneLayout.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The handle IS the window: dropping the last Rc dismisses it, so it is kept here for as long as
// the form is up and cleared by the close callback.
static Rc<ui::SubWindow> s_settingsWindow;

namespace {

// One reachability marker, kept in sync with the controller.
class ReachabilityBadge : public ui::Badge {
public:
	virtual bool init(SourceKind kind) {
		if (!ui::Badge::init()) {
			return false;
		}
		_kind = kind;
		return true;
	}

	virtual void handleEnter(Scene *scene) override {
		ui::Badge::handleEnter(scene);
		auto listener = addSystem(Rc<EventListener>::create());
		listener->listenForEvent(AppController::onReachabilityChanged,
				[this](const Event &event) {
			if (event.getDataValue().getString() == getSourceKindName(_kind)) {
				refresh();
			}
		});
		refresh();
	}

	void refresh() {
		auto controller = AppController::getInstance();
		if (!controller) {
			return;
		}
		const auto &info = controller->getReachability(_kind);
		switch (info.state) {
		case Reachability::Unknown:
			setText(StringView());
			setVariant(StringView());
			break;
		case Reachability::Checking:
			setText(strings::reachChecking());
			setVariant("checking");
			break;
		case Reachability::Ok:
			// The summary is the useful half: "22 refs" or the release the mirror carries says
			// more than "reachable" does.
			setText(info.message.empty() ? toString(strings::reachOk()) : info.message);
			setVariant("reachable");
			break;
		case Reachability::Failed:
			setText(info.message.empty() ? toString(strings::reachFailed()) : info.message);
			setVariant("unreachable");
			break;
		}

		/* Ask for a frame.

		A window is redrawn when something dirties it, and a change made from a CALLBACK does not
		count - nobody is touching this window while a probe is in flight. Probing a git remote has
		been measured at fifteen seconds here, far past any fixed "render for a while after opening"
		window, so the badge asks for the frame that shows its own new text. */
		if (auto *scene = getScene()) {
			if (auto *director = scene->getDirector()) {
				if (auto *server = director->getRenderServer()) {
					server->setReadyForNextFrame();
				}
			}
		}
	}

protected:
	using ui::Badge::init;
	SourceKind _kind = SourceKind::EngineRepo;
};

} // namespace

void showSettingsPage(NotNull<AppWindow> parent) {
	// The Config form rather than openUtility(), because the close callback is a Config field and
	// this slot must not be cleared until the surface has actually gone - releasing the Rc IS what
	// dismisses it.
	ui::SubWindow::Config config;
	config.type = sprt::window::WindowType::Utility;
	// Seven rows at 30px plus the 10px gaps, the frame and the Close button. The two path fields hold
	// absolute paths, so the form is wider than the label+control+marker grid strictly needs.
	config.size = Extent2(760, 470);
	config.minExtent = Extent2(560, 400);
	config.title = strings::settingsTitle();

	/* The same user-space decorations the main window asks for (InstallerInit.cpp), and for the
	same reason: this form draws its own frame below, so the window system must not draw one over
	it. AllowClose matters beyond appearance - AppWindow::hide() is a graceful CLOSE REQUEST, and a
	window that never advertised close can have that request refused while _inCloseRequest stays
	latched, leaving a surface that can no longer be dismissed. */
	config.flags = sprt::window::WindowCreationFlags::AllowMove
			| sprt::window::WindowCreationFlags::AllowResize
			| sprt::window::WindowCreationFlags::AllowClose
			| sprt::window::WindowCreationFlags::UserSpaceDecorations;
	/* NOT config.queue = the main window's queue.

	It is tempting, and it works spectacularly on the clock - measured here, from SubWindow::open to
	the form's content being built: 18069ms without it, 10ms with. That delay is render-queue
	compilation, it happens before the scene can even be adopted, and it is what "the form does not
	draw for the first few seconds" is.

	But the main window's queue is live and bound to its own swapchain, and handing the same one to
	a second surface crashes the render loop in core::FrameCache::clear(), releasing Rc's that are
	already gone. What Config::queue wants is a queue PREWARMED for this surface, not a borrowed
	one; producing that is engine-side work and is not done here. */
	config.onClose = [](NotNull<ui::SubWindow>) { s_settingsWindow = nullptr; };

	config.content = [](NotNull<ui::SubWindow> window) -> Rc<basic2d::SceneLayout2d> {
		auto layout = Rc<basic2d::SceneLayout2d>::create();
		layout->setName("settings-layout");
		/* The stylesheet, and not only a resolver.

		Where this becomes a REAL window it is a scene of its own, and the ui::StyleSystem that
		carries resources/style.css lives on the main window's scene content - so there was no
		stylesheet scope above these nodes at all, and StyleResolver::resolveStyleForNode answered
		`valid == false` for every one of them. The form came up unstyled: no sizes, no colours,
		nothing. On the in-scene fallback the layout is pushed under that same content and would
		inherit its sheet, but declaring one here is harmless there - the resolver applies outer
		sheets first and the inner one last, and both are this same file. */
		layout->addSystem(
				Rc<ui::StyleSystem>::create(FileInfo{"resources/style.css", FileCategory::Bundled}));
		layout->addSystem(Rc<ui::StyleResolver>::create(true));

		/* A SceneLayout2d is a plain Node and paints nothing, so where there is no Subwindows
		capability the form would be a bare set of labels floating over whatever the scene was
		already showing. The surface is this Panel - and it has to be RenderingLevel::Solid, the same
		correction buildConfirmOverlay() makes for the same reason: opaque geometry is drawn first
		and writes depth, while the surface pass only TESTS against it. A Panel left at its default
		Surface level therefore cannot cover the labels of the scene underneath it, whatever its
		z-order - the shell's headings and buttons went on showing straight through this form. The
		colour stays in CSS; only the pass, which CSS has no say over, is set here. */
		auto page = layout->addChild(Rc<ui::Panel>::create());
		page->setName("settings-page");
		page->setRenderingLevel(RenderingLevel::Solid);

		auto form = page->addSystem(Rc<ui::FormSystem>::create());
		form->setValueMode(ui::FormValueMode::Flat);
		form->setInvalidStyleClass("invalid");

		// The column's order is its z-order, and none of it may go NEGATIVE: a child at z < 0 is
		// drawn BEFORE the parent's own geometry, so the heading would end up under the Panel's
		// fill instead of on it.
		/* The window's own decorations, drawn by the application - the flags above tell the window
		system to stay out of the way, and this is what replaces what it would have drawn.

		No OS buttons: ButtonType::Os* acts on the window the node belongs to, which is this
		subwindow only where it became a real one. On the in-scene fallback the very same node would
		be minimizing and closing the MAIN window. The close button below goes through dismiss(),
		which is correct on both paths. */
		auto frame = page->addChild(Rc<ui::WindowFrame>::create(ui::WindowFrame::Config{
										.title = strings::settingsTitle(),
										.minimize = false,
										.maximize = false,
										.close = false,
									}),
				ZOrder(1));

		auto frameClose = static_cast<ui::Button *>(
				frame->addTrailing(Rc<ui::Button>::create([window = window.get()] {
			window->dismiss();
		})));
		frameClose->setName("settings-frame-close");
		frameClose->setIcon(basic2d::IconName::Navigation_close_solid);

		// The column's order is its z-order, and none of it may go NEGATIVE: a child at z < 0 is
		// drawn BEFORE the parent's own geometry, so it would end up under the Panel's fill.
		auto grid = page->addChild(Rc<Node>::create(), ZOrder(2));
		grid->setName("settings-grid");

		auto controller = AppController::getInstance();
		const auto &settings = controller->getSettings();

		// Rows get EXPLICIT, DISTINCT ZOrders: sortAllChildren is an unstable sort and the tab ring
		// is document order, so equal orders would make the Tab sequence arbitrary.
		int16_t z = 0;

		auto addLabel = [&](StringView text) {
			auto label = grid->addChild(Rc<basic2d::Label>::create(), ZOrder(z++));
			label->setType("label");
			label->addStyleClass("settings-label");
			label->setString(text);
		};

		/* `kind` selects the reachability marker for the third cell, or nothing when the field is a
		LOCAL PATH: "reachable" is a question about a URL, and a marker that stayed permanently
		Unknown next to a directory would read as a failed check rather than as no check. The cell is
		still filled, with an empty node — a grid fills in order, so a row that supplies two cells
		instead of three shifts every following row by one. */
		auto addTextField = [&](StringView name, StringView value,
									const SourceKind *kind = nullptr) {
			auto input = grid->addChild(Rc<ui::TextInput>::create(), ZOrder(z++));
			input->setName(name);
			input->setText(value);

			/* Apply on BLUR, not on submit (design.md).

			FormFieldSlots is move-only, so the adapter's slots cannot be taken and wrapped; the
			slots are filled here instead, duplicating what addFormField(TextInput) writes and
			adding the commit. The one that matters is setFocused(false) - the form calls it on
			every focus change, after the focus group has already switched. */
			ui::FormFieldSlots slots;
			auto *raw = input;
			slots.collect = [raw] { return Value(raw->getText()); };
			slots.assign = [raw](const Value &v) { raw->setText(v.getString()); };
			slots.clear = [raw] { raw->setText(StringView()); };
			slots.copy = [raw] { return raw->copy(); };
			slots.cut = [raw] { return raw->cut(); };
			slots.paste = [raw] { return raw->paste(); };
			slots.selectAll = [raw] {
				raw->selectAll();
				return true;
			};
			slots.ownsFocusStyle = true;
			slots.focusable = raw->isEnabled() && !raw->isReadOnly();
			slots.setFocused = [raw, key = toString(name)](bool value) {
				if (value) {
					raw->focus();
					return;
				}
				raw->blur();
				if (auto controller = AppController::getInstance()) {
					controller->setSettingsField(key, Value(raw->getText()));
				}
			};

			auto listener = ui::addFormField(raw, sp::move(slots), name);
			if (listener) {
				// Tab reaches the widget before this listener, so it has to hand it over rather
				// than fall back to its own blur().
				raw->setNavigateCallback(
						[listener](bool backwards) { return listener->requestNavigate(backwards); });
			}

			if (kind) {
				auto badge = grid->addChild(Rc<ReachabilityBadge>::create(*kind), ZOrder(z++));
				badge->setName(
						*kind == SourceKind::EngineRepo ? "engine-url-state" : "release-url-state");
			} else {
				grid->addChild(Rc<Node>::create(), ZOrder(z++));
			}
			return raw;
		};

		auto addCheckbox = [&](StringView name, bool value) {
			auto box = grid->addChild(Rc<ui::Checkbox>::create(), ZOrder(z++));
			box->setName(name);
			box->setChecked(value, true);
			// A checkbox has no blur worth waiting for: the toggle IS the decision.
			box->setCallback([key = toString(name)](bool checked) {
				if (auto controller = AppController::getInstance()) {
					controller->setSettingsField(key, Value(checked));
				}
			});
			ui::addFormField(box);

			// The third track, kept empty. A grid fills in order, so a row that supplies two cells
			// instead of three does not leave a gap - it shifts every following row by one.
			grid->addChild(Rc<Node>::create(), ZOrder(z++));
		};

		static constexpr SourceKind kEngineRepo = SourceKind::EngineRepo;
		static constexpr SourceKind kReleases = SourceKind::Releases;

		addLabel(strings::settingsEngineUrl());
		addTextField("engineRepoUrl", settings.sources.getEngineRepoUrl(), &kEngineRepo);

		addLabel(strings::settingsReleaseUrl());
		addTextField("releaseSourceUrl", settings.sources.getReleasesRoot(), &kReleases);

		/* The RESOLVED paths, not the stored ones: an empty field would say nothing about where the
		tools are actually looking, and this form is where a user goes to find that out. What is
		saved back is whatever the field holds when it loses focus — so a user who never touches
		these two pins today's default, which is the honest reading of "leave it alone" for a path
		that only ever moves when someone moves it. */
		addLabel(strings::settingsEnginePath());
		bool engineOk = false;
		auto enginePathInput = addTextField("enginePath",
				resolveEngineRoot(controller->getLayout(), StringView(), &engineOk));
		if (!engineOk) {
			// The same marker a rejected form field gets (`text-input.invalid` in style.css). There
			// is no validator on this field - the path is checked by resolving it, which has already
			// happened - so the class is set directly rather than through the form.
			enginePathInput->addStyleClass("invalid");
		}

		addLabel(strings::settingsToolchainsPath());
		addTextField("toolchainsPath", controller->getLayout().getToolchainsDir());

		addLabel(strings::settingsAutoUpdateInstaller());
		addCheckbox("autoUpdateInstaller", settings.autoUpdateInstaller);

		addLabel(strings::settingsAutoUpdateEngine());
		addCheckbox("autoUpdateEngine", settings.autoUpdateEngine);

		addLabel(strings::settingsAutoUpdateReleases());
		addCheckbox("autoUpdateReleases", settings.autoUpdateReleases);

		auto close = page->addChild(Rc<ui::Button>::create([window = window.get()] {
			window->dismiss();
		}), ZOrder(3));
		close->setName("settings-close");
		close->addStyleClass("primary");
		close->setString(strings::settingsClose());

		// Both sources are checked when the form opens, so the markers say something before the
		// user touches anything.
		controller->probeSource(SourceKind::EngineRepo, settings.sources.getEngineRepoUrl());
		controller->probeSource(SourceKind::Releases, settings.sources.getReleasesRoot());

		// No RenderContinuously here: where this becomes a real window, SubWindowScene already runs
		// one for the life of the surface, for exactly this reason. What the form still owes is a
		// frame when a probe lands on the IN-SCENE path, and the badge asks for that itself.

		return layout;
	};

	s_settingsWindow = ui::SubWindow::open(parent, sp::move(config));
}

bool closeSettingsPage() {
	if (!s_settingsWindow) {
		return false;
	}
	// Hold it across the call: dismiss() reaches the close callback, which clears the slot below,
	// and that may be the last reference.
	auto window = s_settingsWindow;
	window->dismiss();
	return true;
}

} // namespace stappler::xenolith::installer
