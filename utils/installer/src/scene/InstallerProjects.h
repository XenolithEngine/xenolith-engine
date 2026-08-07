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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERPROJECTS_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERPROJECTS_H_

#include "XL2dSceneLayout.h"
#include "XL2dScrollView.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLUiButton.h"

#include "InstallerController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppWindow;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Projects tab: registry list, new-project form, build/run + console.
class InstallerProjectsView : public Node {
public:
	virtual ~InstallerProjectsView();

	virtual bool init() override;

	void setController(InstallerController *controller);
	void reload();
	void showNewForm();
	void showList();

protected:
	AppWindow *appWindow() const;
	void rebuildList();
	void appendLog(StringView line);
	void setBusy(bool busy);
	void onCreate();
	void onBrowse();
	void onEditName();
	void onBuild(const ProjectEntry &p, bool run);
	void onRemove(const ProjectEntry &p);

	InstallerController *_controller = nullptr;

	Node *_listPane = nullptr;
	Node *_formPane = nullptr;
	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _scrollController;
	basic2d::Layer *_footer = nullptr;
	ui::Button *_btnNew = nullptr;
	ui::Button *_btnOpenFolder = nullptr;

	basic2d::Label *_nameLabel = nullptr;
	basic2d::Label *_locationLabel = nullptr;
	basic2d::Label *_statusLabel = nullptr;
	basic2d::Layer *_consolePanel = nullptr;
	basic2d::Label *_console = nullptr;

	String _name;
	String _location;
	String _defaultTarget;
	Vector<ProjectEntry> _projects;
	Vector<String> _logLines;
	bool _busy = false;
	bool _formVisible = false;
};

// Gear-menu sheets. Both are read-only reports rendered into a confirm overlay; Settings has no
// real surface yet, so its confirm button cycles the UI language.
void showStorageDialog(NotNull<AppWindow> parent, InstallerController *controller);
void showSettingsDialog(NotNull<AppWindow> parent);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERPROJECTS_H_
