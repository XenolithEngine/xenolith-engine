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

#include "../../include/__impl_libc.h"

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/security_api.h>
#include <sprt/wrappers/windows/windows.h>

#include <sprt/cxx/mutex>

#include "specific.h"

namespace sprt {

static void acquireEffectiveIds(__libc *libc) {
	wchar_t tempPath[MAX_PATH];
	wchar_t tempFileName[MAX_PATH];

	UINT uniqueNum = 0;

	if (GetTempPathW(MAX_PATH, tempPath) == 0) {
		return;
	}

	if (GetTempFileNameW(tempPath, L"euidgid", uniqueNum, tempFileName) == 0) {
		return;
	}

	// Open the file with FILE_FLAG_DELETE_ON_CLOSE
	HANDLE hTempFile = CreateFileW(tempFileName, GENERIC_READ | GENERIC_WRITE,
			0, // No sharing
			NULL,
			OPEN_EXISTING, // The file was created by GetTempFileName
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);

	if (hTempFile == INVALID_HANDLE_VALUE) {
		return;
	}

	PSID owner_sid = nullptr;
	PSID group_sid = nullptr;

	if (GetSecurityInfo(hTempFile, SE_FILE_OBJECT,
				OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &owner_sid, &group_sid,
				NULL, NULL, NULL)
			== ERROR_SUCCESS) {
		libc->euid = platform::getRid(owner_sid);
		libc->egid = platform::getRid(group_sid);
	}

	// The file is automatically deleted when the handle is closed.
	CloseHandle(hTempFile);
}

void __init_default_fds(__libc *libc) {
	DWORD dwMode = 0;

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	unique_lock lock(libc->fdMutex);

	libc->fdDispatch->bits.set(__libc::STDIN_FD);
	auto &stdinFd = libc->fdPages[0]->fds[__libc::STDIN_FD];
	stdinFd.ops = &libc->fdFileOps;
	stdinFd.handle = GetStdHandle(STD_INPUT_HANDLE);
	if (stdinFd.handle == INVALID_HANDLE_VALUE) {
		stdinFd.handle = nullptr;
	}

	libc->fdDispatch->bits.set(__libc::STDOUT_FD);
	auto &stdoutFd = libc->fdPages[0]->fds[__libc::STDOUT_FD];
	stdoutFd.ops = &libc->fdFileOps;
	stdoutFd.handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutFd.handle == INVALID_HANDLE_VALUE) {
		stdoutFd.handle = nullptr;
	}

	libc->fdDispatch->bits.set(__libc::STDERR_FD);
	auto &stderrFd = libc->fdPages[0]->fds[__libc::STDERR_FD];
	stderrFd.ops = &libc->fdFileOps;
	stderrFd.handle = GetStdHandle(STD_ERROR_HANDLE);
	if (stderrFd.handle == INVALID_HANDLE_VALUE) {
		stderrFd.handle = nullptr;
	}

	lock.unlock();

	GetConsoleMode(stdinFd.handle, &dwMode);
	SetConsoleMode(stdinFd.handle, dwMode | ENABLE_VIRTUAL_TERMINAL_INPUT);

	GetConsoleMode(stdoutFd.handle, &dwMode);
	SetConsoleMode(stdoutFd.handle, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	GetConsoleMode(stderrFd.handle, &dwMode);
	SetConsoleMode(stderrFd.handle, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

	acquireEffectiveIds(libc);

	DWORD dwIsAppContainer = 0;
	DWORD dwReturnLength = 0;
	if (GetTokenInformation(GetCurrentThreadEffectiveToken(), TokenIsAppContainer,
				&dwIsAppContainer, sizeof(dwIsAppContainer), &dwReturnLength)) {
		libc->isAppContainer = (dwIsAppContainer != 0);
	}
}

} // namespace sprt

HANDLE _get_osfhandle(int fd) {
	auto slot = sprt::__libc::get()->get_fd_slot(fd);
	if (!slot) {
		return nullptr;
	}
	return slot->handle;
}
