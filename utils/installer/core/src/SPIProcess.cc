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

#include "SPIProcess.h"
#include "SPIJob.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/c/__sprt_stdio.h> // fpath_to_native / fpath_is_posix: cmd.exe speaks native paths

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace dispatch = sprt::dispatch;

namespace {

#if SPRT_WINDOWS
// The engine's APIs speak POSIX paths (`/c/Users/…`); cmd.exe and the programs it launches need
// native ones. Only whole words that *are* POSIX paths are converted.
void writeShellWord(const Callback<void(StringView)> &out, StringView word) {
	if (word.empty() || !__sprt_fpath_is_posix(word.data(), word.size())) {
		out << word;
		return;
	}

	auto buf = mem_std::String();
	buf.resize(word.size() * 2 + 1);
	auto len = __sprt_fpath_to_native(word.data(), word.size(), buf.data(), buf.size());
	out << StringView(buf.data(), len);
}
#endif

} // namespace

void shellQuote(const Callback<void(StringView)> &out, StringView word) {
#if SPRT_WINDOWS
	// cmd.exe: double quotes, an embedded quote is doubled.
	out << "\"";
	while (!word.empty()) {
		auto chunk = word.readUntil<StringView::Chars<'"'>>();
		writeShellWord(out, chunk);
		if (word.is('"')) {
			out << "\"\"";
			++word;
		}
	}
	out << "\"";
#else
	// POSIX sh: single quotes protect everything; an embedded quote closes, escapes and reopens.
	out << "'";
	while (!word.empty()) {
		out << word.readUntil<StringView::Chars<'\''>>();
		if (word.is('\'')) {
			out << "'\\''";
			++word;
		}
	}
	out << "'";
#endif
}

String shellQuote(StringView word) {
	// A StringStream is itself a Callback<void(StringView)>.
	mem_std::StringStream stream;
	shellQuote(stream, word);
	return stream.str();
}

String makeShellCommand(SpanView<StringView> argv, StringView cwd) {
	mem_std::StringStream stream;

	if (!cwd.empty()) {
#if SPRT_WINDOWS
		stream << "cd /d "; // a bare `cd` does not switch drives
#else
		stream << "cd ";
#endif
		shellQuote(stream, cwd);
		stream << " && ";
	}

	bool first = true;
	for (auto &arg : argv) {
		if (!first) {
			stream << " ";
		}
		first = false;
		shellQuote(stream, arg);
	}
	return stream.str();
}

ProcessResult runShellCommand(StringView command, const Callback<void(StringView)> *output) {
	ProcessResult result;

	auto jobStatus = runJob([&] {
		// Acquired by the job thread itself, with an engine that really implements spawnProcess.
		auto looper = dispatch::Looper::getIfExists();
		if (!looper) {
			result.setError(Status::ErrorNotImplemented, "no looper on the job thread");
			return;
		}

		bool done = false;
		Status processStatus = Status::Pending;

		// The handle must outlive the wait loop: dropping it would kill the child.
		auto process = looper->spawnProcess(command, [output](StringView chunk) {
			if (output) {
				(*output)(chunk);
			}
		}, [&](int exitCode, Status status) {
			result.exitCode = exitCode;
			processStatus = status;
			done = true;
		});

		if (!process) {
			// nullptr means the spawn failed or the backend has no process support — no completion
			// will ever fire, so the failure has to be handled right here.
			result.setError(Status::ErrorNotImplemented, "failed to spawn: ", command);
			return;
		}

		while (!done) { looper->wait(dispatch::TimeInterval::Infinite); }

		if (!isSuccessful(processStatus)) {
			result.setError(processStatus, "process failed: ", command);
		} else if (result.exitCode != 0) {
			result.setError(Status::ErrorUnknown, "'", command, "' exited with code ",
					result.exitCode);
		}
	});

	if (!isSuccessful(jobStatus) && result.valid()) {
		result.setError(jobStatus, "failed to run the job thread for: ", command);
	}
	return result;
}

ProcessResult runCommand(SpanView<StringView> argv, StringView cwd,
		const Callback<void(StringView)> *output) {
	if (argv.empty()) {
		ProcessResult result;
		result.setError(Status::ErrorInvalidArguemnt, "empty command");
		return result;
	}
	return runShellCommand(makeShellCommand(argv, cwd), output);
}

} // namespace stappler::xenolith::installer
