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

#include "InstallerNativeDialogs.h"

#include "SPIProcess.h"

#include "XLAppThread.h"

#include <sprt/runtime/dispatch/looper.h>

#include <memory>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

#if SPRT_APPLE
// Escape a value for an AppleScript double-quoted string (`\` and `"` are the only specials).
// The shell layer is separate: shellQuote() wraps the whole script into one word for `osascript`.
String applescriptEscape(StringView s) {
	String out;
	for (auto c : s) {
		if (c == '\\' || c == '"') {
			out.push_back('\\');
		}
		out.push_back(c);
	}
	return out;
}

String osascriptCommand(StringView script) { return toString("osascript -e ", shellQuote(script)); }
#endif

// Spawn `command` on the app looper, accumulate its output, and hand the trimmed text to
// `onDone` — on the app thread, because that is where the looper runs its callbacks.
//
// A non-zero exit means "cancelled" or "no such helper" for every dialog here, and both must look
// the same to the caller, so the result collapses to "".
Rc<ProcessHandle> runCapturedAsync(NotNull<AppThread> app, StringView command,
		Function<void(String)> &&onDone, Ref *owner) {
	// Boxed because it has to be reachable both from the completion and from the failure path
	// below, and the completion has already taken ownership by then.
	auto doneCb = std::make_shared<Function<void(String)>>(sp::move(onDone));

	auto looper = app->getLooper();
	if (!looper || command.empty()) {
		if (*doneCb) {
			(*doneCb)(String());
		}
		return nullptr;
	}

	// Reader chunks arrive inside a transient notify pool, so they must be copied into
	// malloc-backed storage rather than referenced.
	auto out = std::make_shared<String>();

	auto handle = looper->spawnProcess(command, [out](StringView chunk) {
		out->append(chunk.data(), chunk.size());
	}, [out, doneCb](int exitCode, Status st) {
		String text;
		if (isSuccessful(st) && exitCode == 0) {
			text = sp::move(*out);
			while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
				text.pop_back();
			}
		}
		if (*doneCb) {
			(*doneCb)(sp::move(text));
		}
	}, owner);

	if (!handle) {
		// The spawn failed, or this backend has no process support (wasm has no exec at all): no
		// completion will ever fire, so the caller has to be answered right here.
		if (*doneCb) {
			(*doneCb)(String());
		}
	}
	return handle;
}

} // namespace

Rc<ProcessHandle> pickFolderAsync(NotNull<AppThread> app, StringView prompt,
		Function<void(String)> &&onDone, Ref *owner) {
	const auto title = prompt.empty() ? StringView("Choose folder") : prompt;
#if SPRT_APPLE
	return runCapturedAsync(app,
			osascriptCommand(toString("POSIX path of (choose folder with prompt \"",
					applescriptEscape(title), "\")")),
			[onDone = sp::move(onDone)](String path) {
		// osascript reports a directory with a trailing slash; the registry stores it without.
		while (!path.empty() && path.back() == '/') { path.pop_back(); }
		if (onDone) {
			onDone(sp::move(path));
		}
	}, owner);
#elif SPRT_LINUX
	// No desktop-neutral picker exists: try the GTK one, fall back to the KDE one. Both exit
	// non-zero when cancelled or absent, which runCapturedAsync collapses to "".
	auto zenity = toString("zenity --file-selection --directory --title=", shellQuote(title));
	auto kdialog = toString("kdialog --getexistingdirectory . --title ", shellQuote(title));
	return runCapturedAsync(app, zenity,
			[app, owner, kdialog = sp::move(kdialog), onDone = sp::move(onDone)](
					String path) mutable {
		if (!path.empty()) {
			if (onDone) {
				onDone(sp::move(path));
			}
			return;
		}
		// Chaining the fallback from the completion keeps the whole probe asynchronous. The
		// second handle is owned by the app thread only for the length of this call — the caller
		// already dropped its Rc, so the child would die immediately; keep it alive by handing
		// ownership to its own completion.
		auto second = std::make_shared<Rc<ProcessHandle>>();
		*second = runCapturedAsync(app, kdialog,
				[second, onDone = sp::move(onDone)](String fallback) {
			*second = nullptr;
			if (onDone) {
				onDone(sp::move(fallback));
			}
		}, owner);
	},
			owner);
#else
	(void)title;
	if (onDone) {
		onDone(String());
	}
	return nullptr;
#endif
}

Rc<ProcessHandle> promptTextAsync(NotNull<AppThread> app, StringView title, StringView def,
		Function<void(String)> &&onDone, Ref *owner) {
#if SPRT_APPLE
	return runCapturedAsync(app,
			osascriptCommand(
					toString("text returned of (display dialog \"", applescriptEscape(title),
							"\" default answer \"", applescriptEscape(def), "\")")),
			sp::move(onDone), owner);
#elif SPRT_LINUX
	auto zenity = toString("zenity --entry --title=", shellQuote(title),
			" --text=", shellQuote(title), " --entry-text=", shellQuote(def));
	auto kdialog = toString("kdialog --inputbox ", shellQuote(title), " ", shellQuote(def));
	return runCapturedAsync(app, zenity,
			[app, owner, kdialog = sp::move(kdialog), onDone = sp::move(onDone)](
					String text) mutable {
		if (!text.empty()) {
			if (onDone) {
				onDone(sp::move(text));
			}
			return;
		}
		auto second = std::make_shared<Rc<ProcessHandle>>();
		*second = runCapturedAsync(app, kdialog,
				[second, onDone = sp::move(onDone)](String fallback) {
			*second = nullptr;
			if (onDone) {
				onDone(sp::move(fallback));
			}
		}, owner);
	},
			owner);
#else
	(void)title;
	(void)def;
	if (onDone) {
		onDone(String());
	}
	return nullptr;
#endif
}

Rc<ProcessHandle> openInFileManagerAsync(NotNull<AppThread> app, StringView path, Ref *owner) {
	if (path.empty()) {
		return nullptr;
	}
#if SPRT_APPLE
	StringView argv[] = {StringView("open"), path};
#elif SPRT_LINUX
	StringView argv[] = {StringView("xdg-open"), path};
#elif SPRT_WINDOWS
	StringView argv[] = {StringView("explorer"), path};
#else
	(void)app;
	(void)owner;
	return nullptr; // no file manager on this target
#endif
	// makeShellCommand quotes every word, so a directory name with spaces, quotes or shell
	// metacharacters is never interpreted. The output is discarded — only the exit matters.
	return runCapturedAsync(app, makeShellCommand(argv), nullptr, owner);
}

} // namespace stappler::xenolith::installer
