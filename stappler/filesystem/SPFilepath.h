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

#include "SPFilesystemLookup.h"
#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/filesystem/lookup.h>

namespace STAPPLER_VERSIONIZED stappler::filepath {

using sprt::filepath::isAbsolute;
using sprt::filepath::isCanonical;
using sprt::filepath::isAboveRoot;
using sprt::filepath::isEmpty;
using sprt::filepath::validatePath;
using sprt::filepath::root;
using sprt::filepath::lastComponent;
using sprt::filepath::fullExtension;
using sprt::filepath::lastExtension;
using sprt::filepath::name;
using sprt::filepath::extensionCount;
using sprt::filepath::split;
using sprt::filepath::merge;
using sprt::filepath::getMimeTypeForExtension;
using sprt::filepath::getExtensionForMimeType;

// remove any ".", ".." and double slashes from path
template <typename Interface>
SP_PUBLIC auto reconstructPath(StringView path) -> typename Interface::StringType;

// encodes path for long-term storage (default application dirs will be replaced with canonical prefix,
// like %CACHE%/dir)
template <typename Interface>
SP_PUBLIC auto canonical(StringView path) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto canonical(const FileInfo &) -> typename Interface::StringType;

// merges two path component, removes or adds '/' where needed

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<std::string>);

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<memory::string>);

SP_PUBLIC void merge(const Callback<void(StringView)> &cb, SpanView<StringView>);

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

// replace root path component in filepath
// replace(/my/dir/first/file, /my/dir/first, /your/dir/second)
// [/my/dir/first -> /your/dir/second] /file
// /my/dir/first/file -> /your/dir/second/file
template <typename Interface>
SP_PUBLIC auto replace(StringView path, StringView source, StringView dest) ->
		typename Interface::StringType;

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

template <typename Interface>
SP_PUBLIC auto reconstructPath(StringView ipath) -> typename Interface::StringType {
	typename Interface::StringType ret;
	sprt::filepath::reconstructPath([&](StringView path) { ret = path.str<Interface>(); }, ipath);
	return ret;
}

template <typename Interface>
SP_PUBLIC inline auto replace(StringView ipath, StringView source, StringView dest) ->
		typename Interface::StringType {
	typename Interface::StringType ret;
	sprt::filepath::replace([&](StringView path) { ret = path.str<Interface>(); }, ipath, source,
			dest);
	return ret;
}

} // namespace stappler::filepath

#endif /* STAPPLER_FILESYSTEM_SPFILEPATH_H_ */
