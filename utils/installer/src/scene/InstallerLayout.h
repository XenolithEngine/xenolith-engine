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
#include "XLUiBadge.h"
#include "XLUiButton.h"

#include "InstallerController.h"
#include "InstallerProjects.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppWindow;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Packages screen: title bar, header, virtualized table, footer. CSS owns paint/geometry.
class InstallerLayout : public basic2d::SceneLayout2d {
public:
	virtual ~InstallerLayout();

	virtual bool init() override;

	void onCatalogReady(InstallerController *controller);
	void dropScrollWarmup();

	void setEngineStatus(const EngineStatusInfo &info);
	void setBusy(bool busy);
	void reloadCatalogue();
	void confirmInstallEverything();

	void showPackagesTab();
	void showProjectsTab();

protected:
	void setNavTabSelected(ui::Button *on, ui::Button *off);
	void rebuildPackages();
	// Progress must NEVER full-rebuild the table — only setText on the status badge.
	void requestRebuildPackages(bool immediate = false);
	void setRowProgress(StringView key, StringView text);
	void bindStatusBadge(StringView key, ui::Badge *badge);
	void updateFooterButtons();
	void setSelection(Kind kind, StringView id, bool on);
	bool isSelected(Kind kind, StringView id) const;
	AppWindow *appWindow() const;

	void confirmInstallSelected();
	void confirmUninstall(Kind kind, StringView id, StringView label);
	void confirmPrepareEngine();
	void onRefreshAll();

	Node *_titleBar = nullptr;
	basic2d::Layer *_header = nullptr;
	ui::Button *_tabPackages = nullptr;
	ui::Button *_tabProjects = nullptr;
	Node *_packagesArea = nullptr;
	InstallerProjectsView *_projectsView = nullptr;
	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _scrollController;
	basic2d::Layer *_footer = nullptr;

	basic2d::Label *_releaseLabel = nullptr;
	basic2d::Label *_nativeLabel = nullptr;

	ui::Button *_btnRefresh = nullptr;
	ui::Button *_btnInstallSelected = nullptr;
	ui::Button *_btnInstallEverything = nullptr;
	ui::Button *_gearButton = nullptr;

	InstallerController *_controller = nullptr;
	EngineStatusInfo _engineInfo;
	Set<String> _selected; // rowKeys of checked rows
	Map<String, String> _progressText; // rowKey → status override while busy
	// Live status badges (valid until next rebuildPackages clear). KeepNodes=true.
	Map<String, ui::Badge *> _statusBadges;
	bool _hostsCollapsed = false;
	bool _targetsCollapsed = false;
	bool _busy = false;
	bool _packagesDirty = false;
	bool _rebuildPending = false;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERLAYOUT_H_
