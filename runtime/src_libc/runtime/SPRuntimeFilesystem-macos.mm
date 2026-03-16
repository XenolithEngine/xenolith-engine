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

#define __SPRT_BUILD 1

#include <sprt/runtime/platform.h>

#if SPRT_MACOS

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/mem/vector.h>
#include <sprt/runtime/enum.h>
#include <sprt/c/__sprt_unistd.h>
#include "private/SPRTFilesystem.h"

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(rsize_t) rsize_t;

#define NULL __SPRT_NULL

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wavailability"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <mach-o/dyld.h>

namespace sprt::filesystem::detail {

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	} else {
		StringView ret;
		key.performWithTerminated([&](const char *tkey, size_t) {
			auto e = ::getenv(tkey);
			if (e) {
				ret = StringView(e).pdup(pool);
			}
		});
		return ret;
	}
	return StringView();
}

static bool s_isBundled = false;
static bool s_isSandboxed = false;

void _initSystemPaths(LookupData &data) {
	auto homePath = platform::getHomePath();
	auto execPath = platform::getExecPath();
	auto defaultInterface = getDefaultInterface();
	if (homePath.empty()) {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "filesystem", "$HOME is not defined");
		return;
	}

	auto &appConfig = getAppConfig();

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

	if (appConfig.pathScheme > AppLocationScheme::ExecutableRelative) {
		data._appPathCommon = true;
	}

	auto pathEnv = ::getenv("PATH");
	if (pathEnv) {
		auto &res = data._resourceLocations[toInt(LocationCategory::Exec)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(LocationInfo{
				value.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		});
		res.init = true;
	}

	auto ldPathEnv = ::getenv("LD_LIBRARY_PATH");
	if (ldPathEnv) {
		auto &res = data._resourceLocations[toInt(LocationCategory::Library)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(LocationInfo{
				value.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		});
		res.init = true;
	}

	if (!homePath.empty()) {
		data._resourceLocations[toInt(LocationCategory::UserHome)].paths.emplace_back(LocationInfo{
			homePath.pdup(data._pool),
			LookupFlags::Shared,
			LocationFlags::Locateable,
			defaultInterface,
		});
		data._resourceLocations[toInt(LocationCategory::UserHome)].init = true;
	}

	auto fm = [NSFileManager defaultManager];

	auto updateDirs = [&](NSSearchPathDirectory nsdir, LocationCategory cat) {
		auto &res = data._resourceLocations[toInt(cat)];
		auto dirs = NSSearchPathForDirectoriesInDomains(nsdir, NSUserDomainMask, YES);

		if (dirs.count > 0) {
			for (NSString *dir in dirs) {
				res.init = true;
				res.paths.emplace_back(LocationInfo{
					StringView(dir.UTF8String).pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable
							| (([fm isWritableFileAtPath:dir]) ? LocationFlags::Writable
															   : LocationFlags::None),
					defaultInterface,
				});
			}
		}
	};

	updateDirs(NSDownloadsDirectory, LocationCategory::UserDownload);
	updateDirs(NSMoviesDirectory, LocationCategory::UserVideos);
	updateDirs(NSMusicDirectory, LocationCategory::UserMusic);
	updateDirs(NSPicturesDirectory, LocationCategory::UserPictures);
	updateDirs(NSDocumentDirectory, LocationCategory::UserDocuments);
	updateDirs(NSDesktopDirectory, LocationCategory::UserDesktop);

	if (!s_isSandboxed) {
		auto updateDataDirs = [&](NSSearchPathDirectory nsdir, LocationCategory common,
									  LocationCategory app) {
			auto &commonRes = data._resourceLocations[toInt(common)];
			auto &appRes = data._resourceLocations[toInt(app)];
			auto dirs = NSSearchPathForDirectoriesInDomains(nsdir, NSUserDomainMask, YES);

			if (dirs.count > 0) {
				commonRes.init = true;

				for (NSString *dir in dirs) {
					auto commonPath = StringView(dir.UTF8String).pdup(data._pool);
					auto isWritable = [fm isWritableFileAtPath:dir];
					commonRes.paths.emplace_back(LocationInfo{
						dir.UTF8String,
						LookupFlags::Shared,
						LocationFlags::Locateable
								| (isWritable ? LocationFlags::Writable : LocationFlags::None),
						defaultInterface,
					});

					filepath::merge([&](StringView appPath) {
						appRes.paths.emplace_back(LocationInfo{
							appPath.pdup(data._pool),
							LookupFlags::Public | LookupFlags::Writable,
							LocationFlags::Locateable
									| (isWritable ? LocationFlags::Writable : LocationFlags::None),
							defaultInterface,
						});
					}, commonPath, appConfig.bundleName);
				}
			}
		};

		updateDataDirs(NSApplicationSupportDirectory, LocationCategory::CommonData,
				LocationCategory::AppData);
		updateDataDirs(NSCachesDirectory, LocationCategory::CommonCache,
				LocationCategory::AppCache);
		updateDataDirs(NSAutosavedInformationDirectory, LocationCategory::CommonState,
				LocationCategory::AppState);

		auto allLibsDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
				NSUserDomainMask | NSSystemDomainMask | NSLocalDomainMask, YES);
		if (allLibsDirs.count > 0) {
			auto &commonRes = data._resourceLocations[toInt(LocationCategory::CommonConfig)];

			commonRes.init = true;

			for (NSString *dir in allLibsDirs) {
				auto commonPath = StringView(dir.UTF8String);
				filepath::merge([&](StringView fontPath) {
					if (::__sprt_access(fontPath.data(), __SPRT_F_OK) == 0) {
						auto &fontsRes = data._resourceLocations[toInt(LocationCategory::Fonts)];
						fontsRes.init = true;

						fontsRes.paths.emplace_back(LocationInfo{
							fontPath.pdup(data._pool),
							LookupFlags::Shared,
							LocationFlags::Locateable,
							defaultInterface,
						});
					}
				}, commonPath, "Fonts");

				commonRes.paths.emplace_back(LocationInfo{
					commonPath.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}
		}

		auto tmpDir = NSTemporaryDirectory();
		if (tmpDir) {
			auto isWritable = [fm isWritableFileAtPath:tmpDir];
			auto commonPath = StringView(tmpDir.UTF8String).pdup(data._pool);

			auto &commonRuntime = data._resourceLocations[toInt(LocationCategory::CommonRuntime)];

			commonRuntime.paths.emplace_back(LocationInfo{
				commonPath.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable
						| (isWritable ? LocationFlags::Writable : LocationFlags::None),
				defaultInterface,
			});

			filepath::merge([&](StringView appRuntimePath) {
				auto &appRuntime = data._resourceLocations[toInt(LocationCategory::AppRuntime)];

				appRuntime.paths.emplace_back(LocationInfo{
					appRuntimePath.pdup(data._pool),
					LookupFlags::Public | LookupFlags::Writable,
					LocationFlags::Locateable
							| (isWritable ? LocationFlags::Writable : LocationFlags::None),
					defaultInterface,
				});
			}, commonPath, "Fonts");

			auto libDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
					NSUserDomainMask, YES);
			if (libDirs.count > 0) {
				for (NSString *dir in libDirs) {
					filepath::merge([&](StringView appConfigPath) {
						auto &appConfigRes =
								data._resourceLocations[toInt(LocationCategory::AppConfig)];

						appConfigRes.paths.emplace_back(LocationInfo{
							appConfigPath.pdup(data._pool),
							LookupFlags::Public | LookupFlags::Writable,
							LocationFlags::Locateable | LocationFlags::Writable,
							defaultInterface,
						});
					}, dir.UTF8String, appConfig.bundleName);
				}
			}
		} else {
			auto libDirs = NSSearchPathForDirectoriesInDomains(NSAllLibrariesDirectory,
					NSUserDomainMask, YES);
			if (libDirs.count > 0) {
				auto &appConfigRes = data._resourceLocations[toInt(LocationCategory::AppConfig)];
				auto &commonRuntimeRes =
						data._resourceLocations[toInt(LocationCategory::CommonRuntime)];
				auto &appRuntimeRes = data._resourceLocations[toInt(LocationCategory::AppRuntime)];

				for (NSString *dir in libDirs) {
					filepath::merge([&](StringView runtimePath) {
						commonRuntimeRes.paths.emplace_back(LocationInfo{
							runtimePath.pdup(data._pool),
							LookupFlags::Shared | LookupFlags::Writable,
							LocationFlags::Locateable | LocationFlags::Writable,
							defaultInterface,
						});

						filepath::merge([&](StringView appRuntimePath) {
							appRuntimeRes.paths.emplace_back(LocationInfo{
								appRuntimePath.pdup(data._pool),
								LookupFlags::Shared | LookupFlags::Writable,
								LocationFlags::Locateable | LocationFlags::Writable,
								defaultInterface,
							});
						}, runtimePath, appConfig.bundleName);
					}, dir.UTF8String, "Runtime");

					filepath::merge([&](StringView appConfigPath) {
						appConfigRes.paths.emplace_back(LocationInfo{
							appConfigPath.pdup(data._pool),
							LookupFlags::Shared | LookupFlags::Writable,
							LocationFlags::Locateable | LocationFlags::Writable,
							defaultInterface,
						});
					}, dir.UTF8String, appConfig.bundleName);
				}
			}
		}
	} else {
		// @TODO
	}

	if (!s_isBundled) {
		auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

		bundledLoc.init = true;

		if (!appConfig.bundlePath.empty()) {
			StringView(appConfig.bundlePath).split<StringView::Chars<':'>>([&](StringView str) {
				auto value = readVariable(data._pool, str);
				if (!value.empty()) {
					bundledLoc.paths.emplace_back(LocationInfo{
						value,
						LookupFlags::Private,
						LocationFlags::Locateable,
						defaultInterface,
					});
				}
			});
		}
	} else {
		auto resPath = [NSBundle mainBundle].resourcePath;
		if (resPath) {
			auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

			bundledLoc.init = true;
			bundledLoc.paths.emplace_back(LocationInfo{
				StringView(resPath.UTF8String).pdup(data._pool),
				LookupFlags::Private,
				LocationFlags::Locateable,
				defaultInterface,
			});
		}
	}
}

void _termSystemPaths(LookupData &) { }

} // namespace sprt::filesystem::detail

#endif
