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

#ifndef XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORPIPE_H_
#define XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORPIPE_H_

#include "XLCompositorBlend.h"
#include "XLCompositorInputRouter.h"
#include "XLCorePlaneSource.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;
class ServerAppThread;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

/* A display pipe: one output, the planes on it, and its vblank.

Planes are the frames of virtual windows. Their state is edited as pending and applied together by
commit(), which refuses the whole set when any plane asks for something the pipe cannot do - the
output never shows half of an update. A plane that is not enabled is paused: its window gets no
DisplayLink, so it draws nothing, and it is told it is minimized.

The pace: a new frame of an enabled plane asks the host for a frame; the host showing it is the
vblank, which lets every enabled plane draw its next one - at most once per minimum interval.

Everything here runs on the app thread. */

class DisplayPipe;
class DisplayPlane;

// What the window's frame listener holds instead of the plane: the plane holds the source that
// holds the listener, and a strong reference back would keep the window alive through it.
struct SP_PUBLIC DisplayPlaneLink : public Ref {
	DisplayPlane *plane = nullptr; // app thread; null once the plane is gone
};

// What the pipe can do. A request outside of it is refused by commit(), never approximated.
struct SP_PUBLIC PlaneCaps {
	bool scaling = false; // src and dst may differ in size
	bool planeAlpha = true;
	bool premultiplied = true;
	uint32_t maxPlanes = 32;
};

struct SP_PUBLIC PlaneState {
	URect src; // the shown part of the window's image, in its pixels
	IRect dst; // where it lands, in host pixels
	int32_t z = 0;
	float alpha = 1.0f;
	PlaneBlend blend = PlaneBlend::Opaque;
	bool enabled = false;
	bool focusable = true;
	Vector<IRect> inputRegion; // empty: the whole dst
};

class SP_PUBLIC DisplayPlane : public Ref {
public:
	virtual ~DisplayPlane();

	bool init(DisplayPipe *, uint32_t id, NotNull<AppWindow>);

	uint32_t getId() const { return _id; }
	AppWindow *getWindow() const { return _window; }
	core::PlaneSource *getSource() const { return _source; }

	// Null once the plane was destroyed.
	DisplayPipe *getPipe() const { return _pipe; }

	// What the output shows, and what the next commit() applies.
	const PlaneState &getState() const { return _state; }
	PlaneState &getPending() { return _pending; }
	void revert() { _pending = _state; }

	uint64_t getPublished() const { return _published; }
	uint64_t getLatestSerial() const { return _latestSerial; }
	uint64_t getDisplayLinks() const { return _displayLinks; }

	// The window's frames follow the pipe's vblank; set with its first published frame.
	bool isPaced() const { return _paced; }

protected:
	friend class DisplayPipe;

	DisplayPipe *_pipe = nullptr;
	uint32_t _id = 0;
	Rc<AppWindow> _window;
	Rc<core::PlaneSource> _source;
	Rc<DisplayPlaneLink> _link;
	PlaneState _state;
	PlaneState _pending;
	uint64_t _published = 0;
	uint64_t _latestSerial = 0;
	uint64_t _displayLinks = 0;
	bool _paced = false;
};

// What one committed state shows: the enabled planes, bottom to top. Immutable once published.
struct SP_PUBLIC PipeSnapshot : public Ref {
	struct Entry {
		Rc<DisplayPlane> plane;
		PlaneState state;
	};

	uint64_t serial = 0;
	Color4F background;
	Vector<Entry> planes;
};

class SP_PUBLIC DisplayPipe : public Ref {
public:
	using VblankCallback = Function<void(uint64_t hostFrames)>;

	virtual ~DisplayPipe();

	bool init(NotNull<ServerAppThread>);

	// A new plane is disabled; set its state and commit() to show it.
	Rc<DisplayPlane> createPlane(NotNull<AppWindow>);
	void destroyPlane(NotNull<DisplayPlane>);

	// Apply every plane's pending state at once. On refusal nothing changes and the pending states
	// are kept for the caller to fix or revert.
	Status commit();

	// The plane keyboard input goes to; the window is told it is focused while the host is.
	void setFocusedPlane(DisplayPlane *);
	DisplayPlane *getFocusedPlane() const { return _focused; }

	void setVblankCallback(VblankCallback &&);

	// The shortest time between two vblanks, microseconds; 0 follows the host.
	void setMinFrameInterval(uint64_t);
	uint64_t getMinFrameInterval() const { return _minInterval; }

	void setBackground(const Color4F &);
	const Color4F &getBackground() const { return _background; }

	ServerAppThread *getApplication() const { return _app; }

	// Hands the host's input to the planes.
	InputRouter *getInputRouter() const { return _inputRouter; }

	const PlaneCaps &getCaps() const { return _caps; }
	const Rc<PipeSnapshot> &getSnapshot() const { return _snapshot; }
	SpanView<Rc<DisplayPlane>> getPlanes() const { return _planes; }
	DisplayPlane *getPlane(uint32_t id) const;
	DisplayPlane *getPlane(const AppWindow *) const;

	uint64_t getHostFrames() const { return _hostFrames; }
	uint64_t getVblanks() const { return _vblanks; }

	virtual Extent2 getExtent() const = 0;

	// The host showed a frame of the pipe.
	void handleHostPresented(uint64_t order);

	// A host frame asked for will not come.
	void handleHostFrameDeclined();

protected:
	// Ask the output for a frame of the current snapshot.
	virtual void requestHostFrame() = 0;

	// What the output's compositor pass can do; commit() refuses anything else.
	void setCaps(const PlaneCaps &caps) { _caps = caps; }

	// Whether there is an output to ask: until then a changed snapshot waits for it.
	virtual bool hasOutput() const = 0;

	void handlePlanePublished(DisplayPlane *, uint64_t serial);
	Status validate(const PlaneState &) const;
	void publishSnapshot();
	void scheduleHostFrame();
	void emitVblank();
	void emitDisplayLink(DisplayPlane *);

	ServerAppThread *_app = nullptr;
	PlaneCaps _caps;
	Rc<InputRouter> _inputRouter;
	Vector<Rc<DisplayPlane>> _planes;
	Rc<PipeSnapshot> _snapshot;
	DisplayPlane *_focused = nullptr;
	Color4F _background = Color4F::BLACK;
	VblankCallback _vblankCallback;

	uint32_t _nextPlaneId = 1;
	uint64_t _minInterval = 0;
	uint64_t _lastVblank = 0;
	uint64_t _hostFrames = 0;
	uint64_t _vblanks = 0;

	// A host frame was asked for and has not been shown yet.
	bool _hostFrameOwed = false;
	Rc<sprt::dispatch::Handle> _vblankTimer;
	Rc<sprt::dispatch::Handle> _watchdog;
};

} // namespace stappler::xenolith::compositor

#endif /* XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORPIPE_H_ */
