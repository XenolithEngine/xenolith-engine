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
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLOCALE_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLOCALE_H_

#include "XLFontLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The example's string tables, under the `FE:` prefix.

Two languages, because the thing worth seeing here is what a locale change does NOT touch: the
names of the standard locations are TITLES the example supplies, so they follow the language, while
every name below a root comes from the filesystem and stays as it is on disk. A tree that
translated its own contents would be lying about what is there. */
void defineFileExplorerLocales();

// Move to the next language and answer its name in itself - the only name someone looking for
// their own language would recognise.
StringView cycleFileExplorerLocale();

StringView currentFileExplorerLocaleName();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLOCALE_H_
