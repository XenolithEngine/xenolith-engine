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

#ifndef EXAMPLES_WINDOW_FORM_SRC_FORM_FORMFIELDS_H_
#define EXAMPLES_WINDOW_FORM_SRC_FORM_FORMFIELDS_H_

#include "XL2dSceneLayout.h"
#include "XLUiFormAdapters.h"
#include "XLUiTextView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// Nothing here is SP_PUBLIC: an example exports no symbols, and the two translation units of this
// app see each other through this header alone.

/* One band of the demo: a handful of related fields.

WHY THE GROUPS ARE A TYPE AND NOT TWO LISTS OF CALLS. The two columns show the SAME fields - the
left one as a flat stack of widgets, the right one parked in ui::AccordionView sections - and the
whole point of the example is that a field does not care which of the two it is in. Two hand-written
copies would drift within a week, and the drift would read as "a field behaves differently inside an
accordion", which is exactly the thing this is here to disprove. */
enum class FieldGroup {
	Text, // ui::TextInput in its three shapes
	Numbers, // ui::NumberField, ui::Slider, ui::VectorField
	Choice, // ui::Checkbox, ui::Select, ui::SearchPicker, ui::ChipRow
	Color, // ui::ColorField, both pickers
	Editors, // ui::TextView, ui::CodeEditor, ui::InlineEditor - NOT form fields, see below
	Actions, // the Submit and Reset buttons
};

// "text" / "numbers" / … - what a command names a group by, and what the accordion uses as a
// panel id.
StringView getFieldGroupName(FieldGroup);
StringView getFieldGroupTitle(FieldGroup);
bool readFieldGroup(StringView, FieldGroup &);

// Every group, in the order both columns show them.
SpanView<FieldGroup> getFieldGroups();

/* The height a group's rows need, for ui::DockPanelDescriptor::minSize.

IT IS NOT AN OPTIMISATION, it is the only thing that makes an accordion section the right size. A
ui::AccordionView in AccordionSizing::Fit floors an OPEN section at its header plus the panel's
DECLARED minimum and never measures the node - the same rule ui::DockSystem::measureLeaf follows,
and the accordion's own getNaturalMinSize() says so outright. The section's body is set up with
`flex-basis: 0, flex-grow: 1`, so it contributes nothing of its own to that sum and simply grows
into whatever the declared minimum left it. Declare nothing and every open section is a header with
a 40-pixel strip under it and its fields clipped away inside.

So this mirrors the row metrics in the layout's stylesheet, and the two have to be read together.
That is the cost of a container that sizes from a declaration rather than from its content, and it
is worth paying here for what the declaration buys: a section's height does not depend on a panel
that may not have been built yet. */
float getFieldGroupHeight(FieldGroup);

/* Build one group's rows into `parent`, joining whatever ui::FormSystem stands above it.

`parent` must be a flex column - the rows are flex items and nothing here positions anything. The
fields join a form by the ordinary route: ui::addFormField walks UP from the widget to the nearest
ui::FormSystem, so this function never has to be told which form it is building into, and the same
call serves a bare column and a lazily-built accordion panel.

ZORDER IS THE TAB ORDER. Node::sortAllChildren is not stable, so siblings sharing a z-order may
permute between passes - and the form's tab ring is document order, which is z-order. Every row
therefore gets a distinct one, counted from `firstZOrder`. */
void buildFieldGroup(NotNull<Node> parent, FieldGroup, int32_t firstZOrder = 1);

/* The FIELDS the group contributes to a form, in the order they are built.

The Editors group answers an empty list: ui::TextView and its subclasses reach the platform through
non-virtual window-based accessors that ui::addFormField's TextInput adapter cannot drive, so they
are in this demo as WIDGETS and not as fields. Saying so here is what keeps the self-check from
asserting a field that was never going to exist. */
SpanView<StringView> getFieldNames(FieldGroup);

// Everything a form in this demo collects, across every group. What the self-check compares a
// collect() against.
SpanView<StringView> getAllFieldNames();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FORM_SRC_FORM_FORMFIELDS_H_
