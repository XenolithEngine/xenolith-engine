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

#include "PugCascadeLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace pugui;

// The OUTER system renders a title near the top and, in its Context, defines the function brand()
// and the variable year.
static constexpr auto s_outerTemplate =
R"Pug(flex(direction="column" justify-content="flex-start" align-items="center" padding=32 gap=8)
	label(font-size=24 color="#90CAF9") Outer system defines brand() + year
)Pug";

// The INNER system's template uses brand() and year WITHOUT defining them - they cascade up from the
// outer system's Context through the VarScope chain.
static constexpr auto s_innerTemplate =
R"Pug(flex(direction="column" justify-content="center" align-items="center" gap=10)
	label(font-size=44 color="#FFEE58") #{brand()} - #{year}
	label(font-size=20 color="#B0BEC5") (resolved by the inner system via the ancestor cascade)
)Pug";

bool PugCascadeLayout::init() {
	if (!SceneLayout2d::init()) {
		return false;
	}

	auto onError = [](StringView err) { log::source().warn("PugCascadeLayout", err); };

	// outer level: owns brand() + year
	_outer = addChild(Rc<Node>::create());

	BuilderConfig outerCfg;
	outerCfg.onError = onError;
	_outerSys = _outer->addSystem(Rc<TemplateSystem>::create(StringView(s_outerTemplate),
			move(outerCfg)));
	_outerSys->setFunction("brand",
			[](spug::VarStorage &, spug::Var *, size_t) -> spug::Var {
		return spug::Var(spug::Value("Xenolith"));
	});
	_outerSys->setVariable("year", Value(2026));
	_outerSys->setBuildCallback([this](TemplateSystem *, SpanView<Rc<Node>> roots) {
		_outerTree = roots.empty() ? nullptr : roots.back().get();
		handleContentSizeDirty();
	});

	// inner level: a child node with its own system; its template consumes brand()/year via cascade
	_inner = _outer->addChild(Rc<Node>::create());

	BuilderConfig innerCfg;
	innerCfg.onError = onError;
	_innerSys = _inner->addSystem(Rc<TemplateSystem>::create(StringView(s_innerTemplate),
			move(innerCfg)));
	_innerSys->setBuildCallback([this](TemplateSystem *, SpanView<Rc<Node>> roots) {
		_innerTree = roots.empty() ? nullptr : roots.back().get();
		handleContentSizeDirty();
	});

	return true;
}

void PugCascadeLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	// the container nodes and the flex roots built into them all fill the layout, so the flex
	// engine centers/positions the labels
	for (auto n : {_outer, _inner, _outerTree, _innerTree}) {
		if (n) {
			n->setAnchorPoint(Anchor::BottomLeft);
			n->setPosition(Vec2::ZERO);
			n->setContentSize(_contentSize);
		}
	}
}

} // namespace stappler::xenolith::app
