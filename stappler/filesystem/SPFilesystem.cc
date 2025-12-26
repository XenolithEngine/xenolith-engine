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

#include "SPFilesystem.h"
#include "SPCore.h"
#include "SPFilepath.h"
#include "SPMemInterface.h"
#include "SPStatus.h"
#include "SPPlatform.h"

#include <stdio.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem {

File File::open_tmp(StringView prefix, bool delOnClose) {
	if (prefix.empty()) {
		prefix = StringView("sa.tmp");
	}

#if WIN32
	log::source().warn("filesystem", "File::open_tmp unavailable on win32");
#else
	char buf[256] = {0};
	const char *tmp = "/tmp";
	size_t len = strlen(tmp);
	strcpy(&buf[0], tmp);
	strcpy(&buf[len], "/");
	strncpy(&buf[len + 1], prefix.data(), prefix.size());
	len += prefix.size();
	strcpy(&buf[len + 1], "XXXXXX");

	if (auto fd = ::mkstemp(buf)) {
		if (auto f = ::fdopen(fd, "wb+")) {
			auto ret = File(f, delOnClose ? Flags::DelOnClose : Flags::None);
			ret.set_tmp_path(buf);
			return ret;
		}
	}
#endif

	return File();
}

File::File() : _isBundled(false), _nativeFile(nullptr) { }
File::File(FILE *f, Flags flags) : _isBundled(false), _flags(flags), _nativeFile(f) {
	if (is_open()) {
		auto pos = seek(0, io::Seek::Current);
		auto size = seek(0, io::Seek::End);
		if (pos != maxOf<size_t>()) {
			seek(pos, io::Seek::Set);
		}
		_size = (size != maxOf<size_t>()) ? size : 0;
	}
}
File::File(void *f) : _isBundled(true), _platformFile(f) {
	if (is_open()) {
		auto pos = seek(0, io::Seek::Current);
		auto size = seek(0, io::Seek::End);
		if (pos != maxOf<size_t>()) {
			seek(pos, io::Seek::Set);
		}
		_size = (size != maxOf<size_t>()) ? size : 0;
	}
}

File::File(void *f, size_t s) : _isBundled(true), _size(s), _platformFile(f) { }

File::File(File &&f) : _isBundled(f._isBundled), _size(f._size) {
	if (_isBundled) {
		_platformFile = f._platformFile;
		f._platformFile = nullptr;
	} else {
		_nativeFile = f._nativeFile;
		f._nativeFile = nullptr;
	}
	f._size = 0;
	if (f._buf[0] != 0) {
		memcpy(_buf, f._buf, 256);
	}
}

File &File::operator=(File &&f) {
	_isBundled = f._isBundled;
	_size = f._size;
	if (_isBundled) {
		_platformFile = f._platformFile;
		f._platformFile = nullptr;
	} else {
		_nativeFile = f._nativeFile;
		f._nativeFile = nullptr;
	}
	f._size = 0;
	if (f._buf[0] != 0) {
		memcpy(_buf, f._buf, 256);
	}
	return *this;
}

File::~File() { close(); }

size_t File::read(uint8_t *buf, size_t nbytes) {
	if (is_open()) {
		if (!_isBundled) {
			size_t remains = _size - ftell(_nativeFile);
			if (nbytes > remains) {
				nbytes = remains;
			}
			if (fread(buf, 1, nbytes, _nativeFile) == nbytes) {
				return nbytes;
			}
		} else {
			return filesystem::platform::_read(_platformFile, buf, nbytes);
		}
	}
	return 0;
}

size_t File::seek(int64_t offset, io::Seek s) {
	if (is_open()) {
		if (!_isBundled) {
			int whence = SEEK_SET;
			switch (s) {
			case io::Seek::Set: whence = SEEK_SET; break;
			case io::Seek::Current: whence = SEEK_CUR; break;
			case io::Seek::End: whence = SEEK_END; break;
			}

			if (offset != 0 || s != io::Seek::Current) {
				if (fseek(_nativeFile, long(offset), whence) != 0) {
					return maxOf<size_t>();
				}
			}
			auto p = ftell(_nativeFile);
			if (p >= 0) {
				return static_cast<size_t>(p);
			} else {
				return maxOf<size_t>();
			}
		} else {
			return filesystem::platform::_seek(_platformFile, offset, s);
		}
	}
	return maxOf<size_t>();
}

size_t File::tell() const {
	if (!_isBundled) {
		auto p = ftell(_nativeFile);
		if (p >= 0) {
			return static_cast<size_t>(p);
		} else {
			return maxOf<size_t>();
		}
	} else {
		return filesystem::platform::_tell(_platformFile);
	}
}

size_t File::size() const { return _size; }

typename File::int_type File::xsgetc() {
	int_type ret = traits_type::eof();
	if (is_open()) {
		if (!_isBundled) {
			ret = fgetc(_nativeFile);
		} else {
			uint8_t buf = 0;
			if (read(&buf, 1) == 1) {
				ret = buf;
			}
		}
	}
	return ret;
}

typename File::int_type File::xsputc(int_type c) {
	int_type ret = traits_type::eof();
	if (is_open() && !_isBundled) {
		ret = fputc(c, _nativeFile);
	}
	++_size;
	return ret;
}

typename File::streamsize File::xsputn(const char *s, streamsize n) {
	streamsize ret = -1;
	if (is_open() && !_isBundled) {
		if (fwrite(s, n, 1, _nativeFile) == 1) {
			ret = n;
		}
	}
	_size += n;
	return ret;
}

typename File::streamsize File::xsgetn(char *s, streamsize n) {
	streamsize ret = -1;
	if (is_open()) {
		ret = read(reinterpret_cast<uint8_t *>(s), n);
	}
	return ret;
}

bool File::eof() const {
	if (is_open()) {
		if (!_isBundled) {
			return feof(_nativeFile) != 0;
		} else {
			return filesystem::platform::_eof(_platformFile);
		}
	}
	return true;
}

void File::close() {
	if (is_open()) {
		if (!_isBundled) {
			fclose(_nativeFile);
			if (_flags != Flags::DelOnClose && _buf[0] != 0) {
				native::unlink_fn(_buf);
			}
			memset(_buf, 0, 256);
			_nativeFile = nullptr;
		} else {
			filesystem::platform::_close(_platformFile);
			_platformFile = nullptr;
		}
	}
}

void File::close_remove() {
	if (is_open()) {
		if (!_isBundled) {
			fclose(_nativeFile);
			if (_buf[0] != 0) {
				native::unlink_fn(_buf);
			}
			memset(_buf, 0, 256);
			_nativeFile = nullptr;
		} else {
			filesystem::platform::_close(_platformFile);
			_platformFile = nullptr;
		}
	}
}

bool File::close_rename(const FileInfo &info) {
	if (is_open()) {
		if (!_isBundled && _buf[0] != 0) {
			fclose(_nativeFile);
			_nativeFile = nullptr;
			if (move(FileInfo{StringView(_buf)}, info)) {
				memset(_buf, 0, 256);
				return true;
			} else {
				_nativeFile = native::fopen_fn(_buf, "wb+");
			}
		}
	}
	return false;
}

bool File::is_open() const { return _nativeFile != nullptr || _platformFile != nullptr; }

const char *File::path() const {
	if (_buf[0] == 0) {
		return nullptr;
	} else {
		return _buf;
	}
}

void File::set_tmp_path(const char *buf) { memcpy(_buf, buf, 256); }

MemoryMappedRegion MemoryMappedRegion::mapFile(const FileInfo &info, MappingType type,
		ProtFlags prot, size_t offset, size_t len) {
	if (math::align(offset, size_t(sp::platform::getMemoryPageSize())) != offset) {
		log::source().error("filesystem",
				"offset for MemoryMappedRegion::mapFile should be aligned as "
				"platform::_getMemoryPageSize");
		return MemoryMappedRegion();
	}

	auto path = findPath<memory::StandartInterface>(info, getAccessProtFlags(prot));

	Stat stat;
	if (native::stat_fn(path, stat) != Status::Ok) {
		log::source().error("filesystem", "Fail to get stat for a file: ", path);
		return MemoryMappedRegion();
	}

	len = std::min(len, stat.size);

	if (offset > 0) {
		if (offset > stat.size) {
			log::source().error("filesystem", "Offset (", offset, ") for a file ", path,
					" is larger then file itself");
			return MemoryMappedRegion();
		} else {
			auto remains = stat.size - offset;
			len = std::min(len, remains);
		}
	}

	PlatformStorage storage;
	auto region = platform::_mapFile(storage.data(), path, type, prot, offset, len);
	if (region) {
		return MemoryMappedRegion(move(storage), region, type, prot, len);
	}

	return MemoryMappedRegion();
}

MemoryMappedRegion::~MemoryMappedRegion() {
	if (_region) {
		platform::_unmapFile(_region, _storage.data());
		_region = nullptr;
	}
}

MemoryMappedRegion::MemoryMappedRegion(MemoryMappedRegion &&other) {
	_region = other._region;
	_storage = sp::move(other._storage);
	_type = other._type;
	_prot = other._prot;

	other._region = nullptr;
	memset(other._storage.data(), 0, other._storage.size());
}

MemoryMappedRegion &MemoryMappedRegion::operator=(MemoryMappedRegion &&other) {
	_region = other._region;
	_storage = sp::move(other._storage);
	_type = other._type;
	_prot = other._prot;

	other._region = nullptr;
	memset(other._storage.data(), 0, other._storage.size());
	return *this;
}

void MemoryMappedRegion::sync() { platform::_syncMappedRegion(_region, _storage.data()); }

MemoryMappedRegion::MemoryMappedRegion()
: _region(nullptr), _type(MappingType::Private), _prot(ProtFlags::None) { }

MemoryMappedRegion::MemoryMappedRegion(PlatformStorage &&storage, uint8_t *ptr, MappingType t,
		ProtFlags p, size_t s)
: _storage(sp::move(storage)), _region(ptr), _size(s), _type(t), _prot(p) { }

bool exists(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	if (hasFlag(getCategoryFlags(info.category), CategoryFlags::PlatformSpecific)) {
		if (filesystem::platform::_access(info.category, info.path, Access::Exists)) {
			return true;
		}
		return false;
	}

	bool found = false;
	enumeratePaths(info, Access::Exists, [&](StringView str, FileFlags) {
		found = true;
		return false;
	});
	return found;
}

bool stat(const FileInfo &info, Stat &stat) {
	if (info.path.empty()) {
		return false;
	}

	if (hasFlag(getCategoryFlags(info.category), CategoryFlags::PlatformSpecific)) {
		if (filesystem::platform::_stat(info.category, info.path, stat)) {
			return true;
		}
		return false;
	}

	bool found = false;
	enumeratePaths(info, Access::Exists, [&](StringView str, FileFlags) {
		found = filesystem::native::stat_fn(str, stat) == Status::Ok;
		return false;
	});
	return found;
}

bool remove(const FileInfo &info, bool recursive, bool withDirs) {
	if (info.path.empty()) {
		return false;
	}

	if (info.category == FileCategory::Bundled) {
		return false; // we can not remove anything from bundle
	}

	if (!recursive) {
		bool found = false;
		enumerateWritablePaths(info, Access::Exists, [&](StringView str, FileFlags) {
			found = filesystem::native::remove_fn(str) == Status::Ok;
			return false;
		});
		return found;
	} else {
		bool success = true;
		ftw(info, [&](const FileInfo &path, FileType type) -> bool {
			if (type != FileType::Dir || withDirs) {
				if (!remove(path)) {
					success = false;
					return false;
				}
			}
			return true;
		}, -1, false);
		return success;
	}
}

bool touch(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	if (info.category == FileCategory::Bundled) {
		return false; // we can not remove anything from bundle
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](StringView str, FileFlags) {
		found = filesystem::native::touch_fn(str) == Status::Ok;
		return false;
	});
	return found;
}

bool mkdir(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](StringView str, FileFlags) {
		found = filesystem::native::mkdir_fn(str) == Status::Ok;
		return false;
	});
	return found;
}

// We need FileResourceInfo for root constraints
static bool _mkdir_recursive(StringView path, const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	// check if root dir exists
	auto root = filepath::root(path);
	auto rootInfo = info;
	rootInfo.path = filepath::root(info.path);

	auto err = filesystem::native::access_fn(root, Access::Exists);
	if (err != Status::Ok) {
		if (!_mkdir_recursive(root, rootInfo)) {
			return false;
		}
	}

	return filesystem::native::mkdir_fn(path) == Status::Ok;
}

bool mkdir_recursive(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](StringView str, FileFlags) {
		found = _mkdir_recursive(str, info);
		return false;
	});
	return found;
}

bool ftw(const FileInfo &info, const Callback<bool(const FileInfo &, FileType)> &cb, int depth,
		bool dirFirst) {
	if (filepath::isEmpty(info)) {
		return false;
	}

	auto fn = [&](StringView p, FileType t) {
		auto tmpPath = filepath::merge<memory::StandartInterface>(info.path, p);
		FileInfo newInfo = info;
		newInfo.path = tmpPath;
		return cb(newInfo, t);
	};

	if (hasFlag(getCategoryFlags(info.category), CategoryFlags::PlatformSpecific)) {
		return filesystem::platform::_ftw(info.category, info.path, fn, depth, dirFirst)
				== Status::Ok;
	} else {
		bool found = false;
		enumeratePaths(info, Access::Exists, [&](StringView str, FileFlags) {
			found = filesystem::native::ftw_fn(str, fn, depth, dirFirst) == Status::Ok;
			return false;
		});
		return found;
	}
}

bool move(const FileInfo &isource, const FileInfo &idest) {
	if (isource.path.empty() || idest.path.empty()) {
		return false;
	}

	memory::StandartInterface::StringType source;
	memory::StandartInterface::StringType dest;

	enumerateWritablePaths(isource, Access::Exists, [&](StringView str, FileFlags) {
		source = str.str<memory::StandartInterface>();
		return false;
	});

	if (source.empty()) {
		return false;
	}

	enumerateWritablePaths(idest, Access::None, [&](StringView str, FileFlags) {
		dest = str.str<memory::StandartInterface>();
		return false;
	});

	if (dest.empty()) {
		return false;
	}

	if (filesystem::native::rename_fn(source, dest) != Status::Ok) {
		if (copy(isource, idest)) {
			return remove(isource, true, true);
		}
		return false;
	}
	return true;
}

// Copy single file
static bool performCopy(const FileInfo &source, const FileInfo &dest) {
	remove(dest);

	memory::StandartInterface::StringType absdest;

	enumerateWritablePaths(dest, Access::None, [&](StringView str, FileFlags) {
		absdest = native::posixToNative<memory::StandartInterface>(str);
		return false;
	});

	if (absdest.empty()) {
		return false;
	}

	std::ofstream destStream(absdest.data(), std::ios::binary);
	auto f = openForReading(source);
	if (f && destStream.is_open()) {
		if (io::read(f, io::Consumer(destStream)) > 0) {
			destStream.flush();
			destStream.close();
			return true;
		}
	}
	return false;
}

static bool isdir(const FileInfo &path) {
	Stat s;
	stat(path, s);
	return s.type == FileType::Dir;
}

// TODO implement 'force' flag
bool copy(const FileInfo &isource, const FileInfo &idest, bool stopOnError) {
	if (filepath::isEmpty(isource.path)) {
		return false;
	}

	auto sourceLastComponent = filepath::lastComponent(isource.path);
	if (sourceLastComponent.empty()) {
		return false;
	}

	memory::StandartInterface::StringType dest;

	if (idest.path.back() == '/') {
		// cp sourcedir targetdir/
		// extend dest with the first source component
		dest = filepath::merge<memory::StandartInterface>(idest.path, sourceLastComponent);
	} else if (isdir(idest) && sourceLastComponent != filepath::lastComponent(idest.path)) {
		dest = filepath::merge<memory::StandartInterface>(idest.path, sourceLastComponent);
	} else if (!filepath::isEmpty(idest)) {
		dest = idest.path.str<memory::StandartInterface>();
	} else {
		return false;
	}

	if (!isdir(isource)) {
		return performCopy(isource, FileInfo{dest, idest.category});
	} else {
		return ftw(isource, [&](const FileInfo &source, FileType type) -> bool {
			auto tmpPath =
					filepath::replace<memory::StandartInterface>(source.path, isource.path, dest);
			auto targetInfo = FileInfo{tmpPath, idest.category};
			if (type == FileType::Dir) {
				if (isource.path == source.path) {
					// root dir
					mkdir(FileInfo{dest, idest.category});
					return true;
				}

				bool ret = mkdir(targetInfo);
				if (stopOnError) {
					return ret;
				} else {
					return true;
				}
			} else if (type == FileType::File) {
				bool ret = performCopy(source, targetInfo);
				if (stopOnError) {
					return ret;
				} else {
					return true;
				}
			}
			return true;
		}, -1, true);
	}
}

bool write(const FileInfo &ipath, const unsigned char *data, size_t len, bool _override) {
	if (ipath.path.empty()) {
		return false;
	}

	bool success = false;
	enumerateWritablePaths(ipath, _override ? Access::None : Access::Empty,
			[&](StringView str, FileFlags) {
		success = filesystem::native::write_fn(str, data, len) == Status::Ok;
		return false;
	});

	return success;
}

File openForReading(const FileInfo &ipath) {
	if (ipath.path.empty()) {
		return File();
	}

	File ret;

	if (hasFlag(getCategoryFlags(ipath.category), CategoryFlags::PlatformSpecific)) {
		return filesystem::platform::_openForReading(ipath.category, ipath.path);
	}

	enumeratePaths(ipath, Access::Read, [&](StringView str, FileFlags) {
		Stat stat;
		filesystem::native::stat_fn(str, stat);

		if (stat.type == FileType::File) {
			auto f = filesystem::native::fopen_fn(str, "rb");
			if (f) {
				ret = File(f);
				return false; // stop iteration
			}
		}
		return true; // try another
	});
	return ret;
}

bool readIntoBuffer(uint8_t *buf, const FileInfo &ipath, size_t off, size_t size) {
	auto f = openForReading(ipath);
	if (f) {
		size_t fsize = f.size();
		if (fsize <= off) {
			f.close();
			return false;
		}
		if (fsize - off < size) {
			size = fsize - off;
		}

		bool ret = true;
		f.seek(off, io::Seek::Set);
		if (f.read(buf, size) != size) {
			ret = false;
		}
		f.close();
		return ret;
	}
	return false;
}

bool readWithConsumer(const io::Consumer &stream, uint8_t *buf, size_t bsize, const FileInfo &ipath,
		size_t off, size_t size) {
	auto f = openForReading(ipath);
	if (f) {
		size_t fsize = f.size();
		if (fsize <= off) {
			f.close();
			return false;
		}
		if (fsize - off < size) {
			size = fsize - off;
		}

		bool ret = true;
		f.seek(off, io::Seek::Set);
		while (size > 0) {
			auto read = min(size, bsize);
			if (f.read(buf, read) == read) {
				stream.write(buf, read);
			} else {
				ret = false;
				break;
			}
			size -= read;
		}
		f.close();
		return ret;
	}
	return false;
}

Access getAccessProtFlags(ProtFlags flags) {
	Access ret = Access::None;

	if (hasFlag(flags, ProtFlags::UserRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::UserWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::UserExecute)) {
		ret |= Access::Execute;
	}
	if (hasFlag(flags, ProtFlags::GroupRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::GroupWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::GroupExecute)) {
		ret |= Access::Execute;
	}
	if (hasFlag(flags, ProtFlags::AllRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::AllWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::AllExecute)) {
		ret |= Access::Execute;
	}
	return ret;
}

std::ostream &operator<<(std::ostream &stream, ProtFlags flags) {
	char buf[11] = "----------";

	if (hasFlag(flags, ProtFlags::AllExecute)) {
		buf[9] = 'x';
	}
	if (hasFlag(flags, ProtFlags::AllWrite)) {
		buf[8] = 'w';
	}
	if (hasFlag(flags, ProtFlags::AllRead)) {
		buf[7] = 'r';
	}
	if (hasFlag(flags, ProtFlags::GroupExecute)) {
		buf[6] = 'x';
	}
	if (hasFlag(flags, ProtFlags::GroupWrite)) {
		buf[5] = 'w';
	}
	if (hasFlag(flags, ProtFlags::GroupRead)) {
		buf[4] = 'r';
	}
	if (hasFlag(flags, ProtFlags::UserExecute)) {
		buf[3] = 'x';
	}
	if (hasFlag(flags, ProtFlags::UserWrite)) {
		buf[2] = 'w';
	}
	if (hasFlag(flags, ProtFlags::UserRead)) {
		buf[1] = 'r';
	}

	stream << buf;
	return stream;
}

std::ostream &operator<<(std::ostream &stream, const Stat &stat) {
	stream << "Stat { size: " << stat.size << "; u: " << stat.user << "; g: " << stat.group << "; "
		   << stat.type << "; " << stat.prot
		   << "; ctime: " << stat.ctime.toHttp<memory::StandartInterface>()
		   << "; mtime: " << stat.mtime.toHttp<memory::StandartInterface>()
		   << "; atime: " << stat.atime.toHttp<memory::StandartInterface>() << " };";
	return stream;
}

} // namespace stappler::filesystem
