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


#ifndef XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORINPUTROUTER_H_
#define XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORINPUTROUTER_H_

#include "XLCommon.h"
#include "XLCoreInput.h"
#include "XLCoreEnum.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

class DisplayPipe;
class DisplayPlane;

/* The host window's input, handed to the planes.

A pointer goes to the topmost enabled plane whose destination (and input region, when it has one)
contains it; a press captures its id for that plane until it ends. Keys go to the plane in focus.
Each plane gets its events in its own pixels, y up, through the native path of its window, so its
text input processor sees the keys.

Which plane has the pointer and which is focused is told to the planes' windows as their Pointer
and Focused states; Focused also needs the host to be focused. Everything runs on the app thread. */
class SP_PUBLIC InputRouter : public Ref {
public:
	enum class Reason : uint8_t {
		None, // nobody took the event
		Hit, // the plane under the point
		Capture, // the plane that captured the pointer
		Focus, // the plane in focus
	};

	struct Route {
		core::InputEventName event = core::InputEventName::None;
		uint32_t id = 0;
		Vec2 host; // host pixels, y up
		uint32_t plane = 0; // 0: no plane
		Vec2 local; // plane pixels, y up
		Reason reason = Reason::None;
	};

	static constexpr size_t MaxRoutes = 32;

	virtual ~InputRouter() = default;

	bool init(DisplayPipe *);

	// A batch from the host window.
	void handleHostEvents(SpanView<core::InputEventData>);

	// The host's state as the router starts to follow it.
	void setHostState(core::WindowState);
	core::WindowState getHostState() const { return _hostState; }

	// The pipe moved its focus.
	void handleFocusChanged(DisplayPlane *prev, DisplayPlane *next);

	// A plane stopped showing (paused or destroyed): its pointers are cancelled, its keys too.
	void handlePlaneHidden(DisplayPlane *);

	// Oldest first.
	Vector<Route> getRoutes() const;
	void clearRoutes();

	static StringView getReasonName(Reason);

protected:
	struct Capture {
		Rc<DisplayPlane> plane;
		core::InputEventData last; // in the plane's pixels
	};

	struct Batch {
		Rc<DisplayPlane> plane;
		Vector<core::InputEventData> events;
	};

	DisplayPlane *findPlane(const Vec2 &host) const;

	// Host pixels (y up) to the plane's pixels (y up).
	Vec2 toPlane(const DisplayPlane *, const Vec2 &host) const;

	void routePoint(const core::InputEventData &);
	void routeKey(const core::InputEventData &);
	void routeState(const core::InputEventData &);

	void deliver(DisplayPlane *, core::InputEventData &&);
	void flush();

	void setPointerPlane(DisplayPlane *);
	void updateFocused(DisplayPlane *);
	void cancelKeys();

	void addRoute(const core::InputEventData &host, const DisplayPlane *, const Vec2 &local,
			Reason);

	DisplayPipe *_pipe = nullptr;
	core::WindowState _hostState = core::WindowState::None;

	Map<uint32_t, Capture> _captures;
	Rc<DisplayPlane> _pointerPlane;

	// Keys pressed while a plane was focused: they are cancelled there when the focus moves.
	Rc<DisplayPlane> _keyPlane;
	Vector<core::InputEventData> _pressedKeys;

	Vector<Batch> _batches;

	Vector<Route> _routes;
	size_t _nextRoute = 0;
};

} // namespace stappler::xenolith::compositor

#endif /* XENOLITH_RENDERER_COMPOSITOR_XLCOMPOSITORINPUTROUTER_H_ */
