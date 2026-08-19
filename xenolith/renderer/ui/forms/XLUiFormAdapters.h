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

#ifndef XENOLITH_RENDERER_UI_FORMS_XLUIFORMADAPTERS_H_
#define XENOLITH_RENDERER_UI_FORMS_XLUIFORMADAPTERS_H_

#include "XLUiFormInputListener.h"
#include "XLUiTextInput.h"
#include "XLUiCheckbox.h"
#include "XLUiButton.h"
#include "XLUiSelect.h"
#include "XLUiSearchPicker.h"
#include "XLUiNumberField.h"
#include "XLUiVectorField.h"
#include "XLUiColorField.h"
#include "XLUiChipRow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Ready-made slot fillers for the widgets this kit ships. This is the ONLY place where the form
// machinery knows what a TextInput or a Checkbox is - and the dependency points from forms/ to
// atoms/, never the other way, so a widget stays usable with no form in sight.
//
// Each one creates the listener, fills in what the widget can do and adds it to the node. The
// field name defaults to the node's own name, which is also its CSS id.

// Also takes a ui::NumberField, which IS a TextInput: it collects the NUMBER rather than the text
// of one. There is no second overload for it, because NotNull<> converts from either and the two
// would be ambiguous at every call site - the adapter branches instead.
SP_PUBLIC FormInputListener *addFormField(NotNull<TextInput>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

SP_PUBLIC FormInputListener *addFormField(NotNull<Checkbox>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

SP_PUBLIC FormInputListener *addFormField(NotNull<Select>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// Collects the chosen id, exactly as the Select adapter does: the same field may be either widget
// depending on how many values there are, and what a form sees must not depend on that choice.
SP_PUBLIC FormInputListener *addFormField(NotNull<SearchPicker>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// A COMPOSITE field: several ui::NumberFields collected as ONE array under one name. It is the
// worked example of what FormFieldSlots is for - the form drives a widget it knows nothing about,
// and the widget's own parts keep their keys because FormSystem admits a listener that sits below
// the focused field's node
SP_PUBLIC FormInputListener *addFormField(NotNull<VectorField>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);


// Collects the CANONICAL HEX of the colour ("#rrggbb", or "#rrggbbaa" where the field carries an
// alpha channel): JSON has no colour type, and hex is what a stylesheet, a schema default and a
// config file all already hold
SP_PUBLIC FormInputListener *addFormField(NotNull<ColorField>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

/* An ARRAY of the chips' ids, left to right. The second composite field in this kit, and the one
that shows the pattern is not about text: the row keeps its own selection and its own keys, the form
sees one value under one name, and a Required row that is empty is refused ONCE. Order is part of
the value - an element chain read back in a different order describes a different type. */
SP_PUBLIC FormInputListener *addFormField(NotNull<ChipRow>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// A button takes part in the tab order and fires the form on Enter, but is never collected
SP_PUBLIC FormInputListener *addFormButton(NotNull<Button>, FormFieldRole);

// Any other node: the caller describes what it can do
SP_PUBLIC FormInputListener *addFormField(NotNull<Node>, FormFieldSlots &&,
		StringView name = StringView(), FormFieldFlags = FormFieldFlags::None);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMADAPTERS_H_
