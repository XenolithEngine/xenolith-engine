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

#ifndef TESTS_WINDOW_SRC_APP_TESTREGISTRY_H_
#define TESTS_WINDOW_SRC_APP_TESTREGISTRY_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// One demo/verification layout of this app.
//
// This is the single place where a test is declared: the env var that selects it, what it is
// called, what to look at, and how to build it. It used to be a hand-maintained if/else chain in
// ExampleScene plus a comment in each header, which meant three copies of the same knowledge.
struct TestInfo {
	// Short stable id: what the `layout` inspector command takes, and the prefix every command the
	// layout registers gets ("flex" -> "flex.mode"). "default" for the front page.
	StringView name;

	// Environment variable that selects this test; empty for the default layout. Must be a string
	// literal - it is handed to getenv(), which needs it null-terminated.
	StringView env;

	// Shown in the on-screen caption
	StringView title;

	// One line, shown under the title. Say what a correct run looks like, not how it is
	// implemented - the header comment of the layout is where the implementation is explained.
	StringView description;

	Rc<basic2d::SceneLayout2d> (*make)();

	// The FPS counter is marked AlwaysDirty, so it damages a region every frame. A test that
	// verifies damage tracking has to run without it.
	bool hideFps = false;
};

SpanView<TestInfo> getTestRegistry();

// The entry with this short name, or null. `name` is what the `layout` command receives.
const TestInfo *findTest(StringView name);

// The entry selected by the environment, or the default one.
const TestInfo &getSelectedTest();

// Build a layout with its caption already filled in.
Rc<basic2d::SceneLayout2d> makeTestLayout(const TestInfo &);

// Same, for whatever the environment selected.
Rc<basic2d::SceneLayout2d> makeSelectedTestLayout();

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_APP_TESTREGISTRY_H_
