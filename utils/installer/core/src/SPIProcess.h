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

#ifndef UTILS_INSTALLER_CORE_SRC_SPIPROCESS_H_
#define UTILS_INSTALLER_CORE_SRC_SPIPROCESS_H_

#include "SPICommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// External programs run through the runtime's process API (Looper::spawnProcess), never through
// fork/exec: that API is the only one that exists on every platform the engine targets.
//
// spawnProcess takes ONE command line, which the system shell interprets (`/bin/sh -c` on POSIX,
// `cmd.exe /c` on Windows), and merges the child's stdout and stderr into a single reader. It
// carries no argv, cwd or environment, so words are quoted here and the working directory is set
// with a leading `cd`.

struct SP_PUBLIC ProcessResult : OperationResult {
	int exitCode = -1; // 128 + signal number when killed by a signal; -1 if the spawn failed
};

// Quote one word for the platform shell.
SP_PUBLIC void shellQuote(const Callback<void(StringView)> &out, StringView word);
SP_PUBLIC String shellQuote(StringView word);

// `[cd <cwd> && ]<argv0> <argv1> …`, every word quoted. An empty `cwd` inherits the caller's.
SP_PUBLIC String makeShellCommand(SpanView<StringView> argv, StringView cwd = StringView());

// Run `command` through the system shell and BLOCK until it exits. Safe to call from any thread:
// the process is driven on a private job thread (see SPIJob.h).
//
// `output` receives the merged stdout+stderr as it arrives, ON THE JOB THREAD and inside a
// transient notify pool — write it through, or accumulate it in malloc-backed storage.
SP_PUBLIC ProcessResult runShellCommand(StringView command,
		const Callback<void(StringView)> *output = nullptr);

// Quote `argv`, apply `cwd`, run.
SP_PUBLIC ProcessResult runCommand(SpanView<StringView> argv, StringView cwd = StringView(),
		const Callback<void(StringView)> *output = nullptr);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPIPROCESS_H_
