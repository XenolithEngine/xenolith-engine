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

#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"
#include "fcntl.h"
#include "specific.h"
#include "unistd.h"

#include "../../include/__impl_libc.h"
#include "../../include/__impl_file.h"

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/user_api.h>

namespace sprt {

__SPRT_C_FUNC FILE *popen(const char *cmd, const char *mode) __SPRT_NOEXCEPT {
	int p[2], op;

	if (*mode == 'r') {
		op = 0;
	} else if (*mode == 'w') {
		op = 1;
	} else {
		errno = EINVAL;
		return 0;
	}

	if (pipe2(p, 0)) {
		return NULL;
	}

	auto libc = __libc::get();

	PROCESS_INFORMATION pi;

	STARTUPINFOW si;
	sprt::memset(&si, 0, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESTDHANDLES;

	//SetHandleInformation(libc->get_fd_handle(p[op]), HANDLE_FLAG_INHERIT, 0);

	if (op == 0) {
		si.hStdOutput = libc->get_fd_handle(p[1]);
		si.hStdError = libc->get_fd_handle(p[1]);
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	} else {
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdInput = libc->get_fd_handle(p[0]);
	}

	// Build command for cmd.exe ('cmd.exe /c )
	auto bufSize = strlen(cmd) + 12;
	auto buf = __sprt_typed_malloca(wchar_t, strlen(cmd) + 12);

	auto targetCmd = buf;
	targetCmd = strappend(targetCmd, &bufSize, L"cmd.exe /c ", 11);

	size_t written = 0;
	unicode::toUtf16((char16_t *)targetCmd, bufSize, cmd, &written);

	targetCmd[written] = 0;

	// Create process
	if (!CreateProcessW(NULL, buf, nullptr, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		close(p[0]);
		close(p[1]);
		errno = platform::lastErrorToErrno(GetLastError());
		return NULL;
	}

	auto f = fdopen(p[op], mode);
	if (!f) {
		TerminateProcess(pi.hProcess, -1);
		close(p[0]);
		close(p[1]);
		return NULL;
	}

	f->pipe_handle = pi.hProcess;

	close(p[op == 0 ? 1 : 0]);

	//ResumeThread(pi.hThread);

	return f;
}

__SPRT_C_FUNC int pclose(FILE *f) __SPRT_NOEXCEPT {
	unsigned long status = -1;
	auto pid = f->pipe_handle;

	fclose(f);

	while (WaitForSingleObject(pid, INFINITE) != WAIT_OBJECT_0);

	GetExitCodeProcess(pid, &status);

	return status;
}

__SPRT_C_FUNC int system(const char *cmd) __SPRT_NOEXCEPT {
	auto envBufSize = GetEnvironmentVariableW(L"COMSPEC", nullptr, 0);
	if (envBufSize == 0 && cmd == nullptr) {
		return 0;
	}

	auto envBuf = __sprt_typed_malloca(wchar_t, envBufSize + 1);
	envBufSize = GetEnvironmentVariableW(L"COMSPEC", envBuf, envBufSize);

	if (cmd == nullptr) {
		return GetFileAttributesW(envBuf) != INVALID_FILE_ATTRIBUTES;
	}

	SHELLEXECUTEINFOW sei;
	sprt::memset(&sei, 0, sizeof(sei));

	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpFile = envBufSize > 0 ? envBuf : L"cmd.exe";

	auto bufSize = strlen(cmd) + 12;
	auto buf = __sprt_typed_malloca(wchar_t, strlen(cmd) + 12);

	auto targetCmd = buf;
	targetCmd = strappend(targetCmd, &bufSize, L"/c ", 3);

	size_t written = 0;
	unicode::toUtf16((char16_t *)targetCmd, bufSize, cmd, &written);

	targetCmd[written] = 0;

	sei.lpParameters = buf;
	sei.nShow = SW_HIDE;

	if (ShellExecuteExW(&sei)) {
		WaitForSingleObject(sei.hProcess, INFINITE);

		DWORD exit_code;
		GetExitCodeProcess(sei.hProcess, &exit_code);
		CloseHandle(sei.hProcess);
		return (int)exit_code;
	}

	errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

} // namespace sprt
