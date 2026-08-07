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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDIALOG_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDIALOG_H_

#include <sprt/runtime/init.h>

#if SPRT_WINDOWS

#include <sprt/runtime/window/dialog.h>
#include <sprt/runtime/window/controller.h>
#include <sprt/cxx/thread>
#include <sprt/cxx/mutex>

#include <sprt/wrappers/windows/com_cxx.hpp>

namespace sprt::window {

// Everything the shell can serve. Unlike Linux there is nothing to probe: the file dialogs are
// part of Windows itself, and comdlg32 has shipped with it since 3.0.
SPRT_API WindowCapabilities getWindowsDialogCapabilities();

// One dialog, one dedicated STA thread.
//
// Every system dialog on Windows blocks: IFileDialog::Show, ChooseColorW and ChooseFontW each run
// their own modal message loop and return only once the user is finished. The context thread IS the
// win32 message pump, so running one there would stall presentation for as long as the dialog is up.
// Hence a thread per dialog: it enters a single-threaded apartment, blocks for the dialog's whole
// life, and posts the answer back to the context looper.
//
// Cancelling therefore always crosses threads. For the file dialogs that is IFileDialog::Close(),
// which is formally a cross-apartment call on an unmarshalled pointer but is the same thing Qt does
// in QWindowsNativeFileDialogBase::close(); it is verified against a live COM runtime in
// tests/com (Show() returns exactly the HRESULT handed to Close()). ChooseColorW / ChooseFontW
// expose no such door — see cancel().
class SPRT_API WindowsDialogHandle : public DialogHandle {
public:
	virtual ~WindowsDialogHandle();

	virtual bool init(NotNull<ContextController>, NotNull<dispatch::Looper> target,
			Rc<DialogRequest> &&, NativeWindow *parent, HWND parentWindow);

	virtual Status cancel(Status st = Status::ErrorCancelled) override;

protected:
	// Runs on the worker thread; everything below it does too.
	void runWorker();

	DialogResult runFileDialog();
	DialogResult runColorDialog();
	DialogResult runFontDialog();
	DialogResult runTrash();
	// SHFileOperationW: what IFileOperation replaced, kept because a host with only a
	// partial shell implementation (wine) answers DeleteItem with E_NOTIMPL.
	DialogResult runTrashLegacy();
	DialogResult runReveal();

	// Hand `result` back to the context looper, which is the only thread allowed to finalize.
	void postResult(DialogResult &&result);

	// Guards _dialog against cancel() racing the worker's release of it. Show() is called with the
	// lock NOT held — it does not return for as long as the dialog is on screen.
	sprt::mutex _mutex;
	IFileDialog *_dialog = nullptr;

	HWND _parentWindow = nullptr;
	sprt::thread _thread;
};

} // namespace sprt::window

#endif // SPRT_WINDOWS

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDIALOG_H_
