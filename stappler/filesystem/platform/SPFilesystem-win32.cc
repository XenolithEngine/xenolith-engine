/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPCore.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

#if WIN32

#include "SPString.h"
#include "SPPlatformUnistd.h"
#include "SPMemInterface.h"
#include "SPSharedModule.h"
#include "detail/SPFilesystemResourceData.h"

#include <wil/result_macros.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil\token_helpers.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

static mem_std::String s_containerPath;
static mem_std::String s_appPath;

struct KnownFolderInfo {
	const KNOWNFOLDERID *folder;
	FileCategory category;
	FileFlags flags;
};

static KnownFolderInfo s_defaultKnownFolders[] = {
	KnownFolderInfo{&FOLDERID_AppDataDesktop, FileCategory::UserDesktop, FileFlags::Private},
	KnownFolderInfo{&FOLDERID_Desktop, FileCategory::UserDesktop, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDesktop, FileCategory::UserDesktop, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Pictures, FileCategory::UserPictures, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicPictures, FileCategory::UserPictures, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Videos, FileCategory::UserVideos, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicVideos, FileCategory::UserVideos, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Music, FileCategory::UserMusic, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicMusic, FileCategory::UserMusic, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Downloads, FileCategory::UserDownload, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDownloads, FileCategory::UserDownload, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Documents, FileCategory::UserDocuments, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDocuments, FileCategory::UserDocuments, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Profile, FileCategory::UserHome, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_Public, FileCategory::UserHome, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_Fonts, FileCategory::Fonts, FileFlags::Shared},
	KnownFolderInfo{&FOLDERID_InternetCache, FileCategory::CommonData, FileFlags::Private},
	KnownFolderInfo{&FOLDERID_LocalAppData, FileCategory::CommonData, FileFlags::Private},
	KnownFolderInfo{&FOLDERID_RoamingAppData, FileCategory::CommonData, FileFlags::Public},
	KnownFolderInfo{&FOLDERID_ProgramData, FileCategory::CommonData, FileFlags::Shared},
};

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(StringView(s_appPath)).pdup(pool);
	} else if (key == "CWD") {
		return StringView(currentDir<memory::PoolInterface>()).pdup(pool);
	} else {
		char *buf = nullptr;
		size_t size = 0;
		if (_dupenv_s(&buf, &size, key.data()) == 0) {
			return StringView(buf, size).pdup(pool);
		}
	}
	return StringView();
}

static void processKnownDir(FilesystemResourceData &data, const KnownFolderInfo &info,
		IKnownFolder *dir) {
	KNOWNFOLDER_DEFINITION def;
	dir->GetFolderDefinition(&def);

	wchar_t *pathAppWide = nullptr;

	auto dirFlagsAppWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
			| KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET | KF_FLAG_CREATE;

	dir->GetPath(dirFlagsAppWide, &pathAppWide);
	/*std::cout << string::toUtf8<memory::StandartInterface>(
			WideStringView((const char16_t *)def.pszName))
			  << " ";

	switch (def.category) {
	case KF_CATEGORY_VIRTUAL: std::cout << "KF_CATEGORY_VIRTUAL"; break;
	case KF_CATEGORY_FIXED: std::cout << "KF_CATEGORY_FIXED"; break;
	case KF_CATEGORY_COMMON: std::cout << "KF_CATEGORY_COMMON"; break;
	case KF_CATEGORY_PERUSER: std::cout << "KF_CATEGORY_PERUSER"; break;
	}

	if (def.kfdFlags & KFDF_LOCAL_REDIRECT_ONLY) {
		std::cout << " KFDF_LOCAL_REDIRECT_ONLY";
	}
	if (def.kfdFlags & KFDF_ROAMABLE) {
		std::cout << " KFDF_ROAMABLE";
	}
	if (def.kfdFlags & KFDF_PRECREATE) {
		std::cout << " KFDF_PRECREATE";
	}
	if (def.kfdFlags & KFDF_STREAM) {
		std::cout << " KFDF_STREAM";
	}
	if (def.kfdFlags & KFDF_PUBLISHEXPANDEDPATH) {
		std::cout << " KFDF_PUBLISHEXPANDEDPATH";
	}
	if (def.kfdFlags & KFDF_NO_REDIRECT_UI) {
		std::cout << " KFDF_NO_REDIRECT_UI";
	}*/

	auto uPath = string::toUtf8<memory::StandartInterface>(
			WideStringView((const char16_t *)pathAppWide));
	auto posixPath = filesystem::native::nativeToPosix<memory::StandartInterface>(uPath);

	//std::cout << "\n";
	if (pathAppWide) {
		//std::cout << "\tApp: " << posixPath << "\n";
		CoTaskMemFree(pathAppWide);
	}

	auto &res = data._resourceLocations[toInt(info.category)];
	res.paths.emplace_back(StringView(posixPath).pdup(data._pool), info.flags);
	res.flags = CategoryFlags::Locateable;
	res.init = true;

	//wchar_t *pathSystemWide = nullptr;
	//auto dirFlagsSystemWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
	//		| KF_FLAG_NO_PACKAGE_REDIRECTION | KF_FLAG_DEFAULT_PATH | KF_FLAG_NOT_PARENT_RELATIVE;
	//dir->GetPath(dirFlagsSystemWide, &pathSystemWide);
	//if (pathSystemWide) {
	//	std::cout << "\tSystem: "
	//			  << string::toUtf8<memory::StandartInterface>(
	//						 WideStringView((const char16_t *)pathSystemWide))
	//			  << "\n";
	//	CoTaskMemFree(pathSystemWide);
	//}
}

static void defineAppPathFromCommon(FilesystemResourceData &data, StringView bundleName) {
	// init with CommonData and CommonCache paths
	auto makeLocation = [&](FileCategory cat, StringView root, StringView subname) {
		auto &res = data._resourceLocations[toInt(cat)];
		res.paths.emplace_back(StringView(filepath::merge<memory::StandartInterface>(root, subname))
									   .pdup(data._pool),
				FileFlags::Private | FileFlags::Writable);
		res.flags |= CategoryFlags::Locateable;
	};

	auto commonData = findPath<memory::StandartInterface>(FileCategory::CommonData);
	auto commonCache = findPath<memory::StandartInterface>(FileCategory::CommonCache);
	if (commonCache.empty()) {
		commonCache = commonData;
	}

	makeLocation(FileCategory::AppData, commonData,
			filepath::merge<memory::StandartInterface>(bundleName, "Data"));
	makeLocation(FileCategory::AppConfig, commonData,
			filepath::merge<memory::StandartInterface>(bundleName, "Config"));
	makeLocation(FileCategory::AppState, commonData,
			filepath::merge<memory::StandartInterface>(bundleName, "State"));
	makeLocation(FileCategory::AppCache, commonData,
			filepath::merge<memory::StandartInterface>(bundleName, "Cache"));
	makeLocation(FileCategory::AppRuntime, commonData,
			filepath::merge<memory::StandartInterface>(bundleName, "Runtime"));
}

static mem_std::String getAppContainerPath(PSID sid) {
	mem_std::String ret;
	PWSTR str = nullptr, path = nullptr;
	::ConvertSidToStringSidW(sid, &str);

	if (SUCCEEDED(::GetAppContainerFolderPath(str, &path))) {
		ret = filesystem::native::nativeToPosix<memory::StandartInterface>(
				string::toUtf8<memory::StandartInterface>(WideStringView((const char16_t *)path)));
		::CoTaskMemFree(path);
	}
	::LocalFree(str);
	return ret;
}

void _initSystemPaths(FilesystemResourceData &data) {
	wchar_t fullpath[NTFS_MAX_PATH] = {0};
	GetModuleFileNameW(NULL, fullpath, NTFS_MAX_PATH - 1);

	s_appPath = filesystem::native::nativeToPosix<memory::StandartInterface>(
			string::toUtf8<memory::StandartInterface>((const char16_t *)fullpath));

	auto manager = wil::CoCreateInstance<KnownFolderManager, IKnownFolderManager,
			wil::err_returncode_policy>();
	if (manager) {
		HRESULT hr;
		IKnownFolder *pKnownFolder = nullptr;
		for (auto &it : s_defaultKnownFolders) {
			hr = manager->GetFolder(*it.folder, &pKnownFolder);
			if (SUCCEEDED(hr)) {
				processKnownDir(data, it, pKnownFolder);
				pKnownFolder->Release();
			}
		}
	}

	auto bundleName = getAppconfigBundleName();
	auto bundleNameW = string::toUtf16<memory::StandartInterface>(bundleName);

	int appPathCommon = 0;
	if (auto v = SharedModule::acquireTypedSymbol<int *>(buildconfig::MODULE_APPCONFIG_NAME,
				"APPCONFIG_APP_PATH_COMMON")) {
		appPathCommon = *v;
	}

	auto bundlePath = SharedModule::acquireTypedSymbol<const char *>(
			buildconfig::MODULE_APPCONFIG_NAME, "APPCONFIG_BUNDLE_PATH");

	if (bundlePath) {
		auto &bundledLoc = data._resourceLocations[toInt(FileCategory::Bundled)];

		bundledLoc.init = true;
		bundledLoc.flags |= CategoryFlags::Locateable;

		StringView(bundlePath).split<StringView::Chars<':'>>([&](StringView str) {
			auto value = FilesystemResourceData::readVariable(data._pool, str);
			if (!value.empty()) {
				bundledLoc.paths.emplace_back(value, FileFlags::Private);
			}
		});
	}

	if (appPathCommon == 0) {
		auto rootPath = filepath::root(s_appPath);
		data.initAppPaths(rootPath);
	} else if (appPathCommon == 1) {
		defineAppPathFromCommon(data, bundleName);
	} else if (appPathCommon == 2) {
		// first, determine app container path, then - set paths within it
		PSID containerId = nullptr;

		auto hr = ::DeriveAppContainerSidFromAppContainerName((const wchar_t *)bundleNameW.data(),
				&containerId);
		if (SUCCEEDED_LOG(hr)) {
			s_containerPath = getAppContainerPath(containerId);
		}

		if (containerId) {
			FreeSid(containerId);
		}

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, bundleName);
		}
	} else if (appPathCommon == 3) {
		// CommonData and CommonCache already within container
		DWORD returnLength = 0;
		TOKEN_APPCONTAINER_INFORMATION info = {nullptr};
		::GetTokenInformation(::GetCurrentThreadEffectiveToken(), TokenAppContainerSid, &info,
				sizeof(TOKEN_APPCONTAINER_INFORMATION), &returnLength);
		if (info.TokenAppContainer) {
			s_containerPath = getAppContainerPath(info.TokenAppContainer);
		} else {
			PSID containerId = nullptr;
			auto hr = ::DeriveAppContainerSidFromAppContainerName(
					(const wchar_t *)bundleNameW.data(), &containerId);
			if (SUCCEEDED_LOG(hr)) {
				s_containerPath = getAppContainerPath(containerId);
			}
			if (containerId) {
				FreeSid(containerId);
			}
		}

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, bundleName);
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
	if (s_appPath.empty()) {
		wchar_t fullpath[NTFS_MAX_PATH] = {0};
		GetModuleFileNameW(NULL, fullpath, NTFS_MAX_PATH - 1);

		s_appPath = filesystem::native::nativeToPosix<memory::StandartInterface>(
				string::toUtf8<memory::StandartInterface>((const char16_t *)fullpath));
	}
	return s_appPath;
}

template <>
auto _getApplicationPath<memory::PoolInterface>() -> memory::PoolInterface::StringType {
	if (s_appPath.empty()) {
		wchar_t fullpath[NTFS_MAX_PATH] = {0};
		GetModuleFileNameW(NULL, fullpath, NTFS_MAX_PATH - 1);

		s_appPath = filesystem::native::nativeToPosix<memory::StandartInterface>(
				string::toUtf8<memory::StandartInterface>((const char16_t *)fullpath));
	}
	using Interface = memory::PoolInterface;
	return StringView(s_appPath).str<Interface>();
}

} // namespace stappler::filesystem::platform

#endif
