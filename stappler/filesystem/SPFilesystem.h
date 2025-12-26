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
#include "SPTime.h"
#include "SPFilepath.h"
#include "SPLog.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::filesystem {

enum class CategoryFlags : uint32_t {
	None = 0,

	// File in this category can be reverse-located via `detectResourceCategory` and 'filepath::canonical'
	Locateable = 1 << 0,

	// Files in this category can be accessed only with platform-specific api (filesystem::platform)
	PlatformSpecific = 1 << 1,

	// Category's root directory can be removed/unmounted when app is still active
	Removable = 1 << 2
};

SP_DEFINE_ENUM_AS_MASK(CategoryFlags)

enum class MappingType {
	Private,
	Shared
};

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

enum class OpenFlags : uint32_t {
	None,
	Read = 1 << 0,
	Write = 1 << 1,
	Create = 1 << 2,
	Append = 1 << 3,
	Truncate = 1 << 4,
	CreateExclusive = 1 << 5,
	DelOnClose = 1 << 6,
};

SP_DEFINE_ENUM_AS_MASK(OpenFlags)

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

class SP_PUBLIC File final {
public:
	enum class Flags {
		None,
		DelOnClose
	};

	using traits_type = std::char_traits<char>;
	using streamsize = std::streamsize;
	using int_type = typename traits_type::int_type;

	static File open_tmp(StringView prefix, bool delOnClose = true);

	File();
	explicit File(FILE *, Flags = Flags::None);
	explicit File(void *);
	explicit File(void *, size_t);

	~File();

	File(File &&);
	File &operator=(File &&);

	File(const File &) = delete;
	File &operator=(const File &) = delete;

	size_t read(uint8_t *buf, size_t nbytes);
	size_t seek(int64_t offset, io::Seek s);

	size_t tell() const;
	size_t size() const;

	int_type xsgetc();
	int_type xsputc(int_type c);

	streamsize xsputn(const char *s, streamsize n);
	streamsize xsgetn(char *s, streamsize n);

	bool eof() const;
	void close();
	void close_remove();

	bool close_rename(const FileInfo &);

	bool is_open() const;
	explicit operator bool() const { return is_open(); }

	const char *path() const;

	template <typename Interface>
	auto readIntoMemory(size_t off = 0, size_t size = maxOf<size_t>()) ->
			typename Interface::BytesType {
		if (is_open()) {
			auto fsize = this->size();
			if (fsize <= off) {
				return typename Interface::BytesType();
			}
			if (fsize - off < size) {
				size = fsize - off;
			}
			typename Interface::BytesType ret;
			ret.resize(size);
			this->seek(off, io::Seek::Set);
			this->read(ret.data(), size);
			return ret;
		}
		return typename Interface::BytesType();
	}

protected:
	void set_tmp_path(const char *);

	bool _isBundled = false;
	size_t _size = 0;
	Flags _flags = Flags::None;
	char _buf[256] = {0};
	union {
		FILE *_nativeFile;
		void *_platformFile;
	};
};

class SP_PUBLIC MemoryMappedRegion final {
public:
	using PlatformStorage = std::array<uint8_t, 16>;

	static MemoryMappedRegion mapFile(const FileInfo &, MappingType, ProtFlags, size_t offset = 0,
			size_t len = maxOf<size_t>());

	~MemoryMappedRegion();

	MemoryMappedRegion(MemoryMappedRegion &&);
	MemoryMappedRegion &operator=(MemoryMappedRegion &&);

	MemoryMappedRegion(const MemoryMappedRegion &) = delete;
	MemoryMappedRegion &operator=(const MemoryMappedRegion &) = delete;

	MappingType getType() const { return _type; }
	ProtFlags getProtectionFlags() const { return _prot; }

	uint8_t *getRegion() const { return _region; }
	size_t getSize() const { return _size; }

	BytesView getView() const { return BytesView(_region, _size); }

	operator bool() const { return _region != nullptr; }

	void sync();

protected:
	MemoryMappedRegion();
	MemoryMappedRegion(PlatformStorage &&, uint8_t *, MappingType, ProtFlags, size_t);

	PlatformStorage _storage;
	uint8_t *_region = nullptr;
	size_t _size = 0;
	MappingType _type;
	ProtFlags _prot;
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
SP_PUBLIC bool remove(const FileInfo &, bool recursive = false, bool withDirs = false);

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

// Resource paths API
// use this to load/save application resources, instead of direct read/write with custom paths

// Returns most prioritized path to search for a resources of specific types
template <typename Interface>
SP_PUBLIC auto findPath(FileCategory, FileFlags = FileFlags::None) ->
		typename Interface::StringType;

// returns path, from which loadable resource can be read (from application bundle or dedicated resource directory)
template <typename Interface>
SP_PUBLIC auto findPath(StringView path, FileCategory = FileCategory::Custom,
		FileFlags = FileFlags::None, Access = Access::None) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto findPath(StringView path, FileCategory cat, Access a) ->
		typename Interface::StringType {
	return findPath<Interface>(path, cat, FileFlags::None, a);
}

template <typename Interface>
SP_PUBLIC auto findPath(const FileInfo &info, Access a = Access::None) ->
		typename Interface::StringType {
	return findPath<Interface>(info.path, info.category, info.flags, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(FileCategory cat, FileFlags flags = FileFlags::None) ->
		typename Interface::StringType {
	return findPath<Interface>(cat, flags | FileFlags::Writable);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(StringView path, FileCategory cat = FileCategory::Custom,
		FileFlags flags = FileFlags::None, Access a = Access::None) ->
		typename Interface::StringType {
	return findPath<Interface>(path, cat, flags | FileFlags::Writable, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(StringView path, FileCategory cat, Access a) ->
		typename Interface::StringType {
	return findPath<Interface>(path, cat, FileFlags::Writable, a);
}

template <typename Interface>
SP_PUBLIC inline auto findWritablePath(const FileInfo &info, Access a = Access::None) ->
		typename Interface::StringType {
	return findPath<Interface>(info.path, info.category, info.flags | FileFlags::Writable, a);
}

// enumerate all paths, that will be used to find a resource of specific types
SP_PUBLIC void enumeratePaths(FileCategory, FileFlags,
		const Callback<bool(StringView, FileFlags)> &);

SP_PUBLIC void enumeratePaths(StringView path, FileCategory, FileFlags, Access,
		const Callback<bool(StringView, FileFlags)> &);

SP_PUBLIC inline void enumeratePaths(FileCategory t,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(t, FileFlags::None, cb);
}

SP_PUBLIC inline void enumeratePaths(StringView path, FileCategory t, FileFlags flags,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(path, t, flags, Access::None, cb);
}

SP_PUBLIC inline void enumeratePaths(StringView path, FileCategory t, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(path, t, FileFlags::None, a, cb);
}

SP_PUBLIC inline void enumeratePaths(const FileInfo &info, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(info.path, info.category, info.flags, a, cb);
}

SP_PUBLIC inline void enumeratePaths(const FileInfo &info,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(info.path, info.category, info.flags, Access::None, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(FileCategory cat,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(cat, FileFlags::Writable, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(FileCategory cat, FileFlags flags,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(cat, flags | FileFlags::Writable, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(StringView path, FileCategory cat, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(path, cat, FileFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(StringView path, FileCategory cat, FileFlags flags,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(path, cat, flags | FileFlags::Writable, Access::None, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(StringView path, FileCategory cat, FileFlags flags,
		Access a, const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(path, cat, flags | FileFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(const FileInfo &info, Access a,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(info.path, info.category, info.flags | FileFlags::Writable, a, cb);
}

SP_PUBLIC inline void enumerateWritablePaths(const FileInfo &info,
		const Callback<bool(StringView, FileFlags)> &cb) {
	enumeratePaths(info.path, info.category, info.flags | FileFlags::Writable, Access::None, cb);
}

// Search for a FileCategory for absolute path
// optionally - returns prefixed path for in in callback
FileCategory detectResourceCategory(StringView,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb = nullptr);
FileCategory detectResourceCategory(const FileInfo &,
		const Callback<void(StringView prefixedPath, StringView categoryPath)> &cb = nullptr);

CategoryFlags getCategoryFlags(FileCategory);

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

SP_PUBLIC Access getAccessProtFlags(ProtFlags);

SP_PUBLIC StringView detectMimeType(StringView path);

SP_PUBLIC std::ostream &operator<<(std::ostream &, ProtFlags);
SP_PUBLIC std::ostream &operator<<(std::ostream &, const Stat &);

} // namespace stappler::filesystem

// *::platform::* functions handles some FS actions in platform-specific way
// (like inside Android apk or MacOS app bundle)
namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

template <typename Interface>
SP_PUBLIC auto _getApplicationPath() -> typename Interface::StringType;

SP_PUBLIC bool _access(FileCategory, StringView path, Access);
SP_PUBLIC bool _stat(FileCategory, StringView path, Stat &);

SP_PUBLIC File _openForReading(FileCategory, StringView);
SP_PUBLIC size_t _read(void *, uint8_t *buf, size_t nbytes);
SP_PUBLIC size_t _seek(void *, int64_t offset, io::Seek s);
SP_PUBLIC size_t _tell(void *);
SP_PUBLIC bool _eof(void *);
SP_PUBLIC void _close(void *);

SP_PUBLIC Status _ftw(FileCategory, StringView path, const Callback<bool(StringView, FileType)> &,
		int depth, bool dirFirst);

SP_PUBLIC uint8_t *_mapFile(uint8_t storage[16], StringView path, MappingType type, ProtFlags prot,
		size_t offset, size_t len);
SP_PUBLIC bool _unmapFile(uint8_t *region, uint8_t storage[16]);
SP_PUBLIC bool _syncMappedRegion(uint8_t *region, uint8_t storage[16]);

} // namespace stappler::filesystem::platform

// *::native::* functions (in contrast with ::platform::) works with native platform filesystem directly
// (so, not in bundle or archive)
//
// libstappler uses posix path scheme, it should be transformed when transferred from/to other apps/libraries
// No transformation required within libstappler itself
namespace STAPPLER_VERSIONIZED stappler::filesystem::native {

// C:\dirname\filename -> /c/dirname/filename
template <typename Interface>
SP_PUBLIC auto nativeToPosix(StringView path) -> typename Interface::StringType;

// /c/dirname/filename -> C:\dirname\filename
// be sure that path is absolute
template <typename Interface>
SP_PUBLIC auto posixToNative(StringView path) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto getcwd_fn() -> typename Interface::StringType;

SP_PUBLIC Status remove_fn(StringView path);
SP_PUBLIC Status unlink_fn(StringView path);
SP_PUBLIC Status mkdir_fn(StringView path, ProtFlags = ProtFlags::MkdirDefault);

SP_PUBLIC Status access_fn(StringView path, Access mode);

SP_PUBLIC Status stat_fn(StringView path, Stat &);

SP_PUBLIC Status touch_fn(StringView path);

// Callback returns relative paths, not absolute
SP_PUBLIC Status ftw_fn(StringView path, const Callback<bool(StringView, FileType)> &, int depth,
		bool dirFirst);

SP_PUBLIC Status rename_fn(StringView source, StringView dest);

SP_PUBLIC FILE *fopen_fn(StringView, StringView mode);

SP_PUBLIC Status write_fn(StringView, const unsigned char *data, size_t len,
		ProtFlags = ProtFlags::WriteDefault);

} // namespace stappler::filesystem::native

namespace STAPPLER_VERSIONIZED stappler::filesystem {

template <typename Interface>
SP_PUBLIC inline auto currentDir(StringView path, bool relative) -> typename Interface::StringType {
	if (filepath::isAboveRoot(path)) {
		typename Interface::StringType();
	}

	if (!path.empty() && !relative && filepath::isAbsolute(path)) {
		return path.str<Interface>();
	}
	auto cwd = filesystem::native::getcwd_fn<Interface>();
	if (!cwd.empty()) {
		if (path.empty()) {
			return cwd;
		} else {
			auto subPath = filepath::merge<Interface>(cwd, path);
			return filepath::reconstructPath<Interface>(subPath);
		}
	}
	return typename Interface::StringType();
}

template <typename Interface>
SP_PUBLIC inline auto findPath(StringView path, FileCategory type, FileFlags flags, Access a) ->
		typename Interface::StringType {
	typename Interface::StringType npath;
	enumeratePaths(path, type, flags, a, [&](StringView p, FileFlags) {
		npath = p.str<Interface>();
		return false;
	});
	return npath;
}

template <typename Interface>
SP_PUBLIC auto findPath(FileCategory type, FileFlags flags) -> typename Interface::StringType {
	typename Interface::StringType npath;
	enumeratePaths(type, flags, [&](StringView p, FileFlags) {
		npath = p.str<Interface>();
		return false;
	});
	return npath;
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

} // namespace stappler::filesystem

namespace STAPPLER_VERSIONIZED stappler::io {

template <>
struct ProducerTraits<filesystem::File> {
	using type = filesystem::File;
	static size_t ReadFn(void *ptr, uint8_t *buf, size_t nbytes) {
		return ((type *)ptr)->read(buf, nbytes);
	}

	static size_t SeekFn(void *ptr, int64_t offset, Seek s) {
		return ((type *)ptr)->seek(offset, s);
	}
	static size_t TellFn(void *ptr) { return ((type *)ptr)->tell(); }
};

} // namespace stappler::io

#endif /* STAPPLER_FILESYSTEM_SPFILESYSTEM_H_ */
