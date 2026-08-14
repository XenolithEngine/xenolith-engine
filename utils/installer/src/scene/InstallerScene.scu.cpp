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

#include "XLCommon.h" // IWYU pragma: keep

#include "InstallerStrings.cc"
#include "InstallerMainScene.cc"
#include "InstallerSceneContent.cc"
#include "InstallerDialogs.cc"

#include "InstallerNavPane.cc"
#include "InstallerActionCell.cc" // before Page: the tools page builds these into its cells
#include "InstallerPage.cc"
#include "InstallerStatusBar.cc"
#include "InstallerSettingsPage.cc"
#include "InstallerShell.cc"

#include "InstallerDoctor.cc"

// Project management is out of scope for the current design pass, so the projects view is not
// reachable from the UI - the navigation tree shows `Projects` as a disabled leaf. The code stays
// compiled because it is the only working implementation of the createProject / buildProject /
// AppThread::perform shapes the future projects page will re-use, and it costs nothing to keep
// while the controller API it depends on is stable.
#include "InstallerProjects.cc"
