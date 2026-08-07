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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DIALOG_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DIALOG_H_

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/dispatch/looper.h>

namespace sprt::window {

class NativeWindow;
class ContextController;

// What the user is asked for. The last two are not dialogs at all — they are OS shell actions
// with no UI of ours — but they ride the same seam so that lifetime, cancellation and
// window-teardown bookkeeping have exactly one implementation instead of two.
enum class DialogType : uint32_t {
	OpenFile, // pick existing file(s); DialogFlags::Multiple selects one-vs-many
	OpenDirectory, // pick an existing folder
	SaveFile, // choose a destination path, which may not exist yet
	Color, // system color picker
	Font, // system font picker

	// DialogRequest::paths is the INPUT; only DialogResult::status is meaningful.
	RevealInFileManager, // select the path in Finder / Explorer / the desktop file manager
	MoveToTrash, // move to Trash / Recycle Bin — recoverable, never a hard delete
};

enum class DialogFlags : uint32_t {
	None,

	// Block the parent window while the dialog is up. The portable guarantee is engine-side:
	// input to the parent is dropped in ContextController::notifyWindowInputEvents. Where the
	// platform reports WindowCapabilities::NativeDialogParenting the OS additionally gets a real
	// parent relationship, so clicking the blocked parent raises the dialog.
	Modal = 1 << 0,

	// OpenFile: allow selecting more than one file.
	Multiple = 1 << 1,

	// Show dot-files / hidden entries by default.
	ShowHidden = 1 << 2,

	// SaveFile: ask before replacing an existing file.
	ConfirmOverwrite = 1 << 3,

	// Color: expose an alpha channel. Without it DialogResult::color.a is always 1.
	AlphaChannel = 1 << 4,
};

SPRT_DEFINE_ENUM_AS_MASK(DialogFlags)

// DialogResult::filter when the platform does not report which file type the user settled on.
constexpr uint32_t DialogFilterUnknown = ~uint32_t(0);

// One entry of the file-type dropdown. `patterns` are globs ("*.png"); `mimeTypes` is preferred by
// xdg-desktop-portal and macOS and is ignored where the platform is glob-only. At least one of the
// two must be non-empty.
struct FileFilter {
	String name;
	Vector<String> patterns;
	Vector<String> mimeTypes;
};

// Result of DialogType::Font. `description` is the backend's own descriptor string ("Sans Bold 12"
// from pango/zenity, the PostScript name on macOS, the face name on Win32) and is the only field
// guaranteed to round-trip back into DialogRequest::font.
struct DialogFontInfo {
	String family;
	String description;
	float size = 0.0f;
	bool bold = false;
	bool italic = false;
};

// Delivered exactly once, on the Looper named at open time.
//
// `status` answers "did this work":
//   Status::Ok                  - user confirmed; the payload fields are valid
//   Status::Declined            - user cancelled or dismissed it; the payload is empty
//   Status::ErrorCancelled      - the application cancelled it, or the parent window died
//   Status::ErrorNotImplemented - the platform has no dialog backend at all
//   Status::ErrorNotSupported   - a backend exists but cannot serve this DialogType
//   anything else               - a backend failure (D-Bus error, HRESULT, failed spawn)
struct DialogResult {
	Status status = Status::ErrorNotImplemented;
	DialogType type = DialogType::OpenFile;

	// OpenFile / OpenDirectory / SaveFile. Absolute native paths, never URIs.
	Vector<String> paths;

	// Index into DialogRequest::filters of the type the user settled on, or
	// DialogFilterUnknown where the platform does not report it.
	uint32_t filter = DialogFilterUnknown;

	Color4F color = Color4F::WHITE;
	DialogFontInfo font;
};

// Description of a dialog to open.
//
// The caller keeps its Rc: the request doubles as the CANCELLATION TOKEN, because the backend's
// DialogHandle is created on the controller's looper and cannot be handed back synchronously to a
// caller on another thread.
//
// Constructed on any thread, consumed on the ContextController's looper. Must not be mutated after
// being passed to openDialog.
struct SPRT_API DialogRequest : public Ref {
	DialogType type = DialogType::OpenFile;
	DialogFlags flags = DialogFlags::None;

	// `WindowInfo::id` of the window that owns this dialog, resolved with
	// ContextController::findWindow at open time. Empty means "no parent": the dialog is not
	// parented, DialogFlags::Modal is ignored, and it is NOT cancelled by any window closing.
	String parentWindowId;

	String title;
	String acceptLabel; // "Open" / "Save" / "Choose"; empty takes the platform default

	// Initial directory. SaveFile additionally uses `filename` as the suggested name.
	String path;
	String filename;

	// Input paths for RevealInFileManager / MoveToTrash.
	Vector<String> paths;

	Vector<FileFilter> filters;
	uint32_t filter = 0; // which filter starts selected

	Color4F color = Color4F::WHITE; // Color: initial value
	DialogFontInfo font; // Font: initial value

	// Invoked exactly once, on the Looper passed to openDialog. Required.
	Function<void(const DialogResult &)> callback;

	// Kept alive by the implementation until `callback` has run.
	Rc<Ref> target;
};

// Backend-side representation of one live dialog.
//
// This never crosses to the application thread. It is created BY the backend on the
// ContextController's looper, lives in that controller's per-window registry, and is destroyed
// there. The application's token is the DialogRequest it already owns.
//
// It is deliberately NOT a dispatch::Handle: that class is welded to Queue/HandleClass/QueueData
// internals (a placement-constructed platform block, friend-only lifecycle, an init() needing a
// HandleClass no window backend can produce). Backends that ARE driven by a dispatch handle — the
// zenity/kdialog subprocess path — wrap an inner Rc<dispatch::ProcessHandle> instead.
class SPRT_API DialogHandle : public Ref {
public:
	virtual ~DialogHandle() = default;

	virtual bool init(NotNull<ContextController>, NotNull<dispatch::Looper> target,
			Rc<DialogRequest> &&, NativeWindow *parent);

	DialogRequest *getRequest() const { return _request; }
	NativeWindow *getParent() const { return _parent; }

	// True until the result has been posted.
	bool isActive() const { return _active; }

	// Dismiss the dialog. The completion still runs exactly once, with `st`. Idempotent.
	// Context looper only.
	virtual Status cancel(Status st = Status::ErrorCancelled);

	// Ask the platform to bring this dialog forward — the "user clicked the blocked parent" path.
	// Backends that cannot raise their dialog leave the default no-op.
	virtual void raise() { }

	// The service this dialog is driven by has gone away — a session bus that died under a portal
	// dialog, say — so its answer can never arrive. Backends that can be orphaned this way override
	// this to finalize; for the rest, whose dialog is a child process of ours and is unaffected by
	// somebody else's service dying, the default no-op is right. Context looper only.
	virtual void handleBackendLost() { }

protected:
	// Post `result` on the target looper and retire the handle: unregisters from the controller
	// (which is what releases the modal block) and delivers the completion. Ignored if already
	// finished, so a backend racing its own cancel cannot double-deliver. Context looper only.
	//
	// EVERY path to a finished dialog must come through here — user confirmed, user cancelled,
	// application cancelled, parent window died, backend failed — or the modal block leaks.
	void finalize(DialogResult &&result);

	// Convenience: finalize with just a status and no payload.
	void finalize(Status);

	// Retire WITHOUT delivering, handing the still-unanswered request back to the caller.
	//
	// The only legitimate use is a backend that has decided, before showing anything to the user,
	// that it cannot serve the request after all — the portal answering an error reply, say — and
	// is passing it to another backend, which then owns the exactly-once contract. Returns nullptr
	// if the dialog has already finished. Context looper only.
	Rc<DialogRequest> abandon();

	Rc<DialogRequest> _request;
	Rc<dispatch::Looper> _target;

	// Raw: the controller owns the registry that owns this handle.
	ContextController *_controller = nullptr;

	// Raw: the controller registry is keyed on the parent and drained in performWindowTeardown
	// before the window is unmapped, so the parent always outlives us.
	NativeWindow *_parent = nullptr;

	bool _active = true;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_DIALOG_H_
