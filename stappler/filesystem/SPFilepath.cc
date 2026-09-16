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
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler {

FileInfo::FileInfo(StringView _path) {
	if (_path.is('%')) {
		category = filesystem::detectResourceCategory(_path);
		if (category == FileCategory::Custom) {
			slog().warn("filesystem", "Invalid category prefix in path: ", _path);
		} else {
			// The prefix has done its job — it named the category. Everything downstream merges
			// `path` onto a location root, so it must be the bare path from here on.
			_path.skipUntil<StringView::Chars<':'>>();
			if (_path.is(':')) {
				++_path;
			}
			_path.skipChars<StringView::Chars<'/'>>();
		}
	}
	path = _path;
}

FileInfo::FileInfo(StringView _path, FileCategory cat) : path(_path), category(cat) { }

FileInfo::FileInfo(StringView _path, FileCategory cat, FileFlags f)
: path(_path), category(cat), flags(f) { }

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::filepath {

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

	typename Interface::StringType result;
	filesystem::detectResourceCategory(path, [&](const filesystem::ReverseLookupInfo &info) {
		result = info.prefixedPath.str<Interface>();
	});

	if (result.empty()) {
		sprt::filesystem::getCurrentDir([&](StringView cpath) { result = cpath.str<Interface>(); },
				path);
	}
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

	typename Interface::StringType result;
	filesystem::detectResourceCategory(info, [&](const filesystem::ReverseLookupInfo &info) {
		result = info.prefixedPath.str<Interface>();
	});

	if (result.empty()) {
		sprt::filesystem::getCurrentDir([&](StringView cpath) { result = cpath.str<Interface>(); },
				info.path);
	}
	return result;
}

template <>
auto canonical<mem_std::Interface>(StringView path) -> mem_std::Interface::StringType {
	return canonical_fn<mem_std::Interface>(path);
}

template <>
auto canonical<memory::PoolInterface>(StringView path) -> memory::PoolInterface::StringType {
	return canonical_fn<memory::PoolInterface>(path);
}

template <>
auto canonical<mem_std::Interface>(const FileInfo &info) -> mem_std::Interface::StringType {
	return canonical_fn<mem_std::Interface>(info);
}

template <>
auto canonical<memory::PoolInterface>(const FileInfo &info) -> memory::PoolInterface::StringType {
	return canonical_fn<memory::PoolInterface>(info);
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
auto _merge<mem_std::Interface>(StringView root, StringView path)
		-> mem_std::Interface::StringType {
	return do_merge<mem_std::Interface>(root, path);
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

void merge(const Callback<void(StringView)> &cb, SpanView<sprt::__malloc_string> vec) {
	doMerge(cb, vec);
}

void merge(const Callback<void(StringView)> &cb, SpanView<sprt::__pool_string> vec) {
	doMerge(cb, vec);
}

void merge(const Callback<void(StringView)> &cb, SpanView<StringView> vec) { doMerge(cb, vec); }

template <>
auto merge<mem_std::Interface>(SpanView<sprt::__malloc_string> vec)
		-> mem_std::Interface::StringType {
	mem_std::Interface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::PoolInterface>(SpanView<sprt::__malloc_string> vec)
		-> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}


template <>
auto merge<mem_std::Interface>(SpanView<sprt::__pool_string> vec)
		-> mem_std::Interface::StringType {
	mem_std::Interface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}

template <>
auto merge<memory::PoolInterface>(SpanView<sprt::__pool_string> vec)
		-> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	ret.reserve(getMergeSize(vec));
	doMerge([&](StringView str) { ret.append(str.data(), str.size()); }, vec);
	return ret;
}


template <>
auto merge<mem_std::Interface>(SpanView<StringView> vec) -> mem_std::Interface::StringType {
	mem_std::Interface::StringType ret;
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
auto merge<mem_std::Interface>(stappler::mem_std::Interface::StringType &&str)
		-> mem_std::Interface::StringType {
	return str;
}

template <>
auto merge<memory::PoolInterface>(stappler::mem_std::Interface::StringType &&str)
		-> memory::PoolInterface::StringType {
	return StringView(str).str<memory::PoolInterface>();
}


template <>
auto merge<mem_std::Interface>(stappler::memory::PoolInterface::StringType &&str)
		-> mem_std::Interface::StringType {
	return StringView(str).str<mem_std::Interface>();
}

template <>
auto merge<memory::PoolInterface>(stappler::memory::PoolInterface::StringType &&str)
		-> memory::PoolInterface::StringType {
	return str;
}

} // namespace stappler::filepath
