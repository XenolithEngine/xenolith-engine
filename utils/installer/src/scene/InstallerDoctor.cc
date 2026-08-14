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

#include "InstallerDoctor.h"
#include "InstallerDialogs.h"
#include "InstallerStrings.h"

#include "SPIDirs.h"
#include "SPIEngineSource.h"

#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

void showDoctorDialog(NotNull<AppWindow> parent, AppController *controller) {
	String body = toString(strings::doctorTitle(), "\n\n");
	if (!controller) {
		body += "Controller unavailable.";
		showConfirmDialog(parent, strings::doctorTitle(), body, strings::actionClose(),
				ConfirmTone::Primary, [] { });
		return;
	}

	const auto &layout = controller->getLayout();
	body += toString("config: ", layout.config, "\n");
	body += toString("data: ", layout.data, "\n");
	body += toString("installed.json: ",
			filesystem::exists(FileInfo{layout.getInstalledManifest()}) ? "ok" : "missing", "\n");

	bool engineOk = false;
	auto root = resolveEngineRoot(layout, StringView(), &engineOk);
	body += toString("engine: ", engineOk ? root : String("not resolved"), "\n");

	if (auto *cat = controller->getCatalogue()) {
		size_t installed = 0;
		size_t updates = 0;
		for (const auto &row : cat->rows) {
			if (row.status == RowStatus::Installed) {
				++installed;
			} else if (row.status == RowStatus::UpdateAvailable) {
				++updates;
			}
		}
		body += toString("catalogue: ", cat->release, " rows=", cat->rows.size(),
				" installed=", installed, " updates=", updates, "\n");
	} else {
		body += "catalogue: not loaded\n";
	}
	body += "signature policy: .sig required (OpenPGP verify pending)\n";

	showConfirmDialog(parent, strings::doctorTitle(), body, strings::actionClose(),
			ConfirmTone::Primary, [] { });
}

} // namespace stappler::xenolith::installer
