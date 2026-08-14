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

#ifndef XENOLITH_APPLICATION_NODES_XLSCENEINSPECTOR_H_
#define XENOLITH_APPLICATION_NODES_XLSCENEINSPECTOR_H_

#include "XLSystem.h"

#include <sprt/runtime/dispatch/handle.h> // ListenHandle

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Scene-graph inspector and external control channel, attached to a node as a System.
//
// While its owner is on a running scene, the system holds a stream-socket listener (the
// platform-independent dispatch::Looper socket API, on the app thread's looper). Everything runs on
// the app thread - the same thread that owns the scene graph - so no snapshot, no timer and no
// scene-graph locking are needed; the tree is walked on demand inside the serve callback.
//
// Two protocols share the socket, chosen by the first line a client sends:
//
//  * legacy text: "scene\n", "logs\n" or "fonts\n" -> text dump followed by EOF. One command per
//    connection.
//
//  * framed session: "xenolith/1\n" (optionally "xenolith/1 json") -> the server answers
//    "# xenolith/1 ok <enc>\n" and both sides switch to length-prefixed frames:
//        [u32 little-endian payload size][payload]
//    where the payload is a data::Value encoded as CBOR (default) or JSON. The connection stays
//    open and requests are correlated by serial:
//        request  { "serial": u32, "cmd": "...", ...arguments }
//        response { "serial": u32, "status": "ok"|"error", "error": "...", "result": ... }
//    Replies may arrive out of order - screenshots and scene commands complete asynchronously.
//
// Commands: `scene`, `logs`, `fonts`, `windows`, `commands`, `invoke`, `screenshot`, `input`,
// `text`, `frame`, `window`, `quit`. `commands`/`invoke` expose whatever the running scene
// registered through addCommand.
//
// Every command except `logs` (a process-wide ring buffer), `fonts` (the application's font
// controller, shared by every window) and `quit` (which shuts the process down) acts on ONE
// window's scene, chosen by an optional `"window": "<id>"` argument and
// defaulting to the scene this system is attached to. `windows` lists the ids. That is what makes
// an auxiliary window reachable: SceneContent attaches an inspector to every scene, but only the
// first one to attach owns the socket - so a popup, a dialog or a second root window is inspected,
// screenshotted, stepped and driven through the id, not through a second connection. Each window
// also has its own presentation engine, so in headless mode `frame` has to be sent per window.
//
// `input` injects events; with "native": true they go through the OS window first, so the
// platform's text-input processor claims printable keys, Backspace, Delete and Escape exactly as
// it would for a real keyboard. `text` drives that processor directly - insert, marked/unmark
// (IME composition, which no keystroke can express), delete-backward/forward, cancel - and
// `{"op":"state"}` reads the application-side mirror back.
//
// Address: the XENOLITH_INSPECTOR_ADDRESS environment variable ("unix:/path", "unix:@abstract",
// "host:port" or ":port"), with per-platform defaults: unix:/tmp/xenolith-inspector.sock on
// Linux/macOS, unix:@xenolith-inspector on Android (adb forward tcp:4490
// localabstract:xenolith-inspector), 127.0.0.1:4490 on Windows. On platforms without socket
// support (wasm) the inspector silently does not start.
//
// The listener is armed in debug builds, whenever XENOLITH_INSPECTOR_ADDRESS is set, and always in
// headless mode (where it is the only way to talk to the process). Otherwise the system is present
// but idle. Only one inspector per process holds the listener; a second one attaches, finds the
// address taken and stays idle until the first releases it on exit.
class SP_PUBLIC SceneInspector : public System {
public:
	// A command the scene exposes to the outside world. `done` reports the result and must be
	// called exactly once; it may be called later and from another thread hop.
	using CommandCallback = Function<void(Value &&args, Function<void(Value &&)> &&done)>;

	virtual ~SceneInspector();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Walks the owner's subtree and writes the text dump; app thread only
	void writeSceneDump(const Callback<void(StringView)> &) const;

	// Every font set the application's FontController currently holds, with what each one costs:
	// glyphs required from the atlas, cached shaping entries, kerning pairs, and how many nodes
	// still hold the set (`users`: zero means the next update drops it). App thread only.
	//
	// { "controller": {...totals...}, "layouts": [ { ..., "faces": [...] } ] }
	Value getFontInfo() const;
	void writeFontDump(const Callback<void(StringView)> &) const;

	// Register a command reachable through the `invoke` protocol command. Replaces an existing
	// command with the same name. App thread only.
	void addCommand(StringView name, StringView description, CommandCallback &&);
	bool removeCommand(StringView name);

	// { "commands": [ { "name", "description" } ] }
	Value getCommandList() const;

	// { "windows": [ { "id", "type", "parent", "title", "width", "height", "density",
	// "default" } ] } - every window in the process that has a scene, not only this one. App
	// thread.
	Value getWindowList() const;

	// WindowInfo::id of the window this inspector's scene runs on; empty before it exists.
	StringView getWindowId() const;

protected:
	struct Command {
		String description;
		CommandCallback callback;
	};

	// One accepted connection. Starts in line mode and either serves a single legacy text command
	// or upgrades to the framed protocol and stays open.
	struct Session : Ref {
		Rc<sprt::dispatch::StreamHandle> handle;
		Rc<SceneInspector> inspector;
		String line; // accumulates the handshake line
		Bytes buffer; // accumulates framed payloads
		bool framed = false;
		bool json = false;
	};

	void serveConnection(Rc<sprt::dispatch::StreamHandle> &&);

	// Stop reading and release the handle (see the .cc: an armed handle blocks app-thread shutdown)
	Status finishSession(NotNull<Session>);

	// Consume as much of the accumulated buffer as forms complete frames
	Status readSession(NotNull<Session>, BytesView);
	Status readHandshake(NotNull<Session>, BytesView);

	void handleRequest(NotNull<Session>, Value &&request);

	// The inspector a request is aimed at: the one whose window matches `request["window"]`, or
	// this one when the key is absent. Null means the id named no live window - the request has
	// already been answered with an error.
	SceneInspector *resolveTarget(NotNull<Session>, int64_t serial, const Value &request);

	void sendResponse(NotNull<Session>, int64_t serial, Value &&result);
	void sendError(NotNull<Session>, int64_t serial, StringView error);
	void sendFrame(NotNull<Session>, const Value &);

	// The render session of the owner's director, or null if the scene is not attached yet
	core::RenderServerChannel *getRenderServer() const;

	void handleScreenshot(NotNull<Session>, int64_t serial, Value &&args);
	void handleInvoke(NotNull<Session>, int64_t serial, Value &&args);
	void handleInput(NotNull<Session>, int64_t serial, Value &&args);
	void handleText(NotNull<Session>, int64_t serial, Value &&args);
	void handleWindow(NotNull<Session>, int64_t serial, Value &&args);

	Rc<sprt::dispatch::ListenHandle> _listener;
	Set<Rc<Session>> _sessions;
	Map<String, Command> _commands;
};

namespace inspector {

// Attach a SceneInspector to `root` (no-op if one is already attached to it)
SP_PUBLIC void attach(Node *root);

// The inspector attached to `root`, if any. Scenes use this to register commands:
//   if (auto i = inspector::get(getContent())) { i->addCommand("reload", "...", ...); }
SP_PUBLIC SceneInspector *get(Node *root);

// Convenience wrapper around get() + addCommand(); returns false if `root` has no inspector
SP_PUBLIC bool addCommand(Node *root, StringView name, StringView description,
		SceneInspector::CommandCallback &&);

} // namespace inspector

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSCENEINSPECTOR_H_ */
