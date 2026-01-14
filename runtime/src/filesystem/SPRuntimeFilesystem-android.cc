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
#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/enum.h>
#include <sprt/jni/jni.h>

#include <stdlib.h>
#include <unistd.h>

#include "private/SPRTFilesystem.h"
#include <sprt/c/sys/__sprt_mman.h>

namespace sprt::filesystem::detail {

static LocationInterface s_apkInterface = {
	._access = [](const LocationInfo &loc, StringView path, Access mode) -> Status {
	return Status::ErrorNotImplemented; //
},

	._stat = [](const LocationInfo &loc, StringView path,
					 struct __SPRT_STAT_NAME *_stat) -> Status {
	return Status::ErrorNotImplemented; //
},

	._open = [](const LocationInfo &loc, StringView path, OpenFlags flags, Status *st) -> void * {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._read = [](void *ptr, uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._write = [](void *ptr, const uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._seek = [](void *ptr, int64_t offset, int w, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
},
	._tell = [](void *ptr, Status *st) -> size_t {
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return 0;
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
	if (st) {
		*st = Status::ErrorNotImplemented;
	}
	return;
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
	return (uint8_t *)__SPRT_MAP_FAILED; //
},
	._munmap = [](uint8_t *region, uint8_t storage[16]) -> Status {
	return Status::ErrorNotImplemented;
},
	._msync = [](uint8_t *region, uint8_t storage[16]) -> Status {
	return Status::ErrorNotImplemented;
},


	._fdopen = [](void *ptr, const char *mode, Status *st) -> __sprt_FILE * { return nullptr; }};

/*struct ArchiveFile {
	using String = memory::StandartInterface::StringType;

	String name;
	size_t size;
	Time time;
};

struct ArchiveHierarchy {
	using Interface = memory::StandartInterface;
	using String = memory::StandartInterface::StringType;
	using StringStream = memory::StandartInterface::StringStreamType;

	template <typename K, typename V, typename Compare = std::less<>>
	using Map = memory::StandartInterface::MapType<K, V, Compare>;

	template <typename T>
	using Vector = memory::StandartInterface::VectorType<T>;

	ArchiveHierarchy() = default;
	ArchiveHierarchy(String &&path) : originPath(sp::move(path)) { }

	void add(StringView path, size_t size, Time time) {
		if (path.empty()) {
			return;
		}

		StringStream stream;

		ArchiveHierarchy *target = this;
		StringView tmp;
		filepath::split(path, [&](StringView path) {
			if (!tmp.empty()) {
				stream << (target->originPath.empty() ? "" : "/") << tmp;
				target = &target->dirs.emplace(tmp.str<Interface>(), ArchiveHierarchy{stream.str()})
								  .first->second;
			}
			tmp = path;
		});
		if (!tmp.empty()) {
			target->files.emplace_back(ArchiveFile{tmp.str<Interface>(), size, time});
		}
	}

	ArchiveHierarchy *getTarget(StringView path) {
		ArchiveHierarchy *target = this;
		filepath::split(path, [&](StringView path) {
			if (target) {
				auto iit = target->dirs.find(path);
				if (iit != target->dirs.end()) {
					target = &target->dirs.find(path)->second;
				} else {
					target = nullptr;
				}
			}
		});
		return target;
	}

	Status ftw(StringView path, const Callback<bool(StringView path, FileType type)> &cb, int depth,
			bool dirFirst) {
		if (path.empty()) {
			if (dirFirst) {
				if (!cb(originPath, FileType::Dir)) {
					return Status::Suspended;
				}
			}

			if (depth < 0 || depth > 0) {
				auto it = dirs.begin();
				while (it != dirs.end()) {
					if (it->second.ftw(StringView(), cb, depth - 1, dirFirst) != Status::Ok) {
						return Status::Suspended;
					}
					++it;
				}

				auto iit = files.begin();
				while (iit != files.end()) {
					if (!originPath.empty()) {
						if (!cb(string::toString<Interface>(originPath, "/", iit->name),
									FileType::File)) {
							return Status::Suspended;
						}
					} else {
						if (!cb(iit->name, FileType::File)) {
							return Status::Suspended;
						}
					}
					++iit;
				}
			}

			if (!dirFirst) {
				if (!cb(originPath, FileType::Dir)) {
					return Status::Suspended;
				}
			}
			return Status::Ok;
		} else {
			if (auto target = getTarget(path)) {
				return target->ftw(StringView(), cb, depth, dirFirst);
			}
		}
		return Status::Declined;
	}

	void clear() {
		originPath.clear();
		dirs.clear();
		files.clear();
	}

	bool empty() const { return dirs.empty() && files.empty(); }

	bool access(StringView path, Access a) {
		if (hasFlag(a, Access::Execute) || hasFlag(a, Access::Write)) {
			return false;
		}

		auto target = getTarget(filepath::root(path));
		if (!target) {
			return hasFlag(a, Access::Empty);
		}

		auto name = filepath::lastComponent(path);
		do {
			for (auto &it : target->files) {
				if (it.name == name) {
					if (hasFlag(a, Access::Empty)) {
						return false;
					}
					return hasFlag(a, Access::Exists) || hasFlag(a, Access::Read);
				}
			}
		} while (0);

		do {
			auto it = target->dirs.find(name);
			if (it != target->dirs.end()) {
				if (hasFlag(a, Access::Empty) || hasFlag(a, Access::Read)) {
					return false;
				}
				return hasFlag(a, Access::Exists);
			}
		} while (0);

		return hasFlag(a, Access::Empty);
	}

	bool stat(StringView path, Stat &stat) {
		auto target = getTarget(filepath::root(path));
		if (!target) {
			return false;
		}

		auto name = filepath::lastComponent(path);
		do {
			for (auto &it : target->files) {
				if (it.name == name) {
					stat.size = it.size;
					stat.mtime = stat.ctime = stat.atime = it.time;
					stat.type = FileType::File;
					return true;
				}
			}
		} while (0);

		do {
			auto it = target->dirs.find(name);
			if (it != target->dirs.end()) {
				stat.type = FileType::Dir;
				return true;
			}
		} while (0);

		return false;
	}

	String originPath;
	Map<String, ArchiveHierarchy> dirs;
	Vector<ArchiveFile> files;
};*/

static constexpr auto MEDIA_MOUNTED = "mounted";
static constexpr auto MEDIA_MOUNTED_READ_ONLY = "mounted_ro";

struct PathSource {
	using String = memory::dynstring;

	String _apkPath;
	String _filesDir;
	String _cacheDir;
	String _externalFilesDir;
	String _externalCacheDir;

	//ArchiveHierarchy _archive;

	qmutex _mutex;

	AAssetManager *_assetManager = nullptr;

	bool _cacheInit = false;
	bool _documentsInit = false;

	static PathSource *getInstance() {
		static qmutex s_mutex;
		static PathSource *s_paths = nullptr;
		unique_lock lock(s_mutex);
		if (!s_paths) {
			s_paths = new PathSource;
		}
		return s_paths;
	}

	PathSource() { }

	Access getExternalStorageState() {
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

	bool initialize(sprt::jni::App *app, const sprt::jni::Ref &ctx, StringView apkPath) {
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
			_externalFilesDir = StringView(app->File.getAbsolutePath(externalFilesDir).getString())
										.str<String>();
		}

		if (externalCacheDir) {
			_externalCacheDir = StringView(app->File.getAbsolutePath(externalCacheDir).getString())
										.str<String>();
		}

		_apkPath = apkPath.str<String>();

		unique_lock lock(_mutex);
		/*_archive.clear();

		if (!_apkPath.empty()) {
			if (auto f = fopen(_apkPath.data(), "r")) {
				ZipArchive<Interface> a(f, true);
				a.ftw([&](uint64_t, StringView path, size_t size, Time time) {
					_archive.add(path, size, time);
				});
				fclose(f);
			}
		}*/

		_assetManager = app->nAssetManager;
		_documentsInit = false;
		_cacheInit = false;
		return true;
	}

	void initSystemPaths(LookupData &data) {
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

	void terminate() { }

	void termSystemPaths(LookupData &data) {
		unique_lock lock(_mutex);
		_assetManager = nullptr;
	}

	StringView getApplicationPath() const { return _apkPath; }
};

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	auto e = ::getenv(key.data());
	if (e) {
		return StringView(e).pdup(pool);
	}
	return StringView();
}

void _initSystemPaths(LookupData &data) { PathSource::getInstance()->initSystemPaths(data); }

void _termSystemPaths(LookupData &data) { PathSource::getInstance()->termSystemPaths(data); }

} // namespace sprt::filesystem::detail
