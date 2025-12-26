/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_FILESYSTEM_DETAIL_SPFILESYSTEMRESOURCEDATA_H_
#define CORE_FILESYSTEM_DETAIL_SPFILESYSTEMRESOURCEDATA_H_

#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

struct FilesystemResourceData : InterfaceObject<memory::StandartInterface> {
	struct ResourceLocation {
		FileCategory category;
		StringView prefix;
		Vector<Pair<StringView, FileFlags>> paths;
		bool init = false;
		CategoryFlags flags = CategoryFlags::None;
		FileFlags defaultFileFlags = FileFlags::None;
	};

	static void initialize(void *ptr);
	static void terminate(void *ptr);

	static StringView getResourcePrefix(FileCategory);

	// Read extended environment variable
	// Allow to read CWD, EXEC_DIR and some others custom platform-dependent variables for appconfig paths
	static StringView readVariable(memory::pool_t *pool, StringView key);

	static FilesystemResourceData &get();

	FilesystemResourceData();
	FilesystemResourceData(const FilesystemResourceData &) = delete;
	FilesystemResourceData &operator=(const FilesystemResourceData &) = delete;

	void initResource(ResourceLocation &);

	void enumeratePaths(ResourceLocation &res, StringView filename, FileFlags flags, Access a,
			const Callback<bool(StringView, FileFlags)> &cb);
	void enumeratePaths(FileCategory type, StringView filename, FileFlags flags, Access a,
			const Callback<bool(StringView, FileFlags)> &cb);

	void init();
	void term();

	// Find resource type for a path
	// return ResourceType::Max on failure
	FileCategory detectResourceCategory(StringView,
			const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb =
					nullptr) const;
	FileCategory detectResourceCategory(const FileInfo &,
			const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb =
					nullptr) const;

	// return ResourceType::Max on failure
	FileCategory getResourceCategoryByPrefix(StringView) const;

	// follows ReadablePath rules
	// returns false if lookup was not performed
	bool enumeratePrefixedPath(StringView, FileFlags, Access a,
			const Callback<bool(StringView, FileFlags)> &cb) const;

	CategoryFlags getCategoryFlags(FileCategory) const;

	void initAppPaths(StringView root);

	bool _initialized = false;
	bool _appPathCommon = false;

	memory::pool_t *_pool = nullptr;

	std::mutex _initMutex;
	std::array<ResourceLocation, toInt(FileCategory::Max)> _resourceLocations;
};


} // namespace stappler::filesystem

#endif /* CORE_FILESYSTEM_DETAIL_SPFILESYSTEMRESOURCEDATA_H_ */
