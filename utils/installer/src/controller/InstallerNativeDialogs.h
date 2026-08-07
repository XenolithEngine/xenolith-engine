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

#ifndef UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERNATIVEDIALOGS_H_
#define UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERNATIVEDIALOGS_H_

#include "SPICommon.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppThread;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Native OS dialogs and the file manager, driven straight off Looper::spawnProcess.
//
// These deliberately do NOT live in installer_core: the core is synchronous by design (the CLI
// uses it with no render loop), and a folder picker blocks for as long as the user looks at it —
// which is unbounded. Spawning on the app looper instead means nothing blocks at all: the child
// is watched by the event loop, and both the output reader and the completion fire ON THE APP
// THREAD, so the callback can touch the scene graph directly with no marshalling.
//
// The returned handle IS the child: keep it alive, because dropping the last Rc kills the
// process. It is also the cancel mechanism (release it to dismiss a picker). A nullptr return
// means the spawn failed or the platform has no such dialog — `onDone` has then ALREADY fired
// with an empty string, because no completion will ever come.
//
// Commands still go through the system shell (spawnProcess takes one command line, not argv), so
// every interpolated value is shellQuote()d exactly as in SPIProcess.h.

using ProcessHandle = sprt::dispatch::ProcessHandle;

// Folder picker: macOS osascript, Linux zenity then kdialog. "" on cancel / unsupported.
Rc<ProcessHandle> pickFolderAsync(NotNull<AppThread> app, StringView prompt,
		Function<void(String path)> &&onDone, Ref *owner = nullptr);

// Single-line text prompt, same backends. "" on cancel / unsupported.
Rc<ProcessHandle> promptTextAsync(NotNull<AppThread> app, StringView title, StringView def,
		Function<void(String text)> &&onDone, Ref *owner = nullptr);

// Reveal `path` in Finder / xdg-open / Explorer. Fire-and-forget, but the handle must still be
// kept until the helper exits or it would be killed on the spot.
Rc<ProcessHandle> openInFileManagerAsync(NotNull<AppThread> app, StringView path,
		Ref *owner = nullptr);

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERNATIVEDIALOGS_H_
