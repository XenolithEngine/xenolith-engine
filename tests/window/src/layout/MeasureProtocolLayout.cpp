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

#include "layout/MeasureProtocolLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;

namespace {

// Natural size the probe reports when nothing constrains it.
static constexpr Size2 ProbeContent(140.0f, 40.0f);

// Width the third probe is clamped to; narrower than ProbeContent, so it has to reflow.
static constexpr float ClampWidth = 80.0f;

// What the MeasureComponent-only box reports for MaxContent.
static constexpr Size2 ComponentContent(110.0f, 25.0f);

static constexpr float FixedWidth = 70.0f;
static constexpr float RowWidth = 700.0f;
static constexpr float RowHeight = 120.0f;

// An application-written answerer: exactly what a custom widget would install to become
// measurable. It reflows like a text run - given less width than it wants, it needs more height.
class ProbeMeasureSystem : public System {
public:
	virtual ~ProbeMeasureSystem() = default;

	virtual bool init() override {
		if (!System::init()) {
			return false;
		}
		_systemFlags = SystemFlags::HandleMeasure;
		return true;
	}

	virtual bool handleMeasure(const MeasureConstraints &c, Size2 &result) override {
		++_measureCount;
		result = ProbeContent;
		if (c.maxWidth != maxOf<float>() && c.maxWidth < result.width) {
			result.width = c.maxWidth;
			result.height = ProbeContent.height * 2.0f; // "wrapped", so twice as tall
		}
		return true;
	}

	virtual void handleLayoutApplied(const Size2 &size) override {
		++_appliedCount;
		_applied = size;
	}

	uint32_t getMeasureCount() const { return _measureCount; }
	uint32_t getAppliedCount() const { return _appliedCount; }
	Size2 getApplied() const { return _applied; }

protected:
	uint32_t _measureCount = 0;
	uint32_t _appliedCount = 0;
	Size2 _applied;
};

static constexpr auto s_css = StringView(R"css(
.row {
	display: flex;
	flex-direction: row;
	align-items: flex-start;
	column-gap: 10px;
}
.fit { flex-basis: fit-content; }
.clamped { flex-basis: fit-content; max-width: 80px; }
.fixed { width: 70px; height: 30px; }
)css");

// A visible box that carries the measure system; a plain Node would be invisible.
Rc<Layer> makeProbe(const Color4F &color) {
	auto node = Rc<Layer>::create(color);
	node->addSystem(Rc<ProbeMeasureSystem>::create());
	return node;
}

ProbeMeasureSystem *getProbeSystem(Node *node) {
	return node ? node->getSystemByType<ProbeMeasureSystem>() : nullptr;
}

} // namespace

bool MeasureProtocolLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_row = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	_row->addStyleClass("row");

	// 1. the default basis: no CSS size, so the content decides
	_autoBasis = _row->addChild(makeProbe(Color::Red_400), ZOrder(1));

	// 2. the same node asked explicitly
	_fitBasis = _row->addChild(makeProbe(Color::Blue_400), ZOrder(1));
	_fitBasis->addStyleClass("fit");

	// 3. clamped below its natural width: the re-measure at the committed width is what makes it
	// taller, and that only happens if the cross axis is measured too
	_clamped = _row->addChild(makeProbe(Color::Green_400), ZOrder(1));
	_clamped->addStyleClass("clamped");

	// 4. no system at all - the precomputed fallback answers instead
	_component = _row->addChild(Rc<Layer>::create(Color::Purple_400), ZOrder(1));
	_component->setOrUpdateComponent<MeasureComponent>([](NotNull<MeasureComponent> mc) {
		// `normal` stays unspecified on both axes, so the style gives no definite size and the
		// max-content entry is what the flex engine ends up reading
		mc->normal = Size2(-1.0f, -1.0f);
		mc->maxContent = ComponentContent;
		return true;
	});

	// 5. the shipped answerer, as a regression guard
	_label = _row->addChild(Rc<Label>::create(), ZOrder(1));
	_label->setFontSize(20);
	_label->setString("Label");
	_label->setColor(Color::Black);

	// 6. an explicit CSS size: definite, so it must never be measured
	_fixed = _row->addChild(Rc<Layer>::create(Color::Amber_400), ZOrder(1));
	_fixed->addStyleClass("fixed");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); }));

	return true;
}

void MeasureProtocolLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("MeasureProtocolTest", what);
	}
}

void MeasureProtocolLayout::expectNear(StringView what, float actual, float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 1.0f) {
		++_failures;
		log::source().error("MeasureProtocolTest", what, " is ", actual, ", expected ", expected);
	}
}

void MeasureProtocolLayout::runPhase1() {
	// 1 + 2: a custom answerer is content-sized on both axes, with or without an explicit
	// fit-content basis - `flex-basis: auto` with no CSS width means the same thing
	expectNear("auto-basis probe width", _autoBasis->getContentSize().width, ProbeContent.width);
	expectNear("auto-basis probe height", _autoBasis->getContentSize().height, ProbeContent.height);
	expectNear("fit-basis probe width", _fitBasis->getContentSize().width, ProbeContent.width);
	expectNear("fit-basis probe height", _fitBasis->getContentSize().height, ProbeContent.height);

	// 3: clamped to less than it wants - the engine must re-measure the cross axis at the
	// committed main size instead of keeping the unconstrained height
	expectNear("clamped probe width", _clamped->getContentSize().width, ClampWidth);
	expectNear("clamped probe height", _clamped->getContentSize().height,
			ProbeContent.height * 2.0f);

	// the measured item is told what box it got (handleLayoutApplied), which is how a real widget
	// adapts its content
	if (auto sys = getProbeSystem(_clamped)) {
		expect(sys->getMeasureCount() > 0, "the clamped probe was never asked to measure");
		expect(sys->getAppliedCount() > 0, "the clamped probe was never told its final box");
		expectNear("box reported to the clamped probe", sys->getApplied().width, ClampWidth);
	} else {
		expect(false, "the probe lost its measure system");
	}

	// 4: no system, only the MeasureComponent fallback
	expectNear("component-only width", _component->getContentSize().width, ComponentContent.width);
	expectNear("component-only height", _component->getContentSize().height,
			ComponentContent.height);

	// 5: the shipped answerer still works
	expect(_label->getContentSize().width > 1.0f, "the label was not measured");

	// 6: a definite CSS size is never replaced by a measurement
	expectNear("fixed box width", _fixed->getContentSize().width, FixedWidth);

	log::source().warn("MeasureProtocolTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void MeasureProtocolLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	_row->setAnchorPoint(Vec2(0.0f, 1.0f));
	_row->setPosition(Vec2(24.0f, getWorkTop() - 24.0f));
	_row->setContentSize(Size2(RowWidth, RowHeight));
}

} // namespace stappler::xenolith::app
