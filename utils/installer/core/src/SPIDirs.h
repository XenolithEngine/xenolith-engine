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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_
#define UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_

#include "SPICommon.h"
#include "SPIManifest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Install directories. Precedence (highest first): explicit prefix → $XENOLITH_HOME → OS
// conventions. An override puts everything under one root (home/{config,data,cache}).
//
// The OS conventions are NOT spelled out here: `system()` asks the runtime for the App* location
// categories (XDG on Linux, the system AppData folder on Windows, ~/Library on macOS), which the
// runtime places from APPCONFIG_BUNDLE_NAME. The CLI and the GUI declare the same bundle name and
// the same APPCONFIG_APP_PATH_COMMON on purpose — that is what makes both reach one store.
struct SP_PUBLIC Layout {
	String config; // config + the installed-state registry (installed.json)
	String data; // where the SDK is unpacked (toolchains, engines)
	String cache; // partial downloads / extraction scratch

	/* User overrides from settings.json, written here by Settings::applyTo (SPISettings.h). Empty
	means "derive it from the fields above", which is what every installation that never configured
	one gets.

	They live on the LAYOUT rather than as extra parameters because the layout is the one thing
	every core entry point already takes: `engine` is picked up by resolveEngineRoot and
	`toolchains` by getToolchainsDir, and install, link, list and build all go through those two
	already. A parameter per call site would have to be threaded through buildProject and
	scaffoldProject by hand, and any site that was forgotten would silently ignore what the user
	configured — the failure mode being "the GUI honours my engine path and the CLI does not".

	What fills them is the front end, right after Layout::resolve*: the settings file is found
	through `config`, so the layout has to exist before it can be read. */
	String engine; // engine root (STAPPLER_ROOT); below --engine and $XENOLITH_ENGINE, above the clone
	String toolchains; // the host/target store; empty → <data>/toolchains

	static Layout fromHome(StringView home);
	static Layout system();

	// An empty `prefix`/`envHome` means "not set".
	static Layout resolve(StringView prefix, StringView envHome);
	static Layout resolveFromEnv(StringView prefix = StringView());

	String getInstalledManifest() const; // <config>/installed.json
	String getProjectsManifest() const; // <config>/projects.json
	String getSettingsManifest() const; // <config>/settings.json
	String getToolchainsDir() const; // <data>/toolchains
	String getHostsDir() const; // <data>/toolchains/hosts
	String getTargetsDir() const; // <data>/toolchains/targets

	// <data>/toolchains/<hosts|targets>/<id> — where a host/target archive is unpacked.
	String getToolchainDir(Kind kind, StringView id) const;

	String getEnginesDir() const; // <data>/engines
	String getEngineDir(StringView ref) const; // <data>/engines/<ref>
	String getDownloadDir() const; // <cache>/downloads
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIDIRS_H_
