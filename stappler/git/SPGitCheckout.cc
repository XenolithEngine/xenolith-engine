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

#include "SPGitCheckout.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"

#include <sys/stat.h> // chmod
#include <unistd.h> // symlink

namespace STAPPLER_VERSIONIZED stappler::git {

// Set POSIX file mode; on platforms without unix permissions the libc call is a
// no-op, which is exactly what we want (nothing to preserve there).
static void setFileMode(const String &path, uint32_t mode) { ::chmod(path.data(), mode); }

// Create a symbolic link; returns false if the platform/libc doesn't support it.
static bool makeSymlink(StringView target, const String &path) {
	String t = target.str<mem_std::Interface>();
	return ::symlink(t.data(), path.data()) == 0;
}

static Status walkTree(const ObjectStore &store, const Oid &treeOid, const String &dir,
		const String &relDir, ObjectFormat fmt, CheckoutStats &st) {
	const Object *tree = store.find(treeOid);
	if (!tree || tree->type != ObjectType::Tree) {
		return Status::ErrorNotFound;
	}

	auto entries = parseTree(BytesView(tree->data.data(), tree->data.size()), fmt);
	for (auto &e : entries) {
		String path = filepath::merge<mem_std::Interface>(StringView(dir), StringView(e.name));
		String rel = relDir.empty()
				? e.name
				: filepath::merge<mem_std::Interface>(StringView(relDir), StringView(e.name));

		if (e.isGitlink()) {
			// record the submodule; the caller resolves and clones it recursively
			GitlinkEntry gl;
			gl.path = sp::move(rel);
			gl.oid = e.oid;
			st.gitlinks.emplace_back(sp::move(gl));
			continue;
		}

		if (e.isDir()) {
			if (!filesystem::mkdir_recursive(FileInfo(StringView(path)))) {
				return Status::ErrorNotPermitted;
			}
			++st.dirsCreated;
			auto s = walkTree(store, e.oid, path, rel, fmt, st);
			if (s != Status::Ok) {
				return s;
			}
			continue;
		}

		const Object *blob = store.find(e.oid);
		if (!blob) {
			return Status::ErrorNotFound;
		}

		if (e.isSymlink()) {
			StringView target(reinterpret_cast<const char *>(blob->data.data()), blob->data.size());
			if (!makeSymlink(target, path)) {
				// fallback: write the link target as a regular file
				filesystem::write(FileInfo(StringView(path)), blob->data.data(), blob->data.size());
			}
			++st.symlinks;
		} else {
			if (!filesystem::write(FileInfo(StringView(path)), blob->data.data(),
						blob->data.size())) {
				return Status::ErrorNotPermitted;
			}
			setFileMode(path, e.isExecutable() ? 0755 : 0644);
			++st.filesWritten;
			st.bytesWritten += blob->data.size();
			if (st.firstBlobPath.empty()) {
				st.firstBlobPath = path;
				st.firstBlobOid = e.oid;
			}
		}
	}
	return Status::Ok;
}

Status checkout(const ObjectStore &store, const Oid &want, StringView destDir, CheckoutStats &st) {
	ObjectFormat fmt = want.format;

	// Resolve want -> commit (peel tag chains).
	Oid cur = want;
	const Object *obj = store.find(cur);
	int guard = 0;
	while (obj && obj->type == ObjectType::Tag && guard++ < 16) {
		cur = tagObject(BytesView(obj->data.data(), obj->data.size()), fmt);
		obj = store.find(cur);
	}
	if (!obj || obj->type != ObjectType::Commit) {
		return Status::ErrorNotFound;
	}
	st.commit = cur;

	Oid treeOid = commitTree(BytesView(obj->data.data(), obj->data.size()), fmt);
	if (treeOid.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	// Capture the root `.gitmodules` blob (if any) for submodule resolution.
	if (const Object *rootTree = store.find(treeOid);
			rootTree && rootTree->type == ObjectType::Tree) {
		for (auto &e : parseTree(BytesView(rootTree->data.data(), rootTree->data.size()), fmt)) {
			if (!e.isDir() && !e.isGitlink() && StringView(e.name) == ".gitmodules") {
				if (const Object *blob = store.find(e.oid)) {
					st.gitmodules = blob->data;
				}
				break;
			}
		}
	}

	String dest = destDir.str<mem_std::Interface>();
	if (!filesystem::mkdir_recursive(FileInfo(StringView(dest)))) {
		return Status::ErrorNotPermitted;
	}

	return walkTree(store, treeOid, dest, String(), fmt, st);
}

} // namespace stappler::git
