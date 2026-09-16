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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERSHELL_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERSHELL_H_

#include "XL2dSceneLayout.h"
#include "XLUiWindowFrame.h"
#include "XLUiButton.h"

#include "InstallerNavPane.h"
#include "InstallerStatusBar.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

class InstallerPage;

/* The whole window: a flex column of the frame, the body, and the status bar.

  #installer-shell        display:flex; flex-direction:column
    window-frame                                    order 0
    #shell-body           display:flex; row; grow   order 1
       #nav-pane          flex: 0 0 var(--nav-w)
       #content-pane      flex-grow: 1
    #status-bar                                     order 2

Pages are built lazily and swapped by visibility rather than rebuilt: there are four of them, they
are cheap to keep, and a rebuild would throw away each page's scroll position on every click in the
tree. */
class InstallerShell : public basic2d::SceneLayout2d {
public:
	virtual ~InstallerShell();

	virtual bool init() override;
	virtual void handleEnter(Scene *) override;

	void showPage(PageId);

protected:
	InstallerPage *getPage(PageId);
	AppWindow *appWindow() const;
	void openSettings();

	ui::WindowFrame *_frame = nullptr;
	ui::Button *_settingsButton = nullptr;
	Node *_body = nullptr;
	InstallerNavPane *_nav = nullptr;
	Node *_content = nullptr;
	InstallerStatusBar *_status = nullptr;

	Map<PageId, InstallerPage *> _pages;
	PageId _current = PageId::Welcome;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERSHELL_H_
