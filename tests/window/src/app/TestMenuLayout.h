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

#ifndef TESTS_WINDOW_SRC_APP_TESTMENULAYOUT_H_
#define TESTS_WINDOW_SRC_APP_TESTMENULAYOUT_H_

#include "app/TestLayout.h"
#include "app/TestRegistry.h"
#include "XL2dScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// One level of the test menu: the subgroups of a registry group, then its own tests.
//
// The registry is a tree, so the menu is one too - a group opens this layout over the one that
// listed it, and a test opens the test itself. Depth is not fixed anywhere: a subgroup pushes
// another instance of this layout, and "Go back" pops one level at a time.
//
// It is a TestLayout for the caption alone: the group title and what its tests are about belong in
// the same strip a test writes its own title into, and there is no registry record behind it, so
// it registers no commands.
class TestMenuLayout : public TestLayout {
public:
	virtual ~TestMenuLayout() = default;

	// The group is a static registry record, so keeping a bare pointer to it is safe.
	virtual bool init(const TestGroup &);

	virtual void handleContentSizeDirty() override;

	// Fills the menu of `group` into `controller`: a button per subgroup, then one per test.
	// GeneralLayout, which is the root level of the same menu, builds its own list with it.
	static void buildGroupItems(basic2d::ScrollController *, NotNull<basic2d::SceneLayout2d> owner,
			const TestGroup &group);

protected:
	const TestGroup *_group = nullptr;
	basic2d::ScrollView *_menu = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_APP_TESTMENULAYOUT_H_
