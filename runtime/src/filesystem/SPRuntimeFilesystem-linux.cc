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
#include <sprt/runtime/enum.h>
#include "private/SPRTFilesystem.h"
#include <stdlib.h>
#include <unistd.h>

namespace sprt::filesystem::detail {

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	} else if (key == "XDG_DATA_HOME") {
		auto var = ::getenv("XDG_DATA_HOME");
		if (!var || var[0] == 0) {
			StringView ret;
			filepath::merge([&](StringView path) { ret = path.pdup(pool); },
					platform::getHomePath(), ".local/share");
			return ret;
		}
		return StringView(var).pdup(pool);
	} else if (key == "XDG_CONFIG_HOME") {
		auto var = ::getenv("XDG_CONFIG_HOME");
		if (!var || var[0] == 0) {
			StringView ret;
			filepath::merge([&](StringView path) { ret = path.pdup(pool); },
					platform::getHomePath(), ".config");
			return ret;
		}
		return StringView(var).pdup(pool);
	} else if (key == "XDG_STATE_HOME") {
		auto var = ::getenv("XDG_STATE_HOME");
		if (!var || var[0] == 0) {
			StringView ret;
			filepath::merge([&](StringView path) { ret = path.pdup(pool); },
					platform::getHomePath(), ".local/state");
			return ret;
		}
		return StringView(var).pdup(pool);
	} else if (key == "XDG_CACHE_HOME") {
		auto var = ::getenv("XDG_CACHE_HOME");
		if (!var || var[0] == 0) {
			StringView ret;
			filepath::merge([&](StringView path) { ret = path.pdup(pool); },
					platform::getHomePath(), ".cache");
			return ret;
		}
		return StringView(var).pdup(pool);
	} else if (key == "XDG_RUNTIME_DIR") {
		auto var = ::getenv("XDG_RUNTIME_DIR");
		if (!var || var[0] == 0) {
			StringView ret;
			StreamTraits<char>::merge([&](StringView path) { ret = path.pdup(pool); }, "/run/user/",
					geteuid());
			return ret;
		}
		return StringView(var).pdup(pool);
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

void _initSystemPaths(LookupData &data) {
	auto homePath = platform::getHomePath();
	auto exeecPath = platform::getExecPath();
	auto defaultInterface = getDefaultInterface();
	if (homePath.empty()) {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "filesystem", "$HOME is not defined");
		return;
	}

	auto &appConfig = getAppConfig();

	if (toInt(appConfig.pathScheme) > toInt(AppLocationScheme::ExecutableRelative)) {
		data._appPathCommon = true;
	}

	auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

	bundledLoc.init = true;
	if (!appConfig.bundlePath.empty()) {
		appConfig.bundlePath.split<StringView::Chars<':'>>([&](StringView str) {
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

	do {
		auto &res = data._resourceLocations[toInt(LocationCategory::Library)];
		auto ldPathEnv = ::getenv("LD_LIBRARY_PATH");
		if (ldPathEnv) {
			StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
				res.paths.emplace_back(LocationInfo{
					value.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			});
		}
		res.init = true;
	} while (0);

	// search for XDG envvars
	auto dataHome = _readEnvExt(data._pool, "XDG_DATA_HOME");
	if (!dataHome.empty()) {
		auto &res = data._resourceLocations[toInt(LocationCategory::CommonData)];
		res.paths.emplace_back(LocationInfo{
			dataHome,
			LookupFlags::Shared,
			LocationFlags::Locateable,
			defaultInterface,
		});

		auto dataDirs = ::getenv("XDG_DATA_DIRS");
		if (dataDirs && dataDirs[0] == 0) {
			StringView(dataDirs).split<StringView::Chars<':'>>([&](StringView value) {
				res.paths.emplace_back(LocationInfo{
					value.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			});
		} else {
			res.paths.emplace_back(LocationInfo{
				"/usr/local/share",
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
			res.paths.emplace_back(LocationInfo{
				"/usr/share",
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		}
		res.init = true;
	}

	auto configHome = _readEnvExt(data._pool, "XDG_CONFIG_HOME");
	if (!configHome.empty()) {
		auto &res = data._resourceLocations[toInt(LocationCategory::CommonConfig)];
		res.paths.emplace_back(configHome, LookupFlags::Shared);

		auto configDirs = ::getenv("XDG_CONFIG_DIRS");
		if (configDirs) {
			StringView(configDirs).split<StringView::Chars<':'>>([&](StringView value) {
				res.paths.emplace_back(LocationInfo{
					value.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			});
		} else {
			res.paths.emplace_back(LocationInfo{
				"/etc/xdg",
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		}
		res.init = true;
	}

	auto stateHome = _readEnvExt(data._pool, "XDG_STATE_HOME");
	if (!stateHome.empty()) {
		auto &res = data._resourceLocations[toInt(LocationCategory::CommonState)];
		res.paths.emplace_back(LocationInfo{
			stateHome,
			LookupFlags::Shared,
			LocationFlags::Locateable,
			defaultInterface,
		});
		res.init = true;
	}

	auto cacheHome = _readEnvExt(data._pool, "XDG_CACHE_HOME");
	if (!cacheHome.empty()) {
		auto &res = data._resourceLocations[toInt(LocationCategory::CommonCache)];
		res.paths.emplace_back(LocationInfo{
			cacheHome,
			LookupFlags::Shared,
			LocationFlags::Locateable,
			defaultInterface,
		});
		res.init = true;
	}

	auto runtimeDir = _readEnvExt(data._pool, "XDG_RUNTIME_DIR");
	if (!runtimeDir.empty()) {
		auto &res = data._resourceLocations[toInt(LocationCategory::CommonRuntime)];
		res.paths.emplace_back(LocationInfo{
			runtimeDir,
			LookupFlags::Shared,
			LocationFlags::Locateable,
			defaultInterface,
		});
		res.init = true;
	}

	data._resourceLocations[toInt(LocationCategory::UserHome)].paths.emplace_back(LocationInfo{
		homePath,
		LookupFlags::Shared,
		LocationFlags::Locateable,
		defaultInterface,
	});
	data._resourceLocations[toInt(LocationCategory::UserHome)].init = true;

	do {
		auto &dataRes = data._resourceLocations[toInt(LocationCategory::CommonData)];
		auto &res = data._resourceLocations[toInt(LocationCategory::Fonts)];
		for (auto &it : dataRes.paths) {
			filepath::merge([&](StringView path) {
				res.paths.emplace_back(LocationInfo{
					path.pdup(data._pool),
					LookupFlags::Shared,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}, it.path, "fonts");
		}
		res.init = true;
	} while (0);

	bool userConfigFound = false;
	memory::perform_temporary([&](memory::pool_t *tmpPool) {
		memory::vector<uint8_t> filedata;
		for (auto &it : data._resourceLocations[toInt(LocationCategory::CommonConfig)].paths) {
			filepath::merge([&](StringView path) {
				struct stat s;
				if (defaultInterface->_stat(getCurrentLocation(), path, &s) == Status::Ok) {
					filedata.resize(s.st_size);
					if (auto f = defaultInterface->_open(getCurrentLocation(), path,
								OpenFlags::Read, nullptr)) {
						if (defaultInterface->_read(f, filedata.data(), filedata.size(), nullptr)
								!= filedata.size()) {
							filedata.clear();
						} else {
							userConfigFound = true;
						}
						defaultInterface->_close(f, nullptr);
					}
				}
			}, it.path, "user-dirs.dirs");
			if (userConfigFound) {
				break;
			}
		}
		if (!filedata.empty()) {
			StringView strData(BytesView(filedata).toStringView());

			auto writeLocation = [&](LocationCategory t, StringView var) {
				auto &res = data._resourceLocations[toInt(t)];
				if (!var.empty()) {
					res.paths.emplace_back(LocationInfo{
						var,
						LookupFlags::Shared,
						var == homePath ? LocationFlags::None : LocationFlags::Locateable,
						defaultInterface,
					});
				}
			};

			strData.split<StringView::Chars<'\n', '\r'>>([&](StringView str) {
				if (!str.is('#')) {
					auto var = str.readUntil<StringView::Chars<'='>>();
					if (str.is('=')) {
						++str;
						var.trimChars<StringView::WhiteSpace>();
						if (var == "XDG_DESKTOP_DIR") {
							writeLocation(LocationCategory::UserDesktop,
									readVariable(data._pool, str));
						} else if (var == "XDG_DOWNLOAD_DIR") {
							writeLocation(LocationCategory::UserDownload,
									readVariable(data._pool, str));
						} else if (var == "XDG_DOCUMENTS_DIR") {
							writeLocation(LocationCategory::UserDocuments,
									readVariable(data._pool, str));
						} else if (var == "XDG_MUSIC_DIR") {
							writeLocation(LocationCategory::UserMusic,
									readVariable(data._pool, str));
						} else if (var == "XDG_PICTURES_DIR") {
							writeLocation(LocationCategory::UserPictures,
									readVariable(data._pool, str));
						} else if (var == "XDG_VIDEOS_DIR") {
							writeLocation(LocationCategory::UserVideos,
									readVariable(data._pool, str));
						}
					}
				}
			});
			userConfigFound = true;
		}
	});

	for (auto it :
			each<LocationCategory, LocationCategory::UserHome, LocationCategory::UserVideos>()) {
		auto &res = data._resourceLocations[toInt(it)];
		if (res.paths.empty()) {
			res.paths.emplace_back(LocationInfo{
				homePath,
				LookupFlags::Shared,
				LocationFlags::None,
				defaultInterface,
			});
			res.init = true;
		}
	}

	if (!userConfigFound) {
		log::vprint(log::LogType::Warn, __SPRT_LOCATION, "filesystem",
				"XDG defaults (user-dirs.dirs) not found, fallback to home dir");
	}

	if (!appConfig.bundleName.empty() && data._appPathCommon) {
		// create app dirs on XDG locations
		auto off = toInt(LocationCategory::AppData) - toInt(LocationCategory::CommonData);
		for (auto it :
				each<LocationCategory, LocationCategory::AppData, LocationCategory::AppRuntime>()) {
			auto &res = data._resourceLocations[toInt(it)];
			filepath::merge([&](StringView path) {
				res.paths.emplace_back(LocationInfo{
					path.pdup(data._pool),
					LookupFlags::Private | LookupFlags::Public,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}, data._resourceLocations[toInt(it) - off].paths.front().path, appConfig.bundleName);
		}
	} else {
		auto bundlePath = filepath::root(exeecPath);
		data.initAppPaths(bundlePath);
	}
}

void _termSystemPaths(LookupData &) { }

} // namespace sprt::filesystem::detail
