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

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/enum.h>
#include <sprt/runtime/stringview.h>
#include "private/SPRTFilesystem.h"
#include "private/SPRTPrivate.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#if SPRT_LINUX
#include "SPRuntimeFilesystem-linux.cc"
#endif

#if SPRT_ANDROID
#include "SPRuntimeFilesystem-android.cc"
#endif

#if SPRT_WINDOWS
#include <sys/winapi.h>

#include "SPRuntimeFilesystem-windows.cc"
#endif

namespace sprt::filesystem {

bool getCurrentDir(const callback<void(StringView)> &cb, StringView path) {
	if (!path.empty() && filepath::isAbsolute((path))) {
		filepath::reconstructPath(cb, path);
		return true;
	}

#if __SPRT_CONFIG_HAVE_WINAPI
	auto bufferLen = _GetCurrentDirectory(0, nullptr);
	if (bufferLen == 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char16_t, bufferLen + 1);
	bufferLen = _GetCurrentDirectory(bufferLen + 1, buf);
	buf[bufferLen] = 0;
	if (bufferLen == 0) {
		return false;
	}

	if (path.empty()) {
		unicode::toUtf8(cb, WideStringView(buf, bufferLen));
		__sprt_freea(buf);
		return true;
	} else {
		auto ulen = unicode::getUtf8Length(WideStringView(buf, bufferLen));

		auto bufferSize = ulen + path.size() + 3;
		auto resultBuf = __sprt_typed_malloca(char, bufferSize);

		unicode::toUtf8(resultBuf, bufferSize, WideStringView(buf, bufferLen), &ulen);
		resultBuf[ulen] = 0;

		bufferSize -= ulen;
		auto target = &resultBuf[ulen];

		if (buf[bufferLen - 1] != '/' && path.at(0) != '/') {
			target = strappend(target, &bufferSize, "/", 1);
		}
		target = strappend(target, &bufferSize, path.data(), path.size());
		target[0] = 0;

		filepath::reconstructPath(cb, StringView(resultBuf, target - resultBuf));
		return true;
	}
#else
	char buf[PATH_MAX + 1];
	auto result = getcwd(buf, PATH_MAX);
	if (result) {
		auto len = sprt::detail::length(buf, PATH_MAX);
		if (path.empty()) {
			buf[len] = 0;
			cb(StringView(buf, len));
			return true;
		} else if (PATH_MAX - len > path.size() + 2) {
			auto bufferSize = PATH_MAX - len - 1;
			auto target = &buf[len];

			if (buf[len - 1] != '/' && path.at(0) != '/') {
				target = strappend(target, &bufferSize, "/", 1);
			}
			target = strappend(target, &bufferSize, path.data(), path.size());
			target[0] = 0;

			filepath::reconstructPath(cb, StringView(buf, target - buf));
			return true;
		}
	}
#endif
	return false;
}

static void readSingleQuoted(StringView &str, const callback<void(StringView)> &writeCb) {
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
		const callback<void(StringView)> &writeCb) {
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
				auto env = detail::_readEnvExt(pool, v);
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

StringView readVariable(memory::pool_t *pool, StringView str) {
	StringView result;
	memory::perform_temporary([&](memory::pool_t *tmpPool) {
		memory::string out;

		auto writer = [&](StringView s) {
			out.append(s.data(), s.size()); //
		};

		callback<void(StringView)> writeCb(writer);

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
					auto env = detail::_readEnvExt(tmpPool, v);
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
		result = ret.pdup(pool);
	}, pool);
	return result;
}

static StringView getResourcePrefix(LocationCategory cat) {
	switch (cat) {
	case LocationCategory::Exec: return StringView("%EXEC%:"); break;
	case LocationCategory::Library: return StringView("%LIBRARY%:"); break;
	case LocationCategory::Fonts: return StringView("%FONTS%:"); break;

	case LocationCategory::UserHome: return StringView("%USER_HOME%:"); break;
	case LocationCategory::UserDesktop: return StringView("%USER_DESKTOP%:"); break;
	case LocationCategory::UserDownload: return StringView("%USER_DOWNLOAD%:"); break;
	case LocationCategory::UserDocuments: return StringView("%USER_DOCUMENTS%:"); break;
	case LocationCategory::UserMusic: return StringView("%USER_MUSIC%:"); break;
	case LocationCategory::UserPictures: return StringView("%USER_PICTURES%:"); break;
	case LocationCategory::UserVideos: return StringView("%USER_VIDEOS%:"); break;

	case LocationCategory::CommonData: return StringView("%COMMON_DATA%:"); break;
	case LocationCategory::CommonConfig: return StringView("%COMMON_CONFIG%:"); break;
	case LocationCategory::CommonState: return StringView("%COMMON_STATE%:"); break;
	case LocationCategory::CommonCache: return StringView("%COMMON_CACHE%:"); break;
	case LocationCategory::CommonRuntime: return StringView("%COMMON_RUNTIME%:"); break;

	case LocationCategory::AppData: return StringView("%APP_DATA%:"); break;
	case LocationCategory::AppConfig: return StringView("%APP_CONFIG%:"); break;
	case LocationCategory::AppState: return StringView("%APP_STATE%:"); break;
	case LocationCategory::AppCache: return StringView("%APP_CACHE%:"); break;
	case LocationCategory::AppRuntime: return StringView("%APP_RUNTIME%:"); break;

	case LocationCategory::Bundled: return StringView("%PLATFORM%:"); break;
	case LocationCategory::Max: break;
	}
	return StringView();
}

static detail::LookupData *s_resourceData = nullptr;

void initialize() {
	auto pool = platform::getConfigPool();

	memory::perform([&] {
		s_resourceData = new (pool) detail::LookupData;
		s_resourceData->_pool = pool;

		for (auto it : each<LocationCategory>()) {
			auto &loc = s_resourceData->_resourceLocations[toInt(it)];
			loc.category = it;
			loc.prefix = getResourcePrefix(it);
		}

		detail::_initSystemPaths(*s_resourceData);

		for (auto it : each<LocationCategory>()) {
			auto &loc = s_resourceData->_resourceLocations[toInt(it)];

			for (auto &it : loc.paths) { it.path.backwardSkipChars<StringView::Chars<'/'>>(); }
		}

		s_resourceData->_initialized = true;
	}, pool);
}

void terminate() {
	for (auto it : each<LocationCategory>()) {
		s_resourceData->_resourceLocations[toInt(it)].paths.clear();
	}

	detail::_termSystemPaths(*s_resourceData);
}

LocationCategory getResourceCategoryByPrefix(StringView prefix) {
	auto data = detail::LookupData::get();
	for (auto it : each<LocationCategory>()) {
		if (prefix.starts_with(data->_resourceLocations[toInt(it)].prefix)) {
			return data->_resourceLocations[toInt(it)].category;
		}
	}

	return LocationCategory::Max;
}

const LookupInfo *getLookupInfo(LocationCategory cat) {
	if (toInt(cat) < toInt(LocationCategory::Custom)) {
		return &detail::LookupData::get()->_resourceLocations[toInt(cat)];
	}
	return nullptr;
}

using EnumListType = memory::forward_list<sprt::filesystem::LocationInfo>;

// We need FileInfo for root constraints
static bool _mkdir_recursive(const LocationInfo &info, StringView path) {
	if (info.path.empty()) {
		return false;
	}

	// check if root dir exists
	auto root = filepath::root(path);
	auto rootInfo = info;
	rootInfo.path = filepath::root(info.path);

	auto err = info.interface->_access(info, root, Access::Exists);
	if (err != Status::Ok) {
		if (!_mkdir_recursive(info, root)) {
			return false;
		}
	}

	return info.interface->_mkdir(info, path, LocationInterface::DirMode) == Status::Ok;
}

// recursive-logic trick - ordered enumeration without sorting
template <typename FileCallback>
static bool enumerateOrdered(LookupFlags order, const EnumListType &paths,
		EnumListType::const_iterator it, FileCallback &&cb) {
	if (it == paths.end()) {
		return true;
	}

	bool performInFront = true;
	auto &path = *it;
	switch (order) {
	case LookupFlags::PrivateFirst:
		if (hasFlag(path.lookupType, LookupFlags::Private)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	case LookupFlags::PublicFirst:
		if (hasFlag(path.lookupType, LookupFlags::Public)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	case LookupFlags::SharedFirst:
		if (hasFlag(path.lookupType, LookupFlags::Shared)) {
			performInFront = true;
		} else {
			performInFront = false;
		}
		break;
	default: break;
	}

	if (performInFront) {
		if (!cb(path)) {
			return false;
		}
		auto next = it;
		++next;
		if (next != paths.end()) {
			return enumerateOrdered(order, paths, next, cb);
		}
	} else {
		auto next = it;
		++next;
		if (next != paths.end()) {
			if (!enumerateOrdered(order, paths, next, cb)) {
				return false;
			}
		}
		if (!cb(path)) {
			return false;
		}
	}
	return true;
}

static void initResource(sprt::filesystem::LookupInfo &res) {
	if (res.paths.empty()) {
		return;
	}

	for (auto &it : res.paths) {
		if (hasFlag(it.locationFlags, sprt::filesystem::LocationFlags::Writable)) {
			_mkdir_recursive(it, it.path);

			if (it.interface->_access(it, it.path, Access::Write) != Status::Ok) {
				it.lookupType &= ~LookupFlags::Writable;
			}
		}
	}

	res.init = true;
}

void enumeratePaths(const LookupInfo &res, StringView ifilename, LookupFlags flags, Access a,
		const callback<bool(const LocationInfo &, StringView)> &cb) {
	bool writable = hasFlag(flags, LookupFlags::Writable);
	auto pathFlags = flags & LookupFlags::PathMask;
	auto orderFlags = flags & LookupFlags::OrderMask;

	if (hasFlag(a, Access::Write)) {
		pathFlags |= LookupFlags::Writable;
	}

	if (writable) {
		sprt::unique_lock lock(res.mutex);
		if (!res.init) {
			initResource(const_cast<LookupInfo &>(res));
		}
	}

	sprt::filepath::reconstructPath([&](StringView filename) {
		enumerateOrdered(orderFlags, res.paths, res.paths.begin(),
				[&](const sprt::filesystem::LocationInfo &info) {
			if (writable && !hasFlag(info.lookupType, LookupFlags::Writable)) {
				return true;
			}
			bool ret = true;
			if (pathFlags == LookupFlags::None
					|| (info.lookupType & pathFlags) != LookupFlags::None) {
				sprt::filepath::merge([&](StringView path) {
					if (a == Access::None || info.interface->_access(info, path, a) == Status::Ok) {
						if (hasFlag(flags, LookupFlags::MakeDir)) {
							_mkdir_recursive(info, filepath::root(path));
						}
						if (!cb(info, path)) {
							ret = false;
						}
					}
				}, info.path, filename);
			}
			return ret;
		});
	}, ifilename);
}

void enumeratePaths(LocationCategory t, LookupFlags flags,
		const callback<bool(const LocationInfo &, StringView)> &cb) {
	if (t < LocationCategory::Custom) {
		auto res = sprt::filesystem::getLookupInfo(t);

		if ((flags & LookupFlags::PathMask) == LookupFlags::None) {
			flags |= res->defaultLookupFlags;
		}

		for (auto &it : res->paths) {
			if (flags == LookupFlags::None || (it.lookupType & flags) != LookupFlags::None) {
				if (!cb(it, it.path)) {
					return;
				}
			}
		}
	} else {
		sprt::filesystem::getCurrentDir(
				[&](StringView path) { cb(sprt::filesystem::getCurrentLocation(), path); });
	}
}

} // namespace sprt::filesystem

namespace sprt::filesystem::detail {

LookupData *LookupData::get() { return s_resourceData; }

void LookupData::initAppPaths(StringView root) {
	auto makeLocation = [&](LocationCategory cat, StringView subname) {
		auto &res = _resourceLocations[toInt(cat)];
		filepath::merge([&](StringView path) {
			res.paths.emplace_back(LocationInfo{
				path.pdup(_pool),
				LookupFlags::Private | LookupFlags::Public | LookupFlags::Writable,
				LocationFlags::Locateable | LocationFlags::Writable,
				getDefaultInterface(),
			});
		}, root, "AppData", subname);
	};

	makeLocation(LocationCategory::AppData, "data");
	makeLocation(LocationCategory::AppConfig, "config");
	makeLocation(LocationCategory::AppState, "state");
	makeLocation(LocationCategory::AppCache, "cache");
	makeLocation(LocationCategory::AppRuntime, "runtime");
}

} // namespace sprt::filesystem::detail
