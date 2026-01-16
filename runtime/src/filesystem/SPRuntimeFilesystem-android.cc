/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include <sprt/runtime/platform.h>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/mem/vector.h>
#include <sprt/runtime/mem/map.h>
#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/enum.h>
#include <sprt/jni/jni.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "private/SPRTFilesystem.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_mman.h>

#include <android/asset_manager.h>

namespace sprt::filesystem::detail {

struct PathInfo {
	using String = memory::dynstring;

	String _apkPath;
	struct stat _apkStat;

	String _filesDir;
	String _cacheDir;
	String _externalFilesDir;
	String _externalCacheDir;

	qmutex _mutex;

	AAssetManager *_assetManager = nullptr;

	bool _cacheInit = false;
	bool _documentsInit = false;

	static PathInfo *getInstance();

	PathInfo();

	Access getExternalStorageState();

	bool initialize(sprt::jni::App *app, const sprt::jni::Ref &ctx, StringView apkPath);
	void terminate();

	void initSystemPaths(LookupData &data);
	void termSystemPaths(LookupData &data);

	StringView getApplicationPath() const;
};

static LocationInterface s_apkInterface = {
	._access = [](const LocationInfo &loc, StringView path, Access mode) -> Status {
	if (hasFlag(mode, Access::Write) || hasFlag(mode, Access::Execute) || mode == Access::None) {
		return Status::ErrorInvalidArguemnt;
	}

	if (path.starts_with(loc.path)) {
		path += loc.path.size();
	}
	path.skipChars<StringView::Chars<'/'>>();

	auto app = jni::Env::getApp();

	Status ret = Status::ErrorNotFound;

	path.performWithTerminated([&](const char *path, size_t size) {
		auto asset = AAssetManager_open(app->nAssetManager, path, AASSET_MODE_UNKNOWN);
		if (asset) {
			AAsset_close(asset);
			ret = Status::Ok;
			return;
		}

		auto adir = AAssetManager_openDir(app->nAssetManager, path);
		if (adir) {
			AAssetDir_close(adir);
			ret = Status::Ok;
			return;
		}
	});

	if (ret == Status::ErrorNotFound && hasFlag(mode, Access::Empty)) {
		return Status::Ok;
	}
	return ret;
},

	._stat = [](const LocationInfo &loc, StringView path,
					 struct __SPRT_STAT_NAME *_stat) -> Status {
	if (path.starts_with(loc.path)) {
		path += loc.path.size();
	}
	path.skipChars<StringView::Chars<'/'>>();

	auto app = jni::Env::getApp();
	auto info = PathInfo::getInstance();
	Status ret = Status::ErrorNotFound;
	path.performWithTerminated([&](const char *path, size_t size) {
		auto asset = AAssetManager_open(app->nAssetManager, path, AASSET_MODE_UNKNOWN);
		if (asset) {
			*_stat = info->_apkStat;

			_stat->st_ino ^= hash64(path, size);
			_stat->st_nlink = 1;
			_stat->st_mode = __SPRT_S_IRUSR | __SPRT_S_IFREG;
			_stat->st_size = AAsset_getLength64(asset);
			_stat->st_blksize = _stat->st_size;
			_stat->st_blocks = 1;

			AAsset_close(asset);
			ret = Status::Ok;
			return;
		}

		auto adir = AAssetManager_openDir(app->nAssetManager, path);
		if (adir) {
			*_stat = info->_apkStat;

			_stat->st_ino ^= hash64(path, size);
			_stat->st_nlink = 1;
			_stat->st_mode = __SPRT_S_IRUSR | __SPRT_S_IFDIR;
			_stat->st_size = 0;
			_stat->st_blksize = 0;
			_stat->st_blocks = 1;

			AAssetDir_close(adir);
			ret = Status::Ok;
			return;
		}
	});
	return ret;
},

	._open = [](const LocationInfo &loc, StringView path, OpenFlags flags, Status *st) -> void * {
	if (hasFlag(flags,
				OpenFlags::Write | OpenFlags::Append | OpenFlags::Create | OpenFlags::Truncate
						| OpenFlags::CreateExclusive | OpenFlags::DelOnClose)) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return nullptr;
	}

	if (path.starts_with(loc.path)) {
		path += loc.path.size();
	}
	path.skipChars<StringView::Chars<'/'>>();

	int mode = AASSET_MODE_UNKNOWN;
	if (hasFlag(flags, OpenFlags::ReadHintRandom)) {
		mode = AASSET_MODE_RANDOM;
	}
	if (hasFlag(flags, OpenFlags::ReadHintStreaming)) {
		mode = AASSET_MODE_STREAMING;
	}
	if (hasFlag(flags, OpenFlags::ReadHintOneshotBuffer)) {
		mode = AASSET_MODE_BUFFER;
	}

	auto app = jni::Env::getApp();
	void *result = nullptr;
	path.performWithTerminated([&](const char *path, size_t size) {
		auto asset = AAssetManager_open(app->nAssetManager, path, mode);
		if (asset) {
			result = asset;
		}
	});

	if (!result) {
		if (st) {
			*st = Status::ErrorNotFound;
		}
	}
	return result;
},
	._read = [](void *ptr, uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	if (!ptr) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}
	auto result = AAsset_read((AAsset *)ptr, buf, nbytes);
	if (result < 0) {
		if (st) {
			*st = status::errnoToStatus(__sprt_errno);
		}
		return 0;
	}
	if (st) {
		*st = Status::Ok;
	}
	return size_t(result);
},
	._write = [](void *ptr, const uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._seek = [](void *ptr, int64_t offset, int w, Status *st) -> size_t {
	if (!ptr) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}
	auto ret = AAsset_seek64((AAsset *)ptr, offset, w);
	if (ret == -1) {
		if (st) {
			*st = status::errnoToStatus(__sprt_errno);
		}
		return 0;
	}
	if (st) {
		*st = Status::Ok;
	}
	return size_t(ret);
},
	._tell = [](void *ptr, Status *st) -> size_t {
	if (!ptr) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return 0;
	}

	auto ret = AAsset_seek64((AAsset *)ptr, 0, SEEK_CUR);
	if (ret == -1) {
		if (st) {
			*st = status::errnoToStatus(__sprt_errno);
		}
		return 0;
	}
	if (st) {
		*st = Status::Ok;
	}
	return size_t(ret);
},
	._flush =
			[](void *ptr, Status *st) {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return;
},
	._close =
			[](void *ptr, Status *st) {
	if (!ptr) {
		if (st) {
			*st = Status::ErrorInvalidArguemnt;
		}
		return;
	}
	AAsset_close((AAsset *)ptr);
	if (st) {
		*st = Status::Ok;
	}
},

	._unlink = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._remove = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._touch = [](const LocationInfo &loc, StringView path) -> Status {
	return Status::ErrorNotImplemented; //
},

	._mkdir = [](const LocationInfo &loc, StringView path, __sprt_mode_t mode) -> Status {
	return Status::ErrorNotImplemented; //
},

	._rename = [](const LocationInfo &loc1, StringView from, const LocationInfo &loc2,
					   StringView to) -> Status {
	return Status::ErrorNotImplemented; //
},
	/*
	._copy = [](const LocationInfo &, StringView from, const LocationInfo &,
					 StringView to) -> Status {},
					 */
	._copy = nullptr,

	._ftw = [](const LocationInfo &loc, StringView path,
					const callback<bool(StringView, FileType)> &cb, int depth,
					bool dirFirst) -> Status {
	return Status::ErrorNotImplemented; //
},

	._write_oneshot = [](const LocationInfo &loc, StringView path, const uint8_t *buf,
							  size_t nbytes, __sprt_mode_t mode, bool override) -> Status {
	return Status::ErrorNotImplemented; //
},

	._mmap = [](uint8_t storage[16], const LocationInfo &loc, StringView path, MappingType type,
					 __sprt_mode_t prot, size_t offset, size_t len, Status *st) -> uint8_t * {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return (uint8_t *)__SPRT_MAP_FAILED;
},
	._munmap = [](uint8_t *region, uint8_t storage[16]) -> Status {
	return Status::ErrorNotImplemented;
},
	._msync = [](uint8_t *region, uint8_t storage[16]) -> Status {
	return Status::ErrorNotImplemented;
},


	._fdopen = [](void *ptr, const char *mode, Status *st) -> __sprt_FILE * { return nullptr; }};

static constexpr auto MEDIA_MOUNTED = "mounted";
static constexpr auto MEDIA_MOUNTED_READ_ONLY = "mounted_ro";

PathInfo *PathInfo::getInstance() {
	static qmutex s_mutex;
	static PathInfo *s_paths = nullptr;
	unique_lock lock(s_mutex);
	if (!s_paths) {
		s_paths = new PathInfo;
	}
	return s_paths;
}

PathInfo::PathInfo() { }

Access PathInfo::getExternalStorageState() {
	auto env = sprt::jni::Env::getEnv();
	auto envClass = env.findClass("android/os/Environment");
	if (envClass) {
		auto getExternalStorageStateMethod =
				envClass.getStaticMethodID("getExternalStorageState", "()Ljava/lang/String;");
		auto str = envClass.callStaticMethod<jstring>(getExternalStorageStateMethod);
		if (str) {
			auto v = StringView(str.getString());
			if (v == MEDIA_MOUNTED) {
				return Access::Read | Access::Write | Access::Exists;
			} else if (v == MEDIA_MOUNTED_READ_ONLY) {
				return Access::Read | Access::Exists;
			}
		}
	}
	return Access::None;
}

bool PathInfo::initialize(sprt::jni::App *app, const sprt::jni::Ref &ctx, StringView apkPath) {
	apkPath.performWithTerminated([&](const char *path, size_t) {
		if (::stat(path, &_apkStat) == 0) {
			_apkPath = apkPath.str<String>();
		}
	});

	auto contextClass = ctx.getClass();

	auto filesDir = app->Application.getFilesDir(ctx);
	auto cacheDir = app->Application.getCacheDir(ctx);

	auto externalFilesDir = app->Application.getExternalFilesDir(ctx, nullptr);
	auto externalCacheDir = app->Application.getExternalCacheDir(ctx);

	if (filesDir) {
		_filesDir = StringView(app->File.getAbsolutePath(filesDir).getString()).str<String>();
	}

	if (cacheDir) {
		_cacheDir = StringView(app->File.getAbsolutePath(cacheDir).getString()).str<String>();
	}

	if (externalFilesDir) {
		_externalFilesDir =
				StringView(app->File.getAbsolutePath(externalFilesDir).getString()).str<String>();
	}

	if (externalCacheDir) {
		_externalCacheDir =
				StringView(app->File.getAbsolutePath(externalCacheDir).getString()).str<String>();
	}

	unique_lock lock(_mutex);

	_assetManager = app->nAssetManager;
	_documentsInit = false;
	_cacheInit = false;
	return true;
}

void PathInfo::initSystemPaths(LookupData &data) {
	auto app = sprt::jni::Env::getApp();
	auto env = sprt::jni::Env::getEnv();

	auto thiz = sprt::jni::Ref(app->jApplication, env);

	initialize(app, thiz, app->classLoader.getApkPath());

	auto &resBundled = data._resourceLocations[toInt(LocationCategory::Bundled)];

	resBundled.paths.emplace_back(LocationInfo{
		_apkPath,
		LookupFlags::Private,
		LocationFlags::Specific,
		&s_apkInterface,
	});
	resBundled.init = false;

	auto externalState = getExternalStorageState();
	auto externalFlags = LocationFlags::Locateable | LocationFlags::Unpluggable;
	auto externalFileFlags = LookupFlags::Public;

	if (hasFlag(externalState, Access::Write)) {
		externalFileFlags |= LookupFlags::Writable;
		externalFlags |= LocationFlags::Writable;
	}

	if (hasFlag(externalState, Access::Read)) {
		if (!_externalFilesDir.empty()) {
			filepath::merge([&](StringView mergadPath) {
				auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppConfig)];
				resConfig.paths.emplace_back(LocationInfo{
					StringView(mergadPath).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
				resConfig.init = false;
			}, _externalFilesDir, "config");

			filepath::merge([&](StringView configPath) {
				auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppData)];
				resConfig.paths.emplace_back(LocationInfo{
					StringView(configPath).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
				resConfig.init = false;
			}, _externalFilesDir, "data");

			filepath::merge([&](StringView configPath) {
				auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppState)];
				resConfig.paths.emplace_back(LocationInfo{
					StringView(configPath).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
				resConfig.init = false;
			}, _externalFilesDir, "state");
		}

		if (!_externalCacheDir.empty()) {
			filepath::merge([&](StringView configPath) {
				auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppCache)];
				resConfig.paths.emplace_back(LocationInfo{
					StringView(configPath).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
				resConfig.init = false;
			}, _externalCacheDir, "cache");

			filepath::merge([&](StringView configPath) {
				auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppRuntime)];
				resConfig.paths.emplace_back(LocationInfo{
					StringView(configPath).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
				resConfig.init = false;
			}, _externalCacheDir, "runtime");
		}
	}

	if (!_filesDir.empty()) {
		auto locationFlags = LocationFlags::Locateable;
		auto lookupFlags = LookupFlags::Writable | LookupFlags::Private;

		filepath::merge([&](StringView mergadPath) {
			auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppConfig)];
			resConfig.paths.emplace_back(LocationInfo{
				StringView(mergadPath).pdup(data._pool),
				lookupFlags,
				locationFlags,
				getDefaultInterface(),
			});
			resConfig.init = false;
		}, _filesDir, "config");


		filepath::merge([&](StringView mergadPath) {
			auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppData)];
			resConfig.paths.emplace_back(LocationInfo{
				StringView(mergadPath).pdup(data._pool),
				lookupFlags,
				locationFlags,
				getDefaultInterface(),
			});
			resConfig.init = false;
		}, _filesDir, "data");

		filepath::merge([&](StringView mergadPath) {
			auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppState)];
			resConfig.paths.emplace_back(LocationInfo{
				StringView(mergadPath).pdup(data._pool),
				lookupFlags,
				locationFlags,
				getDefaultInterface(),
			});
			resConfig.init = false;
		}, _filesDir, "state");
	}

	if (!_cacheDir.empty()) {
		auto locationFlags = LocationFlags::Locateable;
		auto lookupFlags = LookupFlags::Writable | LookupFlags::Private;

		filepath::merge([&](StringView mergadPath) {
			auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppCache)];
			resConfig.paths.emplace_back(LocationInfo{
				StringView(mergadPath).pdup(data._pool),
				lookupFlags,
				locationFlags,
				getDefaultInterface(),
			});
			resConfig.init = false;
		}, _cacheDir, "cache");

		filepath::merge([&](StringView mergadPath) {
			auto &resConfig = data._resourceLocations[toInt(LocationCategory::AppRuntime)];
			resConfig.paths.emplace_back(LocationInfo{
				StringView(mergadPath).pdup(data._pool),
				lookupFlags,
				locationFlags,
				getDefaultInterface(),
			});
			resConfig.init = false;
		}, _cacheDir, "runtime");
	}

	if (!hasFlag(externalState, Access::Read)) {
		return;
	}


	auto &resHomeConfig = data._resourceLocations[toInt(LocationCategory::UserHome)];

	resHomeConfig.paths.emplace_back(LocationInfo{
		StringView(_filesDir).pdup(data._pool),
		LookupFlags::Private | LookupFlags::Writable,
		externalFlags,
		getDefaultInterface(),
	});

	auto envClass = app->Environment.getClass().ref(env);

	auto storageDir = app->Environment.getExternalStorageDirectory(envClass);
	if (storageDir) {
		auto path = app->File.getAbsolutePath(storageDir);
		if (path) {
			resHomeConfig.paths.emplace_back(LocationInfo{
				StringView(path.getString()).pdup(data._pool),
				LookupFlags::Shared | LookupFlags::Writable,
				externalFlags,
				getDefaultInterface(),
			});
		}
	}

	resHomeConfig.init = false;

	auto DIRECTORY_DOWNLOADS = envClass.getStaticField<jstring>("DIRECTORY_DOWNLOADS");
	auto DIRECTORY_DOCUMENTS = envClass.getStaticField<jstring>("DIRECTORY_DOCUMENTS");
	auto DIRECTORY_MUSIC = envClass.getStaticField<jstring>("DIRECTORY_MUSIC");
	auto DIRECTORY_PICTURES = envClass.getStaticField<jstring>("DIRECTORY_PICTURES");
	auto DIRECTORY_MOVIES = envClass.getStaticField<jstring>("DIRECTORY_MOVIES");

	auto context = app->jApplication.ref(env);

	auto updatePath = [&](sprt::jni::LocalString &str, LocationCategory cat) {
		auto &res = data._resourceLocations[toInt(cat)];
		auto _public = app->Application.getExternalFilesDir(context, str);
		if (_public) {
			auto path = app->File.getAbsolutePath(_public);
			if (path) {
				res.paths.emplace_back(LocationInfo{
					StringView(path.getString()).pdup(data._pool),
					externalFileFlags,
					externalFlags,
					getDefaultInterface(),
				});
			}
		}

		auto shared = app->Environment.getExternalStoragePublicDirectory(envClass, str);
		if (shared) {
			auto path = app->File.getAbsolutePath(shared);
			if (path) {
				res.paths.emplace_back(LocationInfo{
					StringView(path.getString()).pdup(data._pool),
					LookupFlags::Shared,
					externalFlags,
					getDefaultInterface(),
				});
			}
		}

		if (!res.paths.empty()) {
			res.init = false;
		}
	};

	updatePath(DIRECTORY_PICTURES, LocationCategory::UserPictures);
	updatePath(DIRECTORY_MUSIC, LocationCategory::UserMusic);
	updatePath(DIRECTORY_DOCUMENTS, LocationCategory::UserDocuments);
	updatePath(DIRECTORY_DOWNLOADS, LocationCategory::UserDownload);
	updatePath(DIRECTORY_MOVIES, LocationCategory::UserVideos);
}

void PathInfo::terminate() { }

void PathInfo::termSystemPaths(LookupData &data) {
	unique_lock lock(_mutex);
	_assetManager = nullptr;
}

StringView PathInfo::getApplicationPath() const { return _apkPath; }

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	auto e = ::getenv(key.data());
	if (e) {
		return StringView(e).pdup(pool);
	}
	return StringView();
}

void _initSystemPaths(LookupData &data) { PathInfo::getInstance()->initSystemPaths(data); }

void _termSystemPaths(LookupData &data) { PathInfo::getInstance()->termSystemPaths(data); }

} // namespace sprt::filesystem::detail
