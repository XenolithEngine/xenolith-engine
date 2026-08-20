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

#include "app/TestRegistry.h"

#include "layout/AutoMarginLayout.h"
#include "css/NthChildLayout.h"
#include "css/CssVarLayout.h"
#include "css/CalcLayout.h"
#include "widgets/ButtonLayout.h"
#include "css/CombinatorLayout.h"
#include "render/DamageLayout.h"
#include "layout/FitContentLayout.h"
#include "layout/FlexboxLayout.h"
#include "layout/TableLayout.h"
#include "app/GeneralLayout.h"
#include "css/HoverLayout.h"
#include "layout/CssFlowLayout.h"
#include "layout/OverflowLayout.h"
#include "css/InheritedStyleLayout.h"
#include "layout/LabelUpdateLayout.h"
#include "layout/MeasureProtocolLayout.h"
#include "window/MultiWindowLayout.h"
#include "window/GeometryLayout.h"
#include "window/QueueCacheLayout.h"
#include "widgets/NumberFieldLayout.h"
#include "widgets/VectorFieldLayout.h"
#include "widgets/ColorFieldLayout.h"
#include "widgets/ChipRowLayout.h"
#include "widgets/ClipboardLayout.h"
#include "widgets/TextViewLayout.h"
#include "widgets/PanelLayout.h"
#include "widgets/SelectLayout.h"
#include "widgets/SearchPickerLayout.h"
#include "widgets/InlineEditorLayout.h"
#include "widgets/TableViewLayout.h"
#include "widgets/TextInputLayout.h"
#include "widgets/FormLayout.h"
#include "widgets/HotkeyLayout.h"
#include "widgets/MenuLayout.h"
#include "layout/ParentResizeLayout.h"
#include "css/PlatformLayout.h"
#include "template/PugCascadeLayout.h"
#include "template/PugLayout.h"
#include "render/RenderLevelLayout.h"
#include "widgets/ScrollThrashLayout.h"
#include "text/ShapingLayout.h"
#include "css/SpecificityLayout.h"
#include "css/VisibilityLayout.h"
#include "css/WatchCssLayout.h"
#include "css/WatchCssRecursiveLayout.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

template <typename T>
static Rc<basic2d::SceneLayout2d> TestRegistry_make() {
	return Rc<T>::create();
}

// The registry is the source tree: one array of tests per directory under `src/`, tied together by
// the group list at the bottom, and a test is addressed the way its sources are - `css/nth`.
//
// Order matters twice, and both times it is the order written here: the menu shows a group's
// entries in it, and the environment scan walks the groups in the order they are declared, taking
// the first entry whose variable is set.

// src/css - CSS engine: selectors, cascade, live reload

// clang-format off
static const TestInfo s_cssTests[] = {
	TestInfo{StringView("combinator"), StringView("XL_COMBINATOR_TEST"), StringView("CSS combinators"),
		StringView("Descendant, child, adjacent and general sibling. Per row the left swatch must "
				   "take the rule's colour and the right one must stay grey."),
		TestRegistry_make<CombinatorLayout>},

	TestInfo{StringView("specificity"), StringView("XL_SPECIFICITY_TEST"), StringView("CSS specificity"),
		StringView("Every swatch matches several conflicting rules; the colour of the "
				   "highest-specificity one must win, source order only breaking ties."),
		TestRegistry_make<SpecificityLayout>},

	TestInfo{StringView("nth"), StringView("XL_NTH_TEST"), StringView("Structural pseudo-class selectors"),
		StringView("Rows of swatches coloured by :nth-child and friends; the last two rows are "
				   "mutated at runtime, so their colours must shift as items are inserted, "
				   "removed and re-ordered."),
		TestRegistry_make<NthChildLayout>},
 
	TestInfo{StringView("hover"), StringView("XL_HOVER_TEST"), StringView("Interactive pseudo-classes"),
		StringView("Fixed states first (grey, red, blue, green, purple), then one swatch that must "
				   "follow the pointer through :hover at runtime."),
		TestRegistry_make<HoverLayout>},

	TestInfo{StringView("css-var"), StringView("XL_CSSVAR_TEST"), StringView("CSS custom properties and var()"),
		StringView("Boxes coloured and sized through variables, including a fallback, a nested "
				   "reference and a cycle that must be dropped; the last box repaints when a "
				   "class on its ancestor overrides the variable."),
		TestRegistry_make<CssVarLayout>},

	TestInfo{StringView("calc"), StringView("XL_CALC_TEST"), StringView("calc() and per-node custom properties"),
		StringView("Rows of boxes sized by arithmetic; the five after them use expressions that "
				   "cannot reduce to one unit and must fall back to 40px. The last rows take "
				   "their width from a property declared on the node itself - one widens and one "
				   "falls back to a default while the test runs."),
		TestRegistry_make<CalcLayout>},

	TestInfo{StringView("inherited"), StringView("XL_INHERITED_TEST"), StringView("Inherited CSS properties"),
		StringView("Labels with explicit small black text must render with the inherited style "
				   "instead - and revert once the rule is removed."),
		TestRegistry_make<InheritedStyleLayout>},

	TestInfo{StringView("visibility"), StringView("XL_VISIBILITY_TEST"), StringView("display:none / visibility:hidden"),
		StringView("Row 1 must collapse the hidden box, row 2 must keep its gap. Removing the "
				   "classes must restore both rows identically."),
		TestRegistry_make<VisibilityLayout>},

	TestInfo{StringView("platform"), StringView("XL_PLATFORM_TEST"), StringView("@media (platform: ...)"),
		StringView("The swatch takes the colour of the platform it runs on; the rules for every "
				   "other platform must be filtered out."),
		TestRegistry_make<PlatformLayout>},

	TestInfo{StringView("watch-css"), StringView("XL_WATCH_CSS_TEST"), StringView("CSS live reload"),
		StringView("The stylesheet file is rewritten while running: the swatch must turn from red "
				   "to green with no restart."),
		TestRegistry_make<WatchCssLayout>},

	TestInfo{StringView("watch-css-recursive"), StringView("XL_WATCH_CSS_RECURSIVE_TEST"), StringView("CSS live reload (descendant)"),
		StringView("Same rewrite, but the styled node is a child covered by one recursive "
				   "resolver: the inner square must turn green too."),
		TestRegistry_make<WatchCssRecursiveLayout>},
};

// src/layout - placement and measurement
static const TestInfo s_layoutTests[] = {
	TestInfo{StringView("flex"), StringView("XL_FLEX_TEST"), StringView("Flexbox / grid placement"),
		StringView("The same boxes placed by both LayoutSystem backends; the control bar cycles "
				   "the container parameters, Mode switches flex and grid."),
		TestRegistry_make<FlexboxLayout>},

	TestInfo{StringView("table"), StringView("XL_TABLE_TEST"),
		StringView("Table placement: shared columns, spans, collapsed borders"),
		StringView("Four rows over one column template, each row a node of its own. D spans two "
				   "columns, F spans two rows and row 3 skips the column it still owns. The "
				   "control bar cycles the template, table-layout and border-collapse."),
		TestRegistry_make<TableLayout>},

	TestInfo{StringView("css-flow"), StringView("XL_CSS_FLOW_TEST"), StringView("Size, flow and draw order of a flex item"),
		StringView("Row 1: min-width and max-width hold, the third box absorbs the rest. Row 2: the "
				   "black overlay must not shrink its siblings. Row 3: placed green-blue-red by "
				   "`order`, drawn red-blue-green by `-xl-z-order`."),
		TestRegistry_make<CssFlowLayout>},

	TestInfo{StringView("overflow"), StringView("XL_OVERFLOW_TEST"),
		StringView("CSS overflow: clipping and scrolling"),
		StringView("Five boxes over the same content. `overflow-y: auto` keeps the items at their "
				   "declared height and scrolls; `visible` still crushes them; `hidden` clips an "
				   "oversized child; a box whose content fits keeps flex-grow working and reports "
				   "no range; and a single non-visible axis coerces the other one."),
		TestRegistry_make<OverflowLayout>},

	TestInfo{StringView("auto-margin"), StringView("XL_AUTO_MARGIN_TEST"), StringView("margin: auto on a flex item"),
		StringView("Row 1 pushes its last box to the right edge, row 2 centres its only box, rows 3 "
				   "and 4 centre one box vertically against align-items: flex-start and stretch."),
		TestRegistry_make<AutoMarginLayout>},

	TestInfo{StringView("fit-content"), StringView("XL_FITCONTENT_TEST"), StringView("fit-content sizing"),
		StringView("Boxes sized from their own content, including a nested fit-content container. "
				   "Changing a label's text must resize its ancestors."),
		TestRegistry_make<FitContentLayout>},

	TestInfo{StringView("measure"), StringView("XL_MEASURE_TEST"), StringView("Content measurement protocol"),
		StringView("Six boxes sized by six different routes: a custom measure system, the "
				   "MeasureComponent fallback, a Label, and one fixed box that must not be "
				   "measured at all."),
		TestRegistry_make<MeasureProtocolLayout>},

	TestInfo{StringView("label-update"), StringView("XL_LABEL_UPDATE_TEST"), StringView("Label text change after layout"),
		StringView("Three identical chains per group; the last one gets its text after the first "
				   "layout and then loses it again. Every box must match the chain built with that "
				   "text."),
		TestRegistry_make<LabelUpdateLayout>},

	TestInfo{StringView("parent-resize"), StringView("XL_PARENT_RESIZE_TEST"), StringView("Restyle on parent resize"),
		StringView("Percent metrics resolved against the parent: the nested boxes must keep their "
				   "proportions when the containers change size."),
		TestRegistry_make<ParentResizeLayout>},
};

// src/widgets - ui:: widgets and their styling; scroll virtualization
static const TestInfo s_widgetsTests[] = {
	TestInfo{StringView("button"), StringView("XL_BUTTON_TEST"), StringView("ui::Button styling"),
		StringView("Fill and outline drawn by the button type appliers, label styled by the same "
				   "recursive resolver. The lower button verifies per-corner radii."),
		TestRegistry_make<ButtonLayout>},

	TestInfo{StringView("panel"), StringView("XL_PANEL_TEST"), StringView("ui::Panel / Checkbox / Badge styling"),
		StringView("Panel, checkbox and badge take their fill and corners from CSS through their "
				   "own type appliers; the last box is a plain Layer under the same rule."),
		TestRegistry_make<PanelLayout>},

	TestInfo{StringView("text-input"), StringView("XL_TEXT_INPUT_TEST"), StringView("ui::TextInput"),
		StringView("Four fields: plain, password, read-only and one whose text overflows. Typing "
				   "must move the caret, selection must highlight, a long press must select the "
				   "word under the finger and then, held on, everything, the focused field must "
				   "take the accent outline from `text-input:focus`, and the long field must clip "
				   "at its border. Drive it over the inspector: text-input.focus, send_input "
				   "native=true, send_text."),
		TestRegistry_make<TextInputLayout>, true},

	TestInfo{StringView("form"), StringView("XL_FORM_TEST"), StringView("ui::FormSystem"),
		StringView("Two text fields, a checkbox, a transient field, one collapsed field and "
				   "submit/reset buttons. Tab must walk them in document order and wrap, Shift+Tab "
				   "must walk back, a collapsed or disabled field must drop out of the ring, Enter "
				   "must submit, and a missing or malformed required field must block the submit "
				   "and take the `.invalid` outline. Drive it over the inspector: form.state, "
				   "form.collect, send_input native=true."),
		TestRegistry_make<FormLayout>, true},

	TestInfo{StringView("hotkey"), StringView("XL_HOTKEY_TEST"), StringView("Global hotkeys"),
		StringView("Four subscribers on one combination: one that declines, one global, one "
				   "FocusedOnly inside a SingleFocus group and one inside an Exclusive group. The "
				   "focused subscriber must be offered the key first, a decline must fall through "
				   "to the next, the first to accept must stop the walk, and a combination nobody "
				   "handled must still reach an ordinary key recognizer. Drive it over the "
				   "inspector: hotkey.log, hotkey.list, hotkey.rebind, send_input native=true."),
		TestRegistry_make<HotkeyLayout>, true},

	TestInfo{StringView("menu"), StringView("XL_MENU_TEST"), StringView("ui::MenuSource / ui::MenuSystem"),
		StringView("One model shown twice: an inline menu and a popup. The leading icons and the "
				   "accelerators must line up in one column across every row, the long title must "
				   "wrap and make its row taller, the reported row height must equal the height "
				   "the row is drawn at, a hidden item must not occupy a row, the custom node must "
				   "be built exactly once, a KeepOpen toggle must not close the menu and a submenu "
				   "must open as a second surface that the root takes down with it. Drive it over "
				   "the inspector: menu.metrics, menu.state, menu.open, menu.activate."),
		TestRegistry_make<MenuLayout>, true},

	TestInfo{StringView("number"), StringView("XL_NUMBER_TEST"), StringView("ui::NumberField"),
		StringView("Four numeric fields: whole, real, one ranged 0..999 and one in a form. Typing a "
				   "fractional part into the whole field must be refused, and so must a number past "
				   "the declared range - the value stays put, no callback fires and the node takes "
				   "the `.invalid` outline - while DRAGGING an unfocused field past the end must "
				   "stop at it. Up and Down step by exactly one step, blur restores the text of a "
				   "refused edit, and parse(format(v)) must come back the same number. Drive it "
				   "over the inspector: number.state, number.set-text, number.roundtrip, "
				   "send_input native=true."),
		TestRegistry_make<NumberFieldLayout>, true},

	TestInfo{StringView("select"), StringView("XL_SELECT_TEST"), StringView("ui::Select"),
		StringView("Two drop-downs and a text field. The closed control must show the chosen "
				   "option's title and icon; opening it must produce a menu SURFACE whose rows are "
				   "the options, with the current one checked and highlighted. In the open list the "
				   "arrows must walk the rows and skip the disabled one, Enter must choose and "
				   "close, Escape must close and change nothing - and while it is open the field "
				   "beside it must not see a single arrow. Closed, Up/Down step the value in place. "
				   "The second control is a form field: what is collected is the option's id. Drive "
				   "it over the inspector: select.state, select.open, select.step, send_input "
				   "native=true."),
		TestRegistry_make<SelectLayout>, true},

	TestInfo{StringView("search-picker"), StringView("XL_SEARCH_PICKER_TEST"),
		StringView("ui::SearchPicker"),
		StringView("A query line over a virtualized result list, and the same surface behind a control "
				   "that opens a popup. Typing must narrow the list and order it by score; the "
				   "highlighted characters must be the ones the matcher named, in UTF-16 units - the row "
				   "led by two emoji is what tells the two index spaces apart. Up/Down must walk the "
				   "rows while the caret stays in the query line, Enter must choose and Escape must close "
				   "without choosing; the field beside the control must not see an arrow while the popup "
				   "is up. Drive it over the inspector: search-picker.state, search-picker.query, "
				   "search-picker.select, search-picker.mode, send_input native=true."),
		TestRegistry_make<SearchPickerLayout>, true},

	TestInfo{StringView("inline-edit"), StringView("XL_INLINE_EDIT_TEST"),
		StringView("ui::InlineEditor"),
		StringView("An editor placed OVER a rectangle instead of inside what it edits. Double-clicking "
				   "the label opens one; the table opens one over a cell. Rebuilding every row of the "
				   "table underneath an open editor must leave the typed text alone - that is the whole "
				   "point of the widget - while scrolling must END the edit and KEEP what was typed. "
				   "Escape must put the original back, a refused commit must leave the editor open, and "
				   "the field beside them must not see a key while an editor is up. Drive it over the "
				   "inspector: inline-edit.state, inline-edit.begin, inline-edit.rebuild, "
				   "inline-edit.scroll, send_input native=true."),
		TestRegistry_make<InlineEditorLayout>, true},

	TestInfo{StringView("table-reorder"), StringView("XL_TABLE_REORDER_TEST"),
		StringView("ui::TableView row geometry and reorder"),
		StringView("A table whose rows are dragged by a grip column and moved by Alt+Up / Alt+Down. "
				   "The grip is a column the CALLER declared, so the other columns keep their numbers. "
				   "Dragging and table.reorder must produce the same order; the insertion line must sit "
				   "on a row boundary and never inside a row; a refused move must change neither the "
				   "order nor the selection; and after a move the selection must follow the ROW, not the "
				   "index. Row geometry answers for rows scrolled out of view, which is what the drop "
				   "index is derived from. Drive it over the inspector: table.state, table.reorder, "
				   "table.row-rect, table.boundary-at, send_input native=true."),
		TestRegistry_make<TableViewLayout>, true},

	TestInfo{StringView("vector"), StringView("XL_VECTOR_TEST"), StringView("ui::VectorField"),
		StringView("Three rows of number fields that are one value each, and a text field after "
				   "them. What the form collects must be ONE array under one name; Tab must walk "
				   "the components and leave the row only at its ends, and Shift+Tab entering the "
				   "row must land on its LAST component. A number typed past the declared range "
				   "must mark the ROW, name the component in the message and leave the other "
				   "components alone, while dragging past the end must stop at it. Changing the "
				   "arity must keep the values that still have an index. Drive it over the "
				   "inspector: vector.state, vector.set-text, vector.set-arity, send_input "
				   "native=true."),
		TestRegistry_make<VectorFieldLayout>, true},

	TestInfo{StringView("color"), StringView("XL_COLOR_TEST"), StringView("ui::ColorField"),
		StringView("Three colour fields and a text field. Headless has no system colour dialog, so "
				   "`auto` must open the widget's OWN picker - a real surface with the palette in "
				   "it - while `system`, asked for explicitly, must fail with a reason instead of "
				   "opening nothing. The hex line must read whatever sprt::geom::readColor reads, "
				   "Enter must keep a refusal on screen and blur must put the value's own text "
				   "back, and the form must collect one hex string. Drive it over the inspector: "
				   "color.state, color.set-mode, color.open, send_input native=true."),
		TestRegistry_make<ColorFieldLayout>, true},

	TestInfo{StringView("chip"), StringView("XL_CHIP_TEST"), StringView("ui::Chip / ui::ChipRow"),
		StringView("Four rows of chips and a text field. A row is ONE form field: the form must "
				   "collect one ARRAY of ids in left-to-right order, and a Required row that is "
				   "empty must be refused once. Removing the middle chip must leave the order of "
				   "the rest alone; at the declared maximum the \"+\" must be dead and open "
				   "nothing, and with unique ids the options already in the row must come up "
				   "disabled IN THE MENU. Backspace with nothing selected must SELECT the last "
				   "chip rather than delete it. The narrow row must wrap, and the height it "
				   "reports must be the height it draws at. Drive it over the inspector: "
				   "chip.state, chip.set-width, chip.open, send_input native=true."),
		TestRegistry_make<ChipRowLayout>, true},

	TestInfo{StringView("clipboard"), StringView("XL_CLIPBOARD_TEST"),
		StringView("xenolith::ClipboardSession"),
		StringView("One payload with two representations, and a preference list that decides which "
				   "comes back. A list matching nothing must still be answered EXACTLY ONCE - "
				   "wayland answers an unoffered type with silence and the base controller answers "
				   "twice, which is what the seam exists to hide. A cancelled read must not be "
				   "answered at all, and neither must one whose field lost focus. What a "
				   "ui::TextInput copies a ui::TextView must paste, because copy/cut/paste live in "
				   "the base now; but a masked field must still refuse, and a text view must still "
				   "not mask. Drive it over the inspector: clipboard.write, clipboard.read, "
				   "clipboard.state."),
		TestRegistry_make<ClipboardLayout>, true},

	TestInfo{StringView("text-view"), StringView("XL_TEXT_VIEW_TEST"),
		StringView("ui::TextView undo history"),
		StringView("A multi-line view that answers Ctrl+Z and a plain field beside it that does "
				   "NOT - a field commits into somebody's document, and one that swallowed the "
				   "chord would undo the typing instead of the edit. A run of keystrokes must be "
				   "ONE entry until its idle window passes; a paste must be one of its own; undo "
				   "must put back the caret as well as the characters, and must not record itself. "
				   "Typing goes through the platform (native=true), because the processor owns "
				   "printable keys and a typed character reaches the widget only as an echo. Drive "
				   "it over the inspector: text-view.state, text-view.undo, text-view.redo, "
				   "text-view.history-break, send_input native=true."),
		TestRegistry_make<TextViewLayout>, true},

	TestInfo{StringView("scroll-thrash"), StringView("XL_SCROLL_THRASH_TEST"), StringView("Scroll virtualization runaway"),
		StringView("Rows that never match the size their item declared. The list must still scroll "
				   "and the run must end with 0 failures instead of stalling on a rebuild loop."),
		TestRegistry_make<ScrollThrashLayout>},
};

// src/text - text shaping
static const TestInfo s_textTests[] = {
	TestInfo{StringView("shaping"), StringView("XL_SHAPING_TEST"), StringView("Text shaping"),
		StringView("Rows of the same text with shaping and bidi off, then on: kerning, ligatures, "
				   "Arabic joining and RTL order must differ between them."),
		TestRegistry_make<ShapingLayout>},
};

// src/template - pug templates and the template-system cascade
static const TestInfo s_templateTests[] = {
	TestInfo{StringView("pug"), StringView("XL_PUG_TEST"), StringView("Pug template + CSS"),
		StringView("Node tree built from a pug template and styled by selectors. The buttons flip "
				   "a class, swap the whole stylesheet, and re-run the template."),
		TestRegistry_make<PugLayout>},

	TestInfo{StringView("pug-cascade"), StringView("XL_PUG_CASCADE_TEST"), StringView("Pug template cascade"),
		StringView("The inner template uses a function and a variable it never defines: both must "
				   "resolve through the outer template system."),
		TestRegistry_make<PugCascadeLayout>},
};

// src/render - what reaches the screen
static const TestInfo s_renderTests[] = {
	TestInfo{StringView("render-level"), StringView("XL_RENDER_LEVEL_TEST"), StringView("RenderingLevel passes"),
		StringView("Rows 1 and 3 must each show four identical boxes over the blue strip; row 2 "
				   "must show none - behind opaque geometry every level is hidden."),
		TestRegistry_make<RenderLevelLayout>},

	TestInfo{StringView("damage"), StringView("XL_DAMAGE_TEST"), StringView("Damage tracking"),
		StringView("A red square jumps in discrete steps beside a static grey one. Exactly one red "
				   "square must be visible at any moment - a second one is a trail."),
		TestRegistry_make<DamageLayout>, true},
};

// src/window - windows and the render graphs behind them
static const TestInfo s_windowTests[] = {
	TestInfo{StringView("geometry"), StringView("XL_GEOMETRY_TEST"), StringView("Window geometry protocol"),
		StringView("What the scene can learn about its own window: position and logical size read "
				   "through getWindowGeometry(), and Scene::handleWindowGeometryChanged firing when "
				   "the window changes. A second Root window is opened at a REQUESTED position and "
				   "read back, which is the save/restore round trip through WindowInfo::rect. Drive "
				   "it over the inspector: geometry.state, geometry.open-second, window geometry."),
		TestRegistry_make<GeometryLayout>, true},

	TestInfo{StringView("multi-window"), StringView("XL_MULTIWINDOW_TEST"), StringView("Two Root windows, one font atlas"),
		StringView("A second top-level window opens with the same string as the first; both must "
				   "render it identically, because the atlas they sample is the same object."),
		TestRegistry_make<MultiWindowLayout>},

	TestInfo{StringView("queue-cache"), StringView("XL_QUEUE_CACHE_TEST"),
		StringView("One compiled render graph, many windows"),
		StringView("A render queue is built and compiled before any of the windows that use it "
				   "exist; three secondary windows then open on that same compiled graph."),
		TestRegistry_make<QueueCacheLayout>},
};

// One entry per directory. Nesting is arbitrary - a group may declare `groups` of its own - but the
// source tree is one level deep, so this list is flat as well.
static const TestGroup s_groups[] = {
	TestGroup{StringView("css"), StringView("CSS engine"),
		StringView("Selectors, the cascade, custom properties and live reload of a stylesheet."), {},
		s_cssTests},

	TestGroup{StringView("layout"), StringView("Layout and measurement"),
		StringView("Flex and grid placement, content-driven sizing, margins, resize propagation."),
		{}, s_layoutTests},

	TestGroup{StringView("widgets"), StringView("Widgets"),
		StringView("ui:: widgets taking their look from CSS, and scroll virtualization."), {},
		s_widgetsTests},

	TestGroup{StringView("text"), StringView("Text"),
		StringView("Shaping, kerning, ligatures and bidirectional order."), {}, s_textTests},

	TestGroup{StringView("template"), StringView("Templates"),
		StringView("Scene graphs built from pug templates, and the template-system cascade."), {},
		s_templateTests},

	TestGroup{StringView("render"), StringView("Rendering"),
		StringView("What actually reaches the screen: damage tracking and rendering levels."), {},
		s_renderTests},

	TestGroup{StringView("window"), StringView("Windows"),
		StringView("A second Root window, and render queues compiled before any window exists."),
		{}, s_windowTests},
};

// Belongs to no group: it is the app itself rather than a test of anything. Selected when no
// variable is set, which is why it is the only entry without one.
static const TestInfo s_rootTests[] = {
	TestInfo{StringView("default"), StringView(), StringView("General demo"),
		StringView("Application menu: the other demos, plus window, monitor and fullscreen "
				   "controls."),
		TestRegistry_make<GeneralLayout>},
};

static const TestGroup s_root{StringView(), StringView("Tests"),
	StringView("Everything this app can show, grouped the way its sources are."), s_groups,
	s_rootTests};

const TestGroup &getTestRegistry() { return s_root; }

// clang-format on

size_t getTestCount(const TestGroup &group) {
	auto ret = group.tests.size();
	for (auto &it : group.groups) { ret += getTestCount(it); }
	return ret;
}

// Depth-first, groups in declaration order: the walk both the environment scan and the path lookup
// are built on.
static const TestInfo *TestRegistry_findByName(const TestGroup &group, StringView name) {
	for (auto &it : group.tests) {
		if (it.name == name) {
			return &it;
		}
	}
	for (auto &it : group.groups) {
		if (auto ret = TestRegistry_findByName(it, name)) {
			return ret;
		}
	}
	return nullptr;
}

static const TestGroup *TestRegistry_findChild(const TestGroup &group, StringView name) {
	for (auto &it : group.groups) {
		if (it.name == name) {
			return &it;
		}
	}
	return nullptr;
}

const TestInfo *findTest(StringView path) {
	StringView r(path);
	auto group = &s_root;

	// Every segment but the last names a group; the last one names the test in it.
	while (true) {
		auto segment = r.readUntil<StringView::Chars<'/'>>();
		if (r.is('/')) {
			++r;
			group = TestRegistry_findChild(*group, segment);
			if (!group) {
				return nullptr;
			}
			continue;
		}

		for (auto &it : group->tests) {
			if (it.name == segment) {
				return &it;
			}
		}

		// A bare id names the same test wherever it sits, so an unqualified name is searched
		// through the whole tree - that is the form the inspector tooling has always used.
		return group == &s_root ? TestRegistry_findByName(s_root, segment) : nullptr;
	}
}

const TestGroup *findTestGroup(StringView path) {
	StringView r(path);
	auto group = &s_root;
	while (!r.empty()) {
		auto segment = r.readUntil<StringView::Chars<'/'>>();
		if (r.is('/')) {
			++r;
		}
		if (segment.empty()) {
			continue;
		}
		group = TestRegistry_findChild(*group, segment);
		if (!group) {
			return nullptr;
		}
	}
	return group;
}

// Builds the path on the way out of the recursion, so a group only appears in it when the test was
// actually found below it.
static bool TestRegistry_buildPath(const TestGroup &group, const TestInfo &info, String &path) {
	for (auto &it : group.tests) {
		if (&it == &info) {
			return true;
		}
	}
	for (auto &it : group.groups) {
		if (TestRegistry_buildPath(it, info, path)) {
			path = path.empty() ? toString(it.name) : toString(it.name, "/", path);
			return true;
		}
	}
	return false;
}

String getTestGroupPath(const TestInfo &info) {
	String path;
	TestRegistry_buildPath(s_root, info, path);
	return path;
}

static const TestInfo *TestRegistry_findByEnv(const TestGroup &group) {
	for (auto &it : group.tests) {
		if (!it.env.empty() && ::getenv(it.env.data()) != nullptr) {
			return &it;
		}
	}
	for (auto &it : group.groups) {
		if (auto ret = TestRegistry_findByEnv(it)) {
			return ret;
		}
	}
	return nullptr;
}

const TestInfo &getSelectedTest() {
	if (auto ret = TestRegistry_findByEnv(s_root)) {
		return *ret;
	}
	// Nothing selected: the front page, which is the last entry of the root group.
	return s_root.tests.back();
}

Rc<basic2d::SceneLayout2d> makeTestLayout(const TestInfo &info) {
	auto layout = info.make();

	// Every entry is handed its own registry record - that is what names the commands it exposes over
	// the inspector socket. The general demo is the one entry deliberately left without a caption:
	// it is the app's front page rather than a test, and TestLayout keys the caption off the title.
	if (auto test = dynamic_cast<TestLayout *>(layout.get())) {
		test->setTestInfo(info);
	}

	return layout;
}

Rc<basic2d::SceneLayout2d> makeSelectedTestLayout() { return makeTestLayout(getSelectedTest()); }

} // namespace stappler::xenolith::app
