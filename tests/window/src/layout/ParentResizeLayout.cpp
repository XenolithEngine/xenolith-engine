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

#include "XLCommon.h"

#include "layout/ParentResizeLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr auto s_css = StringView(R"css(
.half { width: 50%; height: 50%; background-color: #1e88e5; }
.abs  { position: absolute; left: 25%; top: 0px; width: 25%; height: 100%; background-color: #43a047; }
/* px-only insets still depend on the parent box (y = parentHeight - top). Without
   HandleParentContentSize this sticks at the first resolve against a zero parent —
   the installer title-line bug. */
.abs-px { position: absolute; left: 10px; top: 8px; width: 40px; height: 20px; background-color: #e53935; }
)css");

static const Size2 s_initial(400.0f, 200.0f);
static const Size2 s_resized(600.0f, 300.0f);

static bool nearSize(const Size2 &got, const Size2 &want) {
	return sprt::abs(got.width - want.width) < 1.0f && sprt::abs(got.height - want.height) < 1.0f;
}

} // namespace

bool ParentResizeLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);

	// A: recursive resolver on the container; percent metrics through the whole subtree
	_containerRec = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_containerRec->addSystem(Rc<ui::StyleResolver>::create(true));

	_child = _containerRec->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_child->addStyleClass("half");
	_child->setAnchorPoint(Vec2(0.0f, 0.0f));
	_child->setPosition(Vec2(0.0f, 0.0f));

	_grandchild = _child->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_grandchild->addStyleClass("half");
	_grandchild->setAnchorPoint(Vec2(0.0f, 0.0f));
	_grandchild->setPosition(Vec2(0.0f, 0.0f));

	_absolute = _containerRec->addChild(Rc<Layer>::create(Color::Black), ZOrder(2));
	_absolute->addStyleClass("abs");

	_absolutePx = _containerRec->addChild(Rc<Layer>::create(Color::Black), ZOrder(3));
	_absolutePx->addStyleClass("abs-px");

	// B: the node carries its OWN non-recursive resolver; the parent resize reaches it
	// through System::handleLayoutInParent
	_containerOwn = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));

	_childOwn = _containerOwn->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	_childOwn->addStyleClass("half");
	_childOwn->addSystem(Rc<ui::StyleResolver>::create());
	_childOwn->setAnchorPoint(Vec2(0.0f, 0.0f));
	_childOwn->setPosition(Vec2(0.0f, 0.0f));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.2f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.2f), [this] { runPhase2(); }));

	return true;
}

void ParentResizeLayout::runPhase1() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("ParentResizeTest", "phase1: ", what);
		}
	};

	expect(nearSize(_child->getContentSize(), Size2(200.0f, 100.0f)),
			"recursive child != 50% of initial container");
	expect(nearSize(_grandchild->getContentSize(), Size2(100.0f, 50.0f)),
			"grandchild != 50% of the child");
	expect(nearSize(_absolute->getContentSize(), Size2(100.0f, 200.0f)),
			"absolute node != 25%x100% of initial container");
	// engine Y is up, anchor (0,1): top: 8px → y = parentHeight - 8
	expect(sprt::abs(_absolutePx->getPosition().y - (s_initial.height - 8.0f)) < 1.0f,
			"px-absolute top did not resolve against the initial parent height");
	expect(sprt::abs(_absolutePx->getPosition().x - 10.0f) < 1.0f, "px-absolute left != 10");
	expect(nearSize(_childOwn->getContentSize(), Size2(200.0f, 100.0f)),
			"own-resolver child != 50% of initial container");

	log::source().warn("ParentResizeTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; resizing containers ", s_initial, " -> ", s_resized);

	_containerRec->setContentSize(s_resized);
	_containerOwn->setContentSize(s_resized);
}

void ParentResizeLayout::runPhase2() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("ParentResizeTest", "phase2: ", what);
		}
	};

	expect(nearSize(_child->getContentSize(), Size2(300.0f, 150.0f)),
			"recursive child did not re-resolve to 50% of the resized container");
	expect(nearSize(_grandchild->getContentSize(), Size2(150.0f, 75.0f)),
			"grandchild did not follow the transitive resize cascade");
	expect(nearSize(_absolute->getContentSize(), Size2(150.0f, 300.0f)),
			"absolute node did not re-resolve its 25%x100% box");
	expect(sprt::abs(_absolute->getPosition().x - 150.0f) < 1.0f,
			"absolute node did not re-resolve left: 25%");
	expect(sprt::abs(_absolutePx->getPosition().y - (s_resized.height - 8.0f)) < 1.0f,
			"px-absolute top did not re-resolve after parent resize");
	expect(nearSize(_childOwn->getContentSize(), Size2(300.0f, 150.0f)),
			"own-resolver child did not re-resolve via handleLayoutInParent");

	log::source().warn("ParentResizeTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void ParentResizeLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	// initial container sizes only: the test itself resizes them later, and this
	// handler must not overwrite that on unrelated relayouts
	Layer *containers[] = {_containerRec, _containerOwn};
	for (size_t i = 0; i < 2; ++i) {
		if (containers[i]->getContentSize() == Size2::ZERO) {
			containers[i]->setContentSize(s_initial);
		}
		containers[i]->setAnchorPoint(Vec2(0.0f, 0.0f));
		containers[i]->setPosition(Vec2(24.0f, getWorkTop() - 340.0f - float(i) * 340.0f));
	}
}

} // namespace stappler::xenolith::app
