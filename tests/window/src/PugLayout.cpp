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

#include "PugLayout.h"

#include <stdlib.h> // getenv for the headless dark-theme screenshot check

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace pugui;

// The demo UI: a flex column; geometry comes from pug attributes, colors and
// fonts come from the CSS stylesheet attached to the layout node.
static constexpr auto s_pugTemplate =
R"Pug(flex(direction="column" align-items="center" justify-content="center" gap=12 padding=16)
	label#hero(flex-basis=36) Pug + CSS demo
	label#greet.subtitle(flex-basis=24) Hello, #{user}!
	layer.stripe(flex-basis=28 cross-size=280)
	layer.stripe.alt(flex-basis=28 cross-size=240)
	button-label(on-tap="accent" flex-basis=36 cross-size=220) Toggle accent
	button-label(on-tap="theme" flex-basis=36 cross-size=220) Toggle theme
	button-label(on-tap="tap" flex-basis=36 cross-size=220) Rebuild: #{taps}
)Pug";

static constexpr StringView s_lightCss(
R"Css(
	label { color: #212121; }
	#hero { font-size: 28px; }
	label.subtitle { font-size: 18px; color: #616161; }
	label.accent { color: #E91E63; }
	.stripe { background-color: #3F51B5; }
	.alt { background-color: #009688; }
	button-label { background-color: #CFD8DC; }
)Css");

static constexpr StringView s_darkCss(
R"Css(
	label { color: #ECEFF1; }
	#hero { font-size: 28px; }
	label.subtitle { font-size: 18px; color: #90A4AE; }
	label.accent { color: #FFC107; }
	.stripe { background-color: #7986CB; }
	.alt { background-color: #4DB6AC; }
	button-label { background-color: #455A64; }
)Css");

bool PugLayout::init() {
	if (!SceneLayout2d::init()) {
		return false;
	}

	_lightSheet = Rc<StyleSheet>::create();
	_lightSheet->addStyle(s_lightCss);

	_darkSheet = Rc<StyleSheet>::create();
	_darkSheet->addStyle(s_darkCss);

	// the stylesheet is attached to THIS node; every descendant resolves against it
	_styles = addSystem(Rc<StyleSheetSystem>::create(Rc<StyleSheet>(_lightSheet)));

	// the pug template is rendered by a TemplateSystem attached to this node; it holds its
	// Context (variables/functions) persistently and rebuilds the subtree on demand
	BuilderConfig config;
	config.enableStyles = true; // attach StyleApplier to every produced node
	config.resolveHandler = [this](StringView name) -> Function<void()> {
		if (name == "tap") {
			return [this] {
				++_taps;
				_template->setVariable("taps", Value(_taps));
				_template->rebuild();
			};
		} else if (name == "theme") {
			return [this] { toggleTheme(); };
		} else if (name == "accent") {
			return [this] {
				if (auto greet = findByName(_tree, "greet")) {
					simpleui::toggleStyleClass(greet, "accent");
				}
			};
		}
		return nullptr;
	};
	config.onError = [](StringView err) { log::source().warn("PugLayout", err); };

	_template = addSystem(Rc<pugui::TemplateSystem>::create(StringView(s_pugTemplate),
			move(config)));
	_template->setVariable("user", Value("Xenolith"));
	_template->setVariable("taps", Value(_taps));
	_template->setBuildCallback([this](pugui::TemplateSystem *, SpanView<Rc<Node>> roots) {
		_tree = roots.empty() ? nullptr : roots.back().get();
		handleContentSizeDirty();
	});

	// headless check of post-enter re-resolution: switch the whole stylesheet
	// after entering the scene, before the XL_SCREENSHOT_FILE capture
	if (::getenv("XL_PUG_DARK")) {
		runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.5f), [this] { toggleTheme(); }));
	}

	return true;
}

void PugLayout::toggleTheme() {
	_dark = !_dark;
	if (_styles) {
		_styles->setStyleSheet(Rc<StyleSheet>(_dark ? _darkSheet : _lightSheet));
	}
}

Node *PugLayout::findByName(Node *root, StringView name) const {
	if (!root) {
		return nullptr;
	}
	if (root->getName() == name) {
		return root;
	}
	for (auto &child : root->getChildren()) {
		if (auto found = findByName(child.get(), name)) {
			return found;
		}
	}
	return nullptr;
}

void PugLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	if (_tree) {
		_tree->setAnchorPoint(Anchor::BottomLeft);
		_tree->setPosition(Vec2::ZERO);
		_tree->setContentSize(_contentSize);
	}
}

} // namespace stappler::xenolith::app
