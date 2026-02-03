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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_errno.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tlhelp32.h>

#include "private/SPRTSpecific.h" // IWYU pragma: keep

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include "sched_win.h"

namespace sprt {

static int sched_get_priority_max(int t) { return __SPRT_SCHED_PRIO_MAX; }

static int sched_get_priority_min(int t) { return __SPRT_SCHED_PRIO_MIN; }

static int sched_getparam(__SPRT_ID(pid_t) pid, struct sched_param *p) {
	if (!p) {
		__sprt_errno = EINVAL;
		return -1;
	}

	HANDLE hProcess = nullptr;
	HANDLE hThread = nullptr;
	if (pid == GetCurrentProcessId()) {
		hProcess = GetCurrentProcess();
	} else {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
			__sprt_errno = ESRCH;
			return -1;
		}
	}

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
	if (hSnap == INVALID_HANDLE_VALUE) {
		CloseHandle(hProcess);
		return EPERM;
	}

	THREADENTRY32 te32 = {sizeof(te32)};
	if (Thread32First(hSnap, &te32)) {
		do {
			if (te32.th32OwnerProcessID == pid) {
				hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
				break;
			}
		} while (Thread32Next(hSnap, &te32));
	}

	CloseHandle(hSnap);
	CloseHandle(hProcess);

	if (!hThread) {
		return ESRCH;
	}

	p->sched_priority = __unmapPriority(GetThreadPriority(hThread));
	return 0;
}

static int sched_getscheduler(__SPRT_ID(pid_t) pid) {
	HANDLE hProcess = nullptr;
	if (pid == GetCurrentProcessId()) {
		hProcess = GetCurrentProcess();
	} else {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
			__sprt_errno = ESRCH;
			return -1;
		}
	}

	auto cl = GetPriorityClass(hProcess);
	switch (cl) {
	case IDLE_PRIORITY_CLASS: return __SPRT_SCHED_IDLE; break;
	case BELOW_NORMAL_PRIORITY_CLASS:
	case NORMAL_PRIORITY_CLASS:
	case ABOVE_NORMAL_PRIORITY_CLASS:
	case HIGH_PRIORITY_CLASS: break;
	case REALTIME_PRIORITY_CLASS: return __SPRT_SCHED_RR; break;
	}
	return __SPRT_SCHED_OTHER;
}

static int sched_rr_get_interval(__SPRT_ID(pid_t) pid, __SPRT_TIMESPEC_NAME *t) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int sched_setparam(__SPRT_ID(pid_t) pid, const struct sched_param *p) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int sched_setscheduler(__SPRT_ID(pid_t) pid, int t, const struct sched_param *p) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int sched_yield(void) {
	YieldProcessor();
	return 0;
}

} // namespace sprt
