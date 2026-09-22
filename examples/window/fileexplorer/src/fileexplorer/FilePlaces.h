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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEPLACES_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEPLACES_H_

#include "XLUiFilesystemModel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The model behind the left pane: the standard locations plus the root of the filesystem.

It is one ui::FilesystemModel with several roots rather than one model per place, so expansion,
selection and the lazy listing are the model's business and the tree stays a plain TreeView over
it. Directories only - a places tree that also listed files would repeat the right pane at a worse
size - and no stat(), since neither size nor mtime is shown here.

The roots are resolved once, at build time. A location the platform does not define, or defines at
a path that is not there, is left out rather than shown as an empty row. */
Rc<ui::FilesystemModel> makePlacesModel();

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEPLACES_H_
