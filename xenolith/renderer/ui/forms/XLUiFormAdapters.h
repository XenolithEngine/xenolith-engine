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
#include "XLUiSlider.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Slot fillers for the kit's widgets: each creates the listener, fills the slots and adds it to the
// node. The only place forms/ knows about atoms/; widgets never depend on forms. The field name
// defaults to the node's name (its CSS id).

// Also takes a ui::NumberField and collects its number; branches inside, since a second overload
// would be ambiguous through NotNull<>.
SP_PUBLIC FormInputListener *addFormField(NotNull<TextInput>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

SP_PUBLIC FormInputListener *addFormField(NotNull<Checkbox>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

SP_PUBLIC FormInputListener *addFormField(NotNull<Select>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// Collects the chosen id, same as the Select adapter.
SP_PUBLIC FormInputListener *addFormField(NotNull<SearchPicker>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// Composite field: the components are collected as one array under one name; their own listeners
// keep receiving keys because FormSystem admits listeners below the focused field's node.
SP_PUBLIC FormInputListener *addFormField(NotNull<VectorField>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);


// Collects the canonical hex of the colour ("#rrggbb", or "#rrggbbaa" with an alpha channel).
SP_PUBLIC FormInputListener *addFormField(NotNull<ColorField>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

/* Collects an array of chip ids, left to right; order is part of the value. An empty Required
row is refused once. */
SP_PUBLIC FormInputListener *addFormField(NotNull<ChipRow>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

/* Collects the value (`min + step * index`), not the index: an integer for an integer slider, a
double otherwise. No `activate`, so Enter on a focused slider submits the form. */
SP_PUBLIC FormInputListener *addFormField(NotNull<Slider>, StringView name = StringView(),
		FormFieldFlags = FormFieldFlags::None);

// A button takes part in the tab order and fires the form on Enter, but is never collected
SP_PUBLIC FormInputListener *addFormButton(NotNull<Button>, FormFieldRole);

// Any other node: the caller describes what it can do
SP_PUBLIC FormInputListener *addFormField(NotNull<Node>, FormFieldSlots &&,
		StringView name = StringView(), FormFieldFlags = FormFieldFlags::None);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMADAPTERS_H_
