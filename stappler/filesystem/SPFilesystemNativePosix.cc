/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPCore.h"
#include "SPFilesystem.h"
#include "SPStatus.h"

#include <fcntl.h>
#include <stdio.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef WIN32

namespace STAPPLER_VERSIONIZED stappler::filesystem::native {

template <>
memory::PoolInterface::StringType nativeToPosix<memory::PoolInterface>(StringView path) {
	return path.str<memory::PoolInterface>();
}

template <>
memory::PoolInterface::StringType posixToNative<memory::PoolInterface>(StringView path) {
	return path.str<memory::PoolInterface>();
}

template <>
memory::StandartInterface::StringType nativeToPosix<memory::StandartInterface>(StringView path) {
	return path.str<memory::StandartInterface>();
}

template <>
memory::StandartInterface::StringType posixToNative<memory::StandartInterface>(StringView path) {
	return path.str<memory::StandartInterface>();
}

template <>
memory::PoolInterface::StringType getcwd_fn<memory::PoolInterface>() {
	char cwd[1'024] = {0};
	if (getcwd(cwd, 1'024 - 1) != NULL) {
		return memory::PoolInterface::StringType((const char *)cwd);
	}
	return memory::PoolInterface::StringType();
}

template <>
memory::StandartInterface::StringType getcwd_fn<memory::StandartInterface>() {
	char cwd[1'024] = {0};
	if (getcwd(cwd, 1'024 - 1) != NULL) {
		return memory::StandartInterface::StringType((const char *)cwd);
	}
	return memory::StandartInterface::StringType();
}

#define SP_TERMINATED_DATA(view) (view.terminated()?view.data():view.str<memory::StandartInterface>().data())

static ProtFlags getProtFlagsFromMode(mode_t m) {
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

static mode_t getModeFormProtFlags(ProtFlags flags) {
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

Status remove_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::remove_fn should be used with absolute paths");
		return Status::Declined;
	}

	if (::remove(SP_TERMINATED_DATA(path)) == 0) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status unlink_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::unlink_fn should be used with absolute paths");
		return Status::Declined;
	}

	if (::unlink(SP_TERMINATED_DATA(path)) == 0) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status mkdir_fn(StringView path, ProtFlags flags) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::mkdir_fn should be used with absolute paths");
		return Status::Declined;
	}

	Status ret = Status::Ok;
	if (::mkdir(SP_TERMINATED_DATA(path), getModeFormProtFlags(flags)) == 0) {
		ret = Status::Ok;
	} else {
		ret = sprt::status::errnoToStatus(errno);
	}
	return ret;
}

Status access_fn(StringView path, Access mode) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::access_fn should be used with absolute paths");
		return Status::Declined;
	}

	int m = 0;
	if (hasFlag(mode, Access::Execute)) {
		m |= X_OK;
	}
	if (hasFlag(mode, Access::Exists)) {
		m |= F_OK;
	}
	if (hasFlag(mode, Access::Read)) {
		m |= R_OK;
	}
	if (hasFlag(mode, Access::Write)) {
		m |= W_OK;
	}

	if (hasFlag(mode, Access::Empty)) {
		if (m != 0) {
			return Status::ErrorInvalidArguemnt;
		}

		m |= F_OK;
	}

	int flags = 0;
	if (getuid() != geteuid()) {
		flags = AT_EACCESS;
	}

	auto st = ::faccessat(-1, SP_TERMINATED_DATA(path), m, flags);
	if (st == 0) {
		if (hasFlag(mode, Access::Empty)) {
			return Status::Declined; // file exists
		} else {
			return Status::Ok;
		}
	}

	if (hasFlag(mode, Access::Empty) && errno == ENOENT) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status stat_fn(StringView path, Stat &stat) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::stat_fn should be used with absolute paths");
		return Status::Declined;
	}

	struct stat s;
	if (::stat(SP_TERMINATED_DATA(path), &s) == 0) {
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

		stat.atime = Time::microseconds(s.st_atim.tv_sec * 1'000'000 + s.st_atim.tv_nsec / 1'000);
		stat.ctime = Time::microseconds(s.st_ctim.tv_sec * 1'000'000 + s.st_ctim.tv_nsec / 1'000);
		stat.mtime = Time::microseconds(s.st_mtim.tv_sec * 1'000'000 + s.st_mtim.tv_nsec / 1'000);

		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status touch_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::touch_fn should be used with absolute paths");
		return Status::Declined;
	}

	if (::utime(SP_TERMINATED_DATA(path), NULL) == 0) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

static constexpr int OpenDirFlags = O_DIRECTORY | O_RDONLY | O_NDELAY | O_CLOEXEC;

static Status _ftw_fn(int dirfd, StringView path,
		const Callback<bool(StringView, FileType)> &callback, int depth, bool dirFirst) {
	memory::StandartInterface::StringType newPath;

	struct Dir {
		Dir(int dirfd) : dp(::fdopendir(dirfd)) { }
		~Dir() {
			if (dp) {
				::closedir(dp);
			}
		}

		struct dirent *read() { return ::readdir(dp); }

		int getFd() { return ::dirfd(dp); }

		explicit operator bool() const { return dp != nullptr; }

		DIR *dp = nullptr;
	};

	Dir dp(dirfd);
	if (!dp) {
		::close(dirfd);
		auto result = sprt::status::errnoToStatus(errno);
		if (callback(path, FileType::File)) {
			return Status::Ok;
		} else {
			return Status::Suspended;
		}
		return result;
	} else {
		if (dirFirst) {
			if (!callback(path, FileType::Dir)) {
				return Status::Suspended;
			}
		}
		if (depth < 0 || depth > 0) {
			struct dirent *entry;
			while ((entry = dp.read())) {
				FileType type = FileType::Unknown;
				switch (entry->d_type) {
				case DT_BLK: type = FileType::BlockDevice; break;
				case DT_CHR: type = FileType::CharDevice; break;
				case DT_FIFO: type = FileType::Pipe; break;
				case DT_LNK: type = FileType::Link; break;
				case DT_REG: type = FileType::File; break;
				case DT_DIR: type = FileType::Dir; break;
				case DT_SOCK: type = FileType::Socket; break;
				default: break;
				}

				if (strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".") != 0) {
					if (path.empty()) {
						newPath = entry->d_name;
					} else {
						newPath = filepath::merge<memory::StandartInterface>(path, entry->d_name);
					}
					if (type == FileType::Unknown || type == FileType::Dir) {
						auto newDirfd = ::openat(dp.getFd(), entry->d_name, OpenDirFlags);
						if (newDirfd < 0) {
							if (!callback(newPath, FileType::File)) {
								return Status::Suspended;
							}
						} else {
							if (depth == 1) {
								::close(newDirfd);
								if (!callback(newPath, FileType::Dir)) {
									return Status::Suspended;
								}
							} else {
								auto status =
										_ftw_fn(newDirfd, newPath, callback, depth - 1, dirFirst);
								if (status != Status::Ok) {
									return status;
								}
							}
						}
					} else {
						if (!callback(newPath, type)) {
							return Status::Suspended;
						}
					}
				}
			}
		}
		if (!dirFirst) {
			if (!callback(path, FileType::Dir)) {
				return Status::Suspended;
			}
		}
	}
	return Status::Ok;
}

Status ftw_fn(StringView path, const Callback<bool(StringView, FileType)> &callback, int depth,
		bool dirFirst) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::ftw_fn should be used with absolute paths");
		return Status::Declined;
	}

	auto dirfd = ::openat(-1, SP_TERMINATED_DATA(path), OpenDirFlags);
	if (dirfd < 0) {
		return sprt::status::errnoToStatus(errno);
	}

	return _ftw_fn(dirfd, StringView(), callback, depth, dirFirst);
}

Status rename_fn(StringView source, StringView dest) {
	if (::rename(SP_TERMINATED_DATA(source), SP_TERMINATED_DATA(dest)) == 0) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

FILE *fopen_fn(StringView path, StringView mode) {
	return ::fopen(SP_TERMINATED_DATA(path), SP_TERMINATED_DATA(mode));
}

Status write_fn(StringView path, const unsigned char *data, size_t len, ProtFlags flags) {
	auto fd = ::open(SP_TERMINATED_DATA(path), O_WRONLY | O_CREAT | O_TRUNC,
			getModeFormProtFlags(flags));
	if (fd < 0) {
		return sprt::status::errnoToStatus(errno);
	}

	Status result = Status::Ok;
	auto ret = ::write(fd, data, len);
	if (ret < 0) {
		result = sprt::status::errnoToStatus(errno);
	} else if (ret != ssize_t(len)) {
		result = Status::Incomplete;
	}
	::close(fd);
	return result;
}

#undef SP_TERMINATED_DATA

} // namespace stappler::filesystem::native

#endif
