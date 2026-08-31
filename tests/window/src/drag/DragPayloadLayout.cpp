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

#include "drag/DragPayloadLayout.h"
#include "XLDropTarget.h"
#include "XLDirector.h"
#include "XLAppThread.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto TargetRect = Rect(60.0f, 60.0f, 240.0f, 140.0f);

static constexpr auto NativeType = StringView("application/x-xenolith-test");
static constexpr auto TextType = StringView("text/plain;charset=utf-8");
static constexpr auto LocalType = StringView("test/local");
static constexpr auto Payload = StringView("hello drag");

static sprt::window::Bytes toBytes(StringView str) {
	return BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size())
			.bytes<sprt::window::Bytes>();
}

} // namespace

bool DragPayloadLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_target = addChild(Rc<basic2d::Layer>::create(Color::Teal_700), ZOrder(1));
	_target->setName("target");
	_target->setAnchorPoint(Anchor::BottomLeft);
	_target->setPosition(TargetRect.origin);
	_target->setContentSize(TargetRect.size);
	setDropTarget(_target,
			DropTargetSlots{
				.accept = [](const DragEvent &event) { return DragResponse{event.allowed}; },
				.drop =
						[this](const DragEvent &event, DragActions) {
		++_drops;
		// The in-process path: take the live object and never ask for bytes. This is the read
		// that must not cost a serialization
		_dropSawLocal = event.data && event.data->isLocal(LocalType);
		return true;
	},
			});

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.8f),
			[this] { runPhase3(); }));
	return true;
}

void DragPayloadLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	_drag = DragSystem::acquireForNode(this);
}

Rc<DragData> DragPayloadLayout::makeData() {
	DragOffer offer;
	offer.label = String("drag payload");
	offer.types = Vector<String>{NativeType.str<Interface>(), TextType.str<Interface>()};
	offer.local = this;
	offer.localType = LocalType.str<Interface>();
	// Captures a COPY of everything it needs and touches no scene node: the contract says this may
	// run on any thread, and once an OS drag exists it will
	offer.encode = [this](StringView) -> sprt::window::Bytes {
		++_encodes;
		return toBytes(Payload);
	};

	return Rc<DragData>::create(offer.takeClipboardData(this), Rc<Ref>(this), offer.localType);
}

void DragPayloadLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DragPayloadTest", phase, ": ", what);
	}
}

void DragPayloadLayout::expectEq(StringView phase, StringView what, size_t actual,
		size_t expected) {
	++_checks;
	if (actual != expected) {
		++_failures;
		log::source().error("DragPayloadTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

void DragPayloadLayout::runPhase1() {
	auto data = makeData();

	expectEq("phase1", "declared types", data->getTypes().size(), 2);

	// hasType is exact; preferType is by prefix, because half the world writes
	// "text/plain;charset=utf-8" where the other half writes "text/plain"
	expect(data->hasType(TextType), "phase1", "the declared text type is missing");
	expect(!data->hasType(StringView("text/plain")), "phase1", "hasType matched a prefix");
	expect(data->preferType(makeSpanView(&NativeType, 1)) == NativeType, "phase1",
			"preferType did not find the native type");

	auto textPreference = StringView("text/plain");
	expect(data->preferType(makeSpanView(&textPreference, 1)) == TextType, "phase1",
			"preferType did not match text/plain by prefix");

	auto missing = StringView("image/png");
	expect(data->preferType(makeSpanView(&missing, 1)).empty(), "phase1",
			"preferType invented a type that was never offered");

	// Everything above only inspected the type list. Not one byte may have been produced
	expectEq("phase1", "encodes after inspecting types alone", _encodes, 0);

	auto bytes = data->encode(NativeType);
	expectEq("phase1", "encodes after one encode()", _encodes, 1);
	expectEq("phase1", "encoded size", bytes.size(), Payload.size());

	// a type that was never offered is refused without calling the encoder
	expect(data->encode(StringView("image/png")).empty(), "phase1",
			"encode produced bytes for a type that was not offered");
	expectEq("phase1", "encodes after an unoffered type", _encodes, 1);

	expect(data->getLocal() == this, "phase1", "the local object did not survive");
	expect(data->isLocal(LocalType), "phase1", "the local type does not match");
	expect(!data->isLocal(NativeType), "phase1", "isLocal matched a MIME type");
}

void DragPayloadLayout::runPhase2() {
	if (!_drag) {
		return;
	}

	const auto before = _encodes;

	// A real drag whose target reads getLocal(). The clipboard half is built - it always is - but
	// nothing may ask it for bytes
	DragOffer offer;
	offer.label = String("drag payload");
	offer.types = Vector<String>{NativeType.str<Interface>(), TextType.str<Interface>()};
	offer.local = this;
	offer.localType = LocalType.str<Interface>();
	offer.encode = [this](StringView) -> sprt::window::Bytes {
		++_encodes;
		return toBytes(Payload);
	};
	offer.allowedActions = DragActions::Move;
	offer.defaultAction = DragActions::Move;

	expect(_drag->beginDrag(sp::move(offer), Rc<Ref>(this)) != nullptr, "phase2",
			"the drag did not start");
	_drag->updateDrag(convertToWorldSpace(Vec2(TargetRect.getMidX(), TargetRect.getMidY())));
	_drag->commitDrag();

	expectEq("phase2", "drops", _drops, 1);
	expect(_dropSawLocal, "phase2", "the drop did not see the local object");
	expectEq("phase2", "encodes during an in-process drop", _encodes, before);

	// The OS-facing half, as far as it can be exercised today: the same object goes to the
	// clipboard, and comes back through the type negotiation a paste performs
	auto app = getDirector() ? getDirector()->getApplication() : nullptr;
	if (!app) {
		expect(false, "phase2", "no application thread");
		return;
	}

	DragOffer clip;
	clip.label = String("drag payload");
	clip.types = Vector<String>{NativeType.str<Interface>(), TextType.str<Interface>()};
	clip.encode = [this](StringView) -> sprt::window::Bytes {
		++_encodes;
		return toBytes(Payload);
	};
	app->writeToClipboard(clip.takeClipboardData(this));

	app->readFromClipboard(
			[this](Status st, BytesView data, StringView type) {
		_readStatus = st;
		_readType = type.str<Interface>();
		_readData = StringView(reinterpret_cast<const char *>(data.data()), data.size())
							.str<Interface>();
		_readDone = true;
	},
			[](SpanView<StringView> types) -> StringView {
		// the selector runs on an unknown thread and may only look at strings
		for (auto &it : types) {
			if (it.starts_with("text/plain")) {
				return it;
			}
		}
		return StringView();
	},
			this);
}

void DragPayloadLayout::runPhase3() {
	expect(_readDone, "phase3", "the clipboard read never answered");
	if (!_readDone) {
		log::source().warn("DragPayloadTest", "SUMMARY: ", _checks, " checks, ", _failures,
				" failures");
		return;
	}

	expect(sprt::status::isSuccessful(_readStatus), "phase3", "the clipboard read failed");
	if (sprt::status::isSuccessful(_readStatus)) {
		log::source().warn("DragPayloadTest", "clipboard returned type '", _readType, "', ",
				_readData.size(), " bytes: '", _readData, "'");
		expect(_readType == TextType, "phase3", "the selector's type is not what came back");
		expect(_readData == Payload, "phase3", "the clipboard returned different bytes");
	}

	log::source().warn("DragPayloadTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

} // namespace stappler::xenolith::app
