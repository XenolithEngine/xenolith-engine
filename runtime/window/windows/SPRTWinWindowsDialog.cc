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

#include "SPRTWinWindowsDialog.h"

#if SPRT_WINDOWS

#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/unicode.h>

#include <sprt/wrappers/windows/com_api.h>
#include <sprt/wrappers/windows/commdlg.h>
#include <sprt/wrappers/windows/shellapi.h>
#include <sprt/wrappers/windows/shlobj.h>
#include <sprt/wrappers/windows/winerror.h>

namespace sprt::window {

namespace {

// UTF-8 -> UTF-16, owned and NUL-terminated, ready to hand to a W entry point. WideString stores a
// terminator, so .data() is safe to pass straight through.
WideString toWide(StringView str) {
	WideString ret;
	unicode::toUtf16([&](WideStringView wide) { ret = wide.str<WideString>(); }, str);
	return ret;
}

String fromWide(const wchar_t *str) {
	if (!str) {
		return String();
	}
	String ret;
	unicode::toUtf8([&](StringView utf8) { ret = utf8.str<String>(); },
			WideStringView(reinterpret_cast<const char16_t *>(str)));
	return ret;
}

// Wrap a path as a shell item. SHCreateItemFromParsingName, unlike SHCreateItemFromIDList, does not
// insist the item already exist — which is what a save dialog's target folder needs.
IShellItem *makeShellItem(StringView path) {
	if (path.empty()) {
		return nullptr;
	}
	auto wide = toWide(path);
	IShellItem *item = nullptr;
	if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(wide.data()), nullptr,
				__uuidof(IShellItem), reinterpret_cast<void **>(&item)))) {
		return nullptr;
	}
	return item;
}

// The path an IShellItem stands for, or empty when it has none (a virtual folder such as
// "This PC" — reachable unless FOS_FORCEFILESYSTEM is set, which it always is here).
String shellItemPath(IShellItem *item) {
	LPWSTR raw = nullptr;
	if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) || !raw) {
		return String();
	}
	auto ret = fromWide(raw);
	CoTaskMemFree(raw);
	return ret;
}

// A COLORREF is 0x00bbggrr, so the channel order is the reverse of everything else here.
Color4F fromColorRef(COLORREF ref) {
	return Color4F(float(ref & 0xFF) / 255.0f, float((ref >> 8) & 0xFF) / 255.0f,
			float((ref >> 16) & 0xFF) / 255.0f, 1.0f);
}

COLORREF toColorRef(const Color4F &color) {
	auto channel = [](float v) -> uint32_t {
		auto scaled = int32_t(v * 255.0f + 0.5f);
		return uint32_t(scaled < 0 ? 0 : (scaled > 255 ? 255 : scaled));
	};
	return COLORREF(channel(color.r) | (channel(color.g) << 8) | (channel(color.b) << 16));
}

} // namespace

WindowCapabilities getWindowsDialogCapabilities() {
	// Nothing to probe: IFileDialog is part of the shell, comdlg32 has shipped with Windows since
	// 3.0, and IFileOperation covers the Recycle Bin. NativeDialogParenting because every one of
	// them takes a real owner HWND, so the OS blocks and raises the parent for us.
	return WindowCapabilities::FileDialogs | WindowCapabilities::ColorDialog
			| WindowCapabilities::FontDialog | WindowCapabilities::SystemFileActions
			| WindowCapabilities::NativeDialogParenting;
}

WindowsDialogHandle::~WindowsDialogHandle() {
	// Detached rather than joined: the worker is blocked inside a modal loop that only the user (or
	// a Close() we can no longer issue) can end, and it holds its own reference to us — so it
	// cannot outlive anything it touches, and waiting for it here would deadlock teardown.
	if (_thread.joinable()) {
		_thread.detach();
	}
}

bool WindowsDialogHandle::init(NotNull<ContextController> controller,
		NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req, NativeWindow *parent,
		HWND parentWindow) {
	if (!DialogHandle::init(controller, target, sprt::move(req), parent)) {
		return false;
	}

	switch (_request->type) {
	case DialogType::RevealInFileManager:
	case DialogType::MoveToTrash:
	case DialogType::RestoreFromTrash:
		if (_request->paths.empty()) {
			return false;
		}
		break;
	default: break;
	}

	_parentWindow = parentWindow;

	// The worker owns a reference for its whole run, so the handle survives even if the controller
	// drops it while the dialog is still up.
	_thread =
			sprt::thread([guard = Rc<WindowsDialogHandle>(this)]() mutable { guard->runWorker(); });
	return true;
}

void WindowsDialogHandle::runWorker() {
	// One apartment per dialog. APARTMENTTHREADED is not optional: the shell dialogs require an STA,
	// and this thread does nothing else, so it can afford to be blocked for the dialog's lifetime.
	auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	const bool owned = SUCCEEDED(hr);

	DialogResult result;
	switch (_request->type) {
	case DialogType::OpenFile:
	case DialogType::OpenDirectory:
	case DialogType::SaveFile: result = runFileDialog(); break;
	case DialogType::Color: result = runColorDialog(); break;
	case DialogType::Font: result = runFontDialog(); break;
	case DialogType::MoveToTrash: result = runTrash(); break;
	case DialogType::RevealInFileManager: result = runReveal(); break;
	case DialogType::RestoreFromTrash: result = runRestoreFromTrash(); break;
	}

	if (owned) {
		CoUninitialize();
	}

	postResult(sprt::move(result));
}

void WindowsDialogHandle::postResult(DialogResult &&result) {
	// finalize() belongs to the context looper — it unregisters from the controller and releases the
	// modal block — so hop there rather than touching any of it from the worker.
	_controller->getLooper()->performOnThread([this, result = sprt::move(result)]() mutable {
		finalize(sprt::move(result));
	}, this, false, "WindowsDialogHandle::postResult");
}

DialogResult WindowsDialogHandle::runFileDialog() {
	DialogResult result;
	const auto &req = *_request;
	const bool save = req.type == DialogType::SaveFile;

	IFileDialog *dialog = nullptr;
	auto hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr,
			CLSCTX_INPROC_SERVER, __uuidof(IFileDialog), reinterpret_cast<void **>(&dialog));
	if (FAILED(hr) || !dialog) {
		result.status = Status::ErrorNotSupported;
		return result;
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	// FORCEFILESYSTEM keeps the result to things that have a real path — without it the user can
	// pick a virtual item and GetDisplayName(SIGDN_FILESYSPATH) then fails.
	options |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
	if (req.type == DialogType::OpenDirectory) {
		options |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
	} else if (save) {
		options |= FOS_PATHMUSTEXIST;
		if (hasFlag(req.flags, DialogFlags::ConfirmOverwrite)) {
			options |= FOS_OVERWRITEPROMPT;
		}
	} else {
		options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
		if (hasFlag(req.flags, DialogFlags::Multiple)) {
			options |= FOS_ALLOWMULTISELECT;
		}
	}
	if (hasFlag(req.flags, DialogFlags::ShowHidden)) {
		options |= FOS_FORCESHOWHIDDEN;
	}
	dialog->SetOptions(options);

	WideString title;
	if (!req.title.empty()) {
		title = toWide(req.title);
		dialog->SetTitle(reinterpret_cast<PCWSTR>(title.data()));
	}
	WideString acceptLabel;
	if (!req.acceptLabel.empty()) {
		acceptLabel = toWide(req.acceptLabel);
		dialog->SetOkButtonLabel(reinterpret_cast<PCWSTR>(acceptLabel.data()));
	}
	WideString filename;
	if (!req.filename.empty()) {
		filename = toWide(req.filename);
		dialog->SetFileName(reinterpret_cast<PCWSTR>(filename.data()));
	}
	if (auto folder = makeShellItem(req.path)) {
		dialog->SetFolder(folder);
		folder->Release();
	}

	// The filter strings have to outlive Show(), so both vectors stay alive for the whole scope.
	Vector<WideString> filterStorage;
	Vector<COMDLG_FILTERSPEC> filterSpecs;
	if (!req.filters.empty()) {
		filterStorage.reserve(req.filters.size() * 2);
		filterSpecs.reserve(req.filters.size());
		for (auto &filter : req.filters) {
			// Win32 has no MIME notion here; only the globs carry over, joined with ';'.
			String spec;
			for (auto &pattern : filter.patterns) {
				if (!spec.empty()) {
					spec.append(";");
				}
				spec.append(pattern);
			}
			if (spec.empty()) {
				spec = "*.*";
			}
			filterStorage.emplace_back(toWide(filter.name));
			auto namePtr = reinterpret_cast<PCWSTR>(filterStorage.back().data());
			filterStorage.emplace_back(toWide(spec));
			auto specPtr = reinterpret_cast<PCWSTR>(filterStorage.back().data());
			filterSpecs.emplace_back(COMDLG_FILTERSPEC{namePtr, specPtr});
		}
		dialog->SetFileTypes(UINT(filterSpecs.size()), filterSpecs.data());
		if (req.filter < filterSpecs.size()) {
			// One-based, unlike everything else in this API.
			dialog->SetFileTypeIndex(UINT(req.filter) + 1);
		}
	}

	// Publish the pointer only now: from here until Show() returns, cancel() may reach in.
	{
		sprt::unique_lock lock(_mutex);
		_dialog = dialog;
	}

	auto showResult = dialog->Show(_parentWindow);

	// Take it back before releasing, so a cancel() arriving late finds nothing rather than a
	// dangling interface.
	{
		sprt::unique_lock lock(_mutex);
		_dialog = nullptr;
	}

	if (SUCCEEDED(showResult)) {
		result.status = Status::Ok;

		UINT typeIndex = 0;
		if (SUCCEEDED(dialog->GetFileTypeIndex(&typeIndex)) && typeIndex > 0
				&& size_t(typeIndex - 1) < req.filters.size()) {
			result.filter = uint32_t(typeIndex - 1);
		}

		if (!save && hasFlag(req.flags, DialogFlags::Multiple)) {
			IFileOpenDialog *openDialog = nullptr;
			if (SUCCEEDED(dialog->QueryInterface(__uuidof(IFileOpenDialog),
						reinterpret_cast<void **>(&openDialog)))
					&& openDialog) {
				IShellItemArray *items = nullptr;
				if (SUCCEEDED(openDialog->GetResults(&items)) && items) {
					DWORD count = 0;
					items->GetCount(&count);
					for (DWORD i = 0; i < count; ++i) {
						IShellItem *item = nullptr;
						if (SUCCEEDED(items->GetItemAt(i, &item)) && item) {
							auto path = shellItemPath(item);
							if (!path.empty()) {
								result.paths.emplace_back(sprt::move(path));
							}
							item->Release();
						}
					}
					items->Release();
				}
				openDialog->Release();
			}
		} else {
			IShellItem *item = nullptr;
			if (SUCCEEDED(dialog->GetResult(&item)) && item) {
				auto path = shellItemPath(item);
				if (!path.empty()) {
					result.paths.emplace_back(sprt::move(path));
				}
				item->Release();
			}
		}

		if (result.paths.empty()) {
			result.status = Status::Declined;
		}
	} else if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		// Both the user dismissing it and our own Close() land here. Which one it was is decided by
		// finalize(): cancel() has already answered, and this result is dropped.
		result.status = Status::Declined;
	} else {
		result.status = Status::ErrorUnknown;
	}

	dialog->Release();
	return result;
}

DialogResult WindowsDialogHandle::runColorDialog() {
	DialogResult result;

	// The dialog writes through this array, and it has to be exactly 16 entries.
	COLORREF customColors[16];
	for (auto &it : customColors) { it = 0x00FF'FFFF; }

	CHOOSECOLORW cc;
	__builtin_memset(&cc, 0, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = _parentWindow;
	cc.lpCustColors = customColors;
	cc.rgbResult = toColorRef(_request->color);
	cc.Flags = CC_RGBINIT | CC_ANYCOLOR | CC_FULLOPEN;

	if (ChooseColorW(&cc)) {
		result.status = Status::Ok;
		result.color = fromColorRef(cc.rgbResult);
	} else {
		// A plain cancel leaves the extended error at 0; anything else is a real failure.
		result.status = CommDlgExtendedError() == 0 ? Status::Declined : Status::ErrorUnknown;
	}
	return result;
}

DialogResult WindowsDialogHandle::runFontDialog() {
	DialogResult result;

	LOGFONTW logFont;
	__builtin_memset(&logFont, 0, sizeof(logFont));
	logFont.lfWeight = _request->font.bold ? FW_BOLD : FW_NORMAL;
	logFont.lfItalic = _request->font.italic ? 1 : 0;
	if (!_request->font.family.empty()) {
		auto wide = toWide(_request->font.family);
		// lfFaceName is a fixed 32-wchar array including the terminator; longer names are simply
		// not addressable by this API.
		auto count = wide.size() < 31 ? wide.size() : size_t(31);
		for (size_t i = 0; i < count; ++i) { logFont.lfFaceName[i] = wchar_t(wide[i]); }
		logFont.lfFaceName[count] = 0;
	}

	CHOOSEFONTW cf;
	__builtin_memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = _parentWindow;
	cf.lpLogFont = &logFont;
	cf.iPointSize = INT(_request->font.size * 10.0f);
	cf.Flags = CF_SCREENFONTS | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT;

	if (ChooseFontW(&cf)) {
		result.status = Status::Ok;
		result.font.family = fromWide(logFont.lfFaceName);
		result.font.size = float(cf.iPointSize) / 10.0f;
		result.font.bold = logFont.lfWeight >= FW_BOLD;
		result.font.italic = logFont.lfItalic != 0;
		// No native descriptor string exists here, so build the same pango-ish shape the other
		// backends produce; DialogFontInfo::description is documented as backend-defined but is the
		// one field that has to round-trip.
		result.font.description = result.font.family;
		if (result.font.bold) {
			result.font.description.append(" Bold");
		}
		if (result.font.italic) {
			result.font.description.append(" Italic");
		}
		result.font.description.append(" ").append(toString(int(result.font.size)));
	} else {
		result.status = CommDlgExtendedError() == 0 ? Status::Declined : Status::ErrorUnknown;
	}
	return result;
}

DialogResult WindowsDialogHandle::runTrash() {
	DialogResult result;

	IFileOperation *op = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
				__uuidof(IFileOperation), reinterpret_cast<void **>(&op)))
			|| !op) {
		result.status = Status::ErrorNotSupported;
		return result;
	}

	// ALLOWUNDO is what makes this the Recycle Bin rather than a permanent delete; RECYCLEONDELETE
	// says the same thing to Windows 8 and newer, which stopped honouring ALLOWUNDO on its own for
	// some item types. The rest suppresses UI: the caller asked for a move to trash, not a wizard.
	op->SetOperationFlags(FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE | FOF_NOCONFIRMATION | FOF_SILENT
			| FOF_NOERRORUI | FOFX_EARLYFAILURE);
	op->SetOwnerWindow(_parentWindow);

	bool queued = false;
	for (auto &path : _request->paths) {
		if (auto item = makeShellItem(path)) {
			if (SUCCEEDED(op->DeleteItem(item, nullptr))) {
				queued = true;
			}
			item->Release();
		}
	}

	if (!queued) {
		// DeleteItem is E_NOTIMPL on hosts with only a partial shell (wine, notably), so nothing
		// could be queued. Fall back to the pre-Vista API, which such hosts do implement.
		op->Release();
		return runTrashLegacy();
	}

	auto hr = op->PerformOperations();
	BOOL aborted = FALSE;
	op->GetAnyOperationsAborted(&aborted);
	op->Release();

	if (FAILED(hr)) {
		return runTrashLegacy();
	}
	result.status = aborted ? Status::ErrorUnknown : Status::Ok;
	return result;
}

DialogResult WindowsDialogHandle::runTrashLegacy() {
	DialogResult result;

	// pFrom is a double-NUL-terminated list: every path NUL-terminated, then one more NUL. Building
	// it by hand is the whole reason this function is separate.
	WideString buffer;
	for (auto &path : _request->paths) {
		auto wide = toWide(path);
		buffer.append(wide.data(), wide.size());
		buffer.push_back(char16_t(0));
	}
	if (buffer.empty()) {
		result.status = Status::ErrorInvalidArguemnt;
		return result;
	}
	buffer.push_back(char16_t(0));

	SHFILEOPSTRUCTW op;
	__builtin_memset(&op, 0, sizeof(op));
	op.hwnd = _parentWindow;
	op.wFunc = FO_DELETE;
	op.pFrom = reinterpret_cast<PCZZWSTR>(buffer.data());
	// fFlags is a WORD, so only the low half of the FOF_ table is reachable here — which is fine,
	// because ALLOWUNDO (the bit that means "Recycle Bin") is in it.
	op.fFlags = FILEOP_FLAGS(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI);

	auto ret = SHFileOperationW(&op);
	result.status = (ret == 0 && !op.fAnyOperationsAborted) ? Status::Ok : Status::ErrorUnknown;
	return result;
}

/* ---- the Recycle Bin, for RestoreFromTrash --------------------------------------------------

Windows has no restore call. What it has is the Recycle Bin as a shell FOLDER whose items carry the
properties Explorer shows in its columns, and whose context menu carries the shell's own `undelete`
verb. So a restore is: enumerate the folder, match an item on the path it used to have, invoke the
verb on it.

The caller only knows the path the file HAD, which is what the two Displaced properties reconstruct:
PKEY_Displaced_From is the folder it was deleted from and the item's own name is the file. There is
no identity for a trashed item that a caller could have kept instead - nothing hands one back when
the file goes in - so matching on the old path is not a shortcut, it is the whole interface.

WHAT IS NOT PINNED DOWN HERE, and is the first thing to check on a real system: which SIGDN spelling
of a Recycle Bin item's name is the ORIGINAL file name. The parsing name is the bin's internal
`$R…` name, and the normal display name follows the user's hide-extensions setting, so the editing
name - the one a rename box would show - is what this asks for, with the display name as a second
chance. Both are compared, so a host that answers differently than expected still matches.
*/

// Path comparison the way Windows means it: case-insensitive, and '/' is a separator too, since a
// caller that came through the runtime's POSIX paths may well hand one over.
static bool WindowsDialog_pathEquals(StringView lhs, StringView rhs) {
	auto norm = [](char c) -> char {
		if (c == '/') {
			return '\\';
		}
		if (c >= 'A' && c <= 'Z') {
			return char(c - 'A' + 'a');
		}
		return c;
	};

	auto trim = [](StringView str) {
		// A trailing separator is not part of the identity of a path, and the shell never reports
		// one.
		while (str.size() > 1 && (str.back() == '\\' || str.back() == '/')) {
			str = StringView(str.data(), str.size() - 1);
		}
		return str;
	};

	lhs = trim(lhs);
	rhs = trim(rhs);
	if (lhs.size() != rhs.size()) {
		return false;
	}
	for (size_t i = 0; i < lhs.size(); ++i) {
		if (norm(lhs[i]) != norm(rhs[i])) {
			return false;
		}
	}
	return true;
}

// A shell-allocated string, read out and released in one step. Every LPWSTR the property system
// and GetDisplayName hand back is CoTaskMemAlloc'd and belongs to the caller.
static String WindowsDialog_takeShellString(LPWSTR str) {
	if (!str) {
		return String();
	}
	auto ret = fromWide(reinterpret_cast<const wchar_t *>(str));
	CoTaskMemFree(str);
	return ret;
}

// FILETIME is a 64-bit count in two halves; compare it as the one number it is.
static uint64_t WindowsDialog_fileTimeValue(const FILETIME &ft) {
	return (uint64_t(ft.dwHighDateTime) << 32) | uint64_t(ft.dwLowDateTime);
}

// The shell's own restore. `item` is one entry of the Recycle Bin.
static bool WindowsDialog_undelete(IShellItem *item, HWND parentWindow) {
	IContextMenu *menu = nullptr;
	if (FAILED(item->BindToHandler(nullptr, BHID_SFUIObject, __uuidof(IContextMenu),
				reinterpret_cast<void **>(&menu)))
			|| !menu) {
		return false;
	}

	/* The menu is never shown, and it is still not optional: QueryContextMenu is where a shell verb
	handler does its discovery, and InvokeCommand on a handler that never saw one is entitled to
	fail. So build one, let the shell fill it, and throw it away. */
	auto hmenu = CreatePopupMenu();
	if (!hmenu) {
		menu->Release();
		return false;
	}
	menu->QueryContextMenu(hmenu, 0, 1, 0x7FFF, CMF_NORMAL);

	CMINVOKECOMMANDINFO info;
	__builtin_memset(&info, 0, sizeof(info));
	info.cbSize = sizeof(info);
	// NO_UI because this runs on a worker thread that owns no window: without it the shell is free
	// to put its own error box up there. NOASYNC because the answer has to be the operation's, not
	// "it has been started".
	info.fMask = CMIC_MASK_FLAG_NO_UI | CMIC_MASK_NOASYNC;
	info.hwnd = parentWindow;
	// The canonical verb, which is language-independent - the menu TEXT is localized, this is not.
	info.lpVerb = "undelete";
	info.nShow = SW_SHOWNORMAL;

	auto hr = menu->InvokeCommand(&info);

	DestroyMenu(hmenu);
	menu->Release();
	return SUCCEEDED(hr);
}

DialogResult WindowsDialogHandle::runRestoreFromTrash() {
	DialogResult result;

	IShellItem *bin = nullptr;
	// "shell:RecycleBinFolder" rather than a path: the bin is a virtual folder, and the per-volume
	// `$Recycle.Bin` directories it is assembled from are not what the shell will enumerate.
	if (FAILED(SHCreateItemFromParsingName(L"shell:RecycleBinFolder", nullptr, __uuidof(IShellItem),
				reinterpret_cast<void **>(&bin)))
			|| !bin) {
		result.status = Status::ErrorNotSupported;
		return result;
	}

	IEnumShellItems *items = nullptr;
	auto hr = bin->BindToHandler(nullptr, BHID_EnumItems, __uuidof(IEnumShellItems),
			reinterpret_cast<void **>(&items));
	bin->Release();
	if (FAILED(hr) || !items) {
		result.status = Status::ErrorNotSupported;
		return result;
	}

	// One entry per requested path: the item to put back, and when it went in. Held by reference
	// across the whole walk, because the enumerator's own reference goes as soon as we step past it.
	struct Candidate {
		IShellItem *item = nullptr;
		uint64_t deleted = 0;
	};

	Vector<Candidate> best;
	best.resize(_request->paths.size());

	IShellItem *entry = nullptr;
	ULONG fetched = 0;
	while (items->Next(1, &entry, &fetched) == S_OK && fetched == 1 && entry) {
		IShellItem2 *entry2 = nullptr;
		if (FAILED(entry->QueryInterface(__uuidof(IShellItem2), reinterpret_cast<void **>(&entry2)))
				|| !entry2) {
			entry->Release();
			entry = nullptr;
			continue;
		}

		LPWSTR raw = nullptr;
		String folder;
		if (SUCCEEDED(entry2->GetString(PKEY_Displaced_From, &raw))) {
			folder = WindowsDialog_takeShellString(raw);
		}

		raw = nullptr;
		String editing;
		if (SUCCEEDED(entry->GetDisplayName(SIGDN_PARENTRELATIVEEDITING, &raw))) {
			editing = WindowsDialog_takeShellString(raw);
		}

		raw = nullptr;
		String display;
		if (SUCCEEDED(entry->GetDisplayName(SIGDN_NORMALDISPLAY, &raw))) {
			display = WindowsDialog_takeShellString(raw);
		}

		FILETIME deleted;
		__builtin_memset(&deleted, 0, sizeof(deleted));
		entry2->GetFileTime(PKEY_Displaced_Date, &deleted);
		entry2->Release();

		if (!folder.empty()) {
			for (size_t i = 0; i < _request->paths.size(); ++i) {
				auto &wanted = _request->paths[i];
				const bool match = (!editing.empty()
										   && WindowsDialog_pathEquals(wanted,
												   toString(folder, "\\", editing)))
						|| (!display.empty()
								&& WindowsDialog_pathEquals(wanted,
										toString(folder, "\\", display)));
				if (!match) {
					continue;
				}

				// The most recent deletion of a path wins: trashing a file, recreating it and
				// trashing it again leaves two entries, and the one the user means is the last one
				// they made.
				const auto stamp = WindowsDialog_fileTimeValue(deleted);
				if (best[i].item && best[i].deleted >= stamp) {
					continue;
				}
				if (best[i].item) {
					best[i].item->Release();
				}
				entry->AddRef();
				best[i].item = entry;
				best[i].deleted = stamp;
			}
		}

		entry->Release();
		entry = nullptr;
	}
	items->Release();

	size_t failed = 0;
	for (size_t i = 0; i < best.size(); ++i) {
		if (!best[i].item) {
			continue; // never trashed, or trashed to a bin this account cannot see
		}
		if (WindowsDialog_undelete(best[i].item, _parentWindow)) {
			result.paths.emplace_back(_request->paths[i]);
		} else {
			++failed;
		}
		best[i].item->Release();
	}

	// Same three-way answer every backend gives: everything came back, nothing was there at all, or
	// some subset failed - and `paths` says which in all three cases.
	if (result.paths.empty()) {
		result.status = failed > 0 ? Status::ErrorUnknown : Status::ErrorNotFound;
	} else {
		result.status = failed > 0 ? Status::ErrorUnknown : Status::Ok;
	}
	return result;
}

DialogResult WindowsDialogHandle::runReveal() {
	DialogResult result;

	auto wide = toWide(_request->paths.front());
	auto pidl = ILCreateFromPathW(reinterpret_cast<PCWSTR>(wide.data()));
	if (!pidl) {
		result.status = Status::ErrorInvalidArguemnt;
		return result;
	}

	// cidl 0 with a full path selects the item inside its parent folder — the documented way to say
	// "reveal this", as opposed to "open this folder".
	auto hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
	ILFree(pidl);

	result.status = SUCCEEDED(hr) ? Status::Ok : Status::ErrorUnknown;
	return result;
}

Status WindowsDialogHandle::cancel(Status st) {
	if (!isActive()) {
		return Status::ErrorAlreadyPerformed;
	}

	// Ask the dialog to come down. Show() then returns ERROR_CANCELLED on the worker and posts a
	// result that finalize() drops, because the answer below has already been delivered.
	{
		sprt::unique_lock lock(_mutex);
		if (_dialog) {
			_dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
		}
	}

	// ChooseColorW / ChooseFontW have no equivalent of Close(): they own their dialog outright and
	// expose no handle to it. The caller is answered either way — that contract is not negotiable —
	// but the colour or font picker stays on screen until the user dismisses it, and its result is
	// then discarded. Better a stray window than a callback that never comes.
	return DialogHandle::cancel(st);
}

} // namespace sprt::window

#endif // SPRT_WINDOWS
