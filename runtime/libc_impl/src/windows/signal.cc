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
*/

/*
	Windows backend for the cross-process half of kill().

	Signals are a per-process, in-process emulation in sprt (see builtin_signal.cpp):
	there is no kernel mechanism to deliver one to another process, and Windows has no
	equivalent concept. What it does have is the two operations callers of kill()
	actually reach for across a process boundary - "does this process still exist"
	(sig 0) and "end it now" (anything else) - so those are what this maps.

	The exit code handed to TerminateProcess is 128 + signo, matching the status a
	POSIX shell reports for a signal-terminated child, so a waiter that decodes the
	exit code the usual way recovers the signal number.

	Included by builtin_signal.cpp on Windows.
*/

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/windows.h>

#include "specific.h"

static int __sprt_kill_process(__SPRT_ID(pid_t) pid, int sig) {
	DWORD access = (sig == 0) ? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_TERMINATE;

	HANDLE process = OpenProcess(access, FALSE, static_cast<DWORD>(pid));
	if (!process) {
		DWORD err = GetLastError();
		// A pid that no longer names a process is ESRCH; one we are not allowed to
		// touch is EPERM. Both are the POSIX answers for the same conditions.
		*__sprt___errno_location() =
				(err == ERROR_INVALID_PARAMETER) ? ESRCH : sprt::platform::lastErrorToErrno(err);
		return -1;
	}

	int ret = 0;
	if (sig != 0 && !TerminateProcess(process, static_cast<UINT>(128 + sig))) {
		*__sprt___errno_location() = sprt::platform::lastErrorToErrno(GetLastError());
		ret = -1;
	}

	CloseHandle(process);
	return ret;
}
