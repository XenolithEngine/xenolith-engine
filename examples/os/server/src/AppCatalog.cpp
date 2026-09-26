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


#include "AppCatalog.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPData.h"

#include <sprt/runtime/platform.h>

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

static constexpr StringView AppCatalog_buildDir = "/stappler-build/";

bool AppCatalog::init() {
	// <repo>/examples/os/server/stappler-build/<target>/<build>/cc/wmserver
	auto exec = StringView(sprt::platform::getExecPath());
	auto pos = exec.find(AppCatalog_buildDir);
	if (pos == maxOf<size_t>()) {
		log::source().error("AppCatalog", "the server was not started from a build dir: ", exec);
		return false;
	}

	auto projectDir = exec.sub(0, pos);
	auto rest = exec.sub(pos + AppCatalog_buildDir.size());
	auto target = rest.readUntil<StringView::Chars<'/'>>();
	rest.skipChars<StringView::Chars<'/'>>();
	auto build = rest.readUntil<StringView::Chars<'/'>>();

	_repoRoot = filepath::root(FileInfo(projectDir), 3).str<Interface>();
	_buildDir = toString(target, "/", build);

	if (auto env = ::getenv(protocol::EnvCatalog)) {
		return load(StringView(env));
	}

	addApp(CatalogApp{"dock", "Dock", "Action_dashboard_solid", "examples/window/dock", "dock"});
	addApp(CatalogApp{"form", "Form", "Action_assignment_solid", "examples/window/form", "form"});
	addApp(CatalogApp{"dndtree", "Tree", "Action_list_solid", "examples/window/dndtree",
		"dndtree"});
	addApp(CatalogApp{"clientapp", "Client", "Hardware_computer_solid", "tests/window/client",
		"clientapp", LaunchStyle::Positional});
	return true;
}

String AppCatalog::resolve(StringView project, StringView exe) const {
	return filepath::merge<Interface>(_repoRoot, project, "stappler-build", _buildDir, "cc", exe);
}

const CatalogApp *AppCatalog::get(StringView id) const {
	for (auto &it : _apps) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

Vector<protocol::CatalogEntry> AppCatalog::getEntries() const {
	Vector<protocol::CatalogEntry> ret;
	for (auto &it : _apps) {
		ret.emplace_back(protocol::CatalogEntry{it.id, it.title, it.icon, it.available});
	}
	return ret;
}

void AppCatalog::addApp(CatalogApp &&app) {
	app.path = resolve(app.project, app.exe);
	app.available = filesystem::exists(FileInfo(app.path));
	_apps.emplace_back(sp::move(app));
}

bool AppCatalog::load(StringView file) {
	auto data = data::readFile<Interface>(FileInfo(file));
	if (!data.isDictionary()) {
		log::source().error("AppCatalog", "no catalog in '", file, "'");
		return false;
	}

	const Value &catalog = data;
	for (auto &it : catalog.getArray("apps")) {
		addApp(CatalogApp{it.getString("id"), it.getString("title"), it.getString("icon"),
			it.getString("project"), it.getString("exe"),
			it.getString("launch") == "positional" ? LaunchStyle::Positional
												   : LaunchStyle::Connect});
	}
	return true;
}

} // namespace stappler::xenolith::wm
