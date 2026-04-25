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

#define SPRT_BUILD 1

#include <sprt/cxx/debugging>

#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>

#if SPRT_MACOS
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>
#endif

namespace sprt {

bool is_debugger_present() noexcept {
#if SPRT_LINUX
	// Try to trace ourselves - if we're already being traced, this fails
	if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
		return true; // Already being traced by a debugger
	}
	ptrace(PTRACE_DETACH, 0, nullptr, nullptr);
	return false; // Not being traced
#elif SPRT_ANDROID
	char cmdline[4'096];
	FILE *fp = fopen("/proc/self/cmdline", "r");

	if (!fp) {
		return 0;
	}
	fread(cmdline, 1, sizeof(cmdline) - 1, fp);
	fclose(fp);

	// Debuggable builds often have specific flags or are from emulator
	if (strstr(cmdline, "emulator") || strstr(cmdline, "gdbserver")) {
		return true;
	}
	return false;
#elif SPRT_MACOS
	int mib[4];
	struct kinfo_proc info;
	size_t size;

	// Initialize the Management Information Base (MIB)
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid(); // Get current process ID

	// Initialize the info structure
	size = sizeof(info);
	info.kp_proc.p_flag = 0;

	// Query the system for process info
	if (sysctl(mib, 4, &info, &size, NULL, 0) == -1) {
		return false; // Error occurred
	}

	// Check if the P_TRACED flag is set
	return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
#error "Not implemented
#endif
}

} // namespace sprt
