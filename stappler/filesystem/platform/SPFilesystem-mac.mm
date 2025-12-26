/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFilesystem.h"
#include "SPLog.h"

#if MACOS

#include "SPFilesystem.h"
#include "SPMemInterface.h"
#include "SPSharedModule.h"
#include "detail/SPFilesystemResourceData.h"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <mach-o/dyld.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

/*template <>
auto _getApplicationPath<memory::StandartInterface>() -> memory::StandartInterface::StringType {
	char fullpath[PATH_MAX] = {0};
	uint32_t bufsize = PATH_MAX;

	if (_NSGetExecutablePath(fullpath, &bufsize) == 0) {
		return memory::StandartInterface::StringType(fullpath, bufsize);
	}
	return memory::StandartInterface::StringType();
}

template <>
auto _getApplicationPath<memory::PoolInterface>() -> memory::PoolInterface::StringType {
	char fullpath[PATH_MAX] = {0};
	uint32_t bufsize = PATH_MAX;

	if (_NSGetExecutablePath(fullpath, &bufsize) == 0) {
		return memory::PoolInterface::StringType(fullpath, bufsize);
	}
	return memory::PoolInterface::StringType();
}

struct PathSource {
	memory::StandartInterface::StringType _appPath;
	memory::StandartInterface::StringType _platformPath;
	memory::StandartInterface::StringType _cachePath;
	memory::StandartInterface::StringType _documentsPath;
	memory::StandartInterface::StringType _writablePath;

	bool _platformInit = false;
	bool _cacheInit = false;
	bool _documentsInit = false;
	bool _writableInit = false;
	bool _isBundle = false;
	bool _isSandboxed = false;

	static PathSource *getInstance() {
		static PathSource *s_paths = nullptr;
		if (!s_paths) {
			s_paths = new PathSource;
		}
		return s_paths;
	}

	PathSource() {
		CFBundleRef bundle = CFBundleGetMainBundle();
		CFURLRef bundleUrl = CFBundleCopyBundleURL(bundle);

		CFStringRef uti;
		CFURLCopyResourcePropertyForKey(bundleUrl, kCFURLTypeIdentifierKey, &uti, NULL);
		if (uti) {
			if ([UTTypeApplicationBundle conformsToType: [UTType typeWithIdentifier:(__bridge NSString *)uti]]) {
				// within app bundle
				_isBundle = true;
			}
		}

		CFRelease(bundleUrl);

		auto environment = [[NSProcessInfo processInfo] environment];
		if ([environment objectForKey:@"APP_SANDBOX_CONTAINER_ID"] != nil) {
			_isSandboxed = true;
		}

		if (!_isBundle) {
			_appPath = _getApplicationPath<memory::StandartInterface>();
			if (!_appPath.empty()) {
				_writablePath = _platformPath = _appPath.substr(0, _appPath.find_last_of("/")) + "/AppData";
				_documentsPath = _platformPath + "/Documents";
				_cachePath = _platformPath + "/Caches";
			}

			auto newWD = ::getenv("SP_CWD_OVERRIDE");
			if (newWD && ::strlen(newWD) != 0) {
				if (filesystem::native::access_fn(newWD, Access::Exists)) {
					::chdir(newWD);
				}
			}
		} else {
			auto getDirPath = [] (NSSearchPathDirectory dir) -> NSString * {
				auto dirs = NSSearchPathForDirectoriesInDomains(dir, NSUserDomainMask, YES);
				if (dirs && [dirs count] > 0) {
					return [dirs objectAtIndex:0];
				}
				return nil;
			};

			NSString *nsBundlePath = [[NSBundle mainBundle] bundlePath];
			NSString *nsSupportPath = getDirPath(NSApplicationSupportDirectory);
			NSString *nsCachesPath = getDirPath(NSCachesDirectory);
			NSString *nsDocumentsPath = getDirPath(NSDocumentDirectory);

			if (!nsSupportPath || !nsCachesPath || !nsDocumentsPath || !nsBundlePath) {
				::abort();
			}

			_platformPath = nsBundlePath.UTF8String;
			_platformInit = true;
			
			std::string supportPath = nsSupportPath.UTF8String;
			std::string cachesPath = nsCachesPath.UTF8String;
			std::string documentsPath = nsDocumentsPath.UTF8String;
			
			if (!filesystem::exists(supportPath) && !filesystem::mkdir(supportPath)) {
				log::source().error("filesystem", "Fail to create dir: ", supportPath);
				}
			if (!filesystem::exists(cachesPath) && !filesystem::mkdir(cachesPath)) {
				log::source().error("filesystem", "Fail to create dir: ", cachesPath);
				}
			if (!filesystem::exists(documentsPath) && !filesystem::mkdir(documentsPath)) {
				log::source().error("filesystem", "Fail to create dir: ", documentsPath);
				}

			if (!_isSandboxed) {
				auto bundleId = std::string(CFStringGetCStringPtr(CFBundleGetIdentifier(bundle), kCFStringEncodingUTF8));

				supportPath = filepath::merge<memory::StandartInterface>(supportPath, bundleId);
				cachesPath = filepath::merge<memory::StandartInterface>(cachesPath, bundleId);
				documentsPath = filepath::merge<memory::StandartInterface>(documentsPath, bundleId);

				filesystem::mkdir(supportPath);
				filesystem::mkdir(cachesPath);
				filesystem::mkdir(documentsPath);
			}
			
			_writablePath = supportPath;
			_documentsPath = documentsPath; _documentsInit = true;
			_cachePath = cachesPath; _cacheInit = true;
		}
	}
};*/

static char s_execPath[PATH_MAX] = {0};
static char s_homePath[PATH_MAX] = {0};
static bool s_isBundled = false;
static bool s_isSandboxed = false;

static memory::StandartInterface::StringType getEnvExt(StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(StringView((char *)s_execPath)).str<memory::StandartInterface>();
	} else if (key == "CWD") {
		return currentDir<memory::StandartInterface>();
	} else {
		auto e = ::getenv(key.str<memory::StandartInterface>().data());
		if (e) {
			return memory::StandartInterface::StringType(e);
		}
	}
	return memory::StandartInterface::StringType();
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

static void readDoubleQuoted(StringView &str, const Callback<void(StringView)> &writeCb) {
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
				auto env = getEnvExt(v.str<memory::StandartInterface>().data());
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

static StringView readVariable(memory::pool_t *pool, StringView str) {
	memory::StandartInterface::StringType out;

	auto writer = [&](StringView s) { out.append(s.data(), s.size()); };

	Callback<void(StringView)> writeCb(writer);

	str.trimChars<StringView::WhiteSpace>();
	while (!str.empty()) {
		if (str.is('"')) {
			readDoubleQuoted(str, writeCb);
		} else if (str.is('\'')) {
			readSingleQuoted(str, writeCb);
		} else if (str.is('$')) {
			++str;
			auto v =
					str.readUntil<StringView::Chars<'"', '\'', '$', '/'>, StringView::WhiteSpace>();
			if (!v.empty()) {
				// we need null-terminated string
				auto env = getEnvExt(v.str<memory::StandartInterface>().data());
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
}

void _initSystemPaths(FilesystemResourceData &data) {
	auto home = ::getenv("HOME");
	if (!home) {
		log::source().error("filesystem", "HOME envvar is not defined");
		return;
	}

	memcpy(s_homePath, home, strlen(home));

	uint32_t bufsize = PATH_MAX - 1;

	if (_NSGetExecutablePath(s_execPath, &bufsize) != 0) {
		log::source().error("filesystem", "Fail to detect app path");
		return;
	}

	CFBundleRef bundle = CFBundleGetMainBundle();
	CFURLRef bundleUrl = CFBundleCopyBundleURL(bundle);

	CFStringRef uti;
	CFURLCopyResourcePropertyForKey(bundleUrl, kCFURLTypeIdentifierKey, &uti, NULL);
	if (uti) {
		if ([UTTypeApplicationBundle
					conformsToType:[UTType typeWithIdentifier:(__bridge NSString *)uti]]) {
			// within app bundle
			s_isBundled = true;
		}
	}

	CFRelease(bundleUrl);

	auto environment = [[NSProcessInfo processInfo] environment];
	if ([environment objectForKey:@"APP_SANDBOX_CONTAINER_ID"] != nil) {
		s_isSandboxed = true;
	}

	// read appconfig
	auto bundleName = SharedModule::acquireTypedSymbol<const char *>(
			buildconfig::MODULE_APPCONFIG_NAME, "APPCONFIG_BUNDLE_NAME");
	auto bundlePath = SharedModule::acquireTypedSymbol<const char *>(
			buildconfig::MODULE_APPCONFIG_NAME, "APPCONFIG_BUNDLE_PATH");
	auto appPathCommon = SharedModule::acquireTypedSymbol<int *>(buildconfig::MODULE_APPCONFIG_NAME,
			"APPCONFIG_APP_PATH_COMMON");

	if (appPathCommon && *appPathCommon > 0) {
		data._appPathCommon = true;
	}

	auto pathEnv = ::getenv("PATH");
	if (pathEnv) {
		auto &res = data._resourceLocations[toInt(FileCategory::Exec)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(value.pdup(data._pool), FileFlags::Shared);
		});
		res.flags |= CategoryFlags::Locateable;
	}

	if (home) {
		auto &res = data._resourceLocations[toInt(FileCategory::UserHome)];
		res.paths.emplace_back(StringView(home).pdup(data._pool), FileFlags::Shared);
		res.flags |= CategoryFlags::Locateable;
	}

	auto fm = [NSFileManager defaultManager];

	auto checkAccess = [&](NSString *path) {
		if ([fm isWritableFileAtPath:path]) {
			return FileFlags::Shared | FileFlags::Writable;
		} else {
			return FileFlags::Shared;
		}
	};

	auto updateDirs = [&](NSSearchPathDirectory nsdir, FileCategory cat) {
		auto &res = data._resourceLocations[toInt(cat)];
		auto dirs = NSSearchPathForDirectoriesInDomains(nsdir, NSUserDomainMask, YES);

		if (dirs.count > 0) {
			res.flags |= CategoryFlags::Locateable;
			res.init = true;

			for (NSString *dir in dirs) {
				res.paths.emplace_back(StringView(dir.UTF8String).pdup(data._pool),
						checkAccess(dir));
				res.flags |= CategoryFlags::Locateable;
			}
		}
	};

	updateDirs(NSDownloadsDirectory, FileCategory::UserDownload);
	updateDirs(NSMoviesDirectory, FileCategory::UserVideos);
	updateDirs(NSMusicDirectory, FileCategory::UserMusic);
	updateDirs(NSPicturesDirectory, FileCategory::UserPictures);
	updateDirs(NSDocumentDirectory, FileCategory::UserDocuments);
	updateDirs(NSDesktopDirectory, FileCategory::UserDesktop);

	if (!s_isSandboxed) {
		auto updateDataDirs = [&](NSSearchPathDirectory nsdir, FileCategory common,
									  FileCategory app) {
			auto &commonRes = data._resourceLocations[toInt(common)];
			auto &appRes = data._resourceLocations[toInt(app)];
			auto dirs = NSSearchPathForDirectoriesInDomains(nsdir, NSUserDomainMask, YES);

			if (dirs.count > 0) {
				commonRes.flags |= CategoryFlags::Locateable;
				commonRes.init = true;

				appRes.flags |= CategoryFlags::Locateable;

				for (NSString *dir in dirs) {
					auto commonPath = StringView(dir.UTF8String).pdup(data._pool);
					commonRes.paths.emplace_back(commonPath, checkAccess(dir));
					appRes.paths.emplace_back(StringView(filepath::merge<memory::StandartInterface>(
																 commonPath, bundleName))
													  .pdup(data._pool),
							FileFlags::Public | FileFlags::Writable);
				}
			}
		};

		updateDataDirs(NSApplicationSupportDirectory, FileCategory::CommonData,
				FileCategory::AppData);
		updateDataDirs(NSCachesDirectory, FileCategory::CommonCache, FileCategory::AppCache);
		updateDataDirs(NSAutosavedInformationDirectory, FileCategory::CommonState,
				FileCategory::AppState);

		auto allLibsDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
				NSUserDomainMask | NSSystemDomainMask | NSLocalDomainMask, YES);
		if (allLibsDirs.count > 0) {
			auto &commonRes = data._resourceLocations[toInt(FileCategory::CommonConfig)];

			commonRes.flags |= CategoryFlags::Locateable;
			commonRes.init = true;

			for (NSString *dir in allLibsDirs) {
				auto commonPath = StringView(dir.UTF8String).pdup(data._pool);
				auto fontPath = filepath::merge<memory::StandartInterface>(commonPath, "Fonts");

				commonRes.paths.emplace_back(commonPath, checkAccess(dir));

				if (filesystem::exists(FileInfo{fontPath})) {
					auto &fontsRes = data._resourceLocations[toInt(FileCategory::Fonts)];
					fontsRes.flags |= CategoryFlags::Locateable;
					fontsRes.init = true;

					fontsRes.paths.emplace_back(StringView(fontPath).pdup(data._pool),
							FileFlags::Shared);
				}
			}
		}

		auto tmpDir = NSTemporaryDirectory();
		if (tmpDir) {
			auto commonPath = StringView(tmpDir.UTF8String).pdup(data._pool);

			auto &appRuntime = data._resourceLocations[toInt(FileCategory::AppRuntime)];
			auto &commonRuntime = data._resourceLocations[toInt(FileCategory::CommonRuntime)];

			appRuntime.flags |= CategoryFlags::Locateable;
			commonRuntime.flags |= CategoryFlags::Locateable;

			commonRuntime.paths.emplace_back(commonPath, checkAccess(tmpDir));
			appRuntime.paths.emplace_back(
					StringView(filepath::merge<memory::StandartInterface>(commonPath, bundleName))
							.pdup(data._pool),
					FileFlags::Public | FileFlags::Writable);


			auto libDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
					NSUserDomainMask, YES);
			if (libDirs.count > 0) {
				auto &appConfig = data._resourceLocations[toInt(FileCategory::AppConfig)];
				appConfig.flags |= CategoryFlags::Locateable;
				for (NSString *dir in libDirs) {
					auto commonPath = StringView(dir.UTF8String).pdup(data._pool);
					appConfig.paths.emplace_back(
							StringView(filepath::merge<memory::StandartInterface>(commonPath,
											   bundleName))
									.pdup(data._pool),
							FileFlags::Public | FileFlags::Writable);
				}
			}
		} else {
			auto libDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
					NSUserDomainMask, YES);
			if (libDirs.count > 0) {
				auto &appConfig = data._resourceLocations[toInt(FileCategory::AppConfig)];
				auto &commonRuntime = data._resourceLocations[toInt(FileCategory::CommonRuntime)];
				auto &appRuntime = data._resourceLocations[toInt(FileCategory::AppRuntime)];

				appConfig.flags |= CategoryFlags::Locateable;
				appRuntime.flags |= CategoryFlags::Locateable;
				commonRuntime.flags |= CategoryFlags::Locateable;

				for (NSString *dir in libDirs) {
					auto commonPath = StringView(dir.UTF8String).pdup(data._pool);
					auto runtimePath = StringView(
							filepath::merge<memory::StandartInterface>(commonPath, "Runtime"))
											   .pdup(data._pool);

					appConfig.paths.emplace_back(
							StringView(filepath::merge<memory::StandartInterface>(commonPath,
											   bundleName))
									.pdup(data._pool),
							FileFlags::Public | FileFlags::Writable);
					commonRuntime.paths.emplace_back(runtimePath, FileFlags::Shared);
					appRuntime.paths.emplace_back(
							StringView(filepath::merge<memory::StandartInterface>(runtimePath,
											   bundleName))
									.pdup(data._pool),
							FileFlags::Public | FileFlags::Writable);
				}
			}
		}
	} else {
		// @TODO
	}

	if (!s_isBundled) {
		auto &bundledLoc = data._resourceLocations[toInt(FileCategory::Bundled)];

		bundledLoc.init = true;
		bundledLoc.flags |= CategoryFlags::Locateable;

		if (bundlePath) {
			StringView(bundlePath).split<StringView::Chars<':'>>([&](StringView str) {
				auto value = readVariable(data._pool, str);
				if (!value.empty()) {
					bundledLoc.paths.emplace_back(value, FileFlags::Private);
				}
			});
		}
	} else {
		auto resPath = [NSBundle mainBundle].resourcePath;
		if (resPath) {
			auto &bundledLoc = data._resourceLocations[toInt(FileCategory::Bundled)];

			bundledLoc.init = true;
			bundledLoc.flags |= CategoryFlags::Locateable;

			bundledLoc.paths.emplace_back(StringView(resPath.UTF8String).pdup(data._pool),
					FileFlags::Private);
		}
	}
}

// No PlatformSpecific categories defined for now
void _enumerateObjects(const FilesystemResourceData &data, FileCategory, StringView path, FileFlags,
		Access, const Callback<bool(StringView, FileFlags)> &) { }

bool _access(FileCategory cat, StringView path, Access) { return false; }

bool _stat(FileCategory cat, StringView path, Stat &stat) { return false; }

File _openForReading(FileCategory cat, StringView path) { return File(); }

size_t _read(void *, uint8_t *buf, size_t nbytes) { return 0; }
size_t _seek(void *, int64_t offset, io::Seek s) { return maxOf<size_t>(); }
size_t _tell(void *) { return 0; }
bool _eof(void *) { return true; }
void _close(void *) { }

Status _ftw(FileCategory cat, StringView path,
		const Callback<bool(StringView path, FileType t)> &cb, int depth, bool dirFirst) {
	return Status::Declined;
}

template <>
auto _getApplicationPath<memory::StandartInterface>() -> memory::StandartInterface::StringType {
	if (s_execPath[0] == 0) {
		uint32_t bufsize = PATH_MAX - 1;
		_NSGetExecutablePath(s_execPath, &bufsize);
	}
	return s_execPath;
}

template <>
auto _getApplicationPath<memory::PoolInterface>() -> memory::PoolInterface::StringType {
	if (s_execPath[0] == 0) {
		uint32_t bufsize = PATH_MAX - 1;
		_NSGetExecutablePath(s_execPath, &bufsize);
	}
	using Interface = memory::PoolInterface;
	return StringView(s_execPath).str<Interface>();
}

} // namespace stappler::filesystem::platform

#endif
