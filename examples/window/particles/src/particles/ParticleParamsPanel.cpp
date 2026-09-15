/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "particles/ParticleParamsPanel.h"
#include "particles/ParticleLocale.h"
#include "particles/ParticlePresets.h"
#include "XL2dLabel.h"
#include "XLUiPanel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

using Kind = ParticleParamKind;

// Heights the stylesheet gives a row and a stacked row, the body padding around them and the
// section header: an open section is exactly its declared minimum height
static constexpr float s_headerHeight = 34.0f;
static constexpr float s_rowHeight = 36.0f;
static constexpr float s_stackHeight = 70.0f;
static constexpr float s_rowGap = 4.0f;
static constexpr float s_bodyPadding = 16.0f;

static Color4B toColor4B(SpanView<double> v) {
	auto c = [&](size_t i) {
		return uint8_t(sprt::round(sprt::clamp(i < v.size() ? v[i] : 1.0, 0.0, 1.0) * 255.0));
	};
	return Color4B(c(0), c(1), c(2), c(3));
}

bool ParticleParamsPanel::init(ParticleParamsDelegate *delegate) {
	if (!Node::init()) {
		return false;
	}

	_delegate = delegate;
	setName("panel-column");

	buildHeader();

	_registry = Rc<ui::PanelRegistry>::create();
	_accordion =
			addChild(Rc<ui::AccordionView>::create(Rc<ui::PanelRegistry>(_registry)), ZOrder(10));
	_accordion->setName("params");
	_accordion->setExpansion(ui::AccordionExpansion::Multi);
	_accordion->setSizing(ui::AccordionSizing::Fit);

	Vector<String> sections;
	for (auto section : getParticleSections()) {
		float height = s_headerHeight + s_bodyPadding;
		uint32_t rows = 0;
		for (auto &it : getParticleParams()) {
			if (it.section != section) {
				continue;
			}
			switch (it.kind) {
			case Kind::RangeVec2:
				height += s_stackHeight;
				++rows;
				break;
			case Kind::ColorStops:
				height += s_rowHeight * 4;
				rows += 4;
				break;
			case Kind::AnimCurve:
				height += s_rowHeight * 2;
				rows += 2;
				break;
			default:
				height += s_rowHeight;
				++rows;
				break;
			}
		}
		height += s_rowGap * float(rows > 0 ? rows - 1 : 0);

		ui::DockPanelDescriptor desc;
		desc.id = section.str<Interface>();
		desc.title = getParticleSectionTitle(section).str<Interface>();
		desc.minSize = Size2(360.0f, height);
		desc.flags = ui::DockPanelFlags::None;
		desc.builder = [this, section]() -> Rc<Node> { return buildSection(section); };
		_registry->registerPanel(sp::move(desc));
		sections.emplace_back(section.str<Interface>());
	}
	_accordion->setSections(sp::move(sections));

	return true;
}

void ParticleParamsPanel::buildHeader() {
	auto header = addChild(Rc<Node>::create(), ZOrder(1));
	header->setName("panel-header");

	auto presetRow = header->addChild(Rc<Node>::create(), ZOrder(1));
	presetRow->addStyleClass("field-row");
	auto presetCaption = presetRow->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	presetCaption->addStyleClass("field-caption");
	presetCaption->setString("@Locale:Particles:Preset");

	Vector<ui::SelectOption> presets;
	for (auto name : getParticlePresetNames()) {
		presets.emplace_back(ui::SelectOption{name.str<Interface>(),
			toString("@Locale:Particles:Preset:", name)});
	}
	_presetSelect = presetRow->addChild(Rc<ui::Select>::create(), ZOrder(2));
	_presetSelect->setName("preset");
	_presetSelect->setOptions(presets);
	ui::MenuConfig presetMenu;
	presetMenu.preferNative = false;
	_presetSelect->setPopupConfig(sp::move(presetMenu));
	_presetSelect->setChangeCallback(
			[this](StringView id) { _delegate->handlePresetSelected(id); });

	auto actions = header->addChild(Rc<Node>::create(), ZOrder(2));
	actions->addStyleClass("field-row");

	_emitterSelect = actions->addChild(Rc<ui::Select>::create(), ZOrder(1));
	_emitterSelect->setName("emitter");
	ui::MenuConfig emitterMenu;
	emitterMenu.preferNative = false;
	_emitterSelect->setPopupConfig(sp::move(emitterMenu));
	_emitterSelect->setChangeCallback([this](StringView id) {
		_delegate->handleEmitterSelected(uint32_t(StringView(id).readInteger(10).get(1) - 1));
	});

	auto restart = actions->addChild(Rc<ui::Button>::create([this] { _delegate->handleRestart(); }),
			ZOrder(2));
	restart->setString("@Locale:Particles:Restart");

	auto reset = actions->addChild(Rc<ui::Button>::create([this] { _delegate->handleReset(); }),
			ZOrder(3));
	reset->setString("@Locale:Particles:Reset");

	_localeButton = actions->addChild(Rc<ui::Button>::create([this] {
		if (_localeButton) {
			_localeButton->setString(cycleParticleLocale());
		}
	}),
			ZOrder(4));
	_localeButton->setString(getNextParticleLocaleName());

	auto tools = header->addChild(Rc<Node>::create(), ZOrder(3));
	tools->addStyleClass("field-row");

	_modeSelect = tools->addChild(Rc<ui::Select>::create(), ZOrder(1));
	_modeSelect->setName("scene-mode");
	Vector<ui::SelectOption> modes;
	modes.emplace_back(ui::SelectOption{"move", "@Locale:Particles:Mode:move"});
	modes.emplace_back(ui::SelectOption{"points", "@Locale:Particles:Mode:points"});
	_modeSelect->setOptions(modes);
	_modeSelect->setValue("move", true);
	ui::MenuConfig modeMenu;
	modeMenu.preferNative = false;
	_modeSelect->setPopupConfig(sp::move(modeMenu));
	_modeSelect->setChangeCallback([this](StringView id) { _delegate->handleSceneMode(id); });

	_driveCheck = tools->addChild(Rc<ui::Checkbox>::create(), ZOrder(2));
	_driveCheck->setName("drive");
	_driveCheck->setCallback([this](bool value) { _delegate->handleDrive(value); });

	auto driveLabel = tools->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
	driveLabel->addStyleClass("field-caption");
	driveLabel->addStyleClass("inline-caption");
	driveLabel->setString("@Locale:Particles:Drive");

	auto clear = tools->addChild(Rc<ui::Button>::create([this] { _delegate->handleClearPoints(); }),
			ZOrder(4));
	clear->setString("@Locale:Particles:ClearPoints");

	auto files = header->addChild(Rc<Node>::create(), ZOrder(4));
	files->addStyleClass("field-row");

	int32_t fz = 1;
	auto fileButton = [&](StringView label, Function<void()> &&action) {
		auto button = files->addChild(Rc<ui::Button>::create(sp::move(action)), ZOrder(fz++));
		button->setString(label);
	};
	fileButton("@Locale:Particles:Copy", [this] { _delegate->handleCopy(); });
	fileButton("@Locale:Particles:Paste", [this] { _delegate->handlePaste(); });
	fileButton("@Locale:Particles:Save", [this] { _delegate->handleSave(); });
	fileButton("@Locale:Particles:Open", [this] { _delegate->handleOpen(); });
}

Rc<Node> ParticleParamsPanel::buildSection(StringView section) {
	auto body = Rc<ui::Panel>::create();
	body->setName(toString("section-", section));
	body->addStyleClass("section-body");

	int32_t z = 1;
	for (auto &it : getParticleParams()) {
		if (it.section == section) {
			z = buildRow(body, it, z);
		}
	}

	if (!_system.empty()) {
		for (auto &it : _widgets) {
			if (it.second.info->section == section) {
				writeUi(it.second, particleParamToUi(*it.second.info, _system, _node, _editor));
			}
		}
	}
	return body;
}

Node *ParticleParamsPanel::makeRow(Node *body, StringView caption, int32_t z, StringView name) {
	auto row = body->addChild(Rc<Node>::create(), ZOrder(z));
	row->addStyleClass("field-row");
	row->setName(name);

	auto label = row->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	label->addStyleClass("field-caption");
	label->setString(caption);

	auto controls = row->addChild(Rc<Node>::create(), ZOrder(2));
	controls->addStyleClass("field-controls");
	return controls;
}

int32_t ParticleParamsPanel::buildRow(Node *body, const ParticleParamInfo &info, int32_t z) {
	if (info.kind == Kind::ColorStops) {
		return buildStopsRows(body, info, z);
	}

	auto &w = _widgets.emplace(info.key, ParamWidgets()).first->second;
	w.info = &info;

	if (info.kind == Kind::AnimCurve) {
		auto typeRow = makeRow(body, info.title, z++, toString("param-", info.key));
		Vector<ui::SelectOption> options;
		for (auto &it : getParticleAnimCurveTypes()) {
			options.emplace_back(ui::SelectOption{it.name.str<Interface>(),
				toString("@Locale:Particles:Curve:", it.name)});
		}
		w.select = typeRow->addChild(Rc<ui::Select>::create(), ZOrder(1));
		w.select->setOptions(options);
		ui::MenuConfig menu;
		menu.preferNative = false;
		w.select->setPopupConfig(sp::move(menu));
		w.select->setChangeCallback([this, key = info.key](StringView name) {
			// A new type starts from its own defaults
			auto &widgets = _widgets.find(key)->second;
			if (auto type = getParticleAnimCurveType(name)) {
				for (size_t i = 0; i < widgets.numbers.size(); ++i) {
					widgets.numbers[i]->setValue(type->defaults[i], true);
				}
			}
			emit(widgets);
		});

		auto paramsRow = makeRow(body, "@Locale:Particles:CurveParams", z++,
				toString("param-", info.key, "-params"));
		for (int32_t i = 0; i < 4; ++i) {
			auto field = paramsRow->addChild(Rc<ui::NumberField>::create(), ZOrder(1 + i));
			field->setRange(info.min, info.max);
			field->setStep(info.step);
			field->setDragEnabled(true);
			field->setValueCallback(
					[this, key = info.key](double) { emit(_widgets.find(key)->second); });
			w.numbers.emplace_back(field);
		}
		return z;
	}

	auto row = body->addChild(Rc<Node>::create(), ZOrder(z++));
	row->addStyleClass("field-row");
	if (info.kind == Kind::RangeVec2) {
		row->addStyleClass("stacked");
	}
	row->setName(toString("param-", info.key));

	auto caption = row->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	caption->addStyleClass("field-caption");
	caption->setString(info.title);

	auto controls = row->addChild(Rc<Node>::create(), ZOrder(2));
	controls->addStyleClass((info.kind == Kind::RangeVec2) ? "field-stack" : "field-controls");

	int32_t cz = 1;
	auto number = [&](double min, double max, double step, bool integer) {
		auto field = controls->addChild(Rc<ui::NumberField>::create(), ZOrder(cz++));
		field->setInteger(integer);
		field->setRange(min, max);
		field->setStep(step);
		field->setDragEnabled(true);
		if (!info.unit.empty()) {
			field->setUnit(info.unit);
		}
		field->setValueCallback([this, key = info.key](double) {
			auto it = _widgets.find(key);
			if (it->second.slider && !it->second.numbers.empty()) {
				it->second.slider->setValue(it->second.numbers.front()->getValue(), true);
			}
			emit(it->second);
		});
		w.numbers.emplace_back(field);
		return field;
	};

	auto slider = [&](double min, double max, double step) {
		auto s = controls->addChild(Rc<ui::Slider>::create(), ZOrder(cz++));
		s->setRange(min, max, step);
		s->setCallback([this, key = info.key](int64_t) {
			auto it = _widgets.find(key);
			if (!it->second.numbers.empty()) {
				it->second.numbers.front()->setValue(it->second.slider->getValue(), true);
			}
			emit(it->second);
		});
		w.slider = s;
	};

	auto vector = [&]() {
		auto field = controls->addChild(Rc<ui::VectorField>::create(2), ZOrder(cz++));
		field->setRange(info.min, info.max);
		field->setStep(info.step);
		field->setDragEnabled(true);
		field->setValueCallback(
				[this, key = info.key](SpanView<double>) { emit(_widgets.find(key)->second); });
		w.vectors.emplace_back(field);
	};

	switch (info.kind) {
	case Kind::Int: number(info.min, info.max, info.step, true); break;
	case Kind::Unit:
		number(info.min, info.max, info.step, false);
		slider(info.min, info.max, info.step);
		break;
	case Kind::Range:
		number(info.min, info.max, info.step, false);
		number(info.min, info.max, info.step, false);
		slider(info.min, info.max, info.step);
		break;
	case Kind::RangeVec2:
		vector();
		vector();
		break;
	case Kind::Vec2: vector(); break;
	case Kind::Color: {
		auto field = controls->addChild(Rc<ui::ColorField>::create(), ZOrder(cz++));
		field->setAlphaEnabled(true);
		field->setPickerMode(ui::ColorField::PickerMode::Fallback);
		ui::PopupSurfaceConfig config;
		config.preferNative = false;
		field->setPickerConfig(sp::move(config));
		field->setValueCallback(
				[this, key = info.key](const Color4B &) { emit(_widgets.find(key)->second); });
		w.color = field;
		break;
	}
	case Kind::Flag: {
		auto check = controls->addChild(Rc<ui::Checkbox>::create(), ZOrder(cz++));
		check->setCallback([this, key = info.key](bool) { emit(_widgets.find(key)->second); });
		w.check = check;
		break;
	}
	case Kind::Direction:
		number(-360.0, 360.0, info.step, false);
		number(0.0, 180.0, info.step, false);
		slider(-180.0, 180.0, info.step);
		break;
	case Kind::Fps: number(info.min, info.max, info.step, true); break;
	case Kind::Seed: {
		auto check = controls->addChild(Rc<ui::Checkbox>::create(), ZOrder(cz++));
		check->setCallback([this, key = info.key](bool) { emit(_widgets.find(key)->second); });
		w.check = check;
		number(info.min, info.max, info.step, true);
		break;
	}
	case Kind::Texture: {
		Vector<ui::SelectOption> options;
		for (auto name : getParticleTextureNames()) {
			options.emplace_back(ui::SelectOption{name.str<Interface>(),
				toString("@Locale:Particles:Texture:", name)});
		}
		auto select = controls->addChild(Rc<ui::Select>::create(), ZOrder(cz++));
		select->setOptions(options);
		ui::MenuConfig menu;
		menu.preferNative = false;
		select->setPopupConfig(sp::move(menu));
		select->setChangeCallback(
				[this, key = info.key](StringView) { emit(_widgets.find(key)->second); });
		w.select = select;
		break;
	}
	case Kind::FrameGrid:
		number(info.min, info.max, info.step, true);
		number(info.min, info.max, info.step, true);
		break;
	case Kind::ColorStops:
	case Kind::AnimCurve: break;
	}
	return z;
}

int32_t ParticleParamsPanel::buildStopsRows(Node *body, const ParticleParamInfo &info, int32_t z) {
	auto &w = _widgets.emplace(info.key, ParamWidgets()).first->second;
	w.info = &info;

	auto enabledRow = makeRow(body, info.title, z++, toString("param-", info.key));
	_stops.enabled = enabledRow->addChild(Rc<ui::Checkbox>::create(), ZOrder(1));
	_stops.enabled->setCallback([this](bool value) {
		if (value) {
			editStops([](StopsEditor &) { });
		} else {
			emit(_widgets.find(StringView("colorStops"))->second);
		}
	});
	_stops.preview = enabledRow->addChild(Rc<Node>::create(), ZOrder(2));
	_stops.preview->addStyleClass("curve-preview");
	for (uint32_t i = 0; i < 16; ++i) {
		_stops.previewCells.emplace_back(
				_stops.preview->addChild(Rc<basic2d::Layer>::create(), ZOrder(1 + i)));
	}
	_stops.preview->setContentSizeDirtyCallback([this] {
		auto size = _stops.preview->getContentSize();
		const float cell = size.width / float(_stops.previewCells.size());
		for (size_t i = 0; i < _stops.previewCells.size(); ++i) {
			auto layer = _stops.previewCells[i];
			layer->setAnchorPoint(Anchor::BottomLeft);
			layer->setPosition(Vec2(cell * float(i), 0.0f));
			layer->setContentSize(Size2(cell + 1.0f, size.height));
		}
	});

	auto stopRow = makeRow(body, "@Locale:Particles:Stop", z++, "param-colorStop");
	_stops.select = stopRow->addChild(Rc<ui::Select>::create(), ZOrder(1));
	ui::MenuConfig menu;
	menu.preferNative = false;
	_stops.select->setPopupConfig(sp::move(menu));
	_stops.select->setChangeCallback([this](StringView id) {
		_stops.current = size_t(sprt::max(StringView(id).readInteger(10).get(1) - 1, int64_t(0)));
		updateStopsWidgets();
	});
	auto add = stopRow->addChild(Rc<ui::Button>::create([this] {
		editStops([](StopsEditor &e) {
			auto i = sprt::min(e.current, e.stops.size() - 1);
			auto next = sprt::min(i + 1, e.stops.size() - 1);
			ParticleColorStop stop;
			stop.t = (e.stops[i].t + e.stops[next].t) / 2.0f;
			stop.color = e.stops[i].color * 0.5f + e.stops[next].color * 0.5f;
			e.stops.insert(e.stops.begin() + (next == i ? i + 1 : next), stop);
			e.current = (next == i) ? i + 1 : next;
		});
	}),
			ZOrder(2));
	add->setString("+");
	auto remove = stopRow->addChild(Rc<ui::Button>::create([this] {
		editStops([](StopsEditor &e) {
			if (e.stops.size() > 2) {
				e.stops.erase(e.stops.begin() + e.current);
				e.current = sprt::min(e.current, e.stops.size() - 1);
			}
		});
	}),
			ZOrder(3));
	remove->setString("−");

	auto positionRow =
			makeRow(body, "@Locale:Particles:StopPosition", z++, "param-colorStopPosition");
	_stops.positionField = positionRow->addChild(Rc<ui::NumberField>::create(), ZOrder(1));
	_stops.positionField->setRange(0.0, 1.0);
	_stops.positionField->setStep(0.01);
	_stops.positionField->setDragEnabled(true);
	_stops.position = positionRow->addChild(Rc<ui::Slider>::create(), ZOrder(2));
	_stops.position->setRange(0.0, 1.0, 0.01);

	auto moveStop = [this](double t) {
		editStops([&](StopsEditor &e) {
			auto color = e.stops[e.current].color;
			e.stops[e.current].t = float(sprt::clamp(t, 0.0, 1.0));
			auto moved = e.stops[e.current];
			sprt::stable_sort(e.stops.begin(), e.stops.end(),
					[](const ParticleColorStop &a, const ParticleColorStop &b) {
				return a.t < b.t;
			});
			for (size_t i = 0; i < e.stops.size(); ++i) {
				if (e.stops[i].t == moved.t && e.stops[i].color == color) {
					e.current = i;
				}
			}
		});
	};
	_stops.positionField->setValueCallback(moveStop);
	_stops.position->setCallback(
			[this, moveStop](int64_t) { moveStop(_stops.position->getValue()); });

	auto colorRow = makeRow(body, "@Locale:Particles:StopColor", z++, "param-colorStopColor");
	_stops.color = colorRow->addChild(Rc<ui::ColorField>::create(), ZOrder(1));
	_stops.color->setAlphaEnabled(true);
	_stops.color->setPickerMode(ui::ColorField::PickerMode::Fallback);
	ui::PopupSurfaceConfig config;
	config.preferNative = false;
	_stops.color->setPickerConfig(sp::move(config));
	_stops.color->setValueCallback([this](const Color4B &c) {
		editStops([&](StopsEditor &e) {
			e.stops[e.current].color =
					Color4F(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
		});
	});

	updateStopsWidgets();
	return z;
}

void ParticleParamsPanel::editStops(const Callback<void(StopsEditor &)> &cb) {
	// Editing a stop turns the curve on; turning it on without stops starts from a fade out
	if (_stops.stops.size() < 2) {
		_stops.stops.clear();
		_stops.stops.emplace_back(ParticleColorStop{0.0f, Color4F(1.0f, 1.0f, 1.0f, 1.0f)});
		_stops.stops.emplace_back(ParticleColorStop{1.0f, Color4F(1.0f, 1.0f, 1.0f, 0.0f)});
		_stops.current = 0;
	}
	_stops.enabled->setChecked(true, true);
	cb(_stops);
	updateStopsWidgets();
	emit(_widgets.find(StringView("colorStops"))->second);
}

void ParticleParamsPanel::updateStopsWidgets() {
	auto &e = _stops;
	if (!e.select) {
		return;
	}

	Vector<ui::SelectOption> options;
	for (size_t i = 0; i < e.stops.size(); ++i) {
		options.emplace_back(ui::SelectOption{toString(i + 1), toString("#", i + 1)});
	}
	e.select->setOptions(options);

	if (e.stops.empty()) {
		for (auto cell : e.previewCells) { cell->setColor(Color4F(1.0f, 1.0f, 1.0f, 1.0f)); }
		return;
	}

	e.current = sprt::min(e.current, e.stops.size() - 1);
	e.select->setValue(toString(e.current + 1), true);
	// float32 storage leaves a long tail; the panel shows 1e-4
	const double t = sprt::round(double(e.stops[e.current].t) * 10'000.0) / 10'000.0;
	e.position->setValue(t, true);
	e.positionField->setValue(t, true);
	auto &c = e.stops[e.current].color;
	e.color->setValue(
			Color4B(uint8_t(sprt::round(c.r * 255.0f)), uint8_t(sprt::round(c.g * 255.0f)),
					uint8_t(sprt::round(c.b * 255.0f)), uint8_t(sprt::round(c.a * 255.0f))),
			true);

	auto curve = makeColorCurveValue(e.stops);
	const auto &values = curve.getValue("values");
	const size_t n = values.size() / 4;
	for (size_t i = 0; i < e.previewCells.size(); ++i) {
		auto k = (i * n) / e.previewCells.size();
		auto a = Color4B(uint8_t(values.getDouble(k * 4) * 255.0),
				uint8_t(values.getDouble(k * 4 + 1) * 255.0),
				uint8_t(values.getDouble(k * 4 + 2) * 255.0),
				uint8_t(values.getDouble(k * 4 + 3) * 255.0));
		auto k2 = sprt::min(((i + 1) * n) / e.previewCells.size(), n - 1);
		auto b = Color4B(uint8_t(values.getDouble(k2 * 4) * 255.0),
				uint8_t(values.getDouble(k2 * 4 + 1) * 255.0),
				uint8_t(values.getDouble(k2 * 4 + 2) * 255.0),
				uint8_t(values.getDouble(k2 * 4 + 3) * 255.0));
		e.previewCells[i]->setGradient(
				basic2d::SimpleGradient(a, b, basic2d::SimpleGradient::Horizontal));
	}
}

Vector<double> ParticleParamsPanel::readUi(const ParticleParamInfo &info) const {
	auto it = _widgets.find(info.key);
	if (it == _widgets.end()) {
		return Vector<double>();
	}

	auto &w = it->second;
	Vector<double> ret;
	switch (info.kind) {
	case Kind::ColorStops:
		if (_stops.enabled && _stops.enabled->isChecked()) {
			for (auto &stop : _stops.stops) {
				ret.emplace_back(stop.t);
				ret.emplace_back(stop.color.r);
				ret.emplace_back(stop.color.g);
				ret.emplace_back(stop.color.b);
				ret.emplace_back(stop.color.a);
			}
		}
		return ret;
	case Kind::AnimCurve: {
		auto types = getParticleAnimCurveTypes();
		double index = 0.0;
		for (size_t i = 0; i < types.size(); ++i) {
			if (types[i].name == w.select->getValue()) {
				index = double(i);
			}
		}
		ret.emplace_back(index);
		break;
	}
	case Kind::Seed: ret.emplace_back(w.check->isChecked() ? 1.0 : 0.0); break;
	case Kind::Flag: ret.emplace_back(w.check->isChecked() ? 1.0 : 0.0); break;
	case Kind::Color: {
		auto &c = w.color->getValue();
		ret = Vector<double>{c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0};
		break;
	}
	case Kind::Texture: {
		auto names = getParticleTextureNames();
		for (size_t i = 0; i < names.size(); ++i) {
			if (names[i] == w.select->getValue()) {
				ret.emplace_back(double(i));
			}
		}
		if (ret.empty()) {
			ret.emplace_back(0.0);
		}
		break;
	}
	default: break;
	}

	for (auto field : w.numbers) { ret.emplace_back(field->getValue()); }
	for (auto field : w.vectors) {
		for (auto v : field->getValue()) { ret.emplace_back(v); }
	}
	return ret;
}

void ParticleParamsPanel::writeUi(ParamWidgets &w, SpanView<double> values) {
	size_t i = 0;
	auto next = [&]() { return i < values.size() ? values[i++] : 0.0; };

	switch (w.info->kind) {
	case Kind::ColorStops: {
		Vector<ParticleColorStop> stops;
		for (size_t k = 0; k + 4 < values.size(); k += 5) {
			stops.emplace_back(ParticleColorStop{float(values[k]),
				Color4F(float(values[k + 1]), float(values[k + 2]), float(values[k + 3]),
						float(values[k + 4]))});
		}
		// An empty curve only switches the editor off; the stops stay for turning it back on
		if (!stops.empty()) {
			_stops.stops = sp::move(stops);
		}
		if (_stops.enabled) {
			_stops.enabled->setChecked(!values.empty(), true);
			updateStopsWidgets();
		}
		return;
	}
	case Kind::AnimCurve: {
		auto types = getParticleAnimCurveTypes();
		auto index = size_t(sprt::clamp(next(), 0.0, double(types.size() - 1)));
		w.select->setValue(types[index].name, true);
		break;
	}
	case Kind::Seed:
	case Kind::Flag: w.check->setChecked(next() != 0.0, true); break;
	case Kind::Color: {
		w.color->setValue(toColor4B(values), true);
		i = 4;
		break;
	}
	case Kind::Texture: {
		auto names = getParticleTextureNames();
		auto index = size_t(sprt::clamp(next(), 0.0, double(names.size() - 1)));
		w.select->setValue(names[index], true);
		break;
	}
	default: break;
	}

	for (auto field : w.numbers) { field->setValue(next(), true); }
	for (auto field : w.vectors) {
		double v[2];
		v[0] = next();
		v[1] = next();
		field->setValue(makeSpanView(v, 2), true);
	}
	if (w.slider && !w.numbers.empty()) {
		w.slider->setValue(w.numbers.front()->getValue(), true);
	}
}

void ParticleParamsPanel::emit(const ParamWidgets &w) {
	auto patch = particleParamFromUi(*w.info, readUi(*w.info), _system);

	// Keep the local copy current: the next flag change builds on it
	const Value &src = patch;
	for (auto &it : src.getDict()) {
		if (it.first == "texture" || it.first == "frameGrid") {
			_node.setValue(it.second, it.first);
		} else if (it.first == "colorStops" || it.first == "animCurve") {
			_editor.setValue(it.second, it.first);
		} else {
			_system.setValue(it.second, it.first);
		}
	}

	_delegate->handleParamsPatch(patch);
}

void ParticleParamsPanel::refresh(const Value &system, const Value &node, const Value &editor) {
	_system = system;
	_node = node;
	_editor = editor;
	for (auto &it : _widgets) {
		writeUi(it.second, particleParamToUi(*it.second.info, _system, _node, _editor));
	}
}

void ParticleParamsPanel::setSceneMode(StringView mode) { _modeSelect->setValue(mode, true); }

void ParticleParamsPanel::setDrive(bool value) { _driveCheck->setChecked(value, true); }

void ParticleParamsPanel::setEmitters(uint32_t count, uint32_t selected) {
	Vector<ui::SelectOption> options;
	for (uint32_t i = 0; i < count; ++i) {
		options.emplace_back(ui::SelectOption{toString(i + 1), toString("#", i + 1)});
	}
	_emitterSelect->setOptions(options);
	_emitterSelect->setValue(toString(selected + 1), true);
}

void ParticleParamsPanel::setPreset(StringView name) { _presetSelect->setValue(name, true); }

bool ParticleParamsPanel::setSectionOpen(StringView id, bool open) {
	if (getParticleSectionTitle(id).empty()) {
		return false;
	}
	if (open) {
		_accordion->expandPanel(id);
	} else {
		_accordion->collapsePanel(id);
	}
	return true;
}

bool ParticleParamsPanel::setWidgetValue(StringView key, SpanView<double> values) {
	auto it = _widgets.find(key);
	if (it == _widgets.end()) {
		return false;
	}
	writeUi(it->second, values);
	emit(it->second);
	return true;
}

Value ParticleParamsPanel::encodeUi() const {
	Value ret;
	for (auto &it : _widgets) {
		Value values(Value::Type::ARRAY);
		for (auto v : readUi(*it.second.info)) { values.addDouble(v); }
		ret.setValue(sp::move(values), it.first);
	}
	return ret;
}

} // namespace stappler::xenolith::examples
