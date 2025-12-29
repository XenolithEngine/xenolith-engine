/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPFilepath.h"
#include "SPStringView.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler {

FileInfo::FileInfo(StringView _path) {
	if (_path.is('%')) {
		category = filesystem::detectResourceCategory(_path,
				[&](StringView prefixed, StringView p) { path = p; });
		if (category == FileCategory::Custom) {
			path = _path;
		}
	}
	path = _path;
}

FileInfo::FileInfo(StringView _path, FileCategory cat) : path(_path), category(cat) { }

FileInfo::FileInfo(StringView _path, FileCategory cat, FileFlags f)
: path(_path), category(cat), flags(f) { }

std::ostream &operator<<(std::ostream &stream, FileCategory cat) {
	switch (cat) {
	case FileCategory::Exec: stream << "FileCategory::Exec"; break;
	case FileCategory::Library: stream << "FileCategory::Library"; break;
	case FileCategory::Fonts: stream << "FileCategory::Fonts"; break;
	case FileCategory::UserHome: stream << "FileCategory::UserHome"; break;
	case FileCategory::UserDesktop: stream << "FileCategory::UserDesktop"; break;
	case FileCategory::UserDownload: stream << "FileCategory::UserDownload"; break;
	case FileCategory::UserDocuments: stream << "FileCategory::UserDocuments"; break;
	case FileCategory::UserMusic: stream << "FileCategory::UserMusic"; break;
	case FileCategory::UserPictures: stream << "FileCategory::UserPictures"; break;
	case FileCategory::UserVideos: stream << "FileCategory::UserVideos"; break;
	case FileCategory::CommonData: stream << "FileCategory::CommonData"; break;
	case FileCategory::CommonConfig: stream << "FileCategory::CommonConfig"; break;
	case FileCategory::CommonState: stream << "FileCategory::CommonState"; break;
	case FileCategory::CommonCache: stream << "FileCategory::CommonCache"; break;
	case FileCategory::CommonRuntime: stream << "FileCategory::CommonRuntime"; break;
	case FileCategory::AppData: stream << "FileCategory::AppData"; break;
	case FileCategory::AppConfig: stream << "FileCategory::AppConfig"; break;
	case FileCategory::AppState: stream << "FileCategory::AppState"; break;
	case FileCategory::AppCache: stream << "FileCategory::AppCache"; break;
	case FileCategory::AppRuntime: stream << "FileCategory::AppRuntime"; break;
	case FileCategory::Bundled: stream << "FileCategory::Bundled"; break;
	case FileCategory::Custom: stream << "FileCategory::Custom"; break;
	}
	return stream;
}

std::ostream &operator<<(std::ostream &stream, FileType type) {
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

std::ostream &operator<<(std::ostream &stream, const FileInfo &fileInfo) {
	stream << "FileInfo{\"" << fileInfo.path << "\"," << fileInfo.category << "}";
	return stream;
}

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::filepath {

// check if filepath is absolute
bool isAbsolute(StringView path) {
	if (path.empty()) {
		return true;
	}
	return path[0] == '/';
}

bool isCanonical(StringView path) {
	if (path.empty()) {
		return false;
	}
	return path[0] == '%';
}

// check if filepath is in application bundle
bool isBundled(StringView path) {
	if (path.size() >= "%PLATFORM%:"_len) {
		return path.starts_with("%PLATFORM%:");
	}
	return false;
}

// check if filepath above it's current root
bool isAboveRoot(StringView path) {
	size_t components = 0;
	StringView r(path);
	while (!r.empty()) {
		auto str = r.readUntil<StringView::Chars<'/'>>();
		if (str == ".." && str.size() == 2) {
			if (components == 0) {
				return true;
			}
			--components;
		} else if ((str == "." && str.size() == 1) || str.size() == 0) {
			return false;
		} else {
			++components;
		}
		if (r.is('/')) {
			++r;
		}
	}
	return false;
}

bool isEmpty(StringView path) {
	path.trimChars<StringView::Chars<'/'>>();
	return path.empty();
}

// check for ".", ".." and double slashes in path
bool validatePath(StringView path) {
	StringView r(path);
	if (r.is('/')) {
		++r;
	}
	while (!r.empty()) {
		auto str = r.readUntil<StringView::Chars<'/'>>();
		if ((str == ".." && str.size() == 2) || (str == "." && str.size() == 1)
				|| str.size() == 0) {
			return false;
		}
		if (r.is('/')) {
			++r;
		}
	}
	return true;
}

template <typename Interface>
auto canonical_fn(StringView path) -> typename Interface::StringType {
	if (isEmpty(path)) {
		return typename Interface::StringType();
	}

	if (isAboveRoot(path)) {
		return typename Interface::StringType();
	}

	if (path.front() == '%') {
		return path.str<Interface>();
	}

	typename Interface::StringType reconstructed = reconstructPath<Interface>(path);
	typename Interface::StringType result;

	filesystem::detectResourceCategory(reconstructed,
			[&](StringView ret, StringView n) { result = ret.str<Interface>(); });

	return result;
}

template <typename Interface>
auto canonical_fn(const FileInfo &info) -> typename Interface::StringType {
	if (isEmpty(info.path)) {
		return typename Interface::StringType();
	}

	if (isAboveRoot(info.path)) {
		return typename Interface::StringType();
	}

	if (info.path.front() == '%') {
		return info.path.str<Interface>();
	}

	typename Interface::StringType reconstructed = reconstructPath<Interface>(info.path);
	typename Interface::StringType result;

	if (info.category == FileCategory::Custom) {
		if (isAbsolute(info.path)) {
			result = info.path.str<Interface>();
		} else {
			result = filesystem::currentDir<Interface>(info.path);
		}
	} else {
		filesystem::detectResourceCategory(info,
				[&](StringView ret, StringView n) { result = ret.str<Interface>(); });
	}

	return result;
}

template <>
auto canonical<memory::StandartInterface>(StringView path)
		-> memory::StandartInterface::StringType {
	return canonical_fn<memory::StandartInterface>(path);
}

template <>
auto canonical<memory::PoolInterface>(StringView path) -> memory::PoolInterface::StringType {
	return canonical_fn<memory::PoolInterface>(path);
}

template <>
auto canonical<memory::StandartInterface>(const FileInfo &info)
		-> memory::StandartInterface::StringType {
	return canonical_fn<memory::StandartInterface>(info);
}

template <>
auto canonical<memory::PoolInterface>(const FileInfo &info) -> memory::PoolInterface::StringType {
	return canonical_fn<memory::PoolInterface>(info);
}

StringView root(StringView path) {
	size_t pos = path.rfind('/');
	if (pos == maxOf<size_t>()) {
		return StringView();
	} else {
		if (pos == 0) {
			return StringView("/");
		} else {
			return path.sub(0, pos);
		}
	}
}

StringView root(StringView path, uint32_t levels) {
	while (levels > 0 && !path.empty() && path != StringView("/")) {
		size_t pos = path.rfind('/');
		if (pos == maxOf<size_t>()) {
			return StringView();
		} else {
			if (pos == 0) {
				path = StringView("/");
			} else {
				path = path.sub(0, pos);
			}
		}
		--levels;
	}
	return path;
}

StringView lastComponent(StringView path) {
	size_t pos = path.rfind('/');
	if (pos != maxOf<size_t>()) {
		return path.sub(pos + 1);
	} else {
		return path;
	}
}

StringView lastComponent(StringView path, size_t allowedComponents) {
	if (allowedComponents == 0) {
		return "";
	}
	size_t pos = path.rfind('/');
	allowedComponents--;
	if (pos == 0) {
		pos = maxOf<size_t>();
	}

	while (pos != maxOf<size_t>() && allowedComponents > 0) {
		pos = path.rfind('/', pos - 1);
		allowedComponents--;
		if (pos == 0) {
			pos = maxOf<size_t>();
		}
	}

	if (pos != maxOf<size_t>()) {
		return path.sub(pos + 1);
	} else {
		return path;
	}
}

StringView fullExtension(StringView path) {
	auto cmp = lastComponent(path);

	size_t pos = cmp.find('.');
	if (pos == maxOf<size_t>()) {
		return StringView();
	} else {
		return cmp.sub(pos + 1);
	}
}

StringView lastExtension(StringView path) {
	auto cmp = lastComponent(path);

	size_t pos = cmp.rfind('.');
	if (pos == maxOf<size_t>()) {
		return "";
	} else {
		return cmp.sub(pos + 1);
	}
}

StringView name(StringView path) {
	auto cmp = lastComponent(path);

	size_t pos = cmp.find('.');
	if (pos == maxOf<size_t>()) {
		return cmp;
	} else {
		return cmp.sub(0, pos);
	}
}

template <typename Interface>
auto do_merge(StringView root, StringView path) -> typename Interface::StringType {
	if (path.empty()) {
		return root.str<Interface>();
	}
	return StringView::merge<Interface, '/'>(root, path);
}


template <typename SourceVector>
static size_t getMergeSize(const SourceVector &vec) {
	size_t ret = vec.size();
	for (auto it = vec.begin(); it != vec.end(); it++) { ret += it->size(); }
	return ret;
}

template <typename SourceVector>
static void doMerge(const Callback<void(StringView)> &cb, const SourceVector &vec) {
	auto stripSeps = [](auto str) {
		StringView tmp(str);
		tmp.trimChars<StringView::Chars<'/'>>();
		return tmp;
	};

	bool front = true;
	auto it = vec.begin();
	for (; it != vec.end(); it++) {
		if (*it == "/") {
			front = false;
		}

		StringView tmp(*it);
		tmp.trimChars<StringView::Chars<'/'>>();

		if (tmp.empty()) {
			continue;
		}

		if (front) {
			front = false;
			if (it->front() == '/') {
				cb << '/';
			}
		} else {
			cb << '/';
		}

		cb << stripSeps(*it);
	}
}

template <>
auto _merge<memory::StandartInterface>(StringView root, StringView path)
		-> memory::StandartInterface::StringType {
	return do_merge<memory::StandartInterface>(root, path);
}

template <>
auto _merge<memory::PoolInterface>(StringView root, StringView path)
		-> memory::PoolInterface::StringType {
	return do_merge<memory::PoolInterface>(root, path);
}

void _merge(const Callback<void(StringView)> &cb, bool init, StringView root) {
	if (init) {
		root.backwardSkipChars<StringView::Chars<'/'>>();
		cb << root;
	} else {
		root.trimChars<StringView::Chars<'/'>>();
		cb << '/' << root;
	}
}

void _merge(const Callback<void(StringView)> &cb, StringView root) { _merge(cb, false, root); }

void merge(const Callback<void(StringView)> &cb, SpanView<std::string> vec) { doMerge(cb, vec); }

void merge(const Callback<void(StringView)> &cb, SpanView<memory::string> vec) { doMerge(cb, vec); }

void merge(const Callback<void(StringView)> &cb, SpanView<StringView> vec) { doMerge(cb, vec); }

template <>
auto merge<memory::StandartInterface>(SpanView<std::string> vec)
		-> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::PoolInterface>(SpanView<std::string> vec) -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}


template <>
auto merge<memory::StandartInterface>(SpanView<memory::string> vec)
		-> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::PoolInterface>(SpanView<memory::string> vec)
		-> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}


template <>
auto merge<memory::StandartInterface>(SpanView<StringView> vec)
		-> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::PoolInterface>(SpanView<StringView> vec) -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::StandartInterface>(stappler::memory::StandartInterface::StringType &&str)
		-> memory::StandartInterface::StringType {
	return str;
}

template <>
auto merge<memory::PoolInterface>(stappler::memory::StandartInterface::StringType &&str)
		-> memory::PoolInterface::StringType {
	return StringView(str).str<memory::PoolInterface>();
}


template <>
auto merge<memory::StandartInterface>(stappler::memory::PoolInterface::StringType &&str)
		-> memory::StandartInterface::StringType {
	return StringView(str).str<memory::StandartInterface>();
}

template <>
auto merge<memory::PoolInterface>(stappler::memory::PoolInterface::StringType &&str)
		-> memory::PoolInterface::StringType {
	return str;
}

size_t extensionCount(StringView path) {
	size_t ret = 0;
	auto cmp = lastComponent(path);
	for (auto c : cmp) {
		if (c == '.') {
			ret++;
		}
	}
	return ret;
}

StringView extensionForContentType(StringView ct) {
	// TODO: reimplement with list from Serenity
	if (ct.equals("application/pdf") == 0 || ct.equals("application/x-pdf") == 0) {
		return ".pdf";
	} else if (ct.equals("image/jpeg") == 0 || ct.equals("image/pjpeg") == 0) {
		return ".jpeg";
	} else if (ct.equals("image/png") == 0) {
		return ".png";
	} else if (ct.equals("image/gif") == 0) {
		return ".gif";
	} else if (ct.equals("image/tiff") == 0) {
		return ".tiff";
	} else if (ct.equals("application/json") == 0) {
		return ".json";
	} else if (ct.equals("application/zip") == 0) {
		return ".zip";
	}
	return StringView();
}

} // namespace stappler::filepath
