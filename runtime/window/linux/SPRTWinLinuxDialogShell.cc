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

#include <stdlib.h>
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
				|| _request->type == DialogType::MoveToTrash;
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
