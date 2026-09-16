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

#include "drag/DragTextLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto s_css = StringView(R"css(
text-input { display: flex; width: 240px; height: 32px; background-color: #202020; }
text-input > label { color: #e0e0e0; font-size: 14px; }
)css");

static constexpr auto Payload = StringView("dropped");

} // namespace

bool DragTextLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_field->setName("field");
	_field->setAnchorPoint(Anchor::BottomLeft);
	_field->setPosition(Vec2(60.0f, 200.0f));
	_field->setContentSize(Size2(240.0f, 32.0f));
	_field->setText(StringView("ab"));

	_readOnly = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_readOnly->setName("read-only");
	_readOnly->setAnchorPoint(Anchor::BottomLeft);
	_readOnly->setPosition(Vec2(60.0f, 120.0f));
	_readOnly->setContentSize(Size2(240.0f, 32.0f));
	_readOnly->setText(StringView("locked"));
	_readOnly->setReadOnly(true);

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }));
	return true;
}

void DragTextLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	_drag = DragSystem::acquireForNode(this);
}

bool DragTextLayout::dropOn(Node *target, StringView text, SpanView<StringView> types) {
	if (!_drag) {
		return false;
	}

	DragOffer offer;
	offer.label = String("text");
	for (auto &it : types) { offer.types.emplace_back(it.str<Interface>()); }
	offer.allowedActions = DragActions::Copy | DragActions::Move;
	offer.defaultAction = DragActions::Copy;
	// captures a copy of the string, never the field: the encoder's thread is not ours to assume
	offer.encode = [this, text = text.str<Interface>()](StringView) -> sprt::window::Bytes {
		++_encodes;
		return BytesView(reinterpret_cast<const uint8_t *>(text.data()), text.size())
				.bytes<sprt::window::Bytes>();
	};

	if (!_drag->beginDrag(sp::move(offer), Rc<Ref>(this))) {
		return false;
	}

	const auto size = target->getContentSize();
	_drag->updateDrag(target->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f)));

	const bool hasTarget = _drag->getSession() && _drag->getSession()->getTarget() != nullptr;
	_drag->commitDrag();
	return hasTarget;
}

void DragTextLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DragTextTest", phase, ": ", what);
	}
}

void DragTextLayout::expectText(StringView phase, StringView what, StringView expected) {
	++_checks;
	auto actual = _field->getText();
	if (actual != expected) {
		++_failures;
		log::source().error("DragTextTest", phase, ": ", what, " is '", actual, "', expected '",
				expected, "'");
	}
}

void DragTextLayout::runPhase1() {
	expect(_drag != nullptr, "phase1", "no drag system was acquired");
	if (!_drag) {
		return;
	}

	// Both fields registered themselves as targets simply by being drawn
	expect(_drag->getTargetCount() >= 2, "phase1", "the text fields are not drop targets");

	auto type = StringView("text/plain");
	expect(dropOn(_field, Payload, makeSpanView(&type, 1)), "phase1",
			"the field refused a text/plain drop");
	expect(_encodes == 1, "phase1", "the drop did not encode exactly once");
}

void DragTextLayout::runPhase2() {
	if (!_drag) {
		return;
	}

	// The insertion is a text-input request like any other, so it lands a hop later - which is
	// exactly what makes it identical to a paste
	expectText("phase2", "the field's text after the drop", StringView("abdropped"));

	// A charset-qualified type must be matched too: the field asks for "text/plain" and the rule
	// is by prefix. Getting this wrong is silent - the drop is simply refused
	auto qualified = StringView("text/plain;charset=utf-8");
	_encodes = 0;
	expect(dropOn(_field, StringView("!"), makeSpanView(&qualified, 1)), "phase2",
			"the field refused a charset-qualified text type");

	// and a payload with nothing textual in it is not for a text field
	auto binary = StringView("application/octet-stream");
	expect(!dropOn(_field, StringView("x"), makeSpanView(&binary, 1)), "phase2",
			"the field accepted a payload with no text type");
}

void DragTextLayout::runPhase3() {
	if (!_drag) {
		return;
	}

	expectText("phase3", "the field's text after the second drop", StringView("abdropped!"));

	// A read-only field refuses a drop for the same reason it refuses a paste
	auto type = StringView("text/plain");
	expect(!dropOn(_readOnly, Payload, makeSpanView(&type, 1)), "phase3",
			"a read-only field accepted a drop");
	expect(_readOnly->getText() == "locked", "phase3", "a read-only field was modified by a drop");

	log::source().warn("DragTextTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

} // namespace stappler::xenolith::app
