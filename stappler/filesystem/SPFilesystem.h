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

#ifndef STAPPLER_FILESYSTEM_SPFILESYSTEM_H_
#define STAPPLER_FILESYSTEM_SPFILESYSTEM_H_

#include "SPIO.h" // IWYU pragma: keep
#include "SPLog.h" // IWYU pragma: keep
#include "SPTime.h"
#include "SPFilepath.h"
#include "SPFilesystemFile.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

struct Stat {
	size_t size = 0;
	uint32_t user = 0;
	uint32_t group = 0;
	FileType type = FileType::Unknown;
	ProtFlags prot = ProtFlags::None;
	Time ctime;
	Time mtime;
	Time atime;
};

// Check if file at path exists
SP_PUBLIC bool exists(const FileInfo &);

SP_PUBLIC bool stat(const FileInfo &, Stat &);

// create dir at path (just mkdir, not mkdir -p)
SP_PUBLIC bool mkdir(const FileInfo &);

// mkdir -p
SP_PUBLIC bool mkdir_recursive(const FileInfo &);

// touch (set mtime to now) file
SP_PUBLIC bool touch(const FileInfo &);

// move file from source to dest (tries to rename file, then copy-remove, rename will be successful only if both path is on single physical drive)
SP_PUBLIC bool move(const FileInfo &source, const FileInfo &dest);

// copy file or directory to dest; use ftw_b for dirs, no directory tree check
SP_PUBLIC bool copy(const FileInfo &source, const FileInfo &dest, bool stopOnError = true);

// remove file or directory
// if not recursive, only single file or empty dir will be removed
// if withDirs == false, only file s in directory tree will be removed
SP_PUBLIC bool remove(const FileInfo &, bool recursive = false);

// file-tree-walk, walk across directory tree at path, callback will be called for each file or directory
// path in callback is fixed with resource origin or absolute for Custom category
// depth = -1 - unlimited
// dirFirst == true - directory will be shown before files inside them, useful for listings and copy
// dirFirst == false - directory will be shown after files, useful for remove
SP_PUBLIC bool ftw(const FileInfo &, const Callback<bool(const FileInfo &, FileType)> &,
		int depth = -1, bool dirFirst = false);

// returns application current work dir from getcwd (or path inside current dir, if path is set
// if relative == false - do not merge paths, if provided path is absolute
//
// It's forbidden to use this function to acquire path above CWD for security reasons.
// To acquire path above cwd, use currentDir without argument, then filepath::merge and filepath::reconstructPath
//
// Current work dir is the technical concept. Use it only if there is a good reason for it
template <typename Interface>
SP_PUBLIC auto currentDir(StringView = StringView(), bool relative = false) ->
		typename Interface::StringType;

// write data into file on path
SP_PUBLIC bool write(const FileInfo &, const uint8_t *data, size_t len, bool _override = true);

template <typename BytesView>
inline bool write(const FileInfo &info, const BytesView &view) {
	return write(info, reinterpret_cast<const uint8_t *>(view.data()), size_t(view.size()));
}

SP_PUBLIC File openForReading(const FileInfo &);

// read file to string (if it was a binary file, string will be invalid)
template <typename Interface>
SP_PUBLIC auto readTextFile(const FileInfo &) -> typename Interface::StringType;

SP_PUBLIC bool readIntoBuffer(uint8_t *buf, const FileInfo &, size_t off = 0,
		size_t size = maxOf<size_t>());

SP_PUBLIC bool readWithConsumer(const io::Consumer &stream, uint8_t *buf, size_t bsize,
		const FileInfo &, size_t off, size_t size);

template <size_t Buffer = 1_KiB>
bool readWithConsumer(const io::Consumer &stream, const FileInfo &info, size_t off = 0,
		size_t size = maxOf<size_t>()) {
	uint8_t b[Buffer];
	return readWithConsumer(stream, b, Buffer, info, off, size);
}

template <typename Interface>
auto readIntoMemory(const FileInfo &info, size_t off = 0, size_t size = maxOf<size_t>()) ->
		typename Interface::BytesType {
	auto f = openForReading(info);
	if (f) {
		auto ret = f.readIntoMemory<Interface>();
		f.close();
		return ret;
	}
	return typename Interface::BytesType();
}

SP_PUBLIC StringView detectMimeType(StringView path);

SP_PUBLIC std::ostream &operator<<(std::ostream &, ProtFlags);
SP_PUBLIC std::ostream &operator<<(std::ostream &, const Stat &);

template <typename Interface>
SP_PUBLIC inline auto currentDir(StringView ipath, bool relative) ->
		typename Interface::StringType {
	if (filepath::isAboveRoot(ipath)) {
		typename Interface::StringType();
	}

	if (!ipath.empty() && !relative && filepath::isAbsolute(ipath)) {
		return ipath.str<Interface>();
	}

	typename Interface::StringType ret;
	sprt::filesystem::getCurrentDir([&](StringView path) { ret = path.str<Interface>(); }, ipath);
	return ret;
}


template <typename Interface>
SP_PUBLIC auto readTextFile(const FileInfo &info) -> typename Interface::StringType {
	auto f = openForReading(info);
	if (f) {
		auto fsize = f.size();
		typename Interface::StringType ret;
		ret.resize(fsize);
		f.read((uint8_t *)ret.data(), fsize);
		f.close();
		return ret;
	}
	return typename Interface::StringType();
}

SP_PUBLIC ProtFlags getProtFlagsFromMode(__sprt_mode_t m);
SP_PUBLIC __sprt_mode_t getModeFromProtFlags(ProtFlags flags);

} // namespace stappler::filesystem

#endif /* STAPPLER_FILESYSTEM_SPFILESYSTEM_H_ */
