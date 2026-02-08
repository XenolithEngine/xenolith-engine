/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sprt/c/sys/__sprt_utsname.h>

#include <sprt/runtime/platform.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/filesystem/filepath.h>

#include "src/pthread.h"

void performUnameTest() {
	struct utsname uname;
	__sprt_uname(&uname);

	printf("uname.sysname: %s\n", uname.sysname);
	printf("uname.nodename: %s\n", uname.nodename);
	printf("uname.release: %s\n", uname.release);
	printf("uname.version: %s\n", uname.version);
	printf("uname.machine: %s\n", uname.machine);
	printf("uname.domainname: %s\n", uname.domainname);
}

void performUnistdTest() {
	printf("getuid: %u\n", getuid());
	printf("getgid: %u\n", getgid());
	printf("geteuid: %u\n", geteuid());
	printf("getegid: %u\n", getegid());
	printf("getpid: %u\n", getpid());
	printf("gettid: %u\n", gettid());
	printf("sysconf(SC_PAGESIZE): %u\n", sysconf(SC_PAGESIZE));
	printf("pathconf(PC_NAME_MAX): %u\n", pathconf("/", PC_NAME_MAX));
	printf("pathconf(PC_PATH_MAX): %u\n", pathconf("/", PC_PATH_MAX));
	printf("pathconf(PC_LINK_MAX): %u\n", pathconf("/", PC_LINK_MAX));

	auto buf = getcwd(nullptr, 0);
	printf("getcwd: %s\n", buf);
	free(buf);
};

void performDirTest() {
	auto rootDir = opendir("/");

	while (auto dirent = readdir(rootDir)) {
		printf("readdir: %s %s %ld\n", "/", dirent->d_name, telldir(rootDir)); //
	}
	closedir(rootDir);

#if SPRT_WINDOWS
	auto driveDir = opendir("/c");
	while (auto dirent = readdir(driveDir)) {
		printf("readdir: %s %s %ld\n", "/c", dirent->d_name, telldir(driveDir)); //
	}
	closedir(driveDir);
#endif

	auto buf = getcwd(nullptr, 0);
	auto dir = opendir(buf);

	// seekpos is opaque
	long seekpos = 0;
	int seekoff = 10;

	while (auto dirent = readdir(dir)) {
		if (--seekoff > 0) {
			seekpos = telldir(dir);
		}
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("rewinddir\n");
	rewinddir(dir);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("seekdir backward\n");
	seekdir(dir, seekpos);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("seekdir forward\n");
	rewinddir(dir);
	seekdir(dir, seekpos);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	closedir(dir);

	free(buf);
}

static void removeFileAt(const char *dirPath, const char *fileName) {
	sprt::filepath::merge([&](sprt::StringView filepath) {
		filepath.performWithTerminated([&](const char *cFilePath, size_t) { remove(cFilePath); });
	}, dirPath, fileName);
}

void performFileLinkatTest(int dirfd, const char *dirPath, const char *originalFileName,
		const char *linkFileName) {
	linkat(dirfd, linkFileName, dirfd, "ссылка1.txt", 0);
	linkat(dirfd, linkFileName, dirfd, "ссылка2.txt", AT_SYMLINK_FOLLOW);

	removeFileAt(dirPath, "ссылка1.txt");
	removeFileAt(dirPath, "ссылка2.txt");
}

void performFileLinkTest(const char *originalFilePath, const char *linkFilePath) {
	struct stat stOrig;
	struct stat stLink;
	struct stat stlLink;
	stat(originalFilePath, &stOrig);
	stat(linkFilePath, &stLink);
	lstat(linkFilePath, &stlLink);

	printf("stat: %lld %lld %lld\n", stOrig.st_ino, stLink.st_ino, stlLink.st_ino);
}

void performDirLinkTest(const char *originalDirPath, const char *linkDirPath) {
	auto rp = realpath(linkDirPath, nullptr);
	printf("realpath: %s\n", rp);
	free(rp);

	auto dirfd = open(linkDirPath, O_PATH | O_DIRECTORY | O_NOFOLLOW);
	if (dirfd >= 0) {
		sprt::log::vperror(__SPRT_LOCATION, "main", "Open symlink with O_NOFOLLOW should fail");
	}

	dirfd = open(originalDirPath, O_PATH | O_DIRECTORY | O_NOFOLLOW);
	if (dirfd < 0) {
		sprt::log::vperror(__SPRT_LOCATION, "main", "Open dir failed");
	}

	auto fd = openat(dirfd, "testfile.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);

	auto content = "TestFileContent\n";
	write(fd, content, strlen(content));
	close(fd);

	sprt::filepath::merge([&](sprt::StringView filepath) {
		filepath.performWithTerminated([&](const char *cFilePath, size_t) {
			sprt::filepath::merge([&](sprt::StringView filepath) {
				filepath.performWithTerminated([&](const char *linkFilePath, size_t) {
					auto ret = symlink(cFilePath, linkFilePath);
					if (ret != 0) {
						sprt::log::vperror(__SPRT_LOCATION, "main",
								"fail to symlink file: ", sprt::status::errnoToStatus(errno));
					} else {
						performFileLinkatTest(dirfd, originalDirPath, "testfile.txt",
								"testfile_link.txt");
						performFileLinkTest(cFilePath, linkFilePath);
						remove(linkFilePath); //
					}
				});
			}, originalDirPath, "testfile_link.txt");

			if (remove(cFilePath) != 0) {
				sprt::log::vperror(__SPRT_LOCATION, "main", "fail to remove file",
						sprt::status::errnoToStatus(errno));
			}
		});
	}, originalDirPath, "testfile.txt");

	close(dirfd);
}

void performLinkTest() {
	auto dirPath = sprt::filepath::root(sprt::platform::getExecPath());

	int dirfd = -1;
	dirPath.performWithTerminated([&](const char *cDirPath, size_t) {
		dirfd = open(cDirPath, O_PATH | O_DIRECTORY); //
	});
	if (mkdirat(dirfd, "testdir_link", S_IWUSR | S_IRUSR) != 0) {
		sprt::log::vperror(__SPRT_LOCATION, "main",
				"fail to create dir: ", sprt::status::errnoToStatus(errno));
	}

	sprt::filepath::merge([&](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			sprt::filepath::merge([&](sprt::StringView path) {
				path.performWithTerminated([&](const char *nDirPath, size_t) {
					auto ret = rename(cDirPath, nDirPath);
					if (ret != 0) {
						sprt::log::vperror(__SPRT_LOCATION, "main",
								"fail to rename directory: ", sprt::status::errnoToStatus(errno));
					} else {
						ret = symlink(nDirPath, cDirPath);
						if (ret != 0) {
							sprt::log::vperror(__SPRT_LOCATION, "main",
									"fail to symlink directory: ",
									sprt::status::errnoToStatus(errno));
						} else {
							performDirLinkTest(nDirPath, cDirPath);
						}
					}
				});
			}, dirPath, "testdir_new");
		});
	}, dirPath, "testdir_link");

	close(dirfd);

	sprt::filepath::merge([](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			auto ret = remove(cDirPath);
			if (ret != 0) {
				sprt::log::vperror(__SPRT_LOCATION, "main",
						"fail to remove directory: ", sprt::status::errnoToStatus(errno));
			}
		});
	}, dirPath, "testdir_link");

	sprt::filepath::merge([](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			auto ret = remove(cDirPath);
			if (ret != 0) {
				sprt::log::vperror(__SPRT_LOCATION, "main",
						"fail to remove directory: ", sprt::status::errnoToStatus(errno));
			}
		});
	}, dirPath, "testdir_new");
}

int main(int argc, const char *argv[]) {
	auto str =
			"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	fwrite(str, strlen(str), 1, stdout);

	sprt::log::vpinfo(__SPRT_LOCATION, "main", "Exec path: ", sprt::platform::getExecPath());
	sprt::log::vpinfo(__SPRT_LOCATION, "main", "Home path: ", sprt::platform::getHomePath());
	sprt::log::vpinfo(__SPRT_LOCATION, "main",
			"Unique Device Id: ", sprt::platform::getUniqueDeviceId());

	performPthreadTest();
	performPthreadCondTest();
	performPthreadRwlockTest();
	performPthreadBarrierTest();
	performPthreadSpinlockTest();
	performUnameTest();
	performUnistdTest();
	performDirTest();
	performLinkTest();
	return 0;
}
