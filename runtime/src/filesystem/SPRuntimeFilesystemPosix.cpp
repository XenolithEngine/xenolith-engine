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

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_utime.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_limits.h>
#include <sprt/c/sys/__sprt_mman.h>
#include <string.h>

namespace sprt::filesystem {

struct MmapStorage {
	uint64_t length;
	uint64_t offset;
};

template <typename Callback>
static void performWithPath(const LocationInfo &loc, StringView path, const Callback &cb) {
	if (filepath::isAbsolute(path)) {
		path.performWithTerminated(cb);
	} else {
		filepath::merge([&](StringView ipath) { ipath.performWithTerminated(cb); }, loc.path, path);
	}
}

static constexpr int OpenDirFlags =
		__SPRT_O_DIRECTORY | __SPRT_O_RDONLY | __SPRT_O_NDELAY | __SPRT_O_CLOEXEC;

static Status _ftw_fn(int dirfd, const callback<bool(StringView, FileType)> &callback, int depth,
		bool dirFirst, const char *origBuf, char *buf, size_t bufMax) {

	struct Dir {
		Dir(int dirfd) : dp(::__sprt_fdopendir(dirfd)) { }
		~Dir() {
			if (dp) {
				::__sprt_closedir(dp);
			}
		}

		struct dirent *read() { return ::__sprt_readdir(dp); }

		int getFd() { return ::__sprt_dirfd(dp); }

		explicit operator bool() const { return dp != nullptr; }

		__sprt_DIR *dp = nullptr;
	};

	Dir dp(dirfd);

	if (!dp) {
		::__sprt_close(dirfd);
		auto result = sprt::status::errnoToStatus(__sprt_errno);
		buf[0] = 0;
		if (callback(StringView(origBuf, buf - origBuf), FileType::File)) {
			return Status::Ok;
		} else {
			return Status::Suspended;
		}
		return result;
	} else {
		if (dirFirst) {
			buf[0] = 0;
			if (!callback(StringView(origBuf, buf - origBuf), FileType::Dir)) {
				return Status::Suspended;
			}
		}
		if (depth < 0 || depth > 0) {
			struct dirent *entry;
			while ((entry = dp.read())) {
				FileType type = FileType::Unknown;
				switch (entry->d_type) {
				case __SPRT_DT_BLK: type = FileType::BlockDevice; break;
				case __SPRT_DT_CHR: type = FileType::CharDevice; break;
				case __SPRT_DT_FIFO: type = FileType::Pipe; break;
				case __SPRT_DT_LNK: type = FileType::Link; break;
				case __SPRT_DT_REG: type = FileType::File; break;
				case __SPRT_DT_DIR: type = FileType::Dir; break;
				case __SPRT_DT_SOCK: type = FileType::Socket; break;
				default: break;
				}

				if (strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".") != 0) {
					size_t bufSize = bufMax;
					auto newBufEnd = buf;
					if (buf != origBuf) {
						newBufEnd = strappend(newBufEnd, &bufSize, "/", 1);
					}
					newBufEnd =
							strappend(newBufEnd, &bufSize, entry->d_name, strlen(entry->d_name));
					if (bufSize == 0) {
						return Status::ErrorBufferOverflow;
					}
					newBufEnd[0] = 0;

					if (type == FileType::Unknown || type == FileType::Dir) {
						auto newDirfd = ::__sprt_openat(dp.getFd(), entry->d_name, OpenDirFlags);
						if (newDirfd < 0) {
							if (!callback(StringView(origBuf, newBufEnd - origBuf),
										FileType::File)) {
								return Status::Suspended;
							}
						} else {
							if (depth == 1) {
								::__sprt_close(newDirfd);
								if (!callback(StringView(origBuf, newBufEnd - origBuf),
											FileType::Dir)) {
									return Status::Suspended;
								}
							} else {
								auto status = _ftw_fn(newDirfd, callback, depth - 1, dirFirst,
										origBuf, newBufEnd, bufSize);
								if (status != Status::Ok) {
									return status;
								}
							}
						}
					} else {
						if (!callback(StringView(origBuf, newBufEnd - origBuf), type)) {
							return Status::Suspended;
						}
					}
				}
			}
		}
		if (!dirFirst) {
			buf[0] = 0;
			if (!callback(StringView(origBuf, buf - origBuf), FileType::Dir)) {
				return Status::Suspended;
			}
		}
	}
	return Status::Ok;
}

static LocationInterface s_defaultInterface = {
	._access = [](const LocationInfo &loc, StringView path, Access mode) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		int m = 0;
		if (hasFlag(mode, Access::Execute)) {
			m |= __SPRT_X_OK;
		}
		if (hasFlag(mode, Access::Exists)) {
			m |= __SPRT_F_OK;
		}
		if (hasFlag(mode, Access::Read)) {
			m |= __SPRT_R_OK;
		}
		if (hasFlag(mode, Access::Write)) {
			m |= __SPRT_W_OK;
		}

		if (hasFlag(mode, Access::Empty)) {
			if (m != 0) {
				st = Status::ErrorInvalidArguemnt;
				return;
			}

			m |= __SPRT_F_OK;
		}

		int flags = 0;
		if (__sprt_getuid() != __sprt_geteuid()) {
			flags = __SPRT_AT_EACCESS;
		}

		auto ret = __sprt_faccessat(-1, tpath, m, flags);
		if (ret == 0) {
			if (hasFlag(mode, Access::Empty)) {
				st = Status::Declined; // file exists
				return;
			} else {
				st = Status::Ok;
				return;
			}
		}

		if (hasFlag(mode, Access::Empty) && __sprt_errno == ENOENT) {
			st = Status::Ok;
		} else {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._stat = [](const LocationInfo &loc, StringView path,
					 struct __SPRT_STAT_NAME *_stat) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		if (__sprt_stat(tpath, _stat) != 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._open = [](const LocationInfo &loc, StringView path, OpenFlags flags, Status *st) -> void * {
	void *res = nullptr;

	performWithPath(loc, path, [&](const char *tpath, size_t) {
		int f = 0;
		if (hasFlag(flags, OpenFlags::Read) && hasFlag(flags, OpenFlags::Write)) {
			f |= __SPRT_O_RDWR;
		} else if (hasFlag(flags, OpenFlags::Read)) {
			f |= __SPRT_O_RDONLY;
		} else if (hasFlag(flags, OpenFlags::Write)) {
			f |= __SPRT_O_WRONLY;
		}

		if (hasFlag(flags, OpenFlags::Create)) {
			f |= __SPRT_O_CREAT;
		}
		if (hasFlag(flags, OpenFlags::Append)) {
			f |= __SPRT_O_APPEND;
		}
		if (hasFlag(flags, OpenFlags::Truncate)) {
			f |= __SPRT_O_TRUNC;
		}
		if (hasFlag(flags, OpenFlags::CreateExclusive)) {
			f |= __SPRT_O_EXCL;
		}

		auto fd = ::__sprt_open(tpath, f,
				__SPRT_S_IRUSR | __SPRT_S_IWUSR | __SPRT_S_IRGRP | __SPRT_S_IROTH);
		if (fd < 0) {
			if (st) {
				*st = sprt::status::errnoToStatus(__sprt_errno);
			}
		} else {
			if (st) {
				*st = Status::Ok;
			}
			res = reinterpret_cast<void *>(static_cast<intptr_t>(fd));
		}
	});

	return res;
},
	._read = [](void *ptr, uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	auto ret = ::__sprt_read(fd, buf, nbytes);
	if (ret < 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
	return ret;
},
	._write = [](void *ptr, const uint8_t *buf, size_t nbytes, Status *st) -> size_t {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	auto ret = ::__sprt_write(fd, buf, nbytes);
	if (ret < 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
	return ret;
},
	._seek = [](void *ptr, int64_t offset, int w, Status *st) -> size_t {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	auto ret = ::__sprt_lseek(fd, offset, w);
	if (ret < 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
	return ret;
},
	._tell = [](void *ptr, Status *st) -> size_t {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	auto ret = ::__sprt_lseek(fd, 0, __SPRT_SEEK_CUR);
	if (ret < 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
	return ret;
},
	._flush =
			[](void *ptr, Status *st) {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	if (::__sprt_fsync(fd) != 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
},

	._close =
			[](void *ptr, Status *st) {
	auto fd = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
	if (::__sprt_close(fd) != 0) {
		if (st) {
			*st = sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	if (st) {
		*st = Status::Ok;
	}
},

	._unlink = [](const LocationInfo &loc, StringView path) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		if (__sprt_unlink(tpath) != 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._remove = [](const LocationInfo &loc, StringView path) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		if (__sprt_remove(tpath) != 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._touch = [](const LocationInfo &loc, StringView path) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		if (__sprt_utime(tpath, nullptr) != 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._mkdir = [](const LocationInfo &loc, StringView path, __sprt_mode_t mode) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		if (__sprt_mkdir(tpath, mode) != 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
		}
	});
	return st;
},

	._rename = [](const LocationInfo &loc1, StringView from, const LocationInfo &loc2,
					   StringView to) -> Status {
	Status st = Status::Ok;
	performWithPath(loc1, from, [&](const char *tfrom, size_t) {
		performWithPath(loc2, to, [&](const char *tto, size_t) {
			if (::__sprt_rename(tfrom, tto) != 0) {
				st = sprt::status::errnoToStatus(__sprt_errno);
			}
		});
	});
	return st;
},
	/*
	._copy = [](const LocationInfo &, StringView from, const LocationInfo &,
					 StringView to) -> Status {},
					 */
	._copy = nullptr,

	._ftw = [](const LocationInfo &loc, StringView path,
					const callback<bool(StringView, FileType)> &cb, int depth,
					bool dirFirst) -> Status {
	Status st = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		auto dirfd = ::__sprt_openat(-1, tpath, OpenDirFlags);
		if (dirfd < 0) {
			st = sprt::status::errnoToStatus(__sprt_errno);
			return;
		}

		auto pathConfMax = ::__sprt_fpathconf(dirfd, __SPRT_PC_PATH_MAX);
		auto pathMax =
				sprt::max(size_t(pathConfMax > 0 ? pathConfMax : 0), size_t(__SPRT_PATH_MAX));

		auto bufSize = __SPRT_PATH_MAX + pathMax;

		auto workBuf = (char *)__sprt_malloca(bufSize);

		st = _ftw_fn(dirfd, cb, depth, dirFirst, workBuf, workBuf, bufSize);

		__sprt_freea(workBuf);
	});
	return st;
},

	._write_oneshot = [](const LocationInfo &loc, StringView path, const uint8_t *buf,
							  size_t nbytes, __sprt_mode_t mode, bool override) -> Status {
	Status result = Status::Ok;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		auto openflags = __SPRT_O_WRONLY | __SPRT_O_CREAT | __SPRT_O_TRUNC;
		if (!override) {
			openflags |= __SPRT_O_EXCL;
		}

		auto fd = ::__sprt_open(tpath, openflags, mode);
		if (fd < 0) {
			result = sprt::status::errnoToStatus(__sprt_errno);
			return;
		}

		auto ret = ::__sprt_write(fd, buf, nbytes);
		if (ret < 0) {
			result = sprt::status::errnoToStatus(__sprt_errno);
		} else if (ret != ssize_t(nbytes)) {
			result = Status::Incomplete;
		}
		::__sprt_close(fd);
	});
	return result;
},

	._mmap = [](uint8_t storage[16], const LocationInfo &loc, StringView path, MappingType type,
					 __sprt_mode_t prot, size_t offset, size_t len, Status *st) -> uint8_t * {
	uint8_t *ret = nullptr;
	performWithPath(loc, path, [&](const char *tpath, size_t) {
		int oFlags = 0;
		if ((prot & __SPRT_S_IROTH) != 0) {
			if ((prot & __SPRT_S_IWOTH) != 0) {
				oFlags = __SPRT_O_RDWR;
			} else {
				oFlags = __SPRT_O_RDONLY;
			}
		} else if ((prot & __SPRT_S_IWOTH) != 0) {
			oFlags = __SPRT_O_WRONLY;
		}

		int fd = ::__sprt_open(tpath, oFlags);

		if (fd < 0) {
			if (st) {
				*st = sprt::status::errnoToStatus(__sprt_errno);
			}
			return;
		}

		int mProt = 0;
		if ((prot & __SPRT_S_IROTH) != 0) {
			mProt = __SPRT_PROT_READ;
		}
		if ((prot & __SPRT_S_IWOTH) != 0) {
			mProt = __SPRT_PROT_WRITE;
		}
		if ((prot & __SPRT_S_IXOTH) != 0) {
			mProt = __SPRT_PROT_EXEC;
		}

		int mFlags = 0;
		switch (type) {
		case MappingType::Private: mFlags = __SPRT_MAP_PRIVATE; break;
		case MappingType::Shared: mFlags = __SPRT_MAP_SHARED; break;
		}

		auto ptr = ::__sprt_mmap(nullptr, len, mProt, mFlags, fd, (off_t)offset);

		::__sprt_close(fd); // Pre-close file, God bless POSIX

		if (ptr != __SPRT_MAP_FAILED) {
			auto s = (MmapStorage *)&storage[0];
			s->length = len;
			s->offset = offset;
			ret = (uint8_t *)ptr;
			if (st) {
				*st = Status::Ok;
			}
		} else {
			if (st) {
				*st = sprt::status::errnoToStatus(__sprt_errno);
			}
		}
	});
	return ret;
},
	._munmap = [](uint8_t *region, uint8_t storage[16]) -> Status {
	if (region) {
		auto s = (MmapStorage *)&storage[0];
		if (::__sprt_munmap(region, size_t(s->length)) != 0) {
			return sprt::status::errnoToStatus(__sprt_errno);
		}
		return Status::Ok;
	}
	return Status::ErrorInvalidArguemnt;
},
	._msync = [](uint8_t *region, uint8_t storage[16]) -> Status {
	if (region) {
		auto s = (MmapStorage *)&storage[0];
		if (::__sprt_msync(region, size_t(s->length), __SPRT_MS_SYNC) != 0) {
			return sprt::status::errnoToStatus(__sprt_errno);
		}
	}
	return Status::ErrorInvalidArguemnt;
},
};

SPRT_API const LocationInterface *getDefaultInterface() { return &s_defaultInterface; }

} // namespace sprt::filesystem
