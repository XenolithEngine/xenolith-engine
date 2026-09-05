/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "form/FormDemoLayout.h"
#include "XLUiStyleSystem.h" // the rule-supplying half of the stylesheet pair
#include "XLUiStyleResolver.h" // the half that actually applies them
#include "XLUiLayoutSystem.h"
#include "XLScene.h" // Scene::getContent, which the inspector commands are registered on
#include "XL2dSceneContent.h" // and the definition of what it answers, so it is a Node here
#include "XLSceneInspector.h"
#include "XLAction.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* The stylesheet.

Everything the demo looks like is here rather than in C++, which is the rule the kit is written to:
the layout builds structure and data, the sheet decides sizes and colours. Two things are worth
pointing out to anyone copying it:

  * the two columns are flex items of one row, and the fixed one is `flex: 0 0 <w>` and NOT
    `min-width`. On a flex item's CROSS axis min-/max- are parsed and silently ignored by this
    engine, so a min-width there would do nothing at all and the column would collapse;
  * a scroll container needs a DEFINITE size on its scroll axis. `#left-column` gets its height
    from being a stretched item of `#columns`, which is what lets `overflow-y: auto` have anything
    to scroll. `height: fit-content` with an overflow beside it never scrolls, because the box
    grows to whatever it holds. */
static constexpr auto s_css = StringView(R"css(
:root {
	--surface:   #1a1a1f;
	--panel:     #232329;
	--control:   #2e2e36;
	--outline:   #4a4a55;
	--accent:    #3d7ecf;
	--text:      #e8e8e8;
	--muted:     #9a9aa5;
	--danger:    #e53935;
	--caption-w: 150px;
	--field-h:   30px;
}

/* ---- the frame ------------------------------------------------------- */

#demo-root  { display: flex; flex-direction: column; }
/* `height` is not declared: this node is sized by hand in handleContentSizeDirty, because nothing
   above it lays it out. */
#demo-bar   { order: 0; -xl-z-order: 2; display: flex; flex-direction: row; align-items: center;
              column-gap: 10px; padding: 8px 12px; background-color: #141418; }
#columns    { order: 1; -xl-z-order: 1; flex-grow: 1; display: flex; flex-direction: row;
              align-items: stretch; column-gap: 12px; padding: 12px; }

/* The left column is the SCROLLER; the right one is a fixed band beside it. `flex: 0 0 <w>` and
   never min-width - see the note above. */
#left-column  { flex-grow: 1; flex-shrink: 1; flex-basis: 0px;
                display: flex; flex-direction: column; row-gap: 6px;
                padding: 10px; background-color: var(--panel); border-radius: 8px;
                overflow-y: auto; }
#right-column { flex: 0 0 460px; display: flex; flex-direction: column; }

/* ---- one labelled row ------------------------------------------------ */

.field-row     { display: flex; flex-direction: row; align-items: center; column-gap: 10px;
                 padding: 3px 0px; }
.field-caption { flex: 0 0 var(--caption-w); color: var(--muted); font-size: 13px; }
.actions-row   { column-gap: 10px; padding: 8px 0px 2px 0px; }

/* ---- the fields ------------------------------------------------------ */

text-input, number-field {
	flex-grow: 1; height: var(--field-h);
	background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
	border-radius: 5px; padding: 0px 8px; color: var(--text); font-size: 13px;
	--caret-color: #fcb400; --selection-color: rgba(252,180,0,.35);
}
text-input:hover, number-field:hover { outline-color: #6a6a78; }
text-input:focus, number-field:focus { outline-color: var(--accent); }
text-input:invalid, number-field:invalid { outline-color: var(--danger); }
label.xl-ui-text-input-placeholder { color: #6a6a72; }

/* A composite field is one control made of several: the row inside it is the widget's, and only
   its parts are addressed here. */
vector-field { flex-grow: 1; display: flex; flex-direction: row; column-gap: 6px; }
vector-field > number-field { flex-grow: 1; flex-basis: 0px; }

checkbox { flex: 0 0 18px; height: 18px; border-radius: 4px;
           background-color: var(--control); outline-color: var(--outline); outline-width: 1px; }
checkbox:checked { background-color: var(--accent); outline-color: var(--accent); }
checkbox:focus   { outline-color: #fcb400; }
checkbox > icon  { width: 14px; height: 14px; color: #ffffff; }

select, search-picker {
	flex-grow: 1; height: var(--field-h); display: flex; flex-direction: row; align-items: center;
	column-gap: 6px; padding: 0px 8px;
	background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
	border-radius: 5px;
}
select:hover, search-picker:hover { outline-color: #6a6a78; }
select:focus, search-picker:focus { outline-color: var(--accent); }
select > label, search-picker > label { flex-grow: 1; color: var(--text); font-size: 13px; }
select > icon, search-picker > icon   { width: 18px; height: 18px; color: var(--muted); }

slider       { flex-grow: 1; height: 18px; background-color: var(--control); border-radius: 3px; }
slider-fill  { background-color: var(--accent); border-radius: 3px; }
slider-thumb { width: 14px; height: 14px; border-radius: 7px; background-color: #f2f2f2; }
slider:focus > slider-thumb { background-color: #fcb400; }

chip-row { flex-grow: 1; display: flex; flex-direction: row; flex-wrap: wrap; align-items: center;
           column-gap: 6px; row-gap: 6px; padding: 3px 6px;
           background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
           border-radius: 5px; }
chip-row:focus { outline-color: var(--accent); }
chip     { height: 22px; display: flex; flex-direction: row; align-items: center; column-gap: 4px;
           padding: 0px 8px; border-radius: 11px; background-color: #3b3b45; }
chip:selected { background-color: var(--accent); }
chip > label  { color: var(--text); font-size: 12px; }
chip > button { width: 14px; height: 14px; }

color-field { flex-grow: 1; height: var(--field-h); display: flex; flex-direction: row;
              align-items: center; column-gap: 8px; padding: 0px 8px;
              background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
              border-radius: 5px; }
color-field:focus { outline-color: var(--accent); }
color-field:invalid { outline-color: var(--danger); }
color-field > swatch     { flex: 0 0 26px; height: 18px; }
color-field > text-input { flex-grow: 1; outline-width: 0px; background-color: transparent;
                           padding: 0px; }
color-field > icon       { flex: 0 0 18px; height: 18px; color: var(--muted); }

/* The two text WIDGETS that are not fields. They need a definite height: neither measures itself
   the way a single-line input does, and a flex item with nothing to measure gets nothing. */
.demo-editor { flex-grow: 1; height: 86px; background-color: #17171b;
               outline-color: var(--outline); outline-width: 1px; border-radius: 5px;
               padding: 4px 6px; color: var(--text); font-size: 12px;
               --caret-color: #fcb400; }

/* ---- buttons --------------------------------------------------------- */

button { height: 30px; display: flex; flex-direction: row; justify-content: center;
         align-items: center; padding: 0px 14px; border-radius: 5px;
         background-color: var(--control); outline-color: var(--outline); outline-width: 1px; }
button:hover { background-color: #3a3a44; }
button:focus { outline-color: #fcb400; }
button > label { color: var(--text); font-size: 13px; }
.actions-row > button { flex: 0 0 110px; }

/* ---- the accordion --------------------------------------------------- */

/* THE ACCORDION'S CONTAINERS PAINT NOTHING, and that is a workaround rather than a taste.

Give `accordion-view` an opaque `background-color` and every widget inside it loses its FILL - the
text inputs, the checkbox, the select, the chips all go flat - while their labels and icons keep
drawing. Measured, not guessed: with `var(--panel)` the pixel under a field reads the panel's
colour, with `transparent` it reads the field's. `#left-column` carries the same fill and is fine,
because it is a plain Node and draws nothing at all.

It is NOT simply "an opaque ui::Panel hides its children": `search-picker-content` below is an
opaque Panel and its rows draw correctly, so something about the accordion in particular - most
likely its scrolling viewport - is involved. Reported rather than worked around in the engine: the
fix belongs in the renderer, and guessing at it from here would be worse than living without one
container's fill. The paint therefore lives on the leaves: the headers, the rows and the widgets. */
accordion-view    { flex-grow: 1; background-color: transparent; }
accordion-section { background-color: transparent; }
accordion-header  { display: flex; flex-direction: row; align-items: center; column-gap: 6px;
                    padding: 5px 8px; background-color: #2b2b33; }
accordion-header:hover    { background-color: #34343e; }
accordion-header.expanded { background-color: #3a3a46; }
accordion-header > label  { flex-grow: 1; color: var(--text); font-size: 13px; }
.accordion-chevron { width: 16px; height: 16px; color: var(--muted); }
.accordion-grip    { width: 16px; height: 16px; color: var(--muted); }
/* An icon button is a ui::Panel too, and a Panel with nothing declared is opaque WHITE - so
   without this the close affordance is a white block, not a glyph. */
accordion-close    { width: 16px; height: 16px; background-color: transparent; }
accordion-close > icon { width: 14px; height: 14px; color: var(--muted); }
accordion-body     { display: flex; flex-direction: column; row-gap: 6px; padding: 8px 10px;
                     background-color: #1e1e24; }
accordion-drop-indicator { background-color: var(--accent); }

/* A section's panel is the flex column buildFieldGroup() wants; it is `fit-content` on the main
   axis so an open section is exactly as tall as what it holds. */
.section-body { display: flex; flex-direction: column; row-gap: 4px; flex-basis: fit-content; }

/* ---- the two in-scene popups ----------------------------------------- */

/* Both surfaces are pushed into THIS scene as overlays, so the sheet above reaches them and they
   are styled here like anything else. On the native path they would be scenes of their own and
   would need PopupSurfaceConfig::stylesheetSource instead - which is the whole reason ui::ColorField
   paints its picker in code rather than relying on a sheet arriving. */
search-picker-content { background-color: var(--panel); outline-color: var(--outline);
                        outline-width: 1px; border-radius: 6px; padding: 6px; }
search-picker-content > text-input { height: 28px; }

tree-view  { background-color: transparent; }
tree-row   { display: flex; flex-direction: row; align-items: center; padding: 0px 8px;
             background-color: transparent; border-radius: 4px; }
tree-row:hover    { background-color: #34343e; }
tree-row:selected { background-color: var(--accent); }
tree-row > label  { color: var(--text); font-size: 13px; }

/* ---- the chrome ------------------------------------------------------ */

#demo-bar > button { flex: 0 0 fit-content; }
.status        { flex-grow: 1; color: #9ecbff; font-size: 13px; }
.column-title  { color: var(--muted); font-size: 12px; text-transform: uppercase;
                 padding: 0px 0px 4px 0px; }
scroll-indicator       { background-color: #55555f; border-radius: 2px; }
scroll-indicator-track { background-color: transparent; }
)css");

// A section's panel: the flex column buildFieldGroup fills. Built at most once per section, and
// kept across a collapse - the accordion detaches it rather than destroying it.
static Rc<Node> makeSectionBody(FieldGroup group) {
	auto body = Rc<Node>::create();
	body->setName(mem_std::toString("section-", getFieldGroupName(group)));
	body->addStyleClass("section-body");
	buildFieldGroup(body, group);
	return body;
}

} // namespace

StringView getFormDemoStylesheet() { return s_css; }

// ---- construction --------------------------------------------------------------------------

bool FormDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	// The stylesheet and its resolver are NOT installed here - they are on the SceneContent, so
	// that they reach the in-scene popups too. See getFormDemoStylesheet().

	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.10f, 0.10f, 0.12f, 1.0f)), ZOrder(0));
	_background->setName("demo-background");

	/* THE FLEX COLUMN IS A CHILD, not this node, and that is not a spare wrapper.

	A recursive ui::StyleResolver re-resolves its DESCENDANTS and never its own owner - so
	`#demo-root { display: flex }` written for the node carrying the resolver is a rule that never
	runs, the node never becomes a flex container, and the resolver then writes no FlexItemInfo for
	its children either. The visible result is a `flex-grow: 1` that does nothing and a column
	collapsed to the height of its contents. One node down, everything is an ordinary styled
	subtree, and this layout's only job is to give it a size. */
	_root = addChild(Rc<Node>::create(), ZOrder(1));
	_root->setName("demo-root");

	buildControlBar();

	auto columns = _root->addChild(Rc<Node>::create(), ZOrder(1));
	columns->setName("columns");

	_leftColumn = columns->addChild(Rc<Node>::create(), ZOrder(1));
	_leftColumn->setName("left-column");

	_rightColumn = columns->addChild(Rc<Node>::create(), ZOrder(2));
	_rightColumn->setName("right-column");

	buildLeftColumn();
	buildRightColumn();

	// The self-check runs from handleEnter, not from here: see the comment there.
	refreshStatus("ready");

	/* The engine renders on demand: with nothing dirty it stops producing frames, and a callback
	that changes state off-screen is only picked up the next time something wakes the loop. This
	demo is meant to be watched and driven, so hold the loop open - RenderContinuously draws nothing
	and damages nothing, it only keeps frames coming. */
	runAction(Rc<RenderContinuously>::create());

	return true;
}

void FormDemoLayout::buildLeftColumn() {
	auto title = _leftColumn->addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	title->addStyleClass("column-title");
	title->setString("Plain widgets — one form");

	/* The form goes on the COLUMN, so every field built into it joins by walking up one parent.
	That the column is also the scroll container is a coincidence of layout, not a relationship:
	ui::FormSystem knows nothing about ui::ScrollSystem and neither has to be told about the other.
	*/
	_leftForm = _leftColumn->addSystem(Rc<ui::FormSystem>::create());
	wireForm(_leftForm, "left");

	// The z-orders continue across groups: the tab ring is document order, which here is z-order,
	// so the left column walks name -> email -> ... -> reset in one ring.
	int32_t z = 1;
	for (auto group : getFieldGroups()) {
		buildFieldGroup(_leftColumn, group, z);
		// Each group's rows took a z-order apiece; leave a gap so a group can grow without
		// colliding with the next one's.
		z += 16;
	}
}

void FormDemoLayout::buildRightColumn() {
	auto title = _rightColumn->addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	title->addStyleClass("column-title");
	title->setString("Accordion — a second, independent form");

	/* The form goes on the WRAPPER and not on the accordion.

	Either would work for the fields - both are above them - but the wrapper is the node that stays
	put. It is also what keeps this readable: the accordion is a container the form happens to
	contain, not a thing the form is made of. */
	_rightForm = _rightColumn->addSystem(Rc<ui::FormSystem>::create());
	wireForm(_rightForm, "right");

	_registry = Rc<ui::PanelRegistry>::create();

	_accordion = _rightColumn->addChild(
			Rc<ui::AccordionView>::create(Rc<ui::PanelRegistry>(_registry)), ZOrder(1));
	_accordion->setName("accordion");
	_accordion->setExpansion(ui::AccordionExpansion::Multi);

	/* Fit, not Fill: each open section is as tall as the fields it holds and the view scrolls when
	the total runs past it. Fill would divide the height evenly between the open sections, which is
	right for a pane of working panels and wrong for a list of forms - the Actions section is two
	buttons and would get as much room as the four text fields. */
	_accordion->setSizing(ui::AccordionSizing::Fit);

	Vector<String> sections;
	for (auto group : getFieldGroups()) {
		auto name = getFieldGroupName(group);

		ui::DockPanelDescriptor desc;
		desc.id = name.str<Interface>();
		desc.title = getFieldGroupTitle(group).str<Interface>();
		desc.icon = basic2d::IconName::Action_list_solid;
		/* The height an OPEN section gets, and the width the view will not go below. Not a
		guess and not a floor to be grown out of - in AccordionSizing::Fit the declared minimum IS
		the section's height, because the body contributes nothing of its own. */
		desc.minSize = Size2(360.0f, getFieldGroupHeight(group));

		/* The builder takes no arguments and runs at most once, the first time the section is
		OPEN. Its node is then kept across every collapse and every move, which is what makes the
		fields inside survive a section being shut - half-typed text and all.

		"The first time it is open" is not "the first time somebody opens it": in Multi mode a
		newly declared section starts EXPANDED, so setSections below opens all six at once and
		every builder runs there. The demo shuts four of them immediately afterwards, which is a
		display choice and is written out as one. */
		desc.builder = [this, group, key = desc.id]() -> Rc<Node> {
			auto it = _builds.find(key);
			_builds.insert_or_assign(key, (it != _builds.end()) ? it->second + 1 : 1);
			return makeSectionBody(group);
		};

		_registry->registerPanel(sp::move(desc));
		sections.emplace_back(name.str<Interface>());
	}

	_accordion->setSections(sp::move(sections));

	/* Shut everything, then open two.

	Declaring the sections left them all open - see the builder above - and four open forms is a
	wall nobody reads. Two open and four shut is also what makes the demo's point visible at a
	glance: `form.state` shows the right form holding exactly the fields of the OPEN sections, and
	nothing at all from the others. Spelled out rather than left to the default, because the default
	is the opposite of what is wanted here. */
	for (auto group : getFieldGroups()) { _accordion->collapsePanel(getFieldGroupName(group)); }
	_accordion->expandPanel("text");
	_accordion->expandPanel("choice");

	_accordion->setPanelExpandedCallback([this](StringView id) {
		refreshStatus(mem_std::toString("section '", id, "' toggled"));
	});
}

void FormDemoLayout::wireForm(NotNull<ui::FormSystem> form, StringView which) {
	auto name = which.str<Interface>();

	form->setSubmitCallback([this, name](Value &&value) {
		++_submitCount;
		_lastSubmit = sp::move(value);
		_lastInvalid.clear();
		refreshStatus(mem_std::toString("submit(", name, ") ok"));
	});

	form->setResetCallback([this, name] {
		++_resetCount;
		_lastSubmit = Value();
		_lastInvalid.clear();
		refreshStatus(mem_std::toString("reset(", name, ")"));
	});

	form->setInvalidCallback([this, name](SpanView<ui::FormValidationError> errors) {
		++_invalidCount;
		_lastInvalid.clear();
		for (auto &it : errors) { _lastInvalid.emplace_back(it.name); }
		refreshStatus(mem_std::toString("submit(", name, ") refused ",
				_lastInvalid.empty() ? String("a field") : _lastInvalid.front()));
	});
}

void FormDemoLayout::buildControlBar() {
	_controlBar = _root->addChild(Rc<Node>::create(), ZOrder(2));
	_controlBar->setName("demo-bar");

	_statusLabel = _controlBar->addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	_statusLabel->addStyleClass("status");

	makeControl("Submit left", [this] { _leftForm->submit(); });
	makeControl("Submit right", [this] { _rightForm->submit(); });
	makeControl("Reset both", [this] {
		_leftForm->reset();
		_rightForm->reset();
	});
	makeControl("Expand all", [this] {
		for (auto group : getFieldGroups()) { _accordion->expandPanel(getFieldGroupName(group)); }
		refreshStatus("every section open");
	});
	makeControl("Collapse all", [this] {
		for (auto group : getFieldGroups()) { _accordion->collapsePanel(getFieldGroupName(group)); }
		refreshStatus("every section shut");
	});
	makeControl("Self-check", [this] { runSelfCheck(); });
}

ui::Button *FormDemoLayout::makeControl(StringView label, Function<void()> &&action) {
	// The z-order counts up from the status label at 0 so the buttons keep the order they were
	// declared in: inside a flex row the child order IS the placement order.
	auto button = _controlBar->addChild(Rc<ui::Button>::create(sp::move(action)),
			ZOrder(int32_t(_controlBar->getChildren().size())));
	button->setString(label);
	return button;
}

void FormDemoLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	/* The only two nodes placed by hand, and both for the same reason: SceneLayout2d carries no
	LayoutSystem, so nothing above them lays them out. Everything below `#demo-root` is a flex item
	of something, and hand-placing one of those would fight the layout pass. */
	for (auto node : {_background, _root}) {
		if (node) {
			node->setAnchorPoint(Anchor::BottomLeft);
			node->setPosition(Vec2::ZERO);
			node->setContentSize(getContentSize());
		}
	}
}

// ---- state ---------------------------------------------------------------------------------

ui::FormSystem *FormDemoLayout::getForm(StringView which) const {
	if (which == "left") {
		return _leftForm;
	} else if (which == "right") {
		return _rightForm;
	}
	return nullptr;
}

Node *FormDemoLayout::getFieldNode(StringView form, StringView field) const {
	auto system = getForm(form);
	if (!system) {
		return nullptr;
	}
	// THROUGH the form, not off a member: a field in an accordion section did not exist when this
	// layout was built, and will not exist again once its section is shut.
	if (auto listener = system->getField(field)) {
		return listener->getOwner();
	}
	return nullptr;
}

ui::ColorField *FormDemoLayout::getOpenColorField() const {
	for (auto form : {_leftForm, _rightForm}) {
		if (!form) {
			continue;
		}
		for (auto &it : form->getFields()) {
			if (auto field = dynamic_cast<ui::ColorField *>(it->getOwner())) {
				if (field->isOpen()) {
					return field;
				}
			}
		}
	}
	return nullptr;
}

void FormDemoLayout::refreshStatus(StringView lastAction) {
	if (!lastAction.empty()) {
		_lastAction = lastAction.str<Interface>();
	}
	if (!_statusLabel) {
		return;
	}

	StringStream out;
	out << (_lastAction.empty() ? StringView("ready") : StringView(_lastAction));

	if (_selfCheckDone) {
		out << "  |  self-check: " << _checks << " checks, " << _failures << " failures";
	}

	if (!_lastInvalid.empty()) {
		out << "  |  refused:";
		for (auto &it : _lastInvalid) { out << " " << it; }
	} else if (_lastSubmit) {
		/* The collected value as JSON, TRIMMED to what one line holds.

		A status strip is one line high and the label wraps rather than clips, so a full collect()
		of thirteen fields pushes a second line up over the row above it. The whole value is on the
		`form.collect` command for anything that needs to read it; what belongs here is its shape. */
		auto text = data::toString(_lastSubmit, false);
		constexpr size_t kMaxStatus = 64;
		if (text.size() > kMaxStatus) {
			text.resize(kMaxStatus);
			text.append("...");
		}
		out << "  |  " << text;
	}

	_statusLabel->setString(out.str());
}

// ---- the inspector -------------------------------------------------------------------------

Value FormDemoLayout::encodeForm(StringView which) const {
	Value ret;
	auto form = getForm(which);
	if (!form) {
		ret.setString("no such form", "error");
		return ret;
	}

	ret.setString(which, "form");
	ret.setValue(form->collect(), "collected");

	Value fields(Value::Type::ARRAY);
	for (auto &it : form->getFields()) { fields.addString(it->getFieldName()); }
	ret.setValue(sp::move(fields), "fields");

	// The RING is not the field list: it is what Tab walks, in document order, minus everything
	// hidden, disabled or locked as of the last committed frame.
	Value ring(Value::Type::ARRAY);
	for (auto &it : form->getTabRing()) { ring.addString(it->getFieldName()); }
	ret.setValue(sp::move(ring), "tabRing");

	if (auto focused = form->getFocusedField()) {
		ret.setString(focused->getFieldName(), "focused");
	}
	// What has been ASKED for and not yet committed - the difference between what the form was told
	// to do and what has happened.
	if (auto pending = form->getPendingField()) {
		ret.setString(pending->getFieldName(), "pending");
	}
	if (auto def = form->getDefaultButton()) {
		ret.setString(def->getFieldName(), "defaultButton");
	}
	return ret;
}

void FormDemoLayout::addCommand(StringView name, StringView description,
		Function<Value(const Value &)> &&handler) {
	if (!_inspectorScene || !handler) {
		return;
	}

	auto full = mem_std::toString("form.", name);
	if (!inspector::addCommand(_inspectorScene->getContent(), full, description,
				[this, handler = sp::move(handler)](Value &&args,
						Function<void(Value &&)> &&done) mutable {
		/* CONST, and that is what keeps a missing argument from being a crash rather than a default.

		A command is called with whatever a script sent, so every read here is of a key that may not
		be there - and the NON-const data::Value getters assert in debug when the key is missing or
		holds another type, because what they would otherwise hand back is the shared read-only
		Value::Null. Read through a const reference and the same reads answer an empty value. */
		const Value &in = args;
		auto result = handler(in);

		/* Answer only once the change has been on screen for a moment. Every action here can move
		geometry - opening a popup, expanding a section - and this layout holds a
		RenderContinuously, so that is real rendering time: a screenshot taken when the reply lands
		shows the settled scene rather than a half-finished relayout. */
		runAction(Rc<Sequence>::create(0.15f,
				Function<void()>([done = sp::move(done), result = sp::move(result)]() mutable {
			done(sp::move(result));
		})));
	})) {
		return; // no inspector on this scene
	}

	_inspectorCommands.emplace_back(sp::move(full));
}

void FormDemoLayout::registerCommands() {
	addCommand("state", "Both forms: fields, tab ring, focus, default button",
			[this](const Value &) {
		Value ret;
		ret.setValue(encodeForm("left"), "left");
		ret.setValue(encodeForm("right"), "right");
		ret.setString(_lastAction, "lastAction");
		ret.setInteger(int64_t(_submitCount), "submits");
		ret.setInteger(int64_t(_resetCount), "resets");
		ret.setInteger(int64_t(_invalidCount), "invalids");

		Value builds;
		for (auto &it : _builds) { builds.setInteger(int64_t(it.second), it.first); }
		ret.setValue(sp::move(builds), "sectionBuilds");

		Value sections(Value::Type::ARRAY);
		for (auto &it : _accordion->getSections()) {
			Value entry;
			entry.setString(it, "id");
			entry.setBool(_accordion->isPanelExpanded(it), "expanded");
			sections.addValue(sp::move(entry));
		}
		ret.setValue(sp::move(sections), "sections");
		return ret;
	});

	addCommand("collect", "What one form collects: {form}",
			[this](const Value &args) { return encodeForm(args.getString("form")); });

	addCommand("submit", "Submit one form: {form}", [this](const Value &args) {
		Value ret;
		auto form = getForm(args.getString("form"));
		if (!form) {
			ret.setString("no such form", "error");
			return ret;
		}
		ret.setBool(form->submit(), "ok");
		ret.setValue(_lastSubmit, "collected");
		Value refused(Value::Type::ARRAY);
		for (auto &it : _lastInvalid) { refused.addString(it); }
		ret.setValue(sp::move(refused), "refused");
		return ret;
	});

	addCommand("reset", "Reset one form: {form}", [this](const Value &args) {
		Value ret;
		auto form = getForm(args.getString("form"));
		if (!form) {
			ret.setString("no such form", "error");
			return ret;
		}
		form->reset();
		ret.setBool(true, "ok");
		return ret;
	});

	addCommand("set", "Assign one field: {form, field, value}", [this](const Value &args) {
		Value ret;
		auto form = getForm(args.getString("form"));
		if (!form) {
			ret.setString("no such form", "error");
			return ret;
		}

		auto name = args.getString("field");
		auto field = form->getField(name);
		if (!field) {
			ret.setString("no such field", "error");
			return ret;
		}

		field->assign(args.getValue("value"));
		ret.setValue(field->collect(), "value");
		refreshStatus(mem_std::toString("set ", name));
		return ret;
	});

	addCommand("focus", "Focus one field: {form, field}", [this](const Value &args) {
		Value ret;
		auto form = getForm(args.getString("form"));
		if (!form) {
			ret.setString("no such form", "error");
			return ret;
		}
		auto field = form->getField(args.getString("field"));
		if (!field) {
			ret.setString("no such field", "error");
			return ret;
		}

		ret.setBool(form->focusField(field), "ok");
		/* A focus change is DEFERRED to the next commit, so the answer here is what was asked for,
		not what has happened. Reading it back in the same hop is the mistake this reports its way
		out of. */
		if (auto pending = form->getPendingField()) {
			ret.setString(pending->getFieldName(), "pending");
		}
		return ret;
	});

	addCommand("open", "Open a field's popup: {form, field}", [this](const Value &args) {
		Value ret;
		auto node = getFieldNode(args.getString("form"), args.getString("field"));
		if (!node) {
			ret.setString("no such field", "error");
			return ret;
		}

		if (auto picker = dynamic_cast<ui::SearchPicker *>(node)) {
			ret.setBool(picker->open(), "ok");
		} else if (auto color = dynamic_cast<ui::ColorField *>(node)) {
			ret.setBool(color->open(), "ok");
		} else if (auto select = dynamic_cast<ui::Select *>(node)) {
			ret.setBool(select->open(), "ok");
		} else {
			ret.setString("that field has no popup", "error");
		}
		return ret;
	});

	addCommand("close", "Dismiss a field's popup: {form, field}", [this](const Value &args) {
		Value ret;
		auto node = getFieldNode(args.getString("form"), args.getString("field"));
		if (!node) {
			ret.setString("no such field", "error");
			return ret;
		}

		if (auto picker = dynamic_cast<ui::SearchPicker *>(node)) {
			picker->close();
		} else if (auto color = dynamic_cast<ui::ColorField *>(node)) {
			color->close();
		} else if (auto select = dynamic_cast<ui::Select *>(node)) {
			select->close();
		}
		ret.setBool(true, "ok");
		return ret;
	});

	addCommand("picker", "The open colour picker: its tab, its channels, its hex",
			[this](const Value &args) {
		Value ret;
		auto field = getOpenColorField();
		if (!field) {
			ret.setString("no picker is open", "error");
			return ret;
		}

		auto content = dynamic_cast<ui::ColorPickerContent *>(
				field->getPicker() ? field->getPicker()->getPanel() : nullptr);
		if (!content) {
			// A system dialog is up: it is the platform's window, and it has no nodes to report.
			ret.setString("the open picker is the system dialog", "error");
			return ret;
		}

		// Setting the tab is part of the same command: the state a test wants to assert about is
		// usually the one it just asked for.
		ui::ColorPickerMode mode;
		if (ui::readColorPickerMode(args.getString("mode"), mode)) {
			content->setMode(mode);
		}
		if (args.hasValue("value")) {
			content->setValueFromString(args.getString("value"));
		}
		if (args.hasValue("channel") && args.hasValue("channelValue")) {
			content->setChannel(uint32_t(args.getInteger("channel")),
					float(args.getDouble("channelValue")));
		}
		if (args.hasValue("alpha")) {
			content->setAlpha(float(args.getDouble("alpha")));
		}
		if (args.getBool("copy")) {
			ret.setBool(content->copyToClipboard(), "copied");
		}
		if (args.getBool("paste")) {
			ret.setBool(content->pasteFromClipboard(), "pasteStarted");
		}

		ret.setString(ui::getColorPickerModeName(content->getMode()), "mode");
		ret.setString(content->formatValue(), "hex");
		ret.setBool(content->isAlphaEnabled(), "alphaEnabled");
		ret.setBool(content->isValid(), "valid");
		ret.setInteger(int64_t(content->getAlpha()), "alpha");

		Value channels(Value::Type::ARRAY);
		for (auto it : content->getChannels()) { channels.addInteger(int64_t(std::lround(it))); }
		ret.setValue(sp::move(channels), "channels");

		// What the FIELD holds, which is what the live `onChange` seam is worth checking through.
		ret.setString(field->formatValue(), "fieldValue");
		return ret;
	});

	addCommand("section", "Expand or collapse one accordion section: {id, expand}",
			[this](const Value &args) {
		Value ret;
		auto id = args.getString("id");
		const bool expand = args.getBool("expand");

		ret.setBool(expand ? _accordion->expandPanel(id) : _accordion->collapsePanel(id), "ok");
		ret.setBool(_accordion->isPanelExpanded(id), "expanded");

		// The point of the command: what the RIGHT form can see afterwards.
		Value fields(Value::Type::ARRAY);
		for (auto &it : _rightForm->getFields()) { fields.addString(it->getFieldName()); }
		ret.setValue(sp::move(fields), "rightFields");
		return ret;
	});

	addCommand("selfcheck", "Re-run the structural checks", [this](const Value &) {
		runSelfCheck();
		Value ret;
		ret.setInteger(int64_t(_checks), "checks");
		ret.setInteger(int64_t(_failures), "failures");
		ret.setBool(_failures == 0, "ok");
		return ret;
	});
}

void FormDemoLayout::handleEnter(Scene *scene) {
	/* The base FIRST, and that is what makes the self-check below possible at all.

	A field does not join a form when it is built - it joins when it ENTERS THE SCENE, from
	FormInputListener::handleEnter. Node::handleEnter runs a node's own systems and then descends
	into its children, so on the way back out of this call every widget in both columns has
	registered and the forms can be asked about themselves. Run from init() the same check finds two
	empty forms and fails every claim it makes. */
	basic2d::SceneLayout2d::handleEnter(scene);

	_inspectorScene = scene;
	registerCommands();

	runSelfCheck();
}

void FormDemoLayout::handleExit() {
	// A lambda that captured a destroyed layout is a dangling call from the inspector socket, so
	// the commands go down with the layout rather than with the scene.
	if (!_inspectorCommands.empty()) {
		if (_inspectorScene) {
			if (auto i = inspector::get(_inspectorScene->getContent())) {
				for (auto &it : _inspectorCommands) { i->removeCommand(it); }
			}
		}
		_inspectorCommands.clear();
	}
	_inspectorScene = nullptr;

	basic2d::SceneLayout2d::handleExit();
}

// ---- the self-check ------------------------------------------------------------------------

void FormDemoLayout::expect(bool condition, StringView message) {
	++_checks;
	if (!condition) {
		++_failures;
		log::source().warn("FormExample", "FAILED: ", message);
	}
}

void FormDemoLayout::runSelfCheck() {
	_checks = 0;
	_failures = 0;

	/* Everything here is answered SYNCHRONOUSLY, and that is the reason it is worth having.

	The claims are about what the forms KNOW, not about what is on screen, and a form answers
	collect(), getFields() and validate() from state it already holds - no frame, no layout pass,
	no settling. The one thing that is not synchronous, focus, is checked through getPendingField()
	rather than getFocusedField(), because a focus change is a request until the next commit. */

	// --- two forms, and they do not see each other -----------------------

	expect(_leftForm != nullptr && _rightForm != nullptr, "both forms exist");
	expect(_leftForm != _rightForm, "they are two systems, not one");

	expect(ui::FormSystem::findForNode(_leftColumn) == _leftForm,
			"the left column resolves to the left form");
	expect(ui::FormSystem::findForNode(_rightColumn) == _rightForm,
			"the right column resolves to the right form");

	// The left column carries every group at once; the right one only what is expanded.
	for (auto &name : getAllFieldNames()) {
		expect(_leftForm->getField(name) != nullptr,
				mem_std::toString("the left form has '", name, "'"));
	}

	// --- Transient is in the ring and out of the value -------------------

	expect(_leftForm->getField("notes") != nullptr, "'notes' IS a field of the left form");

	/* A const reference, and that is not style.

	collect() of a form with nothing in it answers an EMPTY value, and the non-const asDict() on one
	returns the shared Value::Null container - which is a debug assert, because writing through it
	would write into read-only memory. Reading a container through a `const Value &` is the way to
	ask a question of a value that may not be of that shape. */
	const Value collected = _leftForm->collect();
	expect(collected.isDictionary(), "collect() answers a dictionary");
	expect(!collected.hasValue("notes"), "but a Transient field is not collected");
	expect(!collected.hasValue("submit") && !collected.hasValue("reset"),
			"and neither are the buttons");

	for (auto &name : getAllFieldNames()) {
		expect(collected.hasValue(name), mem_std::toString("collect() carries '", name, "'"));
	}
	expect(collected.size() == getAllFieldNames().size(), "and carries nothing else");

	// --- the composite fields are ONE value each -------------------------

	expect(collected.getValue("offset").isArray() && collected.getValue("offset").size() == 3,
			"a ui::VectorField collects one array of three");
	expect(collected.getValue("tags").isArray(), "and a ui::ChipRow one array of ids");

	// The value, not the index: a slider at notch 12 of a 0..100/5 scale means 60.
	expect(collected.getInteger("volume") == 60, "a ui::Slider collects its VALUE");

	/* Canonical hex, and the alpha field carries a fourth byte because it was told to.

	Against the field's OWN value rather than the colour the demo starts on: a check that named
	`#1e88e5` was really asserting "nobody has touched this yet", so it failed the moment the
	picker was driven - which is the one time this file is worth re-running. */
	if (auto accent = dynamic_cast<ui::ColorField *>(getFieldNode("left", "accent"))) {
		expect(collected.getString("accent") == ui::formatColorHex(accent->getValue(), false),
				"a ui::ColorField collects canonical hex");
	} else {
		expect(false, "a ui::ColorField collects canonical hex");
	}
	expect(collected.getString("overlay").size() == 9,
			"and one with an alpha channel collects #rrggbbaa");

	// --- Required refuses, and says which field ---------------------------

	{
		// The demo opens with the three text fields EMPTY - a placeholder is not a value - so the
		// form starts invalid, and that is the state to check FROM rather than around.
		Vector<ui::FormValidationError> errors;
		expect(!_leftForm->validate(errors), "an empty Required field fails validation");
		expect(!errors.empty() && errors.front().name == "name",
				"and the error names the first one");
		expect(errors.size() == 3, "and all three Required fields are among them");

		expect(!_leftForm->submit(), "submit() refuses it");
		expect(_leftForm->getField("name")->collect().getString().empty(),
				"and the value it refused is still empty");
	}

	/* WHAT THIS CHECK DELIBERATELY DOES NOT ASSERT, and why it is the engine's rule rather than a
	gap: that filling the fields in makes the form validate.

	Assigning to a ui::TextInput is a REQUEST to the platform - the text arrives back by echo - and
	the field only writes locally while nothing is focused. submit() above focuses the field it
	refused, so from the next run onwards an assign here would not be readable in the same hop and
	the claim would fail for a reason that has nothing to do with forms. The same goes for the
	validator, which needs a non-empty value to see at all.

	Both are checked where a settle is available instead: `form.set` followed by `form.submit`, one
	inspector round trip apart. Two claims moved rather than dropped - and the second run of this
	check passing is what says the move was needed.

	The FOCUS submit() puts on the offender is not asserted either: getPendingField() resolves the
	request against the TAB RING, which is a per-frame result and is empty at enter. `form.state`
	answers that one, also after a settle. */

	// --- the accordion: a shut section has no fields ----------------------

	{
		// Whatever the demo is showing when this is re-run from the button, start from a known
		// state.
		for (auto group : getFieldGroups()) { _accordion->collapsePanel(getFieldGroupName(group)); }

		expect(_rightForm->getFields().empty(),
				"with every section shut the right form has no fields at all");

		expect(_accordion->expandPanel("text"), "expanding a section builds its panel");
		for (auto &name : getFieldNames(FieldGroup::Text)) {
			expect(_rightForm->getField(name) != nullptr,
					mem_std::toString("and the right form gains '", name, "'"));
		}
		expect(_leftForm->getField("name") != _rightForm->getField("name"),
				"the two forms' fields of the same name are different fields");

		// The builder ran once - when the section was declared, since a new one starts open - and
		// its node is KEPT, so a collapse and a re-expand does not rebuild it. That is what lets
		// half-typed text survive being shut away.
		auto builds = _builds.find("text");
		expect(builds != _builds.end() && builds->second == 1, "the panel was built exactly once");

		expect(_accordion->collapsePanel("text"), "collapsing it detaches the panel");
		expect(_rightForm->getField("name") == nullptr,
				"and its fields leave the form without the form being told");

		expect(_accordion->expandPanel("text"), "re-expanding brings it back");
		builds = _builds.find("text");
		expect(builds != _builds.end() && builds->second == 1, "without building it again");

		// Back to what the demo opens with: `text` is already open from the check above.
		_accordion->expandPanel("choice");
	}

	// --- the colour arithmetic the picker rests on ------------------------

	{
		static constexpr Color4B samples[] = {
			Color4B(0x00, 0x00, 0x00, 0xFF),
			Color4B(0xFF, 0xFF, 0xFF, 0xFF),
			Color4B(0x1E, 0x88, 0xE5, 0xFF),
			Color4B(0xFD, 0xD8, 0x35, 0xFF),
			Color4B(0x8E, 0x24, 0xAA, 0x80),
			Color4B(0x7F, 0x7F, 0x7F, 0xFF),
		};

		for (auto &it : samples) {
			for (bool alpha : {false, true}) {
				auto text = ui::formatColorHex(it, alpha);
				Color4B back;
				expect(sprt::geom::readColor(text, back), "the hex a picker prints is readable");

				auto expected = it;
				if (!alpha) {
					// readColor answers an opaque colour for a six-digit tag, which is what the
					// six digits mean.
					expected.a = 0xFF;
				}
				expect(back == expected, "and reading it back gives the colour");
			}

			// The conversions the bars are driven through, over the same colours.
			Color3B rgb(it.r, it.g, it.b);
			float h, s, l;
			sprt::geom::rgbToHsl(rgb, h, s, l);
			expect(sprt::geom::hslToRgb(h, s, l) == rgb, "HSL round-trips");

			float hv, sv, v;
			sprt::geom::rgbToHsv(rgb, hv, sv, v);
			expect(sprt::geom::hsvToRgb(hv, sv, v) == rgb, "HSV round-trips");
		}
	}

	// --- the picker's surface, with no window at all ----------------------

	{
		/* Built straight into a node rather than through a popup. That is what the surface being a
		class of its own buys: the whole model - the tabs, the channels, the hue that survives a
		grey - is checkable with no window system, on the platform this widget exists FOR. */
		ui::ColorPickerParams params;
		params.value = Color4B(0x1E, 0x88, 0xE5, 0xFF);
		params.alpha = true;
		params.palette = Vector<Color4B>{Color4B(0xFF, 0x00, 0x00, 0xFF)};

		auto content = Rc<ui::ColorPickerContent>::create(sp::move(params));
		expect(content != nullptr, "a picker surface can be built with no window");

		if (content) {
			expect(content->formatValue() == "#1e88e5ff", "it prints the colour it was given");

			content->setMode(ui::ColorPickerMode::HSL);
			expect(content->getMode() == ui::ColorPickerMode::HSL, "and takes a tab");
			expect(content->formatValue() == "#1e88e5ff",
					"changing the tab does not change the colour");

			/* Saturation to nothing: the colour becomes a grey, and a grey has no hue to read
			back. The channels are the surface's STATE rather than a projection of the value, so
			both the hue and the lightness stay exactly where they were - which is what makes the
			move reversible, and what a picker that recomputed its bars from the colour could not
			offer. */
			const float hue = content->getChannels().at(0);
			const float saturation = content->getChannels().at(1);
			content->setChannel(1, 0.0f);
			expect(content->getChannels().at(0) == hue,
					"dragging the saturation to zero KEEPS the hue");
			expect(content->getValue().r == content->getValue().g
							&& content->getValue().g == content->getValue().b,
					"and the colour really is a grey");

			content->setChannel(1, saturation);
			expect(content->formatValue() == "#1e88e5ff",
					"and dragging it back gives the very colour it started from");

			content->setAlpha(128.0f);
			expect(content->formatValue() == "#1e88e580", "the alpha reaches the hex");

			expect(content->setValueFromString("#fdd835"), "and a hex assigns the colour");
			expect(content->getValue() == Color4B(0xFD, 0xD8, 0x35, 0xFF), "as written");
		}
	}

	_selfCheckDone = true;
	log::source().warn("FormExample", "self-check: ", _checks, " checks, ", _failures, " failures");
	refreshStatus(_failures == 0 ? StringView("self-check passed")
								 : StringView("self-check FAILED - see the log"));
}

} // namespace stappler::xenolith::examples
