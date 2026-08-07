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

#include "SPIProjects.h"

#include <algorithm>
#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

ProjectRegistry ProjectRegistry::load(StringView path) {
	ProjectRegistry st;
	auto v = data::readFile<mem_std::Interface>(FileInfo(path));
	auto arr = v.getValue("projects");
	if (!arr.isArray()) {
		return st;
	}
	for (const auto &item : arr.getArray()) {
		ProjectEntry p;
		p.name = item.getString("name");
		p.path = item.getString("path");
		p.engine = item.getString("engine");
		p.target = item.getString("target");
		p.makeTool = item.getString("make_tool");
		p.createdAt = item.getString("created_at");
		if (!p.path.empty()) {
			st.projects.emplace_back(sp::move(p));
		}
	}
	return st;
}

bool ProjectRegistry::save(StringView path) const {
	Value v;
	Value arr;
	for (const auto &p : projects) {
		Value item;
		item.setString(p.name, "name");
		item.setString(p.path, "path");
		item.setString(p.engine, "engine");
		item.setString(p.target, "target");
		item.setString(p.makeTool, "make_tool");
		item.setString(p.createdAt, "created_at");
		arr.addValue(sp::move(item));
	}
	v.setValue(sp::move(arr), "projects");
	filesystem::mkdir_recursive(FileInfo(filepath::root(path)));
	return data::save(v, FileInfo(path), data::EncodeFormat::Pretty);
}

void ProjectRegistry::upsert(ProjectEntry p) {
	for (auto &it : projects) {
		if (it.path == p.path) {
			it = sp::move(p);
			return;
		}
	}
	projects.emplace_back(sp::move(p));
}

bool ProjectRegistry::remove(StringView path) {
	for (auto it = projects.begin(); it != projects.end(); ++it) {
		if (it->path == path) {
			projects.erase(it);
			return true;
		}
	}
	return false;
}

const ProjectEntry *ProjectRegistry::find(StringView path) const {
	for (const auto &it : projects) {
		if (it.path == path) {
			return &it;
		}
	}
	return nullptr;
}

Vector<String> listSubdirs(StringView dir) {
	Vector<String> out;
	if (!isDirectory(dir)) {
		return out;
	}
	filesystem::ftw(FileInfo(dir), [&](const FileInfo &info, FileType type) {
		if (type == FileType::Dir && info.path != dir) {
			out.emplace_back(filepath::lastComponent(info.path).str<mem_std::Interface>());
		}
		return true;
	}, 1);
	std::sort(out.begin(), out.end());
	return out;
}

Vector<String> listInstalledEngines(const Layout &layout) {
	return listSubdirs(layout.getEnginesDir());
}

Vector<String> listInstalledTargets(const Layout &layout) {
	return listSubdirs(layout.getTargetsDir());
}

String defaultProjectsLocation() {
	const char *home = ::getenv("HOME");
	if (!home || !*home) {
		home = ::getenv("USERPROFILE");
	}
	if (!home || !*home) {
		return String("Projects");
	}
	auto path = mergePath(StringView(home), "Projects");
	filesystem::mkdir_recursive(FileInfo(path));
	return path;
}

} // namespace stappler::xenolith::installer
