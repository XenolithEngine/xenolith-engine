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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERSCENECONTENT_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERSCENECONTENT_H_

#include "XL2dSceneContent.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XL2dIconSprite.h"
#include "XLUiStyleSystem.h"

#include "InstallerController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

class InstallerLayout;

// Owns the window-wide pieces: the stylesheet every node below resolves against, the background
// behind the layout, and the loading overlay shown until the catalogue arrives.
class InstallerSceneContent : public basic2d::SceneContent2d {
public:
	virtual ~InstallerSceneContent();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

	// Single modal slot shared by the confirm dialog and the gear popup: presenting one dismisses
	// whatever was up. It lives here, not in a file-scope Rc, so the overlay dies with the scene
	// instead of at static destruction — long after the renderer has gone.
	void presentOverlay(basic2d::SceneLayout2d *overlay);
	// No-op unless `overlay` is still the current one, so a stale dismiss cannot close a newer
	// dialog that replaced it.
	void dismissOverlay(basic2d::SceneLayout2d *overlay);
	basic2d::SceneLayout2d *getModalOverlay() const { return _modalOverlay; }

protected:
	// Hides the overlay, stops the spinner and returns the renderer to on-demand frames.
	void hideLoadingState();

	ui::StyleSystem *_rootStyle = nullptr;
	basic2d::Layer *_globalBackground = nullptr;

	// Loading overlay: a semi-opaque surface dimming the content area (everything below the title
	// bar) while the catalogue loads, plus a spinner and a caption. The three are SIBLINGS, not
	// parent/child — see init() for why.
	basic2d::Layer *_loadingOverlay = nullptr;
	basic2d::IconSprite *_spinner = nullptr;
	basic2d::Label *_loadingLabel = nullptr;
	bool _spinnerScheduled = false;

	Rc<InstallerController> _controller;
	Rc<InstallerLayout> _layout;
	Rc<basic2d::SceneLayout2d> _modalOverlay;
};

// The content behind `window`, or nullptr — how the dialog/gear helpers reach the modal slot.
InstallerSceneContent *getSceneContent(NotNull<AppWindow> window);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERSCENECONTENT_H_
