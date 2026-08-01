/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "TestRegistry.h"

#include "ButtonLayout.h"
#include "CombinatorLayout.h"
#include "DamageLayout.h"
#include "FitContentLayout.h"
#include "FlexboxLayout.h"
#include "GeneralLayout.h"
#include "HoverLayout.h"
#include "InheritedStyleLayout.h"
#include "ParentResizeLayout.h"
#include "PlatformLayout.h"
#include "PugCascadeLayout.h"
#include "PugLayout.h"
#include "ShapingLayout.h"
#include "SpecificityLayout.h"
#include "VisibilityLayout.h"
#include "WatchCssLayout.h"
#include "WatchCssRecursiveLayout.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

template <typename T>
static Rc<basic2d::SceneLayout2d> TestRegistry_make() {
	return Rc<T>::create();
}

// Order matters only for the environment scan: the first entry whose variable is set wins.
static const TestInfo s_tests[] = {
	TestInfo{StringView("XL_SHAPING_TEST"), StringView("Text shaping"),
		StringView("Rows of the same text with shaping and bidi off, then on: kerning, ligatures, "
				   "Arabic joining and RTL order must differ between them."),
		TestRegistry_make<ShapingLayout>},

	TestInfo{StringView("XL_PUG_TEST"), StringView("Pug template + CSS"),
		StringView("Node tree built from a pug template and styled by selectors. The buttons flip "
				   "a class, swap the whole stylesheet, and re-run the template."),
		TestRegistry_make<PugLayout>},

	TestInfo{StringView("XL_PUG_CASCADE_TEST"), StringView("Pug template cascade"),
		StringView("The inner template uses a function and a variable it never defines: both must "
				   "resolve through the outer template system."),
		TestRegistry_make<PugCascadeLayout>},

	TestInfo{StringView("XL_FLEX_TEST"), StringView("Flexbox / grid placement"),
		StringView("The same boxes placed by both LayoutSystem backends; the control bar cycles "
				   "the container parameters, Mode switches flex and grid."),
		TestRegistry_make<FlexboxLayout>},

	TestInfo{StringView("XL_FITCONTENT_TEST"), StringView("fit-content sizing"),
		StringView("Boxes sized from their own content, including a nested fit-content container. "
				   "Changing a label's text must resize its ancestors."),
		TestRegistry_make<FitContentLayout>},

	TestInfo{StringView("XL_COMBINATOR_TEST"), StringView("CSS combinators"),
		StringView("Descendant, child, adjacent and general sibling. Per row the left swatch must "
				   "take the rule's colour and the right one must stay grey."),
		TestRegistry_make<CombinatorLayout>},

	TestInfo{StringView("XL_WATCH_CSS_TEST"), StringView("CSS live reload"),
		StringView("The stylesheet file is rewritten while running: the swatch must turn from red "
				   "to green with no restart."),
		TestRegistry_make<WatchCssLayout>},

	TestInfo{StringView("XL_WATCH_CSS_RECURSIVE_TEST"), StringView("CSS live reload (descendant)"),
		StringView("Same rewrite, but the styled node is a child covered by one recursive "
				   "resolver: the inner square must turn green too."),
		TestRegistry_make<WatchCssRecursiveLayout>},

	TestInfo{StringView("XL_HOVER_TEST"), StringView("Interactive pseudo-classes"),
		StringView("Fixed states first (grey, red, blue, green, purple), then one swatch that must "
				   "follow the pointer through :hover at runtime."),
		TestRegistry_make<HoverLayout>},

	TestInfo{StringView("XL_SPECIFICITY_TEST"), StringView("CSS specificity"),
		StringView("Every swatch matches several conflicting rules; the colour of the "
				   "highest-specificity one must win, source order only breaking ties."),
		TestRegistry_make<SpecificityLayout>},

	TestInfo{StringView("XL_BUTTON_TEST"), StringView("ui::Button styling"),
		StringView("Fill and outline drawn by the button type appliers, label styled by the same "
				   "recursive resolver. The lower button verifies per-corner radii."),
		TestRegistry_make<ButtonLayout>},

	TestInfo{StringView("XL_PLATFORM_TEST"), StringView("@media (platform: ...)"),
		StringView("The swatch takes the colour of the platform it runs on; the rules for every "
				   "other platform must be filtered out."),
		TestRegistry_make<PlatformLayout>},

	TestInfo{StringView("XL_INHERITED_TEST"), StringView("Inherited CSS properties"),
		StringView("Labels with explicit small black text must render with the inherited style "
				   "instead - and revert once the rule is removed."),
		TestRegistry_make<InheritedStyleLayout>},

	TestInfo{StringView("XL_VISIBILITY_TEST"), StringView("display:none / visibility:hidden"),
		StringView("Row 1 must collapse the hidden box, row 2 must keep its gap. Removing the "
				   "classes must restore both rows identically."),
		TestRegistry_make<VisibilityLayout>},

	TestInfo{StringView("XL_PARENT_RESIZE_TEST"), StringView("Restyle on parent resize"),
		StringView("Percent metrics resolved against the parent: the nested boxes must keep their "
				   "proportions when the containers change size."),
		TestRegistry_make<ParentResizeLayout>},

	TestInfo{StringView("XL_DAMAGE_TEST"), StringView("Damage tracking"),
		StringView("A red square jumps in discrete steps beside a static grey one. Exactly one red "
				   "square must be visible at any moment - a second one is a trail."),
		TestRegistry_make<DamageLayout>, true},

	// Default, selected when nothing above is set. Must stay last.
	TestInfo{StringView(), StringView("General demo"),
		StringView("Application menu: the other demos, plus window, monitor and fullscreen "
				   "controls."),
		TestRegistry_make<GeneralLayout>},
};

SpanView<TestInfo> getTestRegistry() { return makeSpanView(s_tests); }

const TestInfo &getSelectedTest() {
	for (auto &it : s_tests) {
		if (!it.env.empty() && ::getenv(it.env.data()) != nullptr) {
			return it;
		}
	}
	return s_tests[sizeof(s_tests) / sizeof(s_tests[0]) - 1];
}

Rc<basic2d::SceneLayout2d> makeTestLayout(const TestInfo &info) {
	auto layout = info.make();

	// The general demo is a TestLayout as well, but it is the app's front page rather than a test,
	// so it is the one entry deliberately left without a caption.
	if (!info.env.empty()) {
		if (auto test = dynamic_cast<TestLayout *>(layout.get())) {
			test->setTestInfo(info.title, info.description, info.env);
			test->setHideFps(info.hideFps);
		}
	}

	return layout;
}

Rc<basic2d::SceneLayout2d> makeSelectedTestLayout() { return makeTestLayout(getSelectedTest()); }

} // namespace stappler::xenolith::app
