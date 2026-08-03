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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERLAYOUT_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dScrollView.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"

#include "InstallerController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The packages screen: title bar, header, virtualized component table, footer. The whole layout is
// driven by resources/style.css — this class only builds the node tree and assigns identity
// (type / name / style class). It sets no colours, sizes or positions.
class InstallerLayout : public basic2d::SceneLayout2d {
public:
	virtual ~InstallerLayout();

	virtual bool init() override;

	// Called by InstallerSceneContent once the controller has loaded its catalogue; refreshes the
	// header meta labels and rebuilds the table from controller->catalog().
	void onCatalogReady(InstallerController *controller);

	// Reverts the pre-warm window expansion (see onCatalogReady) once the loading overlay hides, so
	// the virtualizer keeps only the visible rows on screen.
	void dropScrollWarmup();

protected:
	void rebuildPackages();

	Node *_titleBar = nullptr;
	Node *_packagesArea = nullptr;
	basic2d::Layer *_header = nullptr;
	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _scrollController;
	basic2d::Layer *_footer = nullptr;

	basic2d::Label *_releaseLabel = nullptr;
	basic2d::Label *_nativeLabel = nullptr;

	InstallerController *_controller = nullptr;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERLAYOUT_H_
