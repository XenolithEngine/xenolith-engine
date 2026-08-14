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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERSTRINGS_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERSTRINGS_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer::strings {

enum class Lang {
	En,
	Ru,
	Zh
};

void setLang(Lang lang);
Lang getLang();

StringView appTitle();
StringView colName();
StringView colSize();
StringView colStatus();
StringView colActions();
StringView actionUpdate();

// Page headings and the sentence under each one.
StringView pageWelcomeTitle();
StringView pageWelcomeNote();
StringView pageEnginesTitle();
StringView pageEnginesNote();
StringView pageHostsTitle();
StringView pageHostsNote();
StringView pageTargetsTitle();
StringView pageTargetsNote();

StringView welcomeArch();
StringView welcomeEngine();
StringView welcomeEngineMissing();
StringView welcomeRelease();

// Settings form.
StringView settingsTitle();
StringView settingsEngineUrl();
StringView settingsReleaseUrl();
StringView settingsAutoUpdateInstaller();
StringView settingsAutoUpdateEngine();
StringView settingsAutoUpdateReleases();
StringView settingsClose();
StringView reachChecking();
StringView reachOk();
StringView reachFailed();
StringView groupEngine();
StringView groupHosts();
StringView groupTargets();
StringView engineNotReady();
StringView statusChecking();
StringView statusInstalled();
StringView statusNotInstalled();
StringView statusUpdateAvailable();
StringView phaseDownloading();
StringView actionInstallEverything();
String actionInstallSelected(size_t n);
StringView actionRefreshAll();
StringView actionInstall();
StringView actionDelete();
StringView actionPrepare();
StringView actionRefresh();
StringView actionCancel();
StringView actionClose();
StringView confirmInstallTitle();
StringView confirmInstallMessage();
StringView confirmInstallSelectedTitle();
String confirmInstallSelectedMessage(size_t n);
StringView confirmDeleteTitle();
String confirmDeleteMessage(StringView label);
StringView confirmEngineTitle();
StringView confirmEngineMessage();
StringView confirmRefreshTitle();
StringView confirmRefreshMessage();
StringView gearOpenDataDir();
StringView gearStorage();
StringView gearSettings();
StringView gearDoctor();
StringView storageTitle();
StringView settingsTitle();
StringView doctorTitle();
StringView projectsTitle();
StringView onboardingTitle();
StringView onboardingBody();
StringView tabPackages();
StringView tabProjects();
StringView projectNew();
StringView projectBack();
StringView projectName();
StringView projectLocation();
StringView projectChoose();
StringView projectCreate();
StringView projectBuild();
StringView projectRun();
StringView projectRemove();
String projectRemoveMessage(StringView name);
StringView projectOpenFolder();
StringView projectsEmpty();
StringView projectNameRule();
StringView projectPathNoSpace();
StringView projectNeedSdk();
StringView projectCreating();

} // namespace stappler::xenolith::installer::strings

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERSTRINGS_H_
