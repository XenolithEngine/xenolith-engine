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

#include "widgets/ClipboardLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAppThread.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto s_clipboardCss = StringView(R"css(
label {
	color: #e8e8e8;
	font-size: 14px;
}
text-input {
	width: 260px;
	height: 30px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 4px;
	padding: 0 8px;
	color: #e8e8e8;
	font-size: 14px;
}
.text-view {
	width: 260px;
	height: 120px;
	background-color: #202026;
	outline-width: 1px;
	outline-color: #3d3d3d;
	color: #e8e8e8;
	font-size: 14px;
}
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool ClipboardLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_clipboardCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_caption = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_caption->setType("label");
	_caption->setString("ClipboardSession: two representations, one payload");

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_field->setName("field");

	_secret = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_secret->setName("secret");

	_view = addChild(Rc<ui::TextView>::create(), ZOrder(1));
	_view->setName("view");

	return true;
}

void ClipboardLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto top = getWorkTop();
	const auto size = getWorkSize();
	auto x = size.width / 2.0f;

	_caption->setAnchorPoint(Anchor::MiddleTop);
	_caption->setPosition(Vec2(x, top - 8.0f));

	_field->setAnchorPoint(Anchor::MiddleTop);
	_field->setPosition(Vec2(x, top - 44.0f));

	_secret->setAnchorPoint(Anchor::MiddleTop);
	_secret->setPosition(Vec2(x, top - 88.0f));

	_view->setAnchorPoint(Anchor::MiddleTop);
	_view->setPosition(Vec2(x, top - 132.0f));
}

ui::TextInput *ClipboardLayout::widget(StringView name) const {
	if (name == "field") {
		return _field;
	} else if (name == "secret") {
		return _secret;
	} else if (name == "view") {
		return _view;
	}
	return nullptr;
}

Value ClipboardLayout::encodeState() const {
	Value ret;
	ret.setBool(_session && _session->isAvailable(), "available");
	ret.setBool(_session && _session->isPending(), "pending");
	ret.setInteger(_deliveries, "deliveries");

	Value widgets;
	auto describe = [&](StringView name, ui::TextInput *input) {
		Value w;
		w.setString(input->getText(), "text");
		w.setInteger(input->getCursor().length, "cursorLength");
		widgets.setValue(sp::move(w), name);
	};
	describe("field", _field);
	describe("secret", _secret);
	describe("view", _view);
	ret.setValue(sp::move(widgets), "widgets");

	ret.setValue(encodeLastRead(), "lastRead");
	return ret;
}

Value ClipboardLayout::encodeLastRead() const {
	Value ret;
	ret.setBool(_everAnswered, "answered");
	ret.setInteger(_lastSerial, "serial");
	ret.setInteger(toInt(_lastStatus), "status");
	ret.setBool(sprt::status::isSuccessful(_lastStatus), "ok");
	ret.setString(_lastType, "type");
	ret.setString(_lastText, "text");

	Value available;
	for (auto &it : _lastAvailable) { available.addString(it); }
	ret.setValue(sp::move(available), "available");
	return ret;
}

void ClipboardLayout::registerCommands() {
	// Built here rather than in init(): the director - and therefore the app thread the session is
	// a seam over - is only there once the layout has entered a scene.
	_session = Rc<ClipboardSession>::create(getDirector()->getApplication());

	addCommand("state", "The session, the widgets and the last answer",
			[this](Value &&) { return encodeState(); });

	addCommand("reset-counters", "Zero the delivery counter and forget the last answer",
			[this](Value &&) {
		_deliveries = 0;
		_everAnswered = false;
		_lastSerial = 0;
		_lastStatus = Status::Declined;
		_lastType.clear();
		_lastText.clear();
		_lastAvailable.clear();
		return ackValue(true);
	});

	addCommand("write", "Put one payload up: {label, reps:[{type, text}]}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);

		ClipboardOffer offer;
		offer.setLabel(in.getString("label"));
		for (auto &it : in.getArray("reps")) {
			offer.addText(it.getString("text"), it.getString("type"));
		}

		auto st = _session->write(sp::move(offer));
		Value ret;
		ret.setBool(sprt::status::isSuccessful(st), "ok");
		ret.setInteger(toInt(st), "status");
		return ret;
	});

	addCommand("write-empty", "Offer nothing at all - which must be REFUSED, not sent",
			[this](Value &&) {
		// On Android an empty type list means "clear the clipboard", so an offer with no
		// representations must never reach the transport
		ClipboardOffer offer;
		auto st = _session->write(sp::move(offer));
		Value ret;
		ret.setBool(sprt::status::isSuccessful(st), "ok");
		ret.setInteger(toInt(st), "status");
		return ret;
	});

	addCommand("read", "Start a read: {prefer:[type]}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);

		Vector<String> owned;
		for (auto &it : in.getArray("prefer")) { owned.emplace_back(it.getString()); }

		Vector<StringView> prefer;
		prefer.reserve(owned.size());
		for (auto &it : owned) { prefer.emplace_back(it); }

		auto serial = _session->read(prefer, [this](const ClipboardSession::Result &result) {
			// Counted rather than merely recorded: the claim this stand exists to check is that
			// this runs exactly once per read, including when nothing matched
			++_deliveries;
			_everAnswered = true;
			_lastStatus = result.status;
			_lastType = result.type.str<Interface>();
			_lastText = result.text().str<Interface>();
			_lastAvailable.clear();
			for (auto &it : result.available) { _lastAvailable.emplace_back(it.str<Interface>()); }
		});
		_lastSerial = serial;

		Value ret;
		ret.setBool(serial != 0, "ok");
		ret.setInteger(serial, "serial");
		// Read back before the answer can have arrived on a real platform; on headless it is
		// already over, and the script asserts which of the two happened
		ret.setBool(_session->isPending(), "pending");
		return ret;
	});

	addCommand("read-then-cancel", "Start a read and cancel it in the SAME turn: {prefer:[type]}",
			[this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);

		Vector<String> owned;
		for (auto &it : in.getArray("prefer")) { owned.emplace_back(it.getString()); }
		Vector<StringView> prefer;
		prefer.reserve(owned.size());
		for (auto &it : owned) { prefer.emplace_back(it); }

		auto serial = _session->read(prefer, [this](const ClipboardSession::Result &) {
			// Must never run. If it does, the counter says so
			++_deliveries;
		});
		_session->cancel();

		Value ret;
		ret.setInteger(serial, "serial");
		ret.setBool(_session->isPending(), "pending");
		return ret;
	});

	addCommand("probe", "What the clipboard can produce right now", [this](Value &&) {
		// Answers on a later turn, so the result lands in the state rather than here
		_session->probe([this](Status st, SpanView<StringView> types) {
			++_deliveries;
			_everAnswered = true;
			_lastStatus = st;
			_lastType.clear();
			_lastText.clear();
			_lastAvailable.clear();
			for (auto &it : types) { _lastAvailable.emplace_back(it.str<Interface>()); }
		});
		return ackValue(true);
	});

	addCommand("set-text", "Put text into a widget: {widget, value}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		if (auto w = widget(in.getString("widget"))) {
			w->setText(in.getString("value"));
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("set-password", "Mask a widget: {widget, value}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		if (auto w = widget(in.getString("widget"))) {
			w->setPasswordMode(in.getBool("value") ? ui::TextInputPasswordMode::ShowNone
												   : ui::TextInputPasswordMode::NotPassword);
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("widget-copy", "Select all in a widget and copy it: {widget}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		if (auto w = widget(in.getString("widget"))) {
			w->selectAll();
			return ackValue(w->copy());
		}
		return ackValue(false);
	});

	addCommand("widget-paste", "Paste into a widget: {widget}", [this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		if (auto w = widget(in.getString("widget"))) {
			return ackValue(w->paste());
		}
		return ackValue(false);
	});

	addCommand("widget-paste-then-blur", "Paste and lose focus in the SAME turn: {widget}",
			[this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		if (auto w = widget(in.getString("widget"))) {
			auto started = w->paste();
			// The answer belongs to the focus that started it, and this is the fix E7 carries:
			// before it, the serial was never invalidated by anything
			w->blur();
			return ackValue(started);
		}
		return ackValue(false);
	});
}

} // namespace stappler::xenolith::app
