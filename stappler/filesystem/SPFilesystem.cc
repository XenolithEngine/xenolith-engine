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
#include <sys/stat.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem {

ProtFlags getProtFlagsFromMode(mode_t m) {
	ProtFlags flags = ProtFlags::None;
	if (m & S_IRUSR) {
		flags |= ProtFlags::UserRead;
	}
	if (m & S_IWUSR) {
		flags |= ProtFlags::UserWrite;
	}
	if (m & S_IXUSR) {
		flags |= ProtFlags::UserExecute;
	}
	if (m & S_ISUID) {
		flags |= ProtFlags::UserSetId;
	}
	if (m & S_IRGRP) {
		flags |= ProtFlags::GroupRead;
	}
	if (m & S_IWGRP) {
		flags |= ProtFlags::GroupWrite;
	}
	if (m & S_IXGRP) {
		flags |= ProtFlags::GroupExecute;
	}
	if (m & S_ISGID) {
		flags |= ProtFlags::GroupSetId;
	}
	if (m & S_IROTH) {
		flags |= ProtFlags::AllRead;
	}
	if (m & S_IWOTH) {
		flags |= ProtFlags::AllWrite;
	}
	if (m & S_IXOTH) {
		flags |= ProtFlags::AllExecute;
	}
	return flags;
}

mode_t getModeFromProtFlags(ProtFlags flags) {
	mode_t ret = 0;
	if (hasFlag(flags, ProtFlags::UserRead)) {
		ret |= S_IRUSR;
	}
	if (hasFlag(flags, ProtFlags::UserWrite)) {
		ret |= S_IWUSR;
	}
	if (hasFlag(flags, ProtFlags::UserExecute)) {
		ret |= S_IXUSR;
	}
	if (hasFlag(flags, ProtFlags::UserSetId)) {
		ret |= S_ISUID;
	}
	if (hasFlag(flags, ProtFlags::GroupRead)) {
		ret |= S_IRGRP;
	}
	if (hasFlag(flags, ProtFlags::GroupWrite)) {
		ret |= S_IWGRP;
	}
	if (hasFlag(flags, ProtFlags::GroupExecute)) {
		ret |= S_IXGRP;
	}
	if (hasFlag(flags, ProtFlags::GroupSetId)) {
		ret |= S_ISGID;
	}
	if (hasFlag(flags, ProtFlags::AllRead)) {
		ret |= S_IROTH;
	}
	if (hasFlag(flags, ProtFlags::AllWrite)) {
		ret |= S_IWOTH;
	}
	if (hasFlag(flags, ProtFlags::AllExecute)) {
		ret |= S_IXOTH;
	}
	return ret;
}

bool exists(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumeratePaths(info, Access::Exists, [&](const LocationInfo &info, StringView str) {
		found = true;
		return false;
	});
	return found;
}

bool stat(const FileInfo &info, Stat &stat) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumeratePaths(info, Access::Exists, [&](const LocationInfo &info, StringView str) {
		struct __SPRT_STAT_NAME s;
		found = info.interface->_stat(info, str, &s) == Status::Ok;
		if (found) {
			stat.size = size_t(s.st_size);
			if (S_ISBLK(s.st_mode)) {
				stat.type = FileType::BlockDevice;
			} else if (S_ISCHR(s.st_mode)) {
				stat.type = FileType::CharDevice;
			} else if (S_ISDIR(s.st_mode)) {
				stat.type = FileType::Dir;
			} else if (S_ISFIFO(s.st_mode)) {
				stat.type = FileType::Pipe;
			} else if (S_ISREG(s.st_mode)) {
				stat.type = FileType::File;
			} else if (S_ISLNK(s.st_mode)) {
				stat.type = FileType::Link;
			} else if (S_ISSOCK(s.st_mode)) {
				stat.type = FileType::Socket;
			} else {
				stat.type = FileType::Unknown;
			}

			stat.prot = getProtFlagsFromMode(s.st_mode);

			stat.user = s.st_uid;
			stat.group = s.st_gid;

			stat.atime =
					Time::microseconds(s.st_atim.tv_sec * 1'000'000 + s.st_atim.tv_nsec / 1'000);
			stat.ctime =
					Time::microseconds(s.st_ctim.tv_sec * 1'000'000 + s.st_ctim.tv_nsec / 1'000);
			stat.mtime =
					Time::microseconds(s.st_mtim.tv_sec * 1'000'000 + s.st_mtim.tv_nsec / 1'000);
		}
		return false;
	});
	return found;
}

bool remove(const FileInfo &info, bool recursive) {
	if (info.path.empty()) {
		return false;
	}

	if (info.category == FileCategory::Bundled) {
		return false; // we can not remove anything from bundle
	}

	bool found = false;
	enumerateWritablePaths(info, Access::Exists, [&](const LocationInfo &info, StringView str) {
		struct stat st;
		if (info.interface->_stat(info, str, &st) == Status::Ok) {
			if (S_ISDIR(st.st_mode)) {
				if (!recursive) {
					slog().error("filesystem",
							"Fail to remove directory in non recursive mode: ", str);
					found = false;
					return false;
				}

				info.interface->_ftw(info, str, [&](StringView isource, FileType type) {
					sprt::filepath::merge([&](StringView path) {
						info.interface->_remove(info, path);
					}, str, isource);
					return true; //
				}, -1, false);
			} else {
				found = info.interface->_remove(info, str) == Status::Ok;
			}
		};
		return false;
	});
	return found;
}

bool touch(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	if (info.category == FileCategory::Bundled) {
		return false; // we can not remove anything from bundle
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](const LocationInfo &info, StringView str) {
		found = info.interface->_touch(info, str) == Status::Ok;
		return false;
	});
	return found;
}

bool mkdir(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](const LocationInfo &info, StringView str) {
		found = info.interface->_mkdir(info, str, getModeFromProtFlags(ProtFlags::MkdirDefault))
				== Status::Ok;
		return false;
	});
	return found;
}

// We need FileInfo for root constraints
static bool _mkdir_recursive(const LocationInfo &info, StringView path) {
	if (info.path.empty()) {
		return false;
	}

	// check if root dir exists
	auto root = filepath::root(path);
	auto rootInfo = info;
	rootInfo.path = filepath::root(info.path);

	auto err = info.interface->_access(info, root, Access::Exists);
	if (err != Status::Ok) {
		if (!_mkdir_recursive(info, root)) {
			return false;
		}
	}

	return info.interface->_mkdir(info, path, getModeFromProtFlags(ProtFlags::MkdirDefault))
			== Status::Ok;
}

bool mkdir_recursive(const FileInfo &info) {
	if (info.path.empty()) {
		return false;
	}

	bool found = false;
	enumerateWritablePaths(info, Access::None, [&](const LocationInfo &info, StringView str) {
		found = _mkdir_recursive(info, str);
		return false;
	});
	return found;
}

bool ftw(const FileInfo &info, const Callback<bool(const FileInfo &, FileType)> &cb, int depth,
		bool dirFirst) {
	if (filepath::isEmpty(info)) {
		return false;
	}

	auto fn = [&](StringView p, FileType t) -> bool {
		auto tmpPath = filepath::merge<memory::StandartInterface>(info.path, p);
		FileInfo newInfo = info;
		newInfo.path = tmpPath;
		return cb(newInfo, t);
	};

	bool found = false;
	enumeratePaths(info, Access::Exists, [&](const LocationInfo &info, StringView str) {
		found = info.interface->_ftw(info, str, fn, depth, dirFirst) == Status::Ok;
		return false;
	});
	return found;
}

static bool doCopyFile(const LocationInfo &fromLoc, StringView from, const LocationInfo &toLoc,
		StringView to) {
	toLoc.interface->_remove(toLoc, to);

	if (fromLoc.interface == toLoc.interface && fromLoc.interface->_copy) {
		return fromLoc.interface->_copy(fromLoc, from, toLoc, to) == Status::Ok;
	} else {
		auto fFrom = File::open(fromLoc, from, OpenFlags::Read);
		auto fTo =
				File::open(toLoc, to, OpenFlags::Write | OpenFlags::Create | OpenFlags::Truncate);
		if (fFrom && fTo) {
			BufferTemplate<memory::StandartInterface> buffer(std::min(size_t(4_MiB), fFrom.size()));
			if (io::read(io::Producer(fFrom), io::Consumer(fTo), io::Buffer(buffer)) > 0) {
				return true;
			}
		}
	}
	return false;
}

bool move(const FileInfo &isource, const FileInfo &idest) {
	if (isource.path.empty() || idest.path.empty()) {
		return false;
	}

	struct __SPRT_STAT_NAME stat;
	memory::StandartInterface::StringType source;
	const LocationInfo *sourceLoc = nullptr;

	memory::StandartInterface::StringType dest;
	const LocationInfo *destLoc = nullptr;

	enumerateWritablePaths(isource, Access::Exists, [&](const LocationInfo &info, StringView str) {
		info.interface->_stat(info, str, &stat);
		source = str.str<memory::StandartInterface>();
		sourceLoc = &info;
		return false;
	});

	if (source.empty()) {
		return false;
	}

	enumerateWritablePaths(idest, Access::None, [&](const LocationInfo &info, StringView str) {
		dest = str.str<memory::StandartInterface>();
		destLoc = &info;
		return false;
	});

	if (dest.empty()) {
		return false;
	}

	if (sourceLoc->interface == destLoc->interface) {
		if (sourceLoc->interface->_rename(*sourceLoc, source, *destLoc, dest) == Status::Ok) {
			return true;
		}
	}

	if (S_ISDIR(stat.st_mode)) {
		// copy directory recursive
		if (sourceLoc->interface->_ftw(*sourceLoc, source,
					[&](StringView isource, FileType type) {
			auto idest = filepath::replace<memory::StandartInterface>(isource, source, dest);

			if (type == FileType::Dir) {
				return destLoc->interface->_mkdir(*destLoc, idest,
							   getModeFromProtFlags(ProtFlags::MkdirDefault))
						== Status::Ok;
			} else if (type == FileType::File) {
				if (doCopyFile(*sourceLoc, isource, *destLoc, idest)) {
					return sourceLoc->interface->_remove(*sourceLoc, isource) == Status::Ok;
				} else {
					return false;
				}
			}
			return true;
		}, -1, true)
				== Status::Ok) {
			sourceLoc->interface->_ftw(*sourceLoc, source, [&](StringView isource, FileType type) {
				return sourceLoc->interface->_remove(*sourceLoc, isource) == Status::Ok;
			}, -1, true);
			return true;
		}
	} else {
		if (doCopyFile(*sourceLoc, source, *destLoc, dest)) {
			return sourceLoc->interface->_remove(*sourceLoc, source) == Status::Ok;
		}
	}
	return false;
}

// TODO implement 'force' flag
bool copy(const FileInfo &isource, const FileInfo &idest, bool stopOnError) {
	if (filepath::isEmpty(isource.path)) {
		return false;
	}

	struct __SPRT_STAT_NAME sourceStat;
	memory::StandartInterface::StringType source;
	const LocationInfo *sourceLoc = nullptr;

	bool destExists = false;
	struct __SPRT_STAT_NAME destStat;
	memory::StandartInterface::StringType dest;
	const LocationInfo *destLoc = nullptr;

	enumeratePaths(isource, Access::Exists, [&](const LocationInfo &info, StringView str) {
		info.interface->_stat(info, str, &sourceStat);
		source = str.str<memory::StandartInterface>();
		sourceLoc = &info;
		return false;
	});

	if (source.empty()) {
		return false;
	}

	enumerateWritablePaths(idest.category, idest.path, idest.flags | FileFlags::MakeDir,
			Access::None, [&](const LocationInfo &info, StringView str) {
		if (info.interface->_stat(info, str, &destStat) == Status::Ok) {
			destExists = true;
		}
		dest = str.str<memory::StandartInterface>();
		destLoc = &info;
		return false;
	});

	if (!sourceLoc || !destLoc) {
		return false;
	}

	auto sourceLastComponent = filepath::lastComponent(source);
	if (sourceLastComponent.empty()) {
		return false;
	}

	if (dest.back() == '/') {
		// cp sourcedir targetdir/
		// extend dest with the first source component
		dest = filepath::merge<memory::StandartInterface>(dest, sourceLastComponent);
	} else if (destExists && S_ISDIR(destStat.st_mode)
			&& sourceLastComponent != filepath::lastComponent(dest)) {
		dest = filepath::merge<memory::StandartInterface>(dest, sourceLastComponent);
	} else if (destExists) {
		slog().error("filesystem", "Fail to copy '", source, "' to '", dest,
				"': destination exists");
		return false;
	}

	if (!S_ISDIR(sourceStat.st_mode)) {
		return doCopyFile(*sourceLoc, source, *destLoc, dest);
	} else {
		return sourceLoc->interface->_ftw(*sourceLoc, source,
					   [&](StringView isource, FileType type) {
			bool ret = true;
			sprt::filepath::merge([&](StringView idest) {
				if (type == FileType::Dir) {
					auto st = destLoc->interface->_mkdir(*destLoc, idest,
							getModeFromProtFlags(ProtFlags::MkdirDefault));
					if (st != Status::Ok && st != Status::ErrorFileExists) {
						slog().error("filesystem", "Fail to mkdir: ", idest, ": ", st);
						ret = false;
					}
				} else if (type == FileType::File) {
					sprt::filepath::merge([&](StringView isource) {
						if (!doCopyFile(*sourceLoc, isource, *destLoc, idest)) {
							ret = !stopOnError;
						}
					}, source, isource);
				}
			}, dest, isource);
			return ret;
		}, -1, true)
				== Status::Ok;
	}
}

bool write(const FileInfo &ipath, const unsigned char *data, size_t len, bool _override) {
	if (ipath.path.empty()) {
		return false;
	}

	bool success = false;
	enumerateWritablePaths(ipath, _override ? Access::None : Access::Empty,
			[&](const LocationInfo &info, StringView str) {
		success = info.interface->_write_oneshot(info, str, data, len, _override,
						  getModeFromProtFlags(ProtFlags::Default))
				== Status::Ok;
		return false;
	});

	return success;
}

File openForReading(const FileInfo &ipath) {
	if (ipath.path.empty()) {
		return File();
	}

	return File::open(ipath, OpenFlags::Read);
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

StringView detectMimeType(StringView path) {
	auto ext = filepath::lastExtension(path);
	if (!ext.empty()) {
		auto type = sprt::filepath::getMimeTypeForExtension(ext);
		if (!type.empty()) {
			return type;
		}
	}

#if MODULE_STAPPLER_BITMAP
	decltype(static_cast<Pair<bitmap::FileFormat, StringView> (*)(const FileInfo &)>(
			bitmap::detectFormat)) bitmap_detectFormat;

	decltype(static_cast<StringView (*)(bitmap::FileFormat)>(
			bitmap::getMimeType)) bitmap_getMimeType1;
	decltype(static_cast<StringView (*)(StringView)>(bitmap::getMimeType)) bitmap_getMimeType2;

	bitmap_detectFormat = SharedModule::acquireTypedSymbol<decltype(bitmap_detectFormat)>(
			buildconfig::MODULE_STAPPLER_BITMAP_NAME, "detectFormat");
	bitmap_getMimeType1 = SharedModule::acquireTypedSymbol<decltype(bitmap_getMimeType1)>(
			buildconfig::MODULE_STAPPLER_BITMAP_NAME, "getMimeType");
	bitmap_getMimeType2 = SharedModule::acquireTypedSymbol<decltype(bitmap_getMimeType2)>(
			buildconfig::MODULE_STAPPLER_BITMAP_NAME, "getMimeType");
	if (!bitmap_detectFormat || !bitmap_getMimeType1 || !bitmap_getMimeType2) {
		log::source().error("filesystem",
				"Module MODULE_STAPPLER_BITMAP declared, but not available in runtime");
		return StringView();
	}

	// try image format
	auto fmt = bitmap_detectFormat(FileInfo(path));
	if (fmt.first != bitmap::FileFormat::Custom) {
		return bitmap_getMimeType1(fmt.first);
	} else {
		return bitmap_getMimeType2(fmt.second);
	}
#endif
	return StringView();
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
