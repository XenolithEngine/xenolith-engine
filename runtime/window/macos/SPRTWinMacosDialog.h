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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_MACOS_SPRTWINMACOSDIALOG_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_MACOS_SPRTWINMACOSDIALOG_H_

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_MACOS

#include <sprt/runtime/window/dialog.h>
#include <sprt/runtime/window/controller.h>

namespace sprt::window {

class MacosWindow;

// Everything AppKit serves. Nothing to probe: NSOpenPanel, NSColorPanel and NSFontPanel are part of
// AppKit itself, and NSWorkspace / NSFileManager cover the two shell actions.
SPRT_API WindowCapabilities getMacosDialogCapabilities();

// One dialog, driven entirely on the main thread.
//
// Unlike the other two platforms there is no worker thread here: AppKit is main-thread-only, and
// the context looper IS the main thread, so a panel can be opened right where the request lands.
// What makes that safe is that nothing blocks — every panel is run with
// -beginSheetModalForWindow:completionHandler: (or its NSColorPanel/NSFontPanel equivalent), which
// returns immediately and calls back later on the same run loop.
//
// That also makes the parent relationship real: a sheet is attached to its window, so the OS blocks
// it and raises the sheet on a click, and the sheet closes with the window.
class SPRT_API MacosDialogHandle : public DialogHandle {
public:
	virtual ~MacosDialogHandle();

	virtual bool init(NotNull<ContextController>, NotNull<dispatch::Looper> target,
			Rc<DialogRequest> &&, NativeWindow *parent) override;

	virtual Status cancel(Status st = Status::ErrorCancelled) override;

	virtual void raise() override;

protected:
	// Each opens its panel and returns true once it is on screen; the answer arrives later through
	// finalize(). False means "could not even be shown", and the caller reports it.
	bool openFilePanel();
	bool openColorPanel();
	bool openFontPanel();

	// No UI at all — these answer synchronously.
	DialogResult runReveal();
	DialogResult runTrash();

	// The live panel (NSSavePanel / NSColorPanel / NSFontPanel), retained. void * because this
	// header is included from plain C++ translation units.
	void *_panel = nullptr;

	// Retained observer token for the colour/font panels, which report through notifications
	// rather than a completion handler.
	void *_observer = nullptr;

	// The parent's NSWindow, or nil when the dialog is unparented and has to run as its own modal
	// window rather than as a sheet.
	void *_parentWindow = nullptr;
};

} // namespace sprt::window

#endif // SPRT_MACOS

#endif // CORE_RUNTIME_PRIVATE_WINDOW_MACOS_SPRTWINMACOSDIALOG_H_
