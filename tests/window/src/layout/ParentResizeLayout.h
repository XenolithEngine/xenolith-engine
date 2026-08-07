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

#ifndef TESTS_WINDOW_SRC_LAYOUT_PARENTRESIZELAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_PARENTRESIZELAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for style re-resolution on parent resize. Percent metrics are computed
// against the parent size at apply time; this test resizes the containers and checks that the
// styles are re-resolved: a recursive-resolver subtree (child 50%, grandchild 50% of the child
// - transitive cascade, plus a position:absolute left:25% node and a px-only absolute inset
// that must also re-resolve) and a standalone node with its own non-recursive resolver
// (the System::handleLayoutInParent path).
class ParentResizeLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();

	basic2d::Layer *_containerRec = nullptr;
	basic2d::Layer *_child = nullptr;
	basic2d::Layer *_grandchild = nullptr;
	basic2d::Layer *_absolute = nullptr;
	basic2d::Layer *_absolutePx = nullptr;

	basic2d::Layer *_containerOwn = nullptr;
	basic2d::Layer *_childOwn = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_PARENTRESIZELAYOUT_H_
