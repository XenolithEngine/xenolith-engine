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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERSETTINGSPAGE_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERSETTINGSPAGE_H_

#include "XLCommon.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppWindow;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

/* Open the settings form for `parent`.

Presented through ui::SubWindow::openUtility, so it is a real OS window wherever
WindowCapabilities::Subwindows exists and an in-scene overlay where it does not - and, unlike
openDialog, it does not compete for InstallerSceneContent's single modal slot, which stays for
confirmations.

The form applies each field AS IT LOSES FOCUS rather than on a submit (design.md), so it has no
submit button at all - only Close. */
void showSettingsPage(NotNull<AppWindow> parent);

// Take the form down from outside, the way its own Close button does. Exists so the dismiss path -
// the one that has to release the surface AND fire the close callback exactly once - can be driven
// without a pointer, on the native window as well as on the in-scene fallback.
// Returns false when no form is open.
bool closeSettingsPage();

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERSETTINGSPAGE_H_
