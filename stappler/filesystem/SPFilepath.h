/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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


#ifndef STAPPLER_FILESYSTEM_SPFILEPATH_H_
#define STAPPLER_FILESYSTEM_SPFILEPATH_H_

#include "SPStringView.h" // IWYU pragma: keep
#include "SPSpanView.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler {

// Useful mostly for apps in containers, that have only limited access for host's FS
// Containers can have internal dirs for files and caches (Private), external dirs, owned by app (Public),
// and shared dirs, provided by host (Shared); external and shared dirs often can be read-only
//
// On common desktop, without app sandboxing, all user's dirs are public, but FS module assumes,
// that App<X> and Bundled categories are private, all others are public or shared
//
// For most of the resources, you should use Public storage, as less-expensive one and easiest for a cleanup
// Shared - for externally exported resources, that should not be owned by application
//  - note that in this locations resources can be placed by another applications, be careful
// Private - for app's sensitive data, that should not be read by anything, but the app itself
//
// If no flags from PathMask are provided - platform-default flags for a category can be set
//
// If Writable location was acquired - it will be initialized (created). If the same directory acquired without
// this flag - it can be non-existant.
//
enum class FileFlags : uint32_t {
	None = 0,

	// find locations, that can be writable for the application
	Writable = 1 << 0,

	// find locations, that can be accessible for other applications
	// if this flag is not specified - private directories will be enumerated first
	Public = 1 << 1,

	// find locations, that can not be accessible for other applications
	// if this flag is not specified - public locations will be enumerated after privates
	Private = 1 << 2,

	// find locations, that is shared between multiple apps, and not owned by any of them
	// if this flag is not specified - all owned and shared locations returned
	Shared = 1 << 3,

	PathMask = Public | Private | Shared,

	PrivateFirst = 1 << 4,
	PublicFirst = 2 << 4,
	SharedFirst = 3 << 4,

	OrderMask = SharedFirst,

	// also emits Writable
	MakeWritableDir = 1 << 7,

	OptionsMask = MakeWritableDir,
};

SP_DEFINE_ENUM_AS_MASK(FileFlags)

struct SP_PUBLIC FileInfo {
	// From most common to most concrete
	enum FileCategory {
		Exec, // user or system executable from PATH
		Library, // user or system dynamic shared library

		Fonts,

		// Based on xdg-user-dirs
		// https://www.freedesktop.org/wiki/Software/xdg-user-dirs/
		UserHome,
		UserDesktop,
		UserDownload,
		UserDocuments,
		UserMusic,
		UserPictures,
		UserVideos,

		// Based on XDG Base Directory spec
		// https://specifications.freedesktop.org/basedir-spec/latest/

		// Common<X> is a base dirs, as defined directly by XDG spec or similar OS spec
		// SDK assumes, that this dirs is read-only
		CommonData,
		CommonConfig,
		CommonState,
		CommonCache,
		CommonRuntime,

		// App<X> targets app-specific dir within specific common location
		// It can match Common<X> on some OS and sandboxes
		// SDK assumes, that this dirs is read-write
		AppData,
		AppConfig,
		AppState,
		AppCache,
		AppRuntime,

		Bundled, // some files, bundled with app executable

		Max, // can be absolute or cwd-relative path
		Custom = Max
	};

	StringView path;
	FileCategory category = FileCategory::Custom;
	FileFlags flags = FileFlags::None;

	explicit FileInfo(StringView);
	FileInfo(StringView, FileCategory);
	FileInfo(StringView, FileCategory, FileFlags);

	bool operator==(const FileInfo &) const = default;
	auto operator<=>(const FileInfo &) const = default;
};

using FileCategory = FileInfo::FileCategory;

enum class FileType : uint16_t {
	File,
	Dir,
	BlockDevice,
	CharDevice,
	Pipe,
	Socket,
	Link,
	Unknown
};

SP_PUBLIC std::ostream &operator<<(std::ostream &, FileCategory);
SP_PUBLIC std::ostream &operator<<(std::ostream &, FileType);
SP_PUBLIC std::ostream &operator<<(std::ostream &, const FileInfo &);

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::filesystem {

enum Access : uint32_t {
	None,
	Exists = 1 << 0,
	Read = 1 << 1,
	Write = 1 << 2,
	Execute = 1 << 3,
	AccessMask = Exists | Read | Write | Execute,

	Empty = 1 << 4, // check if path is empty, do not use in with other flags
};

SP_DEFINE_ENUM_AS_MASK(Access)

} // namespace stappler::filesystem


namespace STAPPLER_VERSIONIZED stappler::filepath {

// check if filepath is absolute
SP_PUBLIC bool isAbsolute(StringView path);

SP_PUBLIC bool isCanonical(StringView path);

// check if filepath is in application bundle
SP_PUBLIC bool isBundled(StringView path);

// check if filepath above it's current root
// Like: dir/../..
SP_PUBLIC bool isAboveRoot(StringView path);

SP_PUBLIC bool isEmpty(StringView path);

// check for ".", ".." and double slashes in path
SP_PUBLIC bool validatePath(StringView path);

// remove any ".", ".." and double slashes from path
template <typename Interface>
SP_PUBLIC auto reconstructPath(StringView path) -> typename Interface::StringType;

// encodes path for long-term storage (default application dirs will be replaced with canonical prefix,
// like %CACHE%/dir)
template <typename Interface>
SP_PUBLIC auto canonical(StringView path) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto canonical(const FileInfo &) -> typename Interface::StringType;

// extract root from path by removing last component (/dir/file.tar.bz -> /dir)
SP_PUBLIC StringView root(StringView path);

// extract root from path by removing <levels> last components
SP_PUBLIC StringView root(StringView path, uint32_t levels);

// extract last component (/dir/file.tar.bz -> file.tar.bz)
SP_PUBLIC StringView lastComponent(StringView path);

SP_PUBLIC StringView lastComponent(StringView path, size_t allowedComponents);

// extract full filename extension (/dir/file.tar.gz -> tar.gz)
SP_PUBLIC StringView fullExtension(StringView path);

// extract last filename extension (/dir/file.tar.gz -> gz)
SP_PUBLIC StringView lastExtension(StringView path);

// /dir/file.tar.bz -> file
SP_PUBLIC StringView name(StringView path);

// /dir/file.tar.bz -> 2
SP_PUBLIC size_t extensionCount(StringView path);

template <typename Interface>
SP_PUBLIC auto split(StringView) -> typename Interface::template VectorType<StringView>;

SP_PUBLIC void split(StringView, const Callback<void(StringView)> &);

// merges two path component, removes or adds '/' where needed

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<std::string>);

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<memory::string>);

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<StringView>);

template <class... Args>
SP_PUBLIC auto merge(const Callback<void(StringView)> &, StringView root, Args &&...args);

template <typename Interface>
SP_PUBLIC auto merge(SpanView<std::string>) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto merge(SpanView<memory::string>) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto merge(SpanView<StringView>) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto merge(stappler::memory::StandartInterface::StringType &&str) ->
		typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto merge(stappler::memory::PoolInterface::StringType &&str) ->
		typename Interface::StringType;

template <typename Interface, class... Args>
SP_PUBLIC auto merge(StringView root, StringView path, Args &&...args) ->
		typename Interface::StringType;

// translate some MIME Content-Type to common extensions
SP_PUBLIC StringView extensionForContentType(StringView type);

// replace root path component in filepath
// replace(/my/dir/first/file, /my/dir/first, /your/dir/second)
// [/my/dir/first -> /your/dir/second] /file
// /my/dir/first/file -> /your/dir/second/file
template <typename Interface>
SP_PUBLIC auto replace(StringView path, StringView source, StringView dest) ->
		typename Interface::StringType;

} // namespace stappler::filepath


// Implementation

namespace STAPPLER_VERSIONIZED stappler::filepath {

SP_PUBLIC inline bool isAbsolute(const FileInfo &path) {
	if (path.category == FileCategory::Custom) {
		return isAbsolute(path.path);
	}
	return false;
}

SP_PUBLIC inline bool isAboveRoot(const FileInfo &path) { return isAboveRoot(path.path); }

SP_PUBLIC inline bool isEmpty(const FileInfo &path) { return isEmpty(path.path); }

SP_PUBLIC inline bool validatePath(const FileInfo &path) { return validatePath(path.path); }

SP_PUBLIC inline StringView root(const FileInfo &path) { return root(path.path); }

SP_PUBLIC inline StringView root(const FileInfo &path, uint32_t levels) {
	return root(path.path, levels);
}

SP_PUBLIC inline StringView lastComponent(const FileInfo &path) { return lastComponent(path.path); }

SP_PUBLIC inline StringView lastComponent(const FileInfo &path, size_t allowedComponents) {
	return lastComponent(path.path, allowedComponents);
}

SP_PUBLIC inline StringView fullExtension(const FileInfo &path) { return fullExtension(path.path); }

SP_PUBLIC inline StringView lastExtension(const FileInfo &path) { return lastExtension(path.path); }

SP_PUBLIC inline StringView name(const FileInfo &path) { return name(path.path); }

SP_PUBLIC inline size_t extensionCount(const FileInfo &path) { return extensionCount(path.path); }

template <typename Interface>
SP_PUBLIC auto _merge(StringView root, StringView path) -> typename Interface::StringType;

SP_PUBLIC void _merge(const Callback<void(StringView)> &cb, bool init, StringView root);
SP_PUBLIC void _merge(const Callback<void(StringView)> &cb, StringView root);

template <typename Interface, class... Args>
SP_PUBLIC inline auto merge(StringView root, StringView path, Args &&...args) ->
		typename Interface::StringType {
	return merge<Interface>(_merge<Interface>(root, path), std::forward<Args>(args)...);
}

template <class... Args>
SP_PUBLIC inline auto _merge(const Callback<void(StringView)> &cb, StringView root,
		Args &&...args) {
	_merge(cb, false, root);
	_merge(cb, std::forward<Args>(args)...);
}

template <class... Args>
SP_PUBLIC inline auto merge(const Callback<void(StringView)> &cb, StringView root, Args &&...args) {
	_merge(cb, true, root);
	_merge(cb, std::forward<Args>(args)...);
}

template <typename Interface>
SP_PUBLIC auto reconstructPath(StringView path) -> typename Interface::StringType {
	typename Interface::StringType ret;
	ret.reserve(path.size());
	bool start = (path.front() == '/');
	bool end = (path.back() == '/');

	typename Interface::template VectorType<StringView> retVec;
	StringView r(path);
	while (!r.empty()) {
		auto str = r.readUntil<StringView::Chars<'/'>>();
		if (str == ".." && str.size() == 2) {
			if (!retVec.empty()) {
				retVec.pop_back();
			}
		} else if ((str == "." && str.size() == 1) || str.size() == 0) {
		} else if (!str.empty()) {
			retVec.emplace_back(str);
		}
		if (r.is('/')) {
			++r;
		}
	}

	if (start) {
		ret.push_back('/');
	}

	bool f = false;
	for (auto &it : retVec) {
		if (f) {
			ret.push_back('/');
		} else {
			f = true;
		}
		ret.append(it.data(), it.size());
	}
	if (end) {
		ret.push_back('/');
	}
	return ret;
}

template <typename Interface>
SP_PUBLIC inline auto split(StringView str) -> typename Interface::template VectorType<StringView> {
	typename Interface::template VectorType<StringView> ret;
	StringView s(str);
	do {
		if (s.is('/')) {
			s++;
		}
		auto path = s.readUntil<StringView::Chars<'/', '?', ';', '&', '#'>>();
		ret.push_back(path);
	} while (!s.empty() && s.is('/'));
	return ret;
}

inline void split(StringView str, const Callback<void(StringView)> &cb) {
	StringView s(str);
	do {
		if (s.is('/')) {
			s++;
		}
		auto path = s.readUntil<StringView::Chars<'/', '?', ';', '&', '#'>>();
		cb(path);
	} while (!s.empty() && s.is('/'));
}

template <typename Interface>
SP_PUBLIC inline auto replace(StringView path, StringView source, StringView dest) ->
		typename Interface::StringType {
	if (path.starts_with(source)) {
		if (dest.empty()) {
			return path.sub(source.size()).str<Interface>();
		} else {
			return filepath::merge<Interface>(dest, path.sub(source.size()));
		}
	}
	return path.str<Interface>();
}


} // namespace stappler::filepath

#endif /* STAPPLER_FILESYSTEM_SPFILEPATH_H_ */
