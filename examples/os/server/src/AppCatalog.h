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


#ifndef EXAMPLES_OS_SERVER_SRC_APPCATALOG_H_
#define EXAMPLES_OS_SERVER_SRC_APPCATALOG_H_

#include "WmProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

enum class LaunchStyle {
	// the standard entry point: `<exe> --connect <address>`
	Connect,
	// tests/window's clientapp: `<exe> <address>`, the window from XL_CLIENT_CREATE_WINDOW
	Positional,
};

struct CatalogApp {
	String id;
	String title;
	String icon;
	String project; // relative to the repository root
	String exe;
	LaunchStyle launch = LaunchStyle::Connect;

	String path; // resolved
	bool available = false; // the binary exists
};

/* What the shell offers. Binaries are looked up the way the server itself was built: the same
target and build type, in each project's own stappler-build. XL_WM_CATALOG names a JSON file
({ apps: [{ id, title, icon, project, exe, launch: connect|positional }] }) that replaces the
built-in list. */
class AppCatalog : public Ref {
public:
	virtual ~AppCatalog() = default;

	bool init();

	// A project's binary built like this server: <repo>/<project>/stappler-build/<target>/<build>/cc/<exe>
	String resolve(StringView project, StringView exe) const;

	const CatalogApp *get(StringView id) const;
	SpanView<CatalogApp> getApps() const { return _apps; }

	Vector<protocol::CatalogEntry> getEntries() const;

	StringView getRepoRoot() const { return _repoRoot; }

protected:
	void addApp(CatalogApp &&);
	bool load(StringView file);

	String _repoRoot;
	String _buildDir; // <target>/<build>
	Vector<CatalogApp> _apps;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_APPCATALOG_H_ */
