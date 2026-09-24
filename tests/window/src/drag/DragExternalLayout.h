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

#ifndef TESTS_WINDOW_SRC_DRAG_DRAGEXTERNALLAYOUT_H_
#define TESTS_WINDOW_SRC_DRAG_DRAGEXTERNALLAYOUT_H_

#include "app/TestLayout.h"
#include "XLDragSystem.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A drag from another application, injected where a platform backend reports one: at the native
// window, with a MemoryDropOffer standing in for the OS. Everything above that - the controller,
// the hop to the app thread, the drag system and the targets - is the real path.
//
// What it checks: an external session is found by the same targets as an in-process one, answers
// the OS on every step, reads its data only through DragData::read, and finishes the OS offer once
// the drop slot returned and its read answered - never for a drag that left, and with None for a
// drop nobody took. A drag already in flight refuses it, and a text field takes external text as
// it takes a paste.
//
// On Windows the stand also drives the window's registered IDropTarget with a CF_HDROP data
// object of its own, which is how a check under Wine reaches the OLE backend.
//
// By hand, on a real window system: run `XL_DRAG_EXTERNAL_TEST=1 testapp`, wait for the SUMMARY
// line, then drag files from a file manager onto the teal box - each is logged with its local path
// - or text from a browser into the upper field. A drag cancelled with Escape or taken back out of
// the window must log nothing.
class DragExternalLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

protected:
	virtual void registerCommands() override;

	struct Counters {
		size_t accepts = 0;
		size_t enters = 0;
		size_t overs = 0;
		size_t leaves = 0;
		size_t drops = 0;
	};

	void expect(bool cond, StringView phase, StringView what);

	Rc<core::MemoryDropOffer> makeOffer(StringView type, StringView data, DragActions allowed);
	void send(core::DropPhase, core::MemoryDropOffer *, Vec2 world,
			DragActions preferred = DragActions::None);
	Vec2 centre(Node *) const;

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();
	void runPhase6();
	void runPhase7();
	void runPhase8();

	// The OS side on Windows: the window's own IDropTarget, fed a CF_HDROP data object
	void runNativePhase1();
	void runNativePhase2();
	void runNativePhase3();

	Node *_files = nullptr;
	ui::TextInput *_field = nullptr;
	ui::TextInput *_readOnly = nullptr;

	DragSystem *_drag = nullptr;
	Rc<sprt::dispatch::Looper> _looper;

	Counters _counters;
	Counters _baseline;
	String _uriList;
	String _readText;
	bool _readBeforeFinish = false;
	bool _summary = false;
	size_t _drops = 0;
	Vector<String> _dropped;

	Rc<core::MemoryDropOffer> _offerA;
	Rc<core::MemoryDropOffer> _offerB;
	Rc<core::MemoryDropOffer> _offerC;
	Rc<core::MemoryDropOffer> _offerD;
	Rc<core::MemoryDropOffer> _offerE;
	Rc<core::MemoryDropOffer> _offerF;

	size_t _checks = 0;
	size_t _failures = 0;

	void *_nativeTarget = nullptr;
	void *_nativeWindow = nullptr;
	sprt::atomic<uint32_t> _nativeEffects[4] = {0, 0, 0, 0};
	size_t _nativeDropsBefore = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DRAG_DRAGEXTERNALLAYOUT_H_
