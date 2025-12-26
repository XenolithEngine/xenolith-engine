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

#include "SPBytesView.h"
#include "SPCore.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPMemInterface.h"
#include "SPStatus.h"
#include "SPString.h"

#ifdef WIN32

#include "SPPlatformUnistd.h"
#include <handleapi.h>
#include <sys/stat.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem::native {

static void nativeToPosix_c(char *path, size_t size) {
	if (size >= 2) {
		char first = path[0];
		char second = path[1];

		if ((('a' <= first && first <= 'z') || ('A' <= first && first <= 'Z')) && second == ':') {
			path[0] = '/';
			path[1] = tolower(first);
		}
	}

	for (size_t i = 0; i < size; ++i) {
		if (path[i] == '\\') {
			path[i] = '/';
		}
	}
}

static void posixToNative_c(char *path, size_t size) {
	if (path[0] == '/' && size >= 2) {
		path[0] = toupper(path[1]);
		path[1] = ':';
	}

	for (size_t i = 0; i < size; ++i) {
		if (path[i] == '/') {
			path[i] = '\\';
		}
	}
}

template <>
memory::PoolInterface::StringType nativeToPosix<memory::PoolInterface>(StringView ipath) {
	memory::PoolInterface::StringType path(ipath.str<memory::PoolInterface>());
	nativeToPosix_c(path.data(), path.size());
	return path;
}

template <>
memory::StandartInterface::StringType nativeToPosix<memory::StandartInterface>(StringView ipath) {
	memory::StandartInterface::StringType path(ipath.str<memory::StandartInterface>());
	nativeToPosix_c(path.data(), path.size());
	return path;
}

template <>
memory::PoolInterface::StringType posixToNative<memory::PoolInterface>(StringView ipath) {
	memory::PoolInterface::StringType path(ipath.str<memory::PoolInterface>());
	posixToNative_c(path.data(), path.size());
	return path;
}

template <>
memory::StandartInterface::StringType posixToNative<memory::StandartInterface>(StringView ipath) {
	memory::StandartInterface::StringType path(ipath.str<memory::StandartInterface>());
	posixToNative_c(path.data(), path.size());
	return path;
}

template <>
memory::PoolInterface::StringType getcwd_fn<memory::PoolInterface>() {
	wchar_t cwd[1'024] = {0};
	if (_wgetcwd(cwd, 1'024 - 1) != NULL) {
		return nativeToPosix<memory::PoolInterface>(
				string::toUtf8<memory::PoolInterface>((const char16_t *)cwd));
	}
	return memory::PoolInterface::StringType();
}

template <>
memory::StandartInterface::StringType getcwd_fn<memory::StandartInterface>() {
	wchar_t cwd[1'024] = {0};
	if (_wgetcwd(cwd, 1'024 - 1) != NULL) {
		return nativeToPosix<memory::StandartInterface>(
				string::toUtf8<memory::StandartInterface>((const char16_t *)cwd));
	}
	return memory::StandartInterface::StringType();
}

Status remove_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::stat_fn should be used with absolute paths");
		return Status::Declined;
	}

	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	if (_wremove((wchar_t *)str.c_str()) == 0) {
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

Status unlink_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::stat_fn should be used with absolute paths");
		return Status::Declined;
	}

	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	if (_wunlink((wchar_t *)str.c_str()) == 0) {
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

Status mkdir_fn(StringView path, ProtFlags flags) {
	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	int oldmask, tmp;
	int newmask = 0;

	if (!hasFlag(flags, ProtFlags::UserRead)) {
		newmask |= _S_IREAD;
	}
	if (!hasFlag(flags, ProtFlags::UserWrite)) {
		newmask |= _S_IWRITE;
	}

	_umask_s(newmask, &oldmask);
	bool ret = _wmkdir((wchar_t *)str.c_str()) == 0;
	_umask_s(oldmask, &tmp);

	if (ret) {
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

Status access_fn(StringView path, Access mode) {
	// https://learn.microsoft.com/ru-ru/cpp/c-runtime-library/reference/access-waccess?view=msvc-170
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::access_fn should be used with absolute paths");
		return Status::Declined;
	}

	int m = 0;
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

	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	auto st = _waccess((wchar_t *)str.c_str(), m);
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
	return status::errnoToStatus(errno);
}

Status stat_fn(StringView path, Stat &stat) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::stat_fn should be used with absolute paths");
		return Status::Declined;
	}

	auto str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));

	struct _stat64 s;
	if (_wstat64((wchar_t *)str.c_str(), &s) == 0) {
		stat.size = size_t(s.st_size);

		if (hasFlag(s.st_mode, uint16_t(_S_IFCHR))) {
			stat.type = FileType::CharDevice;
		} else if (hasFlag(s.st_mode, uint16_t(_S_IFDIR))) {
			stat.type = FileType::Dir;
		} else if (hasFlag(s.st_mode, uint16_t(_S_IFIFO))) {
			stat.type = FileType::Pipe;
		} else if (hasFlag(s.st_mode, uint16_t(_S_IFREG))) {
			stat.type = FileType::File;
		} else {
			stat.type = FileType::Unknown;
		}

		if (s.st_mode & _S_IREAD) {
			stat.prot |= ProtFlags::UserRead;
		}
		if (s.st_mode & _S_IWRITE) {
			stat.prot |= ProtFlags::UserWrite;
		}
		if (s.st_mode & _S_IEXEC) {
			stat.prot |= ProtFlags::UserExecute;
		}

		stat.user = s.st_uid;
		stat.group = s.st_gid;

		stat.atime = Time::seconds(s.st_atime);
		stat.ctime = Time::seconds(s.st_ctime);
		stat.mtime = Time::seconds(s.st_mtime);
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

Status touch_fn(StringView path) {
	if (!path.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::touch_fn should be used with absolute paths");
		return Status::Declined;
	}

	auto str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	if (_wutime((wchar_t *)str.c_str(), NULL) == 0) {
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

struct FtwHandle {
	static constexpr size_t PathBufferSize = NTFS_MAX_PATH + NAME_MAX + 8;

	wchar_t pathBuffer[NTFS_MAX_PATH + NAME_MAX + 8];
	char reparseBuffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
};

static int __islink(FtwHandle &ftw, const wchar_t *name) {
	DWORD io_result = 0;
	DWORD bytes_returned = 0;
	HANDLE hFile = CreateFileW(name, 0, 0, NULL, OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		return 0;
	}

	io_result = DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, ftw.reparseBuffer,
			MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &bytes_returned, NULL);

	CloseHandle(hFile);

	if (io_result == 0) {
		return 0;
	}

	return ((REPARSE_GUID_DATA_BUFFER *)ftw.reparseBuffer)->ReparseTag == IO_REPARSE_TAG_SYMLINK;
}

struct DirHandle {
	DirHandle(FtwHandle &h, StringView path) {
		auto nativePath = native::posixToNative<memory::StandartInterface>(path);
		ftw = &h;
		size_t len = 0;
		unicode::toUtf16((char16_t *)&ftw->pathBuffer[0], FtwHandle::PathBufferSize, nativePath,
				&len);
		ftw->pathBuffer[len] = L'\\';
		ftw->pathBuffer[len + 1] = L'*';
		ftw->pathBuffer[len + 2] = 0;

		wpath = WideStringView((char16_t *)&ftw->pathBuffer[0], len);
		hFind = FindFirstFileW(ftw->pathBuffer, &ffd);
		updateCurrentFile();
	}

	DirHandle(FtwHandle &h, DirHandle &d) {
		ftw = &h;

		size_t len = (d.currentName.data() + d.currentName.size()) - d.wpath.data();
		wpath = WideStringView((char16_t *)&ftw->pathBuffer[0], len);
		ftw->pathBuffer[len] = L'\\';
		ftw->pathBuffer[len + 1] = L'*';
		ftw->pathBuffer[len + 2] = 0;

		hFind = FindFirstFileW(ftw->pathBuffer, &ffd);
		updateCurrentFile();
	}

	~DirHandle() {
		if (hFind != INVALID_HANDLE_VALUE) {
			FindClose(hFind);
		}
	}

	void updateCurrentFile() {
		if (hFind != INVALID_HANDLE_VALUE) {
			auto len = wcslen(ffd.cFileName);
			auto targetLen = wpath.size();
			ftw->pathBuffer[targetLen] = L'\\';
			memcpy(&ftw->pathBuffer[targetLen + 1], ffd.cFileName, len * sizeof(wchar_t));
			ftw->pathBuffer[targetLen + 1 + len] = 0;

			currentName = WideStringView((char16_t *)&ftw->pathBuffer[targetLen + 1], len);

			currentType = FileType::File;
			if (((ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
					&& __islink(*ftw, ftw->pathBuffer)) {
				currentType = FileType::Link;
			} else if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0) {
				currentType = FileType::CharDevice;
			} else if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				currentType = FileType::Dir;
			}
		}
	}

	explicit operator bool() const { return hFind != INVALID_HANDLE_VALUE; }

	bool read() {
		if (FindNextFileW(hFind, &ffd)) {
			updateCurrentFile();
			return true;
		}
		return false;
	}

	FtwHandle *ftw = nullptr;
	WideStringView wpath;
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	WideStringView currentName;
	FileType currentType = FileType::Unknown;
};

static Status _ftw_fn(FtwHandle &ftw, DirHandle &handle, StringView path,
		const Callback<bool(StringView, FileType)> &callback, int depth, bool dirFirst) {
	memory::StandartInterface::StringType newPath;

	if (!handle) {
		auto result = status::errnoToStatus(errno);
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
			do {
				if (handle.currentName != u".." && handle.currentName != u".") {
					auto dname = string::toUtf8<memory::StandartInterface>(handle.currentName);
					if (path.empty()) {
						newPath = dname;
					} else {
						newPath = filepath::merge<memory::StandartInterface>(path, dname);
					}
					if (handle.currentType == FileType::Dir) {
						DirHandle nextDir(ftw, handle);
						if (!nextDir) {
							if (!callback(newPath, FileType::File)) {
								return Status::Suspended;
							}
						} else {
							if (depth == 1) {
								if (!callback(newPath, FileType::Dir)) {
									return Status::Suspended;
								}
							} else {
								auto status = _ftw_fn(ftw, nextDir, newPath, callback, depth - 1,
										dirFirst);
								if (status != Status::Ok) {
									return status;
								}
							}
						}
					} else {
						if (!callback(newPath, handle.currentType)) {
							return Status::Suspended;
						}
					}
				}
			} while (handle.read());
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

	auto ftw = new FtwHandle;
	DirHandle dir(*ftw, path);
	if (!dir) {
		return status::lastErrorToStatus(GetLastError());
	}

	auto ret = _ftw_fn(*ftw, dir, StringView(), callback, depth, dirFirst);
	delete ftw;
	return ret;
}

Status rename_fn(StringView source, StringView dest) {
	memory::StandartInterface::WideStringType wsource = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(source));
	memory::StandartInterface::WideStringType wdest = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(dest));
	if (_wrename((const wchar_t *)wsource.c_str(), (const wchar_t *)wdest.c_str()) == 0) {
		return Status::Ok;
	}
	return status::errnoToStatus(errno);
}

FILE *fopen_fn(StringView path, StringView mode) {
	FILE *ret = nullptr;
	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(path));
	memory::StandartInterface::WideStringType wmode =
			string::toUtf16<memory::StandartInterface>(mode);
	_wfopen_s(&ret, (const wchar_t *)str.c_str(), (const wchar_t *)wmode.c_str());
	return ret;
}

Status write_fn(StringView ipath, const unsigned char *data, size_t len, ProtFlags flags) {
	if (!ipath.starts_with("/")) {
		log::source().error("filesystem",
				"filesystem::native::write_fn should be used with absolute paths");
		return Status::Declined;
	}

	memory::StandartInterface::WideStringType str = string::toUtf16<memory::StandartInterface>(
			posixToNative<memory::StandartInterface>(ipath));

	HANDLE fileToWrite = INVALID_HANDLE_VALUE;
	fileToWrite = CreateFileW((const wchar_t *)str.data(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (fileToWrite != INVALID_HANDLE_VALUE) {
		DWORD outLen = 0;
		auto ret = WriteFile(fileToWrite, data, len, &outLen, nullptr);
		CloseHandle(fileToWrite);
		if (ret && outLen == len) {
			return Status::Ok;
		} else if (!ret) {
			return status::lastErrorToStatus(GetLastError());
		} else {
			return Status::Incomplete;
		}
	}
	return status::lastErrorToStatus(GetLastError());
}

} // namespace stappler::filesystem::native

#endif
