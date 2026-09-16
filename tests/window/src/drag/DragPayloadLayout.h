/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_DRAG_DRAGPAYLOADLAYOUT_H_
#define TESTS_WINDOW_SRC_DRAG_DRAGPAYLOADLAYOUT_H_

#include "app/TestLayout.h"
#include "XLDragSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The payload: one offer, two ways of reading it, and the promise that neither costs anything it
// does not have to.
//
// The whole point of describing a drag with the clipboard's own data model is that the same
// description serves Ctrl+C and a drop, in this process and later between processes. Two properties
// have to hold for that to be true rather than merely tidy:
//
// - the lazy encoder stays lazy. Listing types, matching them, choosing one - none of it may
//   materialize bytes, or every hover over every target would serialize the payload again;
// - the in-process path never encodes at all. A drop that takes the live object must not pay for
//   a serialization nobody reads.
//
// The third thing checked here is the round trip through the clipboard, which is the only part of
// the OS-facing half that can be exercised today.
//
// RUN THIS ONE HEADLESS. The clipboard round trip is deterministic only there, where the controller
// keeps the data in process. On a real window system taking the selection needs an input serial
// from a focused window, so a test window sitting in the background silently fails to become the
// selection owner and the read comes back with whatever some other application put there - a
// normalized `text/plain` and no bytes, in the case that first caught this.
class DragPayloadLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleEnter(Scene *) override;

protected:
	void expect(bool cond, StringView phase, StringView what);
	void expectEq(StringView phase, StringView what, size_t actual, size_t expected);

	Rc<DragData> makeData();

	void runPhase1();
	void runPhase2();
	void runPhase3();

	Node *_target = nullptr;
	DragSystem *_drag = nullptr;

	// bumped by the offer's encode callback, which is the only thing that may materialize bytes
	size_t _encodes = 0;

	size_t _drops = 0;
	bool _dropSawLocal = false;

	Status _readStatus = Status::Declined;
	String _readType;
	String _readData;
	bool _readDone = false;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DRAG_DRAGPAYLOADLAYOUT_H_
