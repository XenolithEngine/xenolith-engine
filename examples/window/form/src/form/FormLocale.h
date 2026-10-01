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


#ifndef EXAMPLES_WINDOW_FORM_SRC_FORM_FORMLOCALE_H_
#define EXAMPLES_WINDOW_FORM_SRC_FORM_FORMLOCALE_H_

#include "XLFontLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* THE DEMO'S THREE LANGUAGES, AND WHY ONE OF THEM IS PERSIAN.

The interesting half of this example is not the translation - it is that choosing `فارسی` turns the
whole form around: the caption column moves to the right of its field, the control bar fills from
the right, the accordion's chevrons point the other way, the scroll bar hangs on the left. None of
that is written anywhere in this demo. It falls out of two lines of CSS:

    @media (x-option: rtl) { :root { direction: rtl; } }
    * { unicode-bidi: plaintext; }

`ui::StyleSystem` seeds the `rtl` media flag from `locale::getTextDirection()` and re-seeds it when
the locale changes, so an application that ships an RTL language gets the flag for nothing. The
first line is the whole of what an author writes; the second is what keeps a Latin string - an
email address, a hex colour, a file path - reading left to right inside a right-to-left window.

WHAT THE STRINGS LOOK LIKE. A label is given a TAG (`@Locale:Form:Name`) and never a translation:
it stores the tag unresolved and expands it while laying out, so `locale::setLocale` firing
`onLocale` is the entire redraw. Nothing here re-assigns a caption. */
void defineFormLocales();

// Move to the next language and answer its NAME IN ITSELF - `English`, `Русский`, `فارسی` - which
// is the only name a person looking for their own language would recognise.
StringView cycleFormLocale();

// The name of the language in force, for the button's caption at startup.
StringView currentFormLocaleName();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FORM_SRC_FORM_FORMLOCALE_H_
