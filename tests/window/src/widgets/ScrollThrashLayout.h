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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SCROLLTHRASHLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SCROLLTHRASHLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A scroll whose rows do not agree with the sizes their items declared must not run away.
//
// ScrollController re-runs its windowing pass whenever building a row changes that row's size,
// because every following item shifts. That is correct and normally settles at once - but a row
// that reports a different size on every build never settles, and with the default keepNodes=false
// each pass destroys the rows that left the moved window and builds their replacements. The loop
// had no bound, so it could spend a whole frame building and dropping nodes: the installer's table
// grew to several gigabytes this way.
//
// The rows here deliberately never settle (each build is one pixel taller than the item says), so
// the run is only survivable if the pass is bounded. Passing means: the app still runs, the phases
// below still fire, and the number of live nodes stays in the range the window can hold.
class ScrollThrashLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();
	void runPhase3();

	void expect(bool, StringView what);

	basic2d::ScrollView *_scroll = nullptr;
	basic2d::ScrollController *_controller = nullptr;

	// how many rows have been built in total; an unbounded pass shows up here as a huge number
	uint32_t _builtRows = 0;
	uint32_t _builtAtPhase1 = 0;
	uint32_t _builtAtPhase2 = 0;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SCROLLTHRASHLAYOUT_H_
