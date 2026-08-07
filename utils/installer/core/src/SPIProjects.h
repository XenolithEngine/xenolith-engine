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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIPROJECTS_H_
#define UTILS_INSTALLER_CORE_SRC_SPIPROJECTS_H_

#include "SPICommon.h"
#include "SPIDirs.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Mirrors the Tauri installer registry at <config>/projects.json so both GUIs share one list.
struct SP_PUBLIC ProjectEntry {
	String name;
	String path;
	String engine; // engines/<ref> name, or absolute STAPPLER_ROOT override
	String target; // default build triple; empty → host at build time
	String makeTool; // "make" / "xlmake"; empty → platform default
	String createdAt; // RFC3339-ish timestamp
};

struct SP_PUBLIC ProjectRegistry {
	Vector<ProjectEntry> projects;

	static ProjectRegistry load(StringView path);
	bool save(StringView path) const;

	void upsert(ProjectEntry p); // replace same path
	bool remove(StringView path);
	const ProjectEntry *find(StringView path) const;
};

// Subdirectory names under `dir` (sorted). Missing dir → empty.
SP_PUBLIC Vector<String> listSubdirs(StringView dir);

SP_PUBLIC Vector<String> listInstalledEngines(const Layout &layout);
SP_PUBLIC Vector<String> listInstalledTargets(const Layout &layout);

// Default parent for new projects: $HOME/Projects (created on demand).
SP_PUBLIC String defaultProjectsLocation();

// Native OS dialogs and the file manager deliberately do NOT live here: a picker blocks for as
// long as the user looks at it, which this synchronous core has no way to express. They are in
// src/controller/InstallerNativeDialogs.h, spawned on the app looper.

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIPROJECTS_H_
