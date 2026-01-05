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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_FILESYSTEM_FILEPATH_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_FILESYSTEM_FILEPATH_H_

#include <sprt/runtime/filesystem/lookup.h>

namespace sprt::filepath {

using filesystem::LocationCategory;
using filesystem::LookupFlags;

// check if filepath is absolute
SPRT_API bool isAbsolute(StringView path);

SPRT_API bool isCanonical(StringView path);

// check if filepath above it's current root
// Like: dir/../..
SPRT_API bool isAboveRoot(StringView path);

// check for ".", ".." and double slashes in path
SPRT_API bool validatePath(StringView path);

SPRT_API bool isEmpty(StringView path);

// extract root from path by removing last component (/dir/file.tar.bz -> /dir)
SPRT_API StringView root(StringView path);

// extract root from path by removing <levels> last components
SPRT_API StringView root(StringView path, uint32_t levels);

// extract last component (/dir/file.tar.bz -> file.tar.bz)
SPRT_API StringView lastComponent(StringView path);

SPRT_API StringView lastComponent(StringView path, size_t allowedComponents);

// extract full filename extension (/dir/file.tar.gz -> tar.gz)
SPRT_API StringView fullExtension(StringView path);

// extract last filename extension (/dir/file.tar.gz -> gz)
SPRT_API StringView lastExtension(StringView path);

// /dir/file.tar.bz -> file
SPRT_API StringView name(StringView path);

// /dir/file.tar.bz -> 2
SPRT_API size_t extensionCount(StringView path);

SPRT_API void split(const callback<void(StringView)> &, StringView);

// merges two path component, removes or adds '/' where needed

template <typename... Args>
SPRT_API auto merge(const callback<void(StringView)> &, StringView root, Args &&...args);

SPRT_API StringView getMimeTypeForExtension(StringView str);

SPRT_API StringView getExtensionForMimeType(StringView str);

SPRT_API void reconstructPath(const callback<void(StringView)> &cb, StringView path);

// replace root path component in filepath
// replace(/my/dir/first/file, /my/dir/first, /your/dir/second)
// [/my/dir/first -> /your/dir/second] /file
// /my/dir/first/file -> /your/dir/second/file
SPRT_API void replace(const callback<void(StringView)> &, StringView path, StringView source,
		StringView dest);

SPRT_API void _merge(const callback<void(StringView)> &cb, bool init, StringView root);
SPRT_API void _merge(const callback<void(StringView)> &cb, StringView root);

template <class... Args>
SPRT_API inline auto _merge(const callback<void(StringView)> &cb, StringView root, Args &&...args) {
	_merge(cb, false, root);
	_merge(cb, sprt::forward<Args>(args)...);
}

template <class... Args>
SPRT_API inline auto merge(const callback<void(StringView)> &cb, StringView root, Args &&...args) {
	size_t bufferSize = 0;

	_merge([&](StringView str) { bufferSize += str.size(); }, true, root);
	_merge([&](StringView str) { bufferSize += str.size(); }, StringView(args)...);

	bufferSize += 1; // null termination
	auto buf = __sprt_malloca(bufferSize);
	auto tmp = (char *)buf;

	_merge([&](StringView str) {
		tmp = strappend(tmp, &bufferSize, str.data(), str.size()); //
	}, true, root);

	_merge([&](StringView str) {
		tmp = strappend(tmp, &bufferSize, str.data(), str.size()); //
	}, StringView(args)...);

	cb(StringView((const char *)buf, tmp - (const char *)buf));

	__sprt_freea(buf);
}

} // namespace sprt::filepath

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_FILESYSTEM_FILEPATH_H_
