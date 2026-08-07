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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDIALOGSHELL_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDIALOGSHELL_H_

#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/window/dialog.h>

#if SPRT_LINUX

namespace sprt::window {

// Desktop helper used to draw the dialog. There is no desktop-neutral picker on Linux, so the
// backend picks whichever of these is installed — probed once, because a capability bit must be a
// stable property of the machine rather than of whichever helper answered last.
enum class ShellDialogTool {
	None,
	Zenity, // GNOME / GTK
	KDialog, // KDE
};

// Look for a helper on PATH. Cheap enough to call at controller startup, and only there.
ShellDialogTool detectShellDialogTool();

// Which dialog types `tool` can serve.
WindowCapabilities getShellDialogCapabilities(ShellDialogTool tool);

// For logging what the startup probe settled on.
StringView getShellDialogToolName(ShellDialogTool tool);

// One dialog, one child process, driven on the controller's looper.
//
// `Rc<dispatch::ProcessHandle>` IS the child: dropping the last reference kills it, which is why
// the handle is held here for as long as the dialog is on screen — and why releasing it is what
// cancel() does.
class ShellDialogHandle : public DialogHandle {
public:
	virtual ~ShellDialogHandle() = default;

	virtual bool init(NotNull<ContextController>, NotNull<dispatch::Looper> target,
			Rc<DialogRequest> &&, NativeWindow *parent, ShellDialogTool tool);

	virtual Status cancel(Status st = Status::ErrorCancelled) override;

protected:
	// Turn the request into a shell command line, or "" when `_tool` cannot serve this type.
	String buildCommand() const;

	// Parse the helper's stdout into a result for this dialog type.
	void handleOutput(int exitCode, Status);

	ShellDialogTool _tool = ShellDialogTool::None;
	String _output;
	Rc<dispatch::ProcessHandle> _process;
};

} // namespace sprt::window

#endif // SPRT_LINUX

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDIALOGSHELL_H_
