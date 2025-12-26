/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPFilesystemResourceData.h"
#include "SPCore.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPMemInterface.h"
#include <mutex>

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

StringView _readEnvExt(memory::pool_t *pool, StringView key);
void _initSystemPaths(FilesystemResourceData &data);
void _termSystemPaths(FilesystemResourceData &data);

void _enumerateObjects(const FilesystemResourceData &data, FileCategory, StringView path, FileFlags,
		Access, const Callback<bool(StringView, FileFlags)> &);

} // namespace stappler::filesystem::platform

namespace STAPPLER_VERSIONIZED stappler::filesystem {

void FilesystemResourceData::initialize(void *ptr) {
	reinterpret_cast<FilesystemResourceData *>(ptr)->init();
}

void FilesystemResourceData::terminate(void *ptr) {
	reinterpret_cast<FilesystemResourceData *>(ptr)->term();
}

StringView FilesystemResourceData::getResourcePrefix(FileCategory cat) {
	switch (cat) {
	case FileCategory::Exec: return StringView("%EXEC%:"); break;
	case FileCategory::Library: return StringView("%LIBRARY%:"); break;
	case FileCategory::Fonts: return StringView("%FONTS%:"); break;

	case FileCategory::UserHome: return StringView("%USER_HOME%:"); break;
	case FileCategory::UserDesktop: return StringView("%USER_DESKTOP%:"); break;
	case FileCategory::UserDownload: return StringView("%USER_DOWNLOAD%:"); break;
	case FileCategory::UserDocuments: return StringView("%USER_DOCUMENTS%:"); break;
	case FileCategory::UserMusic: return StringView("%USER_MUSIC%:"); break;
	case FileCategory::UserPictures: return StringView("%USER_PICTURES%:"); break;
	case FileCategory::UserVideos: return StringView("%USER_VIDEOS%:"); break;

	case FileCategory::CommonData: return StringView("%COMMON_DATA%:"); break;
	case FileCategory::CommonConfig: return StringView("%COMMON_CONFIG%:"); break;
	case FileCategory::CommonState: return StringView("%COMMON_STATE%:"); break;
	case FileCategory::CommonCache: return StringView("%COMMON_CACHE%:"); break;
	case FileCategory::CommonRuntime: return StringView("%COMMON_RUNTIME%:"); break;

	case FileCategory::AppData: return StringView("%APP_DATA%:"); break;
	case FileCategory::AppConfig: return StringView("%APP_CONFIG%:"); break;
	case FileCategory::AppState: return StringView("%APP_STATE%:"); break;
	case FileCategory::AppCache: return StringView("%APP_CACHE%:"); break;
	case FileCategory::AppRuntime: return StringView("%APP_RUNTIME%:"); break;

	case FileCategory::Bundled: return StringView("%PLATFORM%:"); break;
	case FileCategory::Max: break;
	}
	return StringView();
}

static void readSingleQuoted(StringView &str, const Callback<void(StringView)> &writeCb) {
	++str;
	while (!str.empty()) {
		auto v = str.readUntil<StringView::Chars<'\'', '\\'>>();
		if (!v.empty()) {
			writeCb << v;
		}
		if (str.is('\\')) {
			++str;
			writeCb << str[0];
			++str;
		} else if (str.is<'\''>()) {
			++str;
			return;
		}
	}
}

static void readDoubleQuoted(memory::pool_t *pool, StringView &str,
		const Callback<void(StringView)> &writeCb) {
	++str;
	while (!str.empty()) {
		auto v = str.readUntil<StringView::Chars<'"', '\\', '$', '\''>>();
		if (!v.empty()) {
			writeCb << v;
		}
		if (str.is('\\')) {
			++str;
			writeCb << str[0];
			++str;
		} else if (str.is('$')) {
			++str;
			auto v =
					str.readUntil<StringView::Chars<'"', '\'', '$', '/'>, StringView::WhiteSpace>();
			if (!v.empty()) {
				// we need null-terminated string
				auto env = platform::_readEnvExt(pool, v.str<memory::StandartInterface>().data());
				if (!env.empty()) {
					writeCb << env;
				}
			}
		} else if (str.is('\'')) {
			readSingleQuoted(str, writeCb);
		} else if (str.is<'"'>()) {
			++str;
			return;
		}
	}
}

StringView FilesystemResourceData::readVariable(memory::pool_t *pool, StringView str) {
	return memory::perform_temporary([&](memory::pool_t *tmpPool) {
		memory::PoolInterface::StringType out;

		auto writer = [&](StringView s) { out.append(s.data(), s.size()); };

		Callback<void(StringView)> writeCb(writer);

		str.trimChars<StringView::WhiteSpace>();
		while (!str.empty()) {
			if (str.is('"')) {
				readDoubleQuoted(tmpPool, str, writeCb);
			} else if (str.is('\'')) {
				readSingleQuoted(str, writeCb);
			} else if (str.is('$')) {
				++str;
				auto v = str.readUntil<StringView::Chars<'"', '\'', '$', '/'>,
						StringView::WhiteSpace>();
				if (!v.empty()) {
					// we need null-terminated string
					auto env =
							platform::_readEnvExt(tmpPool, v.str<memory::PoolInterface>().data());
					if (!env.empty()) {
						writeCb << env;
					}
				}
			} else {
				auto v = str.readUntil<StringView::Chars<'"', '\'', '$'>>();
				if (!v.empty()) {
					writeCb << v;
				}
			}
		}

		auto ret = StringView(out);
		ret.backwardSkipChars<StringView::Chars<'/'>>();
		return ret.pdup(pool);
	}, pool);
}

FilesystemResourceData::FilesystemResourceData() { addInitializer(this, &initialize, &terminate); }

void FilesystemResourceData::initResource(ResourceLocation &res) {
	if (res.paths.empty()) {
		return;
	}

	if (!hasFlag(res.flags, CategoryFlags::PlatformSpecific)) {
		for (auto &it : res.paths) {
			if (hasFlag(it.second, FileFlags::Writable)) {
				filesystem::mkdir_recursive(FileInfo(it.first));

				if (filesystem::native::access_fn(it.first, Access::Write) != Status::Ok) {
					it.second &= ~FileFlags::Writable;
				}
			}
		}
	}

	res.init = true;
}

// recursive-logic trick - ordered enumeration without sorting
template <typename FileCallback>
static bool enumerateOrdered(FileFlags order, SpanView<Pair<StringView, FileFlags>> paths,
		FileCallback &&cb) {
	if (paths.empty()) {
		return true;
	}

	bool performInFront = true;
	auto &path = paths.front();
	switch (order) {
	case FileFlags::PrivateFirst:
		if (hasFlag(path.second, FileFlags::Private)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	case FileFlags::PublicFirst:
		if (hasFlag(path.second, FileFlags::Public)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	case FileFlags::SharedFirst:
		if (hasFlag(path.second, FileFlags::Shared)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	default: break;
	}

	if (performInFront) {
		if (!cb(path.first, path.second)) {
			return false;
		}
		if (paths.size() > 1) {
			return enumerateOrdered(order, paths.sub(1), cb);
		}
	} else {
		if (paths.size() > 1) {
			if (!enumerateOrdered(order, paths.sub(1), cb)) {
				return false;
			}
		}
		if (!cb(path.first, path.second)) {
			return false;
		}
	}
	return true;
}

void FilesystemResourceData::enumeratePaths(ResourceLocation &res, StringView filename,
		FileFlags flags, Access a, const Callback<bool(StringView, FileFlags)> &cb) {
	String path;

	bool writable = hasFlag(flags, FileFlags::Writable);
	auto pathFlags = flags & FileFlags::PathMask;
	auto orderFlags = flags & FileFlags::OrderMask;

	if (hasFlag(a, Access::Write)) {
		pathFlags |= FileFlags::Writable;
	}

	if (writable) {
		std::unique_lock lock(_initMutex);
		if (!res.init) {
			initResource(res);
		}
	}

	enumerateOrdered(orderFlags, res.paths, [&](StringView locPath, FileFlags locFlags) {
		if (writable && !hasFlag(locFlags, FileFlags::Writable)) {
			return true;
		}
		if (pathFlags == FileFlags::None || (locFlags & pathFlags) != FileFlags::None) {
			path = filepath::merge<Interface>(locPath, filename);
			if (a == Access::None || filesystem::native::access_fn(path, a) == Status::Ok) {
				if (hasFlag(flags, FileFlags::MakeWritableDir)) {
					filesystem::mkdir_recursive(FileInfo{filepath::root(path)});
				}
				if (!cb(path, locFlags)) {
					return false;
				}
			}
		}
		return true;
	});
}

void FilesystemResourceData::enumeratePaths(FileCategory cat, StringView filename, FileFlags flags,
		Access a, const Callback<bool(StringView, FileFlags)> &cb) {
	if (filepath::isAboveRoot(filename)) {
		return;
	}

	auto &res = _resourceLocations[toInt(cat)];

	if (hasFlag(flags, FileFlags::MakeWritableDir)) {
		flags |= FileFlags::Writable;
	}

	if (hasFlag(flags, FileFlags::PathMask)) {
		flags |= res.defaultFileFlags;
	}

	if (hasFlag(res.flags, CategoryFlags::PlatformSpecific)) {
		filesystem::platform::_enumerateObjects(*this, cat, filename, flags, a, cb);
	} else {
		enumeratePaths(res, filename, flags, a, cb);
	}
}

void FilesystemResourceData::init() {
	_pool = memory::pool::acquire();

	for (auto it : each<FileCategory>()) {
		auto &loc = _resourceLocations[toInt(it)];
		loc.category = it;
		loc.prefix = getResourcePrefix(it);
	}

	platform::_initSystemPaths(*this);

	for (auto it : each<FileCategory>()) {
		auto &loc = _resourceLocations[toInt(it)];

		for (auto &it : loc.paths) { it.first.backwardSkipChars<StringView::Chars<'/'>>(); }
	}

	_initialized = true;
}

void FilesystemResourceData::term() {
	for (auto it : each<FileCategory>()) { _resourceLocations[toInt(it)].paths.clear(); }

	platform::_termSystemPaths(*this);
}

FileCategory FilesystemResourceData::detectResourceCategory(StringView path,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb) const {
	if (path.is('%')) {
		auto cat = getResourceCategoryByPrefix(path);
		if (cat != FileCategory::Custom) {
			if (cb) {
				auto prefix = _resourceLocations[toInt(cat)].prefix;
				cb(path, path.sub(prefix.size()));
			}
		}
		return cat;
	}

	auto tmpPath = path;
	tmpPath.skipChars<StringView::Chars<'/'>>();
	for (auto &it : _resourceLocations) {
		if (hasFlag(it.flags, CategoryFlags::PlatformSpecific)) {
			if (filesystem::platform::_access(it.category, tmpPath, Access::Exists)) {
				if (cb) {
					cb(string::toString<Interface>(it.prefix, tmpPath), tmpPath);
				}
				return it.category;
			}
		}
	}

	// In reverse order - most concrete first
	const ResourceLocation *targetLoc = nullptr;
	uint32_t match = 0;

	for (auto &it : _resourceLocations) {
		if (!hasFlag(it.flags, CategoryFlags::PlatformSpecific)
				&& hasFlag(it.flags, CategoryFlags::Locateable)) {
			for (auto &resPath : it.paths) {
				if (path.starts_with(resPath.first) && path.at(resPath.first.size()) == '/') {
					if (resPath.first.size() > match) {
						targetLoc = &it;
						match = static_cast<uint32_t>(resPath.first.size());
					}
				}
			}
		}
	}

	if (targetLoc) {
		if (cb) {
			path += match;
			path.skipChars<StringView::Chars<'/'>>();
			cb(string::toString<Interface>(targetLoc->prefix, path), path);
		}
		return targetLoc->category;
	}
	return FileCategory::Max;
}

FileCategory FilesystemResourceData::detectResourceCategory(const FileInfo &info,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb) const {
	if (info.category == FileCategory::Custom) {
		return FileCategory::Max;
	} else {
		auto &res = _resourceLocations[toInt(info.category)];
		if (hasFlag(res.flags, CategoryFlags::PlatformSpecific)) {
			auto tmpPath = info.path;
			if (filesystem::platform::_access(info.category, tmpPath, Access::Exists)) {
				if (cb) {
					cb(string::toString<Interface>(res.prefix, tmpPath), tmpPath);
				}
			}
		} else {
			if (cb) {
				auto path = info.path;
				path.skipChars<StringView::Chars<'/'>>();
				cb(string::toString<Interface>(res.prefix, path), path);
			}
		}
		return res.category;
	}
}

FileCategory FilesystemResourceData::getResourceCategoryByPrefix(StringView prefix) const {
	for (auto it : each<FileCategory>()) {
		if (prefix.starts_with(_resourceLocations[toInt(it)].prefix)) {
			return _resourceLocations[toInt(it)].category;
		}
	}

	return FileCategory::Max;
}

bool FilesystemResourceData::enumeratePrefixedPath(StringView path, FileFlags flags, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) const {
	if (!path.starts_with("%")) {
		return false;
	}

	auto type = getResourceCategoryByPrefix(path);
	if (type != FileCategory::Max) {
		auto &res = _resourceLocations[toInt(type)];

		path += res.prefix.size();
		path.skipChars<StringView::Chars<'/'>>();

		// enumerate target dirs
		if (path.empty()) {
			if (a == Access::None) {
				filesystem::enumeratePaths(type, flags, cb);
			} else {
				return false;
			}
		}

		if (filepath::isAboveRoot(path)) {
			return false;
		}

		auto reconstructed = filepath::reconstructPath<Interface>(path);
		if (reconstructed.empty()) {
			return false;
		}

		filesystem::enumeratePaths(reconstructed, type, flags, a, cb);
		return true;
	}
	return false;
}

CategoryFlags FilesystemResourceData::getCategoryFlags(FileCategory cat) const {
	auto s = toInt(cat);
	if (s < _resourceLocations.size()) {
		return _resourceLocations[s].flags;
	}
	return CategoryFlags::None;
}

void FilesystemResourceData::initAppPaths(StringView root) {
	auto makeLocation = [&](FileCategory cat, StringView subname) {
		auto &res = _resourceLocations[toInt(cat)];
		res.paths.emplace_back(
				StringView(filepath::merge<memory::StandartInterface>(root, "AppData", subname))
						.pdup(_pool),
				FileFlags::Private | FileFlags::Public | FileFlags::Writable);
		res.flags |= CategoryFlags::Locateable;
	};

	makeLocation(FileCategory::AppData, "data");
	makeLocation(FileCategory::AppConfig, "config");
	makeLocation(FileCategory::AppState, "state");
	makeLocation(FileCategory::AppCache, "cache");
	makeLocation(FileCategory::AppRuntime, "runtime");
}

static FilesystemResourceData s_filesystemPathData;

// enumerate all paths, that will be used to find a resource of specific types
void enumeratePaths(FileCategory t, FileFlags flags,
		const Callback<bool(StringView, FileFlags)> &cb) {
	auto &res = s_filesystemPathData._resourceLocations[toInt(t)];

	if ((flags & FileFlags::PathMask) == FileFlags::None) {
		flags |= res.defaultFileFlags;
	}

	for (auto &it : res.paths) {
		if (flags == FileFlags::None || (it.second & flags) != FileFlags::None) {
			if (!cb(it.first, it.second)) {
				return;
			}
		}
	}
}

void enumeratePaths(StringView path, FileCategory t, FileFlags flags, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) {
	if (t < FileCategory::Custom) {
		s_filesystemPathData.enumeratePaths(t, path, flags, a, cb);
	} else {
		memory::StandartInterface::StringType str;
		if (!filepath::isAbsolute(path)) {
			str = currentDir<memory::StandartInterface>(path);
			path = str;
		}

		if (a == Access::None || native::access_fn(path, a) == Status::Ok) {
			cb(path, FileFlags::None);
		}
	}
}

FileCategory detectResourceCategory(StringView path,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb) {
	return s_filesystemPathData.detectResourceCategory(path, cb);
}

FileCategory detectResourceCategory(const FileInfo &info,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb) {
	return s_filesystemPathData.detectResourceCategory(info, cb);
}

CategoryFlags getCategoryFlags(FileCategory cat) {
	return s_filesystemPathData.getCategoryFlags(cat);
}

} // namespace stappler::filesystem
