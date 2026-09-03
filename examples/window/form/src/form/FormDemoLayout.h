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

#ifndef EXAMPLES_WINDOW_FORM_SRC_FORM_FORMDEMOLAYOUT_H_
#define EXAMPLES_WINDOW_FORM_SRC_FORM_FORMDEMOLAYOUT_H_

#include "form/FormFields.h"
#include "XLUiAccordionView.h"
#include "XLUiFormSystem.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The demo's stylesheet, for whoever installs it.

IT GOES ON THE SCENE CONTENT, not on this layout, and that is the one structural thing about this
example worth copying. An in-scene popup - which is what both of the demo's dropdowns are - is
pushed onto the SceneContent as an overlay by SceneContent2d::pushOverlay, so it is a SIBLING of the
layout rather than a descendant. A ui::StyleSystem installed on the layout therefore cannot reach
it, and the surface comes up as an unstyled white box no matter how many rules are written for it.

On the content, one recursive ui::StyleResolver covers the layout and every overlay pushed beside
it, `:root` is the content, and the custom properties declared there are inherited by both. */
StringView getFormDemoStylesheet();

/** Every input field the kit ships, laid out twice, plus the two popups that open INSIDE the scene.

WHAT IS ON SCREEN, and why it is two columns of the same thing:

  * the LEFT column is the fields as bare widgets - a flex column of labelled rows, scrolling when
    it runs past the window. Nothing between the form and the controls;
  * the RIGHT column is the same fields parked in a ui::AccordionView, one section per group. A
    section's panel is built lazily on first show and DETACHED when the section is collapsed, so
    its fields leave the form's tab ring without the form being told anything - which is the one
    thing about the pair that is worth seeing side by side;
  * TWO ui::FormSystems, one per column, and they do not see each other's fields. A field joins the
    NEAREST form above it, so this costs exactly two addSystem calls and no bookkeeping;
  * the two popups are a ui::SearchPicker (a list too long to be a menu, chosen by typing) and
    ui::ColorField's built-in picker (bars, RGB/HSL/HSV tabs, an alpha bar, a palette and the hex
    on the clipboard). Both are asked for with `preferNative = false`, which is the whole of what
    makes a ui::SubWindow an in-scene overlay instead of a window of its own.

DRIVING IT WITHOUT A MOUSE. Every action is also a `form.*` inspector command, so the whole demo is
scriptable headless - see registerCommands(). runSelfCheck() asserts the structural claims above
synchronously and prints "N checks, M failures"; nothing in it needs a frame, because everything it
asks about is answered by the form rather than by the screen. */
class FormDemoLayout : public basic2d::SceneLayout2d {
public:
	virtual ~FormDemoLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// "left" / "right", the way every command names one. Null for anything else.
	ui::FormSystem *getForm(StringView) const;

protected:
	void buildControlBar();
	ui::Button *makeControl(StringView label, Function<void()> &&action);

	void buildLeftColumn();
	void buildRightColumn();

	// Wire the callbacks of one form to the status line and the counters. Both forms are set up by
	// this, so the two cannot report differently by accident.
	void wireForm(NotNull<ui::FormSystem>, StringView which);

	// One line of live state: what the last action was, and what the last collect() answered.
	void refreshStatus(StringView lastAction = StringView());

	// The widget a field's name stands for, resolved THROUGH the form rather than off a member -
	// so it reaches a field in an accordion section that was built minutes after this layout was.
	Node *getFieldNode(StringView form, StringView field) const;

	// The colour picker that is open, in either column, or null. There is at most one: a second
	// open() on another field closes nothing, but the demo only ever opens one at a time.
	ui::ColorField *getOpenColorField() const;

	// --- inspector -------------------------------------------------------
	void registerCommands();
	/* One inspector command. The handler reads its arguments through a CONST reference: a script
	sends whatever it likes, and the non-const data::Value getters assert on a key that is not
	there. */
	void addCommand(StringView name, StringView description,
			Function<Value(const Value &)> &&handler);

	Value encodeForm(StringView which) const;

	// --- self-check ------------------------------------------------------
	/* The structural claims of the class comment, asserted with no frame needed: the two forms are
	separate, a Transient field is not collected, a Required one refuses an empty submit, and
	collapsing an accordion section takes its fields out of the form. Prints "N checks, M failures".
	*/
	void runSelfCheck();
	void expect(bool, StringView message);

	Node *_background = nullptr;

	// The flex column everything else hangs from. A child rather than this node - see init().
	Node *_root = nullptr;

	Node *_controlBar = nullptr;
	basic2d::Label *_statusLabel = nullptr;

	// The left column: the node carrying the form is the SCROLLER, so a form is not something a
	// scroll container has to know about.
	Node *_leftColumn = nullptr;
	ui::FormSystem *_leftForm = nullptr;

	// The right column: the accordion's own node cannot carry the form, because a collapsed
	// section's panel is detached from it and would take its fields with it either way - the
	// wrapper is what stays put.
	Node *_rightColumn = nullptr;
	ui::FormSystem *_rightForm = nullptr;
	ui::AccordionView *_accordion = nullptr;
	Rc<ui::PanelRegistry> _registry;

	// How many times each section's builder ran: the point of a lazily-built panel, made visible.
	Map<String, size_t> _builds;

	String _lastAction;
	Value _lastSubmit;
	Vector<String> _lastInvalid;
	size_t _submitCount = 0;
	size_t _resetCount = 0;
	size_t _invalidCount = 0;

	// Held so handleExit can take the commands down: a lambda that captured a destroyed layout is
	// a dangling call from the inspector socket.
	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;

	size_t _checks = 0;
	size_t _failures = 0;
	bool _selfCheckDone = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FORM_SRC_FORM_FORMDEMOLAYOUT_H_
