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

Status remove_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::remove_fn should be used with absolute paths");
		return Status::Declined;
	}

	int ret = 0;
	path.performWithTerminated([&](const char *path, size_t) { ret = ::remove(path); });
	if (ret == 0) {
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

	int ret = 0;
	path.performWithTerminated([&](const char *path, size_t) { ret = ::unlink(path); });
	if (ret == 0) {
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

	int ret = 0;
	path.performWithTerminated(
			[&](const char *path, size_t) { ret = ::mkdir(path, getModeFormProtFlags(flags)); });
	if (ret == 0) {
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

	int ret = 0;
	path.performWithTerminated([&](const char *path, size_t) { ret = ::utime(path, NULL); });
	if (ret == 0) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status rename_fn(StringView source, StringView dest) {
	int ret = 0;
	source.performWithTerminated([&](const char *sourcePath, size_t) {
		dest.performWithTerminated([&](const char *destPath, size_t) {
			ret = ::rename(sourcePath, destPath); //
		});
	});

	if (ret) {
		return Status::Ok;
	}
	return sprt::status::errnoToStatus(errno);
}

Status write_fn(StringView path, const unsigned char *data, size_t len, ProtFlags flags) {
	Status result = Status::Ok;
	path.performWithTerminated([&](const char *path, size_t) {
		auto fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, getModeFormProtFlags(flags));
		if (fd < 0) {
			result = sprt::status::errnoToStatus(errno);
			return;
		}

		auto ret = ::write(fd, data, len);
		if (ret < 0) {
			result = sprt::status::errnoToStatus(errno);
		} else if (ret != ssize_t(len)) {
			result = Status::Incomplete;
		}
		::close(fd);
	});
	return result;
}

} // namespace stappler::filesystem::native

#endif
