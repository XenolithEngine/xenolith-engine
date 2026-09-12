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


#ifndef EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREELOCALE_H_
#define EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREELOCALE_H_

#include "XLFontLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* THREE LANGUAGES, AND THE THIRD ONE TURNS THE TREE AROUND.

A tree is the widget where direction shows most: the indent that carries the hierarchy, the
disclosure arrow, the drop line that snaps back to an anchor row's indent and the drag ghost all
have a side, and all of them have to change it together or the tree stops reading as a tree.

None of that is written in this demo. It is two lines of CSS:

    @media (x-option: rtl) { :root { direction: rtl; } }
    * { unicode-bidi: plaintext; }

plus one property in the row rule - `padding-inline-start` instead of `padding-left`, which is what
turns "indent from the left" into "indent from where the text begins". `ui::StyleSystem` seeds the
`rtl` flag from `locale::getTextDirection()`, so the switch below only has to change the language. */
void defineDndTreeLocales();

// Move to the next language and answer its NAME IN ITSELF - the only name someone looking for
// their own language would recognise.
StringView cycleDndTreeLocale();

StringView currentDndTreeLocaleName();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREELOCALE_H_
