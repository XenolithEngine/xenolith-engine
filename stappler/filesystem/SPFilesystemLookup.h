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

#ifndef STAPPLER_FILESYSTEM_SPFILESYSTEMLOOKUP_H_
#define STAPPLER_FILESYSTEM_SPFILESYSTEMLOOKUP_H_

#include "SPIO.h" // IWYU pragma: keep
#include "SPLog.h" // IWYU pragma: keep

#include <sprt/runtime/filesystem/lookup.h>

namespace STAPPLER_VERSIONIZED stappler {

using sprt::filesystem::FileType;
using sprt::filesystem::LookupFlags;
using sprt::filesystem::LookupInfo;
using sprt::filesystem::LocationCategory;
using sprt::filesystem::LocationFlags;
using sprt::filesystem::LocationInfo;

using FileFlags = sprt::filesystem::LookupFlags;
using FileCategory = sprt::filesystem::LocationCategory;

struct SP_PUBLIC FileInfo {
	// From most common to most concrete
	StringView path;
	LocationCategory category = LocationCategory::Custom;
	FileFlags flags = FileFlags::None;

	explicit FileInfo(StringView);
	FileInfo(StringView, LocationCategory);
	FileInfo(StringView, LocationCategory, FileFlags);

	bool operator==(const FileInfo &) const = default;
	auto operator<=>(const FileInfo &) const = default;
};

SP_PUBLIC std::ostream &operator<<(std::ostream &, const FileInfo &);

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::filesystem {

using sprt::filesystem::OpenFlags;
using sprt::filesystem::Access;
using sprt::filesystem::LocationInterface;

enum class ProtFlags : uint16_t {
	None = 0,
	UserSetId = 0x8000,
	UserRead = 0x0400,
	UserWrite = 0x0200,
	UserExecute = 0x0100,
	GroupSetId = 0x4000,
	GroupRead = 0x0040,
	GroupWrite = 0x0020,
	GroupExecute = 0x0010,
	AllRead = 0x0004,
	AllWrite = 0x0002,
	AllExecute = 0x0001,

	// Flags for file mapping (others will be ignored)
	MapRead = AllRead,
	MapWrite = AllWrite,
	MapExecute = AllExecute,

	Default = 0x0FFF,
	MkdirDefault =
			UserRead | UserWrite | UserExecute | GroupRead | GroupExecute | AllRead | AllExecute,
	WriteDefault =
			UserRead | UserWrite | UserExecute | GroupRead | GroupExecute | AllRead | AllExecute,
	MapMask = ProtFlags::MapRead | ProtFlags::MapWrite | ProtFlags::MapExecute,
};

SP_DEFINE_ENUM_AS_MASK(ProtFlags)

// Resource paths API
// use this to load/save application resources, instead of direct read/write with custom paths

// Returns most prioritized path to search for a resources of specific types
template <typename Interface>
SP_PUBLIC auto findPath(LocationCategory, LookupFlags = LookupFlags::None) ->
		typename Interface::StringType;

// returns path, from which loadable resource can be read (from application bundle or dedicated resource directory)
template <typename Interface>
SP_PUBLIC auto findPath(StringView path, LocationCategory = LocationCategory::Custom,
		LookupFlags = LookupFlags::None, Access = Access::None) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto findPath(StringView path, LocationCategory cat, Access a) ->
		typename Interface::StringType {
	return findPath<Interface>(path, cat, LookupFlags::None, a);
}

template <typename Interface>
SP_PUBLIC auto findPath(const FileInfo &info, Access a = Access::None) ->
		typename Interface::StringType {
	return findPath<Interface>(info.path, info.category, info.flags, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(LocationCategory cat, LookupFlags flags = LookupFlags::None)
		-> typename Interface::StringType {
	return findPath<Interface>(cat, flags | LookupFlags::Writable);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(StringView path,
		LocationCategory cat = LocationCategory::Custom, LookupFlags flags = LookupFlags::None,
		Access a = Access::None) -> typename Interface::StringType {
	return findPath<Interface>(path, cat, flags | LookupFlags::Writable, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(StringView path, LocationCategory cat, Access a) ->
		typename Interface::StringType {
	return findPath<Interface>(path, cat, LookupFlags::Writable, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(const FileInfo &info, Access a = Access::None) ->
		typename Interface::StringType {
	return findPath<Interface>(info.path, info.category, info.flags | LookupFlags::Writable, a);
}

// enumerate all paths, that will be used to find a resource of specific types
SP_PUBLIC void enumeratePaths(LocationCategory, LookupFlags,
		const Callback<bool(const LocationInfo &, StringView)> &);

SP_PUBLIC void enumeratePaths(LocationCategory, StringView path, LookupFlags, Access,
		const Callback<bool(const LocationInfo &, StringView)> &);

SP_PUBLIC inline void enumeratePaths(LocationCategory t,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(t, LookupFlags::None, cb);
}

SP_PUBLIC inline void enumeratePaths(LocationCategory t, StringView path, LookupFlags flags,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(t, path, flags, Access::None, cb);
}

SP_PUBLIC inline void enumeratePaths(LocationCategory t, StringView path, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(t, path, LookupFlags::None, a, cb);
}

SP_PUBLIC inline void enumeratePaths(const FileInfo &info, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(info.category, info.path, info.flags, a, cb);
}

SP_PUBLIC inline void enumeratePaths(const FileInfo &info,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(info.category, info.path, info.flags, Access::None, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(LocationCategory cat,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(cat, LookupFlags::Writable, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(LocationCategory cat, LookupFlags flags,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(cat, flags | LookupFlags::Writable, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(LocationCategory cat, StringView path, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(cat, path, LookupFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(LocationCategory cat, StringView path,
		LookupFlags flags, const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(cat, path, flags | LookupFlags::Writable, Access::None, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(LocationCategory cat, StringView path,
		LookupFlags flags, Access a, const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(cat, path, flags | LookupFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(const FileInfo &info, Access a,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(info.category, info.path, info.flags | LookupFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(const FileInfo &info,
		const Callback<bool(const LocationInfo &, StringView)> &cb) {
	enumeratePaths(info.category, info.path, info.flags | LookupFlags::Writable, Access::None, cb);
}

struct ReverseLookupInfo {
	LocationCategory cat = LocationCategory::Custom;
	StringView fullPath;
	StringView prefixedPath;
	const LocationInfo *location = nullptr;
};

/*
	Performs a reverse search for information about the location of a file along its path.
	If an absolute path is passed, it tries to determine its potential category by matching the prefix paths of the locations.
	If passed a relative path, checks whether the file exists at a path relative to the location.
	If passed the canonical path of a category, checks whether such a file exists in that category.

	The Access parameter is used to test file access by potential location. In the case of a relative path,
	if Access::None is passed, the standard existence check is performed. In other cases, with
	Access::None the check is not performed.
*/
SP_PUBLIC LocationCategory detectResourceCategory(StringView,
		const Callback<void(const ReverseLookupInfo &)> &cb = nullptr, Access = Access::Exists);

SP_PUBLIC FileCategory detectResourceCategory(FileCategory category, StringView ipath,
		FileFlags flags, const Callback<void(const ReverseLookupInfo &)> &cb = nullptr,
		Access = Access::Exists);

/*
	For the Custom category it works the same as the function above.
	For the specified category, acts as the function above with the canonical path passed:
	checks whether a file exists in the specified category and potentially returns information about its location.
*/
SP_PUBLIC inline LocationCategory detectResourceCategory(const FileInfo &info,
		const Callback<void(const ReverseLookupInfo &)> &cb = nullptr,
		Access access = Access::Exists) {
	return detectResourceCategory(info.category, info.path, info.flags, cb, access);
}

template <typename Interface>
SP_PUBLIC inline auto findPath(StringView path, LocationCategory type, LookupFlags flags, Access a)
		-> typename Interface::StringType {
	typename Interface::StringType npath;
	enumeratePaths(path, type, flags, a, [&](const LocationInfo &, StringView p) {
		npath = p.str<Interface>();
		return false;
	});
	return npath;
}

template <typename Interface>
SP_PUBLIC auto findPath(LocationCategory type, LookupFlags flags) ->
		typename Interface::StringType {
	typename Interface::StringType npath;
	enumeratePaths(type, flags, [&](const LocationInfo &, StringView p) {
		npath = p.str<Interface>();
		return false;
	});
	return npath;
}

} // namespace stappler::filesystem

namespace sprt::filesystem {

inline std::ostream &operator<<(std::ostream &stream, LocationCategory cat) {
	switch (cat) {
	case LocationCategory::Exec: stream << "LocationCategory::Exec"; break;
	case LocationCategory::Library: stream << "LocationCategory::Library"; break;
	case LocationCategory::Fonts: stream << "LocationCategory::Fonts"; break;
	case LocationCategory::UserHome: stream << "LocationCategory::UserHome"; break;
	case LocationCategory::UserDesktop: stream << "LocationCategory::UserDesktop"; break;
	case LocationCategory::UserDownload: stream << "LocationCategory::UserDownload"; break;
	case LocationCategory::UserDocuments: stream << "LocationCategory::UserDocuments"; break;
	case LocationCategory::UserMusic: stream << "LocationCategory::UserMusic"; break;
	case LocationCategory::UserPictures: stream << "LocationCategory::UserPictures"; break;
	case LocationCategory::UserVideos: stream << "LocationCategory::UserVideos"; break;
	case LocationCategory::CommonData: stream << "LocationCategory::CommonData"; break;
	case LocationCategory::CommonConfig: stream << "LocationCategory::CommonConfig"; break;
	case LocationCategory::CommonState: stream << "LocationCategory::CommonState"; break;
	case LocationCategory::CommonCache: stream << "LocationCategory::CommonCache"; break;
	case LocationCategory::CommonRuntime: stream << "LocationCategory::CommonRuntime"; break;
	case LocationCategory::AppData: stream << "LocationCategory::AppData"; break;
	case LocationCategory::AppConfig: stream << "LocationCategory::AppConfig"; break;
	case LocationCategory::AppState: stream << "LocationCategory::AppState"; break;
	case LocationCategory::AppCache: stream << "LocationCategory::AppCache"; break;
	case LocationCategory::AppRuntime: stream << "LocationCategory::AppRuntime"; break;
	case LocationCategory::Bundled: stream << "LocationCategory::Bundled"; break;
	case LocationCategory::Custom: stream << "LocationCategory::Custom"; break;
	}
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, FileType type) {
	switch (type) {
	case FileType::File: stream << "FileType::File"; break;
	case FileType::Dir: stream << "FileType::Dir"; break;
	case FileType::BlockDevice: stream << "FileType::BlockDevice"; break;
	case FileType::CharDevice: stream << "FileType::CharDevice"; break;
	case FileType::Pipe: stream << "FileType::Pipe"; break;
	case FileType::Socket: stream << "FileType::Socket"; break;
	case FileType::Link: stream << "FileType::Link"; break;
	case FileType::Unknown: stream << "FileType::Unknown"; break;
	}
	return stream;
}

} // namespace sprt::filesystem

#endif
