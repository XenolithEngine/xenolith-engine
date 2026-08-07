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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSPORTAL_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSPORTAL_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/window/dialog.h>
#include "SPRTWinLinuxDBusController.h"

namespace sprt::window::dbus {

// Can xdg-desktop-portal serve this dialog type at all? There is no portal interface for colors or
// fonts, so those always go to the shell helper.
SPRT_API bool isPortalDialogType(DialogType);

// Is xdg-desktop-portal installed on this machine?
//
// Answered from its D-Bus service file rather than from the bus, on purpose. The portal is
// activatable, so it is legitimately absent from ListNames until something first asks for it; and
// the session bus has not necessarily finished connecting when startup wants this answer. Reading
// the filesystem is synchronous, needs no bus, and describes the machine rather than the current
// state of a daemon — which is exactly what a capability bit has to be.
SPRT_API bool detectDesktopPortal();

// What the portal contributes where it is installed. A subset of what the zenity/kdialog helper
// reports, so on the usual desktop — where both exist — ORing it in changes nothing.
SPRT_API WindowCapabilities getPortalDialogCapabilities(bool hasPortal);

// One dialog served by org.freedesktop.portal.{FileChooser,OpenURI,Trash}.
//
// The portal is a two-step protocol: the method call returns an object path immediately, and the
// answer arrives later as a Response signal on that path. Trash is the exception — it replies with
// the result directly and never creates a Request object.
class SPRT_API PortalDialogHandle : public DialogHandle {
public:
	virtual ~PortalDialogHandle() = default;

	// `parentHandle` is the portal's window identifier ("x11:<hex xid>"), or empty when the session
	// cannot produce one — the portal then places the dialog itself.
	//
	// `fallback` is invoked, at most once and only before anything has been shown to the user, when
	// the portal rejects the call outright: no FileChooser backend installed, or a portal too old
	// for the method. It receives the request back and owns the completion from then on. Without it
	// the same situation is reported as an error.
	virtual bool init(NotNull<ContextController>, NotNull<Controller>,
			NotNull<dispatch::Looper> target, Rc<DialogRequest> &&, NativeWindow *parent,
			StringView parentHandle, Function<void(Rc<DialogRequest> &&)> &&fallback);

	virtual Status cancel(Status st = Status::ErrorCancelled) override;

	// The session bus died while this dialog was up. The portal's Response can never arrive now, so
	// answer instead of waiting forever. The dialog the portal drew is left on screen — it belongs
	// to another process and we no longer have any way to reach it.
	virtual void handleBackendLost() override;

protected:
	// Issue the portal method call for _request->type. False means "this portal cannot express it".
	bool sendRequest(StringView parentHandle);

	// The `o handle` reply (or an error) to that call.
	void handleRequestReply(DBusMessage *reply);

	// Request::Response(u response, a{sv} results) on _requestPath.
	void handleResponse(NotNull<DBusMessage>);

	// Read `uris`/`current_filter` out of the results dictionary.
	void readResults(NotNull<DBusMessage>, DialogResult &);

	// Drop the signal subscription on the context looper rather than here: ~BusFilter erases itself
	// from the very set the D-Bus dispatcher is walking, and does a blocking RemoveMatch round trip
	// that must not run on the application thread.
	void releaseFilter();

	// org.freedesktop.portal.Request::Close(), so the portal takes its dialog down.
	void closeRequest();

	Rc<Controller> _dbus;
	Rc<BusFilter> _filter;
	Function<void(Rc<DialogRequest> &&)> _fallback;

	// Empty until the method reply lands. D-Bus preserves per-sender ordering, so the reply always
	// precedes the Response signal and this is always set by the time a signal has to be matched.
	String _requestPath;

	// cancel() before the reply arrived: there is no object path to Close() yet, so remember and do
	// it when the path shows up.
	bool _closePending = false;

	// MoveToTrash only: TrashFile takes one descriptor per call, so a request naming several files
	// becomes several calls, and the dialog is done when the last of them has answered.
	uint32_t _pendingReplies = 0;
	bool _actionFailed = false;
};

} // namespace sprt::window::dbus

#endif // SPRT_LINUX

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSPORTAL_H_
