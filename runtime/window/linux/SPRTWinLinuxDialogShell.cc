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

#include "SPRTWinLinuxDialogShell.h"

#if SPRT_LINUX

#include <sprt/runtime/window/native_window.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sprt::window {

namespace {

// POSIX sh quoting: single quotes protect everything, an embedded quote closes, escapes, reopens.
// The runtime cannot depend on installer_core, and there is no shell-quoting helper anywhere under
// runtime/, so this is a local copy of the one in utils/installer/core/src/SPIProcess.cc.
String shellQuote(StringView word) {
	String out;
	out.push_back('\'');
	for (auto c : word) {
		if (c == '\'') {
			out.append("'\\''");
		} else {
			out.push_back(c);
		}
	}
	out.push_back('\'');
	return out;
}

// Is `name` an executable on PATH?
bool hasExecutable(StringView name) {
	auto path = ::getenv("PATH");
	if (!path) {
		return false;
	}

	StringView reader(path);
	while (!reader.empty()) {
		auto dir = reader.readUntil<StringView::Chars<':'>>();
		reader.skipChars<StringView::Chars<':'>>();
		if (dir.empty()) {
			continue;
		}
		auto full = toString(dir, "/", name);
		if (::access(full.data(), X_OK) == 0) {
			return true;
		}
	}
	return false;
}

// zenity emits "rgb(r,g,b)" or "rgba(r,g,b,a)"; kdialog emits "#rrggbb".
bool parseColor(StringView str, Color4F &out) {
	str.trimChars<StringView::WhiteSpace>();
	if (str.empty()) {
		return false;
	}

	if (str.is('#')) {
		++str;
		if (str.size() < 6) {
			return false;
		}
		auto hex = [](char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
			if (c >= 'a' && c <= 'f') {
				return c - 'a' + 10;
			}
			if (c >= 'A' && c <= 'F') {
				return c - 'A' + 10;
			}
			return -1;
		};
		float channels[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		const size_t count = str.size() >= 8 ? 4 : 3;
		for (size_t i = 0; i < count; ++i) {
			auto hi = hex(str.data()[i * 2]);
			auto lo = hex(str.data()[i * 2 + 1]);
			if (hi < 0 || lo < 0) {
				return false;
			}
			channels[i] = float(hi * 16 + lo) / 255.0f;
		}
		out = Color4F(channels[0], channels[1], channels[2], channels[3]);
		return true;
	}

	if (str.starts_with("rgb")) {
		str.skipUntil<StringView::Chars<'('>>();
		if (str.empty()) {
			return false;
		}
		++str;
		float channels[4] = {0.0f, 0.0f, 0.0f, 255.0f};
		for (size_t i = 0; i < 4; ++i) {
			str.skipChars<StringView::WhiteSpace>();
			auto value = str.readDouble();
			if (!value) {
				// rgb() has three components; the fourth is simply absent.
				break;
			}
			// zenity writes 0..255 for r/g/b but 0..1 for the alpha of rgba().
			channels[i] = float(value.get()) * ((i == 3) ? 255.0f : 1.0f);
			str.skipChars<StringView::WhiteSpace>();
			str.skipChars<StringView::Chars<','>>();
		}
		out = Color4F(channels[0] / 255.0f, channels[1] / 255.0f, channels[2] / 255.0f,
				channels[3] / 255.0f);
		return true;
	}
	return false;
}

// Pango-style descriptor: "Family [Style...] Size", e.g. "DejaVu Sans Bold Italic 12".
void parseFont(StringView str, DialogFontInfo &out) {
	str.trimChars<StringView::WhiteSpace>();
	out.description = str.str<String>();

	auto rest = str;
	// The size is the trailing number, if any.
	auto lastSpace = rest.rfind(' ');
	if (lastSpace != Max<size_t>) {
		StringView tail(rest.data() + lastSpace + 1, rest.size() - lastSpace - 1);
		if (auto value = tail.readDouble()) {
			out.size = float(value.get());
			rest = StringView(rest.data(), lastSpace);
		}
	}

	// Then any number of style words, which are what is left over after the family.
	for (;;) {
		auto space = rest.rfind(' ');
		if (space == Max<size_t>) {
			break;
		}
		StringView word(rest.data() + space + 1, rest.size() - space - 1);
		if (word == "Bold") {
			out.bold = true;
		} else if (word == "Italic" || word == "Oblique") {
			out.italic = true;
		} else if (word != "Regular" && word != "Normal") {
			break;
		}
		rest = StringView(rest.data(), space);
	}

	rest.trimChars<StringView::WhiteSpace>();
	out.family = rest.str<String>();
}

/* ---- the freedesktop trash, for RestoreFromTrash --------------------------------------------

Read directly rather than through a helper. `gio trash --restore` takes a trash:// URI, i.e. it
wants the identity of the trashed item - and the identity is exactly what the caller does not have,
since neither the portal (which answers a bare `u`) nor gio hands one back when the file goes in.
What the caller has is the path the file USED to have, and finding an entry by that means reading
the info directory, which is a documented file format three lines long.

Only the HOME trash ($XDG_DATA_HOME/Trash) is searched. A file trashed from another mount goes to
`<topdir>/.Trash-$uid` instead, and finding those means enumerating mount points and following the
spec's rules about sticky bits and symlinks - real work, for a case a project directory rarely is.
A path that was trashed to one of those is simply not found, which is the same answer as a path
that was never trashed. */

// $XDG_DATA_HOME/Trash, or $HOME/.local/share/Trash. Empty when neither variable is set.
String getHomeTrashDir() {
	if (auto dataHome = ::getenv("XDG_DATA_HOME")) {
		if (*dataHome == '/') {
			return toString(StringView(dataHome), "/Trash");
		}
	}
	if (auto home = ::getenv("HOME")) {
		if (*home == '/') {
			return toString(StringView(home), "/.local/share/Trash");
		}
	}
	return String();
}

// The Path field is escaped as a URL path component (RFC 2396), so a directory with a space or a
// '#' in its name comes back through this and not out of the file as it stands.
String urlDecode(StringView str) {
	auto hex = [](char c) -> int {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		return -1;
	};

	String out;
	out.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '%' && i + 2 < str.size()) {
			auto hi = hex(str[i + 1]);
			auto lo = hex(str[i + 2]);
			if (hi >= 0 && lo >= 0) {
				out.push_back(char(hi * 16 + lo));
				i += 2;
				continue;
			}
		}
		out.push_back(str[i]);
	}
	return out;
}

// One `info/<name>.trashinfo` and the file in `files/` it stands for.
struct TrashEntry {
	String infoPath;
	String filePath;
	String origin; // Path=, decoded
	String deleted; // DeletionDate=, ISO-8601 and therefore ordered by plain comparison
};

// Whole file, capped: a .trashinfo is three lines, and anything claiming to be larger is not one.
String readSmallFile(const String &path, size_t limit) {
	auto f = ::fopen(path.data(), "rb");
	if (!f) {
		return String();
	}
	String out;
	char buf[512];
	while (out.size() < limit) {
		auto read = ::fread(buf, 1, sizeof(buf), f);
		if (read == 0) {
			break;
		}
		out.append(buf, read);
	}
	::fclose(f);
	return out;
}

void readTrashEntries(StringView trashDir, Vector<TrashEntry> &out) {
	auto infoDir = toString(trashDir, "/info");
	auto dir = ::opendir(infoDir.data());
	if (!dir) {
		return;
	}

	while (auto ent = ::readdir(dir)) {
		StringView name(ent->d_name);
		if (!name.ends_with(".trashinfo")) {
			continue;
		}

		TrashEntry entry;
		entry.infoPath = toString(infoDir, "/", name);
		entry.filePath = toString(trashDir, "/files/",
				StringView(name.data(), name.size() - StringView(".trashinfo").size()));

		auto content = readSmallFile(entry.infoPath, 8_KiB);
		StringView reader(content);
		while (!reader.empty()) {
			auto line = reader.readUntil<StringView::Chars<'\n'>>();
			reader.skipChars<StringView::Chars<'\n', '\r'>>();
			line.trimChars<StringView::WhiteSpace>();
			if (line.starts_with("Path=")) {
				entry.origin = urlDecode(StringView(line.data() + 5, line.size() - 5));
			} else if (line.starts_with("DeletionDate=")) {
				entry.deleted = StringView(line.data() + 13, line.size() - 13).str<String>();
			}
		}

		// A relative Path belongs to a volume trash this backend does not search, and an entry
		// whose file is gone is a leftover the desktop will clean up itself.
		if (entry.origin.empty() || entry.origin.front() != '/') {
			continue;
		}
		if (::access(entry.filePath.data(), F_OK) != 0) {
			continue;
		}
		out.emplace_back(sprt::move(entry));
	}

	::closedir(dir);
}

// Put back everything in `paths` that the home trash holds. `restored` collects the ORIGINAL paths
// that came back, which is what the caller asked about.
Status restoreFromHomeTrash(SpanView<String> paths, Vector<String> &restored) {
	if (paths.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	auto trashDir = getHomeTrashDir();
	if (trashDir.empty()) {
		return Status::ErrorNotFound;
	}

	Vector<TrashEntry> entries;
	readTrashEntries(trashDir, entries);
	if (entries.empty()) {
		return Status::ErrorNotFound;
	}

	size_t failed = 0;
	for (auto &path : paths) {
		// The most recent deletion of this path wins: trashing a file, recreating it and trashing
		// it again leaves two entries, and the one the user means is the last one they made.
		const TrashEntry *best = nullptr;
		for (auto &entry : entries) {
			if (entry.origin != path) {
				continue;
			}
			if (!best || entry.deleted > best->deleted) {
				best = &entry;
			}
		}

		if (!best) {
			continue;
		}

		// A restore never clobbers: something is at the path the file came from, and that something
		// is newer than the deletion.
		if (::access(best->origin.data(), F_OK) == 0) {
			++failed;
			continue;
		}

		// rename(2) rather than a copy: the home trash is on the same filesystem as $HOME by
		// construction, and a cross-device EXDEV here means the trash is not the one this file
		// went into - which is a failure to report, not a copy to attempt.
		if (::rename(best->filePath.data(), best->origin.data()) != 0) {
			++failed;
			continue;
		}

		::unlink(best->infoPath.data());
		restored.emplace_back(best->origin);
	}

	if (restored.empty()) {
		return failed > 0 ? Status::ErrorUnknown : Status::ErrorNotFound;
	}
	return failed > 0 ? Status::ErrorUnknown : Status::Ok;
}

} // namespace

ShellDialogTool detectShellDialogTool() {
	// zenity first: it covers every dialog type we ask for, kdialog is the fallback.
	if (hasExecutable("zenity")) {
		return ShellDialogTool::Zenity;
	}
	if (hasExecutable("kdialog")) {
		return ShellDialogTool::KDialog;
	}
	return ShellDialogTool::None;
}

StringView getShellDialogToolName(ShellDialogTool tool) {
	switch (tool) {
	case ShellDialogTool::Zenity: return StringView("zenity");
	case ShellDialogTool::KDialog: return StringView("kdialog");
	case ShellDialogTool::None: break;
	}
	return StringView("none");
}

WindowCapabilities getShellDialogCapabilities(ShellDialogTool tool) {
	switch (tool) {
	case ShellDialogTool::Zenity:
	case ShellDialogTool::KDialog:
		// `xdg-open` and `gio trash` are separate binaries from the dialog helper, but a desktop
		// that has one of these invariably ships both.
		return WindowCapabilities::FileDialogs | WindowCapabilities::ColorDialog
				| WindowCapabilities::FontDialog | WindowCapabilities::SystemFileActions;
	case ShellDialogTool::None: break;
	}
	return WindowCapabilities::None;
}

bool ShellDialogHandle::init(NotNull<ContextController> controller,
		NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req, NativeWindow *parent,
		ShellDialogTool tool) {
	if (!DialogHandle::init(controller, target, sprt::move(req), parent)) {
		return false;
	}
	_tool = tool;

	/* The one type served here rather than by a child process.

	The work is a directory listing and a rename, and it is done on this looper right now - but the
	COMPLETION is posted rather than delivered, because finalize() unregisters the handle and the
	caller has not registered it yet. Answering inside init() would leave a dead handle in the
	registry holding a modal block nothing will release. */
	if (_request->type == DialogType::RestoreFromTrash) {
		DialogResult result;
		result.status = restoreFromHomeTrash(_request->paths, result.paths);
		controller->getLooper()->performOnThread([this, result = sprt::move(result)]() mutable {
			finalize(sprt::move(result));
		}, this, false, "ShellDialogHandle::restore");
		return true;
	}

	auto command = buildCommand();
	if (command.empty()) {
		return false; // this helper cannot serve this type; the caller answers ErrorNotSupported
	}

	_process = controller->getLooper()->spawnProcess(command, [this](StringView chunk) {
		// Reader chunks arrive inside a transient notify pool, so they must be copied out rather
		// than referenced.
		_output.append(chunk.data(), chunk.size());
	}, [this](int exitCode, Status st) { handleOutput(exitCode, st); }, this);

	if (!_process) {
		return false;
	}
	return true;
}

Status ShellDialogHandle::cancel(Status st) {
	if (!isActive()) {
		return Status::ErrorAlreadyPerformed;
	}

	// Finalize BEFORE killing the child. Terminating it fires handleOutput with a non-zero exit,
	// which is indistinguishable from the user dismissing the picker and would report Declined —
	// losing the reason we are cancelling (ErrorCancelled when the parent window died). Once
	// finalize has cleared _active, handleOutput returns early and the intended status stands.
	auto ret = DialogHandle::cancel(st);

	if (_process) {
		// The Rc IS the child: cancelling the handle kills it.
		_process->cancel();
		_process = nullptr;
	}
	return ret;
}

void ShellDialogHandle::handleOutput(int exitCode, Status st) {
	_process = nullptr;
	if (!isActive()) {
		return;
	}

	StringView out(_output);
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
		out = StringView(out.data(), out.size() - 1);
	}

	if (!isSuccessful(st) || exitCode != 0) {
		// A picker exits non-zero both when the user cancels and when it is not installed, and the
		// two are indistinguishable from the outside — so both collapse to Declined. The shell
		// ACTIONS have no cancel at all, so for them a non-zero exit is a genuine failure (gio
		// trash refusing a cross-filesystem path, xdg-open with no handler, and so on).
		const bool isAction = _request->type == DialogType::RevealInFileManager
				|| _request->type == DialogType::MoveToTrash
				|| _request->type == DialogType::RestoreFromTrash;
		finalize(isAction ? Status::ErrorUnknown : Status::Declined);
		return;
	}

	DialogResult result;
	result.status = Status::Ok;

	switch (_request->type) {
	case DialogType::OpenFile:
	case DialogType::OpenDirectory:
	case DialogType::SaveFile:
		if (out.empty()) {
			finalize(Status::Declined);
			return;
		}
		// --separator=\n for zenity --multiple; a single path never contains a newline.
		out.split<StringView::Chars<'\n'>>([&](StringView path) {
			path.trimChars<StringView::WhiteSpace>();
			if (!path.empty()) {
				result.paths.emplace_back(path.str<String>());
			}
		});
		break;
	case DialogType::Color:
		if (!parseColor(out, result.color)) {
			finalize(Status::Declined);
			return;
		}
		break;
	case DialogType::Font:
		if (out.empty()) {
			finalize(Status::Declined);
			return;
		}
		parseFont(out, result.font);
		break;
	case DialogType::RevealInFileManager:
	case DialogType::MoveToTrash:
		// Nothing to read back; a zero exit is the whole answer.
		break;
	case DialogType::RestoreFromTrash:
		// Never spawns a child at all - see init(). Listed so this switch stays exhaustive.
		break;
	}

	finalize(sprt::move(result));
}

String ShellDialogHandle::buildCommand() const {
	const auto &req = *_request;
	const auto title = req.title.empty() ? StringView("Choose") : StringView(req.title);

	switch (req.type) {
	case DialogType::RevealInFileManager: {
		if (req.paths.empty()) {
			return String();
		}
		// No portable "select this file" exists for a bare xdg-open, so open the containing
		// directory. `dirname` is not available as a library call here, so trim in place.
		StringView path(req.paths.front());
		auto slash = path.rfind('/');
		auto dir = (slash == Max<size_t> || slash == 0) ? path : StringView(path.data(), slash);
		return toString("xdg-open ", shellQuote(dir));
	}
	case DialogType::MoveToTrash: {
		if (req.paths.empty()) {
			return String();
		}
		String cmd("gio trash");
		for (auto &it : req.paths) { cmd.append(" ").append(shellQuote(it)); }
		return cmd;
	}
	default: break;
	}

	if (_tool == ShellDialogTool::Zenity) {
		switch (req.type) {
		case DialogType::OpenFile: {
			String cmd = toString("zenity --file-selection --title=", shellQuote(title));
			if (hasFlag(req.flags, DialogFlags::Multiple)) {
				cmd.append(" --multiple --separator=$'\\n'");
			}
			if (!req.path.empty()) {
				cmd.append(" --filename=").append(shellQuote(toString(req.path, "/")));
			}
			for (auto &filter : req.filters) {
				String spec = filter.name;
				for (auto &pattern : filter.patterns) { spec.append("|").append(pattern); }
				cmd.append(" --file-filter=").append(shellQuote(spec));
			}
			return cmd;
		}
		case DialogType::OpenDirectory: {
			String cmd =
					toString("zenity --file-selection --directory --title=", shellQuote(title));
			if (!req.path.empty()) {
				cmd.append(" --filename=").append(shellQuote(toString(req.path, "/")));
			}
			return cmd;
		}
		case DialogType::SaveFile: {
			String cmd = toString("zenity --file-selection --save --title=", shellQuote(title));
			if (hasFlag(req.flags, DialogFlags::ConfirmOverwrite)) {
				cmd.append(" --confirm-overwrite");
			}
			if (!req.path.empty() || !req.filename.empty()) {
				cmd.append(" --filename=")
						.append(shellQuote(toString(req.path, "/", req.filename)));
			}
			return cmd;
		}
		case DialogType::Color: {
			String cmd = toString("zenity --color-selection --title=", shellQuote(title));
			if (hasFlag(req.flags, DialogFlags::AlphaChannel)) {
				cmd.append(" --show-palette");
			}
			return cmd;
		}
		case DialogType::Font:
			return toString("zenity --font-selection --title=", shellQuote(title));
		default: break;
		}
		return String();
	}

	if (_tool == ShellDialogTool::KDialog) {
		const auto startDir = req.path.empty() ? StringView(".") : StringView(req.path);
		switch (req.type) {
		case DialogType::OpenFile: {
			String cmd = toString("kdialog --title ", shellQuote(title), " --getopenfilename ",
					shellQuote(startDir));
			if (hasFlag(req.flags, DialogFlags::Multiple)) {
				cmd.append(" --multiple --separate-output");
			}
			return cmd;
		}
		case DialogType::OpenDirectory:
			return toString("kdialog --title ", shellQuote(title), " --getexistingdirectory ",
					shellQuote(startDir));
		case DialogType::SaveFile:
			return toString("kdialog --title ", shellQuote(title), " --getsavefilename ",
					shellQuote(req.filename.empty()
									? startDir
									: StringView(toString(startDir, "/", req.filename))));
		case DialogType::Color: return toString("kdialog --getcolor");
		case DialogType::Font: return toString("kdialog --getfont");
		default: break;
		}
	}
	return String();
}

} // namespace sprt::window

#endif // SPRT_LINUX
