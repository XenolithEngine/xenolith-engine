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

#if defined(DEBUG)
#include <sprt/runtime/dispatch/handle.h> // ListenHandle
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

#if defined(DEBUG)

// Debug-only scene-graph inspector, attached to a node as a System.
//
// While its owner is on a running scene, the system holds a stream-socket listener (the
// platform-independent dispatch::Looper socket API, on the app thread's looper) that serves the
// live node tree and the process log ring buffer to any connector (e.g. an MCP server).
//
// Protocol: the client sends a single text command line - "scene\n" or "logs\n" - and receives
// the text dump followed by EOF. Nothing is computed until a command arrives: the tree is walked
// on demand inside the serve callback, which runs on the app thread (the listener lives on the
// app looper), so no snapshot, no timer and no scene-graph locking are needed.
//
// Address: the XENOLITH_INSPECTOR_ADDRESS environment variable ("unix:/path", "unix:@abstract",
// "host:port" or ":port"), with per-platform defaults: unix:/tmp/xenolith-inspector.sock on
// Linux/macOS, unix:@xenolith-inspector on Android (adb forward tcp:4490
// localabstract:xenolith-inspector), 127.0.0.1:4490 on Windows. On platforms without socket
// support (wasm) the inspector silently does not start.
//
// Only one inspector per process holds the listener; a second one attaches, finds the address
// taken and stays idle until the first releases it on exit.
class SP_PUBLIC SceneInspector : public System {
public:
	virtual ~SceneInspector();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Walks the owner's subtree and writes the text dump; app thread only
	void writeSceneDump(const Callback<void(StringView)> &) const;

protected:
	// Serve one accepted connection: read the command line, reply, close
	void serveConnection(Rc<sprt::dispatch::StreamHandle> &&);

	Rc<sprt::dispatch::ListenHandle> _listener;
};

#endif

// In release builds this is a no-op inline, so there is zero overhead and no socket at all.
namespace inspector {

#if defined(DEBUG)
// Attach a SceneInspector to `root` (no-op if one is already attached to it)
SP_PUBLIC void attach(Node *root);
#else
inline void attach(Node *) { }
#endif

} // namespace inspector

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSCENEINSPECTOR_H_ */
