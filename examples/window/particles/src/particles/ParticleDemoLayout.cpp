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

#include "particles/ParticleDemoLayout.h"
#include "particles/ParticleTextures.h"
#include "particles/ParticlePresets.h"
#include "particles/ParticleLocale.h"
#include "particles/ParticleCurves.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "SPFilesystem.h"
#include "XLScene.h"
#include "XL2dSceneContent.h"
#include "XLSceneInspector.h"
#include "XLDirector.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

static constexpr uint32_t s_maxEmitters = 2;

// What each emitter starts with
static constexpr StringView s_initialPresets[] = {"fire", "explosion"};

static constexpr auto s_css = StringView(R"css(
:root {
	--panel:     #1e1e24;
	--control:   #2e2e36;
	--outline:   #4a4a55;
	--accent:    #3d7ecf;
	--text:      #e8e8e8;
	--muted:     #9a9aa5;
	--field-h:   30px;
}

#demo-root     { display: flex; flex-direction: row; align-items: stretch; }
#scene-area    { flex: 1 1 0px; overflow: hidden; }
#panel-column  { flex: 0 0 460px; display: flex; flex-direction: column; row-gap: 6px;
                 padding: 8px; }
#panel-header  { display: flex; flex-direction: column; row-gap: 4px; }

/* Rows declare their height: a nested flex row measures to its labels, not to its fields */
.field-row      { display: flex; flex-direction: row; align-items: center; column-gap: 8px;
                  padding: 3px 0px; height: 36px; }
.field-row.stacked { height: 70px; }
.field-caption  { flex: 0 0 140px; color: var(--muted); font-size: 12px; }
.field-controls { flex: 1 1 0px; display: flex; flex-direction: row; align-items: center;
                  column-gap: 6px; }
.field-stack    { flex: 1 1 0px; display: flex; flex-direction: column; row-gap: 4px; }

number-field {
	flex: 1 1 0px; height: var(--field-h);
	background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
	border-radius: 5px; padding: 0px 6px; color: var(--text); font-size: 12px;
	--caret-color: #fcb400; --selection-color: rgba(252,180,0,.35);
}
number-field:hover { outline-color: #6a6a78; }
number-field:focus { outline-color: var(--accent); }

vector-field { flex: 1 1 0px; display: flex; flex-direction: row; column-gap: 4px; }
vector-field > number-field { flex: 1 1 0px; }
vector-field > component-label { display: none; }
.field-stack > vector-field { flex: 0 0 30px; }

slider       { flex: 1 1 0px; height: 16px; background-color: var(--control); border-radius: 3px; }
slider-fill  { background-color: var(--accent); border-radius: 3px; }
slider-thumb { width: 12px; height: 12px; border-radius: 6px; background-color: #f2f2f2; }

checkbox { flex: 0 0 18px; height: 18px; border-radius: 4px;
           background-color: var(--control); outline-color: var(--outline); outline-width: 1px; }
checkbox:checked { background-color: var(--accent); outline-color: var(--accent); }
checkbox > icon  { width: 14px; height: 14px; color: #ffffff; }

select { flex: 1 1 0px; height: var(--field-h); display: flex; flex-direction: row;
         align-items: center; column-gap: 6px; padding: 0px 8px;
         background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
         border-radius: 5px; }
select:focus   { outline-color: var(--accent); }
select > label { flex-grow: 1; color: var(--text); font-size: 12px; }
select > icon  { width: 16px; height: 16px; color: var(--muted); }

color-field { flex: 1 1 0px; height: var(--field-h); display: flex; flex-direction: row;
              align-items: center; column-gap: 8px; padding: 0px 8px;
              background-color: var(--control); outline-color: var(--outline); outline-width: 1px;
              border-radius: 5px; }
color-field > swatch     { flex: 0 0 26px; height: 18px; }
color-field > text-input { flex-grow: 1; outline-width: 0px; background-color: transparent;
                           padding: 0px; color: var(--text); font-size: 12px; }
color-field > icon       { flex: 0 0 18px; height: 18px; color: var(--muted); }

button { height: 30px; display: flex; flex-direction: row; justify-content: center;
         align-items: center; padding: 0px 12px; border-radius: 5px;
         background-color: var(--control); outline-color: var(--outline); outline-width: 1px; }
button:hover   { background-color: #3a3a44; }
button > label { color: var(--text); font-size: 12px; }
#panel-header button { flex: 0 0 fit-content; align-self: stretch; }
#panel-header select#emitter { flex: 0 0 70px; }

accordion-view    { flex-grow: 1; background-color: transparent; }
accordion-section { background-color: transparent; }
accordion-header  { display: flex; flex-direction: row; align-items: center; column-gap: 6px;
                    padding: 5px 8px; background-color: #2b2b33; }
accordion-header:hover    { background-color: #34343e; }
accordion-header.expanded { background-color: #3a3a46; }
accordion-header > label  { flex-grow: 1; color: var(--text); font-size: 13px; }
.accordion-chevron { width: 16px; height: 16px; color: var(--muted); }
accordion-close    { width: 16px; height: 16px; background-color: transparent; }
accordion-body     { display: flex; flex-direction: column; row-gap: 4px; padding: 8px 8px;
                     background-color: var(--panel); }
.section-body { display: flex; flex-direction: column; row-gap: 4px; flex-basis: fit-content;
                background-color: transparent; }

menu { background-color: var(--panel); outline-color: var(--outline); outline-width: 1px;
       border-radius: 6px; }

.inline-caption { flex: 0 0 fit-content; }
.field-controls > button { flex: 0 0 32px; align-self: stretch; padding: 0px; }
.curve-preview  { flex: 1 1 0px; height: 18px; }

.status { color: #9ecbff; font-size: 13px; }
.banner { color: #ef9a9a; font-size: 18px; text-align: center; }

scroll-indicator       { background-color: #55555f; border-radius: 2px; }
scroll-indicator-track { background-color: transparent; }

* { unicode-bidi: plaintext; }
)css");

static StringView getApiName(core::InstanceApi api) {
	switch (api) {
	case core::InstanceApi::None: return StringView("none");
	case core::InstanceApi::Vulkan: return StringView("vulkan");
	case core::InstanceApi::WebGPU: return StringView("webgpu");
	case core::InstanceApi::Metal: return StringView("metal");
	case core::InstanceApi::Software: return StringView("software");
	case core::InstanceApi::GLES: return StringView("gles");
	}
	return StringView("unknown");
}

static StringView getQueueName(uint32_t tag) {
	switch (basic2d::QueueType(tag)) {
	case basic2d::QueueType::Default: return StringView("default");
	case basic2d::QueueType::Flat: return StringView("flat");
	}
	return StringView("unknown");
}

StringView getParticleDemoStylesheet() { return s_css; }

bool ParticleDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.08f, 0.08f, 0.10f, 1.0f)), ZOrder(0));
	_background->setName("particles-background");

	// A child carries the flex row: a recursive StyleResolver never styles its own owner
	_root = addChild(Rc<Node>::create(), ZOrder(1));
	_root->setName("demo-root");

	buildScene();

	_panel = _root->addChild(
			Rc<ParticleParamsPanel>::create(static_cast<ParticleParamsDelegate *>(this)),
			ZOrder(2));

	scheduleUpdate();
	return true;
}

void ParticleDemoLayout::buildScene() {
	_sceneArea = _root->addChild(Rc<Node>::create(), ZOrder(1));
	_sceneArea->setName("scene-area");
	_sceneArea->setContentSizeDirtyCallback([this] { placeScene(); });

	// A drawn node under the emitters takes the clicks that edit emission points
	_sceneBackground = _sceneArea->addChild(
			Rc<basic2d::Layer>::create(Color4F(0.09f, 0.09f, 0.11f, 1.0f)), ZOrder(0));
	_sceneBackground->setName("scene-background");
	_sceneBackground->setAnchorPoint(Anchor::BottomLeft);
	auto listener = _sceneBackground->addSystem(Rc<InputListener>::create());
	listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			handleSceneTap(tap.pos, false);
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseLeft}), 1});
	listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			handleSceneTap(tap.pos, true);
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseRight}), 1});

	_statusLabel = _sceneArea->addChild(Rc<basic2d::Label>::create(), ZOrder(100));
	_statusLabel->setName("particles-status");
	_statusLabel->addStyleClass("status");
	_statusLabel->setAnchorPoint(Anchor::TopLeft);

	_banner = _sceneArea->addChild(Rc<basic2d::Label>::create(), ZOrder(101));
	_banner->setName("particles-banner");
	_banner->addStyleClass("banner");
	_banner->setAnchorPoint(Anchor::Middle);
	_banner->setVisible(false);
}

void ParticleDemoLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	_textures.emplace("circle", makeSoftCircleTexture(_director));
	_textures.emplace("square", makeSquareTexture(_director));
	_textures.emplace("spark", makeSparkTexture(_director));
	_textures.emplace("frames", makeFrameSheetTexture(_director));

	_clipboard = Rc<ClipboardSession>::create(_director->getApplication());

	updateAvailability();
	rebuildEmitters();
	refreshStatus();

	_inspectorScene = scene;
	registerCommands();

	checkRoundtrip();
	checkPanel();
}

void ParticleDemoLayout::handleExit() {
	if (!_inspectorCommands.empty()) {
		if (_inspectorScene) {
			if (auto i = inspector::get(_inspectorScene->getContent())) {
				for (auto &it : _inspectorCommands) { i->removeCommand(it); }
			}
		}
		_inspectorCommands.clear();
	}
	_inspectorScene = nullptr;

	basic2d::SceneLayout2d::handleExit();
}

void ParticleDemoLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	// Nothing above lays these two out; everything below #demo-root is a flex item
	for (auto node : {static_cast<Node *>(_background), _root}) {
		node->setAnchorPoint(Anchor::BottomLeft);
		node->setPosition(Vec2::ZERO);
		node->setContentSize(getContentSize());
	}
}

void ParticleDemoLayout::update(const UpdateTime &time) {
	basic2d::SceneLayout2d::update(time);
	++_updates;

	for (auto &it : _emitters) {
		if (it.drivePeriod > 0.0f) {
			it.driveTime += time.dt;
			updateNodePosition(it);
		}
	}

	// A change that did not come from the panel: a command, a restart, a preset
	if (auto slot = getSelected()) {
		if (slot->system->getId() != _panelSystemId
				|| slot->system->getParamsGeneration() != _panelGeneration) {
			refreshPanel();
			refreshStatus();
		}

		// Births and steps per second over half a second windows: a single frame simulates zero or
		// one step at most refresh rates
		_rateTime += time.dt;
		if (_rateTime >= 0.5f) {
			auto births = slot->node->getFeedbackTotalBirths();
			auto steps = slot->node->getFeedbackTotalSteps();
			if (slot->node->getEmitterId() == _rateEmitterId) {
				_birthsRate = float(births - _rateBirths) / _rateTime;
				_stepsRate = float(steps - _rateSteps) / _rateTime;
			} else {
				_birthsRate = _stepsRate = 0.0f;
			}
			_rateEmitterId = slot->node->getEmitterId();
			_rateBirths = births;
			_rateSteps = steps;
			_rateTime = 0.0f;
		}

		// The feedback changes every frame: a readable rate is enough
		_statusTime += time.dt;
		if (_statusTime >= 0.1f && slot->node->getFeedback().sequence != _statusFeedbackSequence) {
			refreshStatus();
		}
	}
}

// ---- the panel ----------------------------------------------------------------------------

void ParticleDemoLayout::handleParamsPatch(const Value &patch) {
	auto slot = getSelected();
	if (!slot) {
		return;
	}

	applyPatch(*slot, patch);

	// The panel already shows this state
	_panelSystemId = slot->system->getId();
	_panelGeneration = slot->system->getParamsGeneration();
	refreshStatus();
}

void ParticleDemoLayout::handlePresetSelected(StringView name) {
	if (auto slot = getSelected()) {
		applyPreset(*slot, name);
		refreshPanel();
		refreshStatus();
	}
}

void ParticleDemoLayout::handleEmitterSelected(uint32_t index) {
	if (index < _emitters.size()) {
		_selected = index;
		refreshPanel();
		updateMarkers();
		refreshStatus();
	}
}

void ParticleDemoLayout::handleRestart() {
	++_restarts;
	for (auto system : getSystems()) { system->restart(); }
}

void ParticleDemoLayout::handleReset() {
	rebuildEmitters();
	refreshStatus();
}

void ParticleDemoLayout::refreshPanel() {
	auto slot = getSelected();
	if (!slot || !_panel) {
		return;
	}

	_panel->setEmitters(uint32_t(_emitters.size()), _selected);
	_panel->setPreset(slot->preset);
	_panel->refresh(slot->system->encode(), slot->nodeValue, slot->editorValue);
	_panel->setSceneMode(_sceneMode);
	_panel->setDrive(slot->drivePeriod > 0.0f);
	_panelSystemId = slot->system->getId();
	_panelGeneration = slot->system->getParamsGeneration();
}

void ParticleDemoLayout::checkPanel() {
	auto slot = getSelected();
	if (!slot) {
		return;
	}

	auto system = slot->system->encode();
	Vector<StringView> failed;
	for (auto &info : getParticleParams()) {
		auto expected = particleParamToUi(info, system, slot->nodeValue, slot->editorValue);
		auto got = _panel->readUi(info);
		// A colour goes through bytes in ColorField
		const double tolerance = (info.kind == ParticleParamKind::Color) ? 0.5 / 255.0 : 1e-6;
		bool ok = expected.size() == got.size();
		for (size_t i = 0; ok && i < got.size(); ++i) {
			ok = sprt::fabs(expected[i] - got[i])
					<= tolerance * sprt::max(1.0, sprt::fabs(expected[i]));
		}
		if (!ok) {
			failed.emplace_back(info.key);
		}
	}

	if (failed.empty()) {
		log::source().info("ParticleExample", "panel ok");
	} else {
		StringStream out;
		for (auto &it : failed) { out << " " << it; }
		log::source().warn("ParticleExample", "panel FAILED:", out.str());
	}
}

// ---- emitters -----------------------------------------------------------------------------

Vector<ParticleDemoLayout::EmitterSlot *> ParticleDemoLayout::getTargets(const Value &args) {
	Vector<EmitterSlot *> ret;
	if (args.isBasicType("emitter")) {
		auto index = size_t(args.getInteger("emitter"));
		if (index < _emitters.size()) {
			ret.emplace_back(&_emitters[index]);
		}
	} else {
		for (auto &it : _emitters) { ret.emplace_back(&it); }
	}
	return ret;
}

ParticleDemoLayout::EmitterSlot *ParticleDemoLayout::getSelected() {
	return (_selected < _emitters.size()) ? &_emitters[_selected] : nullptr;
}

void ParticleDemoLayout::applyPreset(EmitterSlot &slot, StringView name) {
	auto preset = getParticlePreset(name);
	if (!preset.isDictionary()) {
		return;
	}

	slot.preset = name.str<Interface>();
	applyPatch(slot, preset);
}

void ParticleDemoLayout::applyPatch(EmitterSlot &slot, const Value &patch) {
	Value system = patch;
	applyNodeValue(slot, patch);

	if (patch.hasValue("colorStops")) {
		auto stops = decodeColorStops(patch.getValue("colorStops"));
		slot.editorValue.setValue(encodeColorStops(stops), "colorStops");
		system.setValue(makeColorCurveValue(stops), "colorCurve");
	} else if (patch.hasValue("colorCurve")) {
		// A sampled curve from outside: its stops are read back from the samples
		slot.editorValue.setValue(encodeColorStops(deriveColorStops(patch.getValue("colorCurve"))),
				"colorStops");
	}

	if (patch.isDictionary("animCurve")) {
		const auto &curve = patch.getValue("animCurve");
		Vector<float> params;
		for (auto &it : curve.getArray("params")) { params.emplace_back(float(it.getDouble())); }

		auto type = getParticleAnimCurveType(curve.getString("type"));
		Value editor;
		editor.setString(type ? type->name : StringView("none"), "type");
		Value values(Value::Type::ARRAY);
		for (auto v : params) { values.addDouble(v); }
		editor.setValue(sp::move(values), "params");
		slot.editorValue.setValue(sp::move(editor), "animCurve");

		system.setValue(makeAnimCurveValue(type ? type->name : StringView("none"), params),
				"animFrameCurve");
	} else if (patch.hasValue("animFrameCurve")) {
		Value editor;
		editor.setString(patch.isDictionary("animFrameCurve") ? "linear" : "none", "type");
		slot.editorValue.setValue(sp::move(editor), "animCurve");
	}

	slot.system->apply(system);

	if (&slot == getSelected()) {
		updateMarkers();
	}
}

void ParticleDemoLayout::applyNodeValue(EmitterSlot &slot, const Value &value) {
	if (value.isString("texture")) {
		auto it = _textures.find(value.getString("texture"));
		if (it != _textures.end() && it->second) {
			if (slot.node->getTexture() != it->second) {
				slot.node->setTexture(Rc<Texture>(it->second));
			}
			slot.nodeValue.setString(value.getString("texture"), "texture");
		}
	}

	if (value.isArray("frameGrid")) {
		const auto &grid = value.getValue("frameGrid");
		auto h = uint32_t(sprt::max(grid.getInteger(0, 1), int64_t(1)));
		auto v = uint32_t(sprt::max(grid.getInteger(1, 1), int64_t(1)));
		slot.node->setFrameGrid(h, v);

		Value g(Value::Type::ARRAY);
		g.addInteger(h);
		g.addInteger(v);
		slot.nodeValue.setValue(sp::move(g), "frameGrid");
	}
}

void ParticleDemoLayout::updateNodePosition(EmitterSlot &slot) {
	auto pos = slot.center;
	if (slot.drivePeriod > 0.0f) {
		auto a = float(M_PI * 2.0) * slot.driveTime / slot.drivePeriod;
		pos += Vec2(sprt::cos(a), sprt::sin(a)) * slot.driveRadius;
	}
	slot.node->setPosition(pos);
}

Vector<basic2d::ParticleSystem *> ParticleDemoLayout::getSystems() const {
	Vector<basic2d::ParticleSystem *> ret;
	for (auto &it : _emitters) {
		if (sprt::find(ret.begin(), ret.end(), it.system.get()) == ret.end()) {
			ret.emplace_back(it.system.get());
		}
	}
	return ret;
}

void ParticleDemoLayout::setEmitters(uint32_t count, bool shared) {
	count = sprt::clamp(count, uint32_t(1), s_maxEmitters);
	if (count != _emitterCount || shared != _sharedSystem) {
		_emitterCount = count;
		_sharedSystem = shared;
		rebuildEmitters();
		refreshStatus();
	}
}

void ParticleDemoLayout::rebuildEmitters() {
	for (auto &it : _emitters) {
		if (it.node) {
			it.node->removeFromParent();
		}
	}
	_emitters.clear();
	_selected = 0;

	if (!_available) {
		return;
	}

	for (uint32_t i = 0; i < _emitterCount; ++i) {
		EmitterSlot slot;
		slot.system = (_sharedSystem && i > 0) ? _emitters.front().system
											   : Rc<basic2d::ParticleSystem>::create();

		auto textureIt = _textures.find(StringView("circle"));
		auto texture = (textureIt != _textures.end()) ? textureIt->second : Rc<Texture>();
		Rc<basic2d::ParticleEmitter> emitter;
		if (texture) {
			emitter = Rc<basic2d::ParticleEmitter>::create(slot.system, Rc<Texture>(texture));
		} else {
			emitter = Rc<basic2d::ParticleEmitter>::create(slot.system);
		}
		emitter->setName(toString("particles-emitter-", i));
		emitter->setAnchorPoint(Anchor::Middle);
		emitter->setContentSize(Size2(24.0f, 24.0f));

		slot.node = _sceneArea->addChild(move(emitter), ZOrder(1 + int16_t(i)));
		// The status line and particles.stats show what the renderer reports back
		slot.node->setFeedbackEnabled(true);
		slot.nodeValue.setString("circle", "texture");
		slot.editorValue.setValue(Value(Value::Type::ARRAY), "colorStops");
		Value noCurve;
		noCurve.setString("none", "type");
		slot.editorValue.setValue(sp::move(noCurve), "animCurve");

		if (!_defaultSystems && !(_sharedSystem && i > 0)) {
			applyPreset(slot, s_initialPresets[i % 2]);
		}
		_emitters.emplace_back(sp::move(slot));
		buildMarkers(_emitters.size() - 1);
	}

	placeScene();
	refreshPanel();
	updateMarkers();
}

void ParticleDemoLayout::placeScene() {
	auto size = _sceneArea->getContentSize();
	_sceneBackground->setContentSize(size);
	_statusLabel->setPosition(Vec2(12.0f, size.height - 10.0f));
	_banner->setPosition(Vec2(size.width / 2.0f, size.height / 2.0f));
	_banner->setWidth(sprt::max(size.width - 48.0f, 1.0f));

	const auto n = float(_emitters.size());
	for (size_t i = 0; i < _emitters.size(); ++i) {
		auto &slot = _emitters[i];
		if (!slot.placed) {
			slot.center = Vec2(size.width * (float(i) + 1.0f) / (n + 1.0f), size.height * 0.4f);
		}
		updateNodePosition(slot);
	}
}

void ParticleDemoLayout::checkRoundtrip() {
	auto system = Rc<basic2d::ParticleSystem>::create();
	system->apply(getParticlePreset("fire"));
	system->setOrigin(Vec2(4.0f, -8.0f));
	system->setHue(-0.25f, 0.5f);
	system->setEmissionPoints(makeSpanView({Vec2(-10.0f, 0.0f), Vec2(10.0f, 5.0f)}));
	system->addFlags(basic2d::ParticleSystemFlags::LocalCoords
			| basic2d::ParticleSystemFlags::UseLifetimeMax);
	system->setSeed(42);
	system->setAnimFrameCurve(
			Rc<CurveBuffer>::create(uint32_t(4), interpolation::Type::Linear, SpanView<float>()));

	auto encoded = system->encode();
	auto decoded = Rc<basic2d::ParticleSystem>::create(encoded);
	auto reencoded = decoded->encode();

	// A partial value changes its own keys and nothing else
	Value partial;
	partial.setDouble(0.5, "explosiveness");
	decoded->apply(partial);

	auto expected = encoded;
	expected.setDouble(0.5, "explosiveness");
	auto applied = decoded->encode();

	if (encoded == reencoded && expected == applied) {
		log::source().info("ParticleExample", "roundtrip ok");
	} else {
		log::source().warn("ParticleExample", "roundtrip FAILED:\n", data::EncodeFormat::Pretty,
				encoded, "\n", reencoded, "\n", applied);
	}
}

void ParticleDemoLayout::updateAvailability() {
	auto api = core::InstanceApi::None;
	if (_director) {
		if (auto loop = _director->getGlLoop()) {
			api = loop->getInstance()->getApi();
		}
	}

	uint32_t queueTag = 0;
	if (_scene && _scene->getQueue()) {
		queueTag = _scene->getQueue()->getTypeTag();
	}

	_api = getApiName(api).str<Interface>();
	_queue = getQueueName(queueTag).str<Interface>();

	if (api != core::InstanceApi::Vulkan) {
		_available = false;
		_reason = toString("GPU particles need Vulkan, the scene runs on ", _api);
	} else if (basic2d::QueueType(queueTag) != basic2d::QueueType::Default) {
		_available = false;
		_reason = toString("GPU particles need the Default queue, the scene uses ", _queue);
	} else {
		_available = true;
		_reason.clear();
	}

	_banner->setVisible(!_available);
	_banner->setString(_reason);
}

void ParticleDemoLayout::refreshStatus() {
	StringStream out;
	out << _api << " / " << _queue;
	if (!_available) {
		out << " · particles unavailable";
	} else if (auto slot = getSelected()) {
		auto &system = slot->system;
		out << " · emitter " << (_selected + 1) << "/" << _emitters.size();
		if (!slot->preset.empty()) {
			out << " · " << slot->preset;
		}
		out << " · " << system->getCount() << " particles · "
			<< (hasFlag(system->getFlags(), basic2d::ParticleSystemFlags::LocalCoords) ? "local"
																					   : "scene")
			<< " coordinates";

		auto &feedback = slot->node->getFeedback();
		_statusFeedbackSequence = feedback.sequence;
		if (feedback.sequence > 0) {
			out << " · cycle " << feedback.cycle << " · frame " << feedback.cycleFrame << "/"
				<< feedback.framesInGen;
			if (feedback.counters) {
				out << " · fb ●" << feedback.alive << " +" << int64_t(sprt::round(_birthsRate))
					<< "/s ~" << int64_t(sprt::round(_stepsRate)) << "/s";
			} else {
				out << " · fb off";
			}
		}
	}
	_statusTime = 0.0f;
	if (!_notice.empty()) {
		out << " · " << _notice;
	}
	_statusLabel->setString(out.str());
}

Value ParticleDemoLayout::encodeStats() const {
	Value ret;
	ret.setString(_api, "api");
	ret.setString(_queue, "queue");
	ret.setBool(_available, "available");
	if (!_available) {
		ret.setString(_reason, "reason");
	}
	ret.setInteger(int64_t(_updates), "updates");
	ret.setInteger(int64_t(_restarts), "restarts");
	ret.setBool(_defaultSystems, "defaults");
	ret.setBool(_sharedSystem, "shared");
	ret.setInteger(int64_t(_selected), "selected");
	ret.setString(getParticleLocale(), "locale");
	ret.setString(_sceneMode, "mode");
	if (!_notice.empty()) {
		ret.setString(_notice, "notice");
	}

	Value emitters(Value::Type::ARRAY);
	for (auto &it : _emitters) {
		Value e;
		e.setInteger(int64_t(it.node->getEmitterId()), "emitterId");
		e.setInteger(int64_t(it.system->getId()), "systemId");
		e.setString(it.node->getName(), "name");
		e.setString(it.preset, "preset");
		e.setValue(it.nodeValue, "node");
		e.setValue(it.editorValue, "editor");
		e.setInteger(int64_t(it.system->getParamsGeneration()), "paramsGeneration");
		e.setInteger(int64_t(it.system->getRestartGeneration()), "restartGeneration");
		e.setValue(it.system->encode(), "system");

		auto pos = it.node->getPosition();
		Value p(Value::Type::ARRAY);
		p.addDouble(pos.x);
		p.addDouble(pos.y);
		e.setValue(sp::move(p), "position");
		e.setValue(encodeFeedback(it), "feedback");

		emitters.addValue(sp::move(e));
	}
	ret.setValue(sp::move(emitters), "emitters");
	return ret;
}

Value ParticleDemoLayout::encodeFeedback(const EmitterSlot &slot) const {
	auto ret = encodeFeedback(slot.node->getFeedback());
	if (slot.node->getFeedback().counters) {
		ret.setInteger(int64_t(slot.node->getFeedbackTotalBirths()), "totalBirths");
		ret.setInteger(int64_t(slot.node->getFeedbackTotalSteps()), "totalSteps");
	}
	return ret;
}

Value ParticleDemoLayout::encodeFeedback(const basic2d::ParticleFeedback &feedback) const {
	Value ret;
	ret.setInteger(int64_t(feedback.sequence), "sequence");
	ret.setInteger(int64_t(feedback.restartGeneration), "restartGeneration");
	ret.setInteger(int64_t(feedback.cycle), "cycle");
	ret.setInteger(int64_t(feedback.cycleFrame), "cycleFrame");
	ret.setInteger(int64_t(feedback.framesInGen), "framesInGen");
	ret.setInteger(int64_t(feedback.nframes), "nframes");
	ret.setBool(feedback.counters, "counters");
	if (feedback.counters) {
		ret.setInteger(int64_t(feedback.births), "births");
		ret.setInteger(int64_t(feedback.steps), "steps");
		ret.setInteger(int64_t(feedback.alive), "alive");
	}
	return ret;
}

static Value encodeParticle(uint32_t index, const basic2d::ParticleData &p) {
	auto vec2 = [](const auto &v) {
		Value ret(Value::Type::ARRAY);
		ret.addDouble(v.x);
		ret.addDouble(v.y);
		return ret;
	};

	Value ret;
	ret.setInteger(int64_t(index), "index");
	ret.setValue(vec2(p.position), "position");
	ret.setValue(vec2(p.velocity), "velocity");
	ret.setValue(vec2(p.origin), "origin");
	Value color(Value::Type::ARRAY);
	color.addDouble(p.color.x);
	color.addDouble(p.color.y);
	color.addDouble(p.color.z);
	color.addDouble(p.color.w);
	ret.setValue(sp::move(color), "color");
	ret.setDouble(p.angle, "angle");
	ret.setDouble(p.scale, "scale");
	ret.setDouble(p.hue, "hue");
	ret.setInteger(int64_t(p.currentLifetime), "currentLifetime");
	ret.setInteger(int64_t(p.fullLifetime), "fullLifetime");
	return ret;
}

// ---- the scene tools ----------------------------------------------------------------------

void ParticleDemoLayout::handleSceneMode(StringView mode) {
	_sceneMode = (mode == "points") ? String("points") : String("move");
	refreshStatus();
}

void ParticleDemoLayout::handleDrive(bool value) {
	if (auto slot = getSelected()) {
		slot->driveRadius = 150.0f;
		slot->drivePeriod = value ? 4.0f : 0.0f;
		slot->driveTime = 0.0f;
		updateNodePosition(*slot);
	}
}

void ParticleDemoLayout::handleClearPoints() {
	if (auto slot = getSelected()) {
		Value patch;
		patch.setValue(Value(Value::Type::ARRAY), "emissionPoints");
		applyPatch(*slot, patch);
	}
}

void ParticleDemoLayout::handleSceneTap(Vec2 world, bool remove) {
	auto slot = getSelected();
	if (!slot || _sceneMode != "points") {
		return;
	}

	auto point = slot->node->convertToNodeSpace(world);
	Vector<Vec2> points = slot->system->getEmissionPoints().vec<Interface>();
	if (remove) {
		// The nearest point within reach of the click
		size_t nearest = maxOf<size_t>();
		float distance = 16.0f;
		for (size_t i = 0; i < points.size(); ++i) {
			auto d = points[i].distance(point);
			if (d <= distance) {
				distance = d;
				nearest = i;
			}
		}
		if (nearest == maxOf<size_t>()) {
			return;
		}
		points.erase(points.begin() + nearest);
	} else {
		points.emplace_back(point);
	}

	Value list(Value::Type::ARRAY);
	for (auto &it : points) {
		Value p(Value::Type::ARRAY);
		p.addDouble(it.x);
		p.addDouble(it.y);
		list.addValue(sp::move(p));
	}

	Value patch;
	patch.setValue(sp::move(list), "emissionPoints");
	applyPatch(*slot, patch);
	refreshStatus();
}

void ParticleDemoLayout::buildMarkers(size_t index) {
	auto &slot = _emitters[index];
	slot.markers = slot.node->addChild(Rc<Node>::create(), ZOrder(10));
	slot.markers->setName("markers");

	// The node's own box: tap to select, drag to move
	auto handle = slot.markers->addChild(
			Rc<basic2d::Layer>::create(Color4F(1.0f, 1.0f, 1.0f, 0.18f)), ZOrder(1));
	handle->setName("emitter-handle");
	handle->setAnchorPoint(Anchor::BottomLeft);
	handle->setContentSize(slot.node->getContentSize());

	auto handleListener = handle->addSystem(Rc<InputListener>::create());
	handleListener->addTapRecognizer([this, index](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated && index < _emitters.size()) {
			handleEmitterSelected(uint32_t(index));
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseLeft}), 1});
	handleListener->addSwipeRecognizer([this, index](const GestureSwipe &swipe) {
		if (index >= _emitters.size()) {
			return false;
		}
		auto &s = _emitters[index];
		auto location = _sceneArea->convertToNodeSpace(swipe.location());
		if (swipe.event == GestureEvent::Began) {
			if (_selected != index) {
				handleEmitterSelected(uint32_t(index));
			}
			// From where the press started: at Began the pointer is already a threshold away
			s.dragOffset = s.center - _sceneArea->convertToNodeSpace(swipe.input->originalLocation);
		} else if (swipe.event == GestureEvent::Activated) {
			s.center = location + s.dragOffset;
			s.placed = true;
			updateNodePosition(s);
		}
		return true;
	}, InputSwipeInfo{makeButtonMask({InputMouseButton::MouseLeft}), 0.0f, true});

	// Origin of the orbital, radial and tangential motion: drag to move
	slot.originMarker = slot.markers->addChild(
			Rc<basic2d::Layer>::create(Color4F(0.3f, 0.9f, 1.0f, 0.9f)), ZOrder(3));
	slot.originMarker->setName("origin-marker");
	slot.originMarker->setAnchorPoint(Anchor::Middle);
	slot.originMarker->setContentSize(Size2(10.0f, 10.0f));
	slot.originMarker->setRotation(float(M_PI_4));

	auto originListener = slot.originMarker->addSystem(Rc<InputListener>::create());
	originListener->setTouchPadding(6.0f);
	originListener->addSwipeRecognizer([this, index](const GestureSwipe &swipe) {
		if (index >= _emitters.size()) {
			return false;
		}
		auto &s = _emitters[index];
		auto location = s.node->convertToNodeSpace(swipe.location());
		if (swipe.event == GestureEvent::Began) {
			s.dragOffset = s.system->getOrigin()
					- s.node->convertToNodeSpace(swipe.input->originalLocation);
		} else if (swipe.event == GestureEvent::Activated) {
			auto origin = location + s.dragOffset;
			Value o(Value::Type::ARRAY);
			o.addDouble(origin.x);
			o.addDouble(origin.y);
			Value patch;
			patch.setValue(sp::move(o), "origin");
			applyPatch(s, patch);
		}
		return true;
	}, InputSwipeInfo{makeButtonMask({InputMouseButton::MouseLeft}), 0.0f, true});
}

void ParticleDemoLayout::updateMarkers() {
	for (size_t i = 0; i < _emitters.size(); ++i) {
		auto &slot = _emitters[i];
		if (!slot.markers) {
			continue;
		}

		slot.markers->setVisible(i == _selected);
		slot.originMarker->setPosition(slot.system->getOrigin());

		for (auto it : slot.pointMarkers) { it->removeFromParent(); }
		slot.pointMarkers.clear();

		int32_t z = 10;
		for (auto &point : slot.system->getEmissionPoints()) {
			auto marker = slot.markers->addChild(
					Rc<basic2d::Layer>::create(Color4F(1.0f, 0.85f, 0.2f, 0.9f)), ZOrder(z++));
			marker->setAnchorPoint(Anchor::Middle);
			marker->setContentSize(Size2(6.0f, 6.0f));
			marker->setPosition(point);
			slot.pointMarkers.emplace_back(marker);
		}
	}
}

// ---- JSON ---------------------------------------------------------------------------------

Value ParticleDemoLayout::encodeJson(const EmitterSlot &slot) const {
	Value ret;
	ret.setInteger(1, "version");
	ret.setString(slot.preset, "preset");
	ret.setValue(slot.system->encode(), "system");
	ret.setValue(slot.nodeValue, "node");
	ret.setValue(slot.editorValue, "editor");
	return ret;
}

void ParticleDemoLayout::applyJson(EmitterSlot &slot, const Value &value) {
	const auto &system = value.getValue("system");
	Value patch = system.isDictionary() ? system : Value(Value::Type::DICTIONARY);
	for (auto key : {StringView("node"), StringView("editor")}) {
		const auto &part = value.getValue(key);
		if (part.isDictionary()) {
			for (auto &it : part.getDict()) { patch.setValue(it.second, it.first); }
		}
	}

	slot.preset = value.getString("preset");
	applyPatch(slot, patch);
	slot.system->restart();
	refreshPanel();
	refreshStatus();
}

bool ParticleDemoLayout::saveJson(StringView path) {
	auto slot = getSelected();
	if (!slot) {
		return false;
	}

	auto text = data::toString(encodeJson(*slot), true);
	auto ok =
			filesystem::write(FileInfo{path}, BytesView((const uint8_t *)text.data(), text.size()));
	setNotice(ok ? toString("saved ", path) : toString("failed to save ", path));
	return ok;
}

bool ParticleDemoLayout::loadJson(StringView path) {
	auto slot = getSelected();
	if (!slot) {
		return false;
	}

	auto text = filesystem::readTextFile<Interface>(FileInfo{path});
	auto value = data::read<Interface>(text);
	if (!value.isDictionary() || !value.isDictionary("system")) {
		setNotice(toString("not a particle file: ", path));
		return false;
	}

	applyJson(*slot, value);
	setNotice(toString("loaded ", path));
	return true;
}

void ParticleDemoLayout::setNotice(StringView notice) {
	_notice = notice.str<Interface>();
	refreshStatus();
}

void ParticleDemoLayout::handleCopy() {
	if (auto slot = getSelected()) {
		_clipboard->writeText(data::toString(encodeJson(*slot), true));
		setNotice("copied");
	}
}

void ParticleDemoLayout::handlePaste() {
	_clipboard->readText([this](const ClipboardSession::Result &result) {
		auto slot = getSelected();
		auto value = result ? data::read<Interface>(result.text()) : Value();
		if (!slot || !value.isDictionary("system")) {
			setNotice("nothing to paste");
			return;
		}
		applyJson(*slot, value);
		setNotice("pasted");
	}, this);
}

static AppWindow *getParticleAppWindow(Director *director) {
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

void ParticleDemoLayout::handleSave() {
	auto window = getParticleAppWindow(_director);
	if (!window || !window->isDialogSupported(sprt::window::DialogType::SaveFile)) {
		setNotice("file dialogs are unavailable");
		return;
	}

	auto req = Rc<sprt::window::DialogRequest>::create();
	req->type = sprt::window::DialogType::SaveFile;
	req->flags = sprt::window::DialogFlags::ConfirmOverwrite;
	req->filename = "particles.json";
	req->filters.emplace_back(sprt::window::FileFilter{"JSON", {"*.json"}, {"application/json"}});
	req->target = this;
	req->callback = [this](const sprt::window::DialogResult &result) {
		if (result.status == Status::Ok && !result.paths.empty()) {
			saveJson(result.paths.front());
		}
		_dialog = nullptr;
	};
	_dialog = req;
	window->openDialog(req);
}

void ParticleDemoLayout::handleOpen() {
	auto window = getParticleAppWindow(_director);
	if (!window || !window->isDialogSupported(sprt::window::DialogType::OpenFile)) {
		setNotice("file dialogs are unavailable");
		return;
	}

	auto req = Rc<sprt::window::DialogRequest>::create();
	req->type = sprt::window::DialogType::OpenFile;
	req->filters.emplace_back(sprt::window::FileFilter{"JSON", {"*.json"}, {"application/json"}});
	req->target = this;
	req->callback = [this](const sprt::window::DialogResult &result) {
		if (result.status == Status::Ok && !result.paths.empty()) {
			loadJson(result.paths.front());
		}
		_dialog = nullptr;
	};
	_dialog = req;
	window->openDialog(req);
}

void ParticleDemoLayout::registerCommands() {
	addCommand("restart", "Restart every particle system", [this](const Value &) {
		++_restarts;
		for (auto system : getSystems()) { system->restart(); }
		return encodeStats();
	});

	addCommand("reset",
			"Recreate the emitters and their systems; {defaults: true} keeps the "
			"ParticleSystem::init defaults",
			[this](const Value &args) {
		_defaultSystems = args.getBool("defaults");
		rebuildEmitters();
		refreshStatus();
		return encodeStats();
	});

	addCommand("stats", "Backend, queue, availability and the parameters of every emitter",
			[this](const Value &) { return encodeStats(); });

	addCommand("emitters", "Set the number of emitters: {n: 1|2, shared: bool}",
			[this](const Value &args) {
		setEmitters(uint32_t(sprt::max(args.getInteger("n", 1), int64_t(1))),
				args.getBool("shared"));
		return encodeStats();
	});

	addCommand("set",
			"Apply parameters in the ParticleSystem::encode format (plus the node's texture and "
			"frameGrid) to every emitter, or to {emitter: index}",
			[this](const Value &args) {
		for (auto slot : getTargets(args)) { applyPatch(*slot, args); }
		refreshStatus();
		return encodeStats();
	});

	addCommand("preset", "Apply a preset by {name} to every emitter or to {emitter: index}",
			[this](const Value &args) {
		const auto &name = args.getString("name");
		if (!getParticlePreset(name).isDictionary()) {
			Value error;
			error.setString("unknown preset", "error");
			return error;
		}

		for (auto slot : getTargets(args)) { applyPreset(*slot, name); }
		refreshPanel();
		refreshStatus();
		return encodeStats();
	});

	addCommand("panel.state",
			"What the panel's widgets hold, in UI units, beside the selected system's state",
			[this](const Value &) {
		Value ret;
		if (auto slot = getSelected()) {
			ret.setValue(_panel->encodeUi(), "ui");

			Value expected;
			auto system = slot->system->encode();
			for (auto &info : getParticleParams()) {
				Value values(Value::Type::ARRAY);
				for (auto v : particleParamToUi(info, system, slot->nodeValue, slot->editorValue)) {
					values.addDouble(v);
				}
				expected.setValue(sp::move(values), info.key);
			}
			ret.setValue(sp::move(expected), "system");
		}
		return ret;
	});

	addCommand("panel.set",
			"Put UI values into a parameter's widgets as a user edit would: {key, value: [...]}",
			[this](const Value &args) {
		Vector<double> values;
		const auto &v = args.getValue("value");
		if (v.isArray()) {
			for (auto &it : v.getArray()) { values.emplace_back(it.getDouble()); }
		} else {
			values.emplace_back(v.getDouble());
		}

		Value ret;
		ret.setBool(_panel->setWidgetValue(args.getString("key"), values), "ok");
		return ret;
	});

	addCommand("panel.section", "Open or close a panel section: {id, open: bool}",
			[this](const Value &args) {
		Value ret;
		ret.setBool(_panel->setSectionOpen(args.getString("id"), args.getBool("open")), "ok");
		return ret;
	});

	addCommand("points",
			"Edit emission points of the selected emitter: {add: [x, y]} | {remove: index} | "
			"{clear: true}",
			[this](const Value &args) {
		auto slot = getSelected();
		if (!slot) {
			return Value();
		}
		Vector<Vec2> points = slot->system->getEmissionPoints().vec<Interface>();
		if (args.getBool("clear")) {
			points.clear();
		} else if (args.isArray("add")) {
			const auto &p = args.getValue("add");
			points.emplace_back(Vec2(float(p.getDouble(0)), float(p.getDouble(1))));
		} else if (args.isInteger("remove")) {
			auto index = size_t(args.getInteger("remove"));
			if (index < points.size()) {
				points.erase(points.begin() + index);
			}
		}
		Value list(Value::Type::ARRAY);
		for (auto &it : points) {
			Value p(Value::Type::ARRAY);
			p.addDouble(it.x);
			p.addDouble(it.y);
			list.addValue(sp::move(p));
		}
		Value patch;
		patch.setValue(sp::move(list), "emissionPoints");
		applyPatch(*slot, patch);
		return encodeStats();
	});

	addCommand("mode", "Scene mode for the mouse: {name: move|points}", [this](const Value &args) {
		handleSceneMode(args.getString("name"));
		_panel->setSceneMode(_sceneMode);
		return encodeStats();
	});

	addCommand("select", "Select an emitter: {emitter: index}", [this](const Value &args) {
		handleEmitterSelected(uint32_t(args.getInteger("emitter")));
		return encodeStats();
	});

	addCommand("json.get", "The selected emitter as a particle file", [this](const Value &) {
		auto slot = getSelected();
		return slot ? encodeJson(*slot) : Value();
	});

	addCommand("json.set", "Load a particle file into the selected emitter: {value}",
			[this](const Value &args) {
		if (auto slot = getSelected()) {
			applyJson(*slot, args.getValue("value"));
		}
		return encodeStats();
	});

	addCommand("json.save", "Save the selected emitter to {path}", [this](const Value &args) {
		Value ret;
		ret.setBool(saveJson(args.getString("path")), "ok");
		return ret;
	});

	addCommand("json.load", "Load the selected emitter from {path}", [this](const Value &args) {
		Value ret;
		ret.setBool(loadJson(args.getString("path")), "ok");
		return ret;
	});

	addCommand("copy", "Copy the selected emitter to the clipboard", [this](const Value &) {
		handleCopy();
		return encodeStats();
	});

	addAsyncCommand("paste", "Paste a particle file from the clipboard into the selected emitter",
			[this](const Value &, Function<void(Value &&)> &&done) {
		_clipboard->readText([this, done = sp::move(done)](const ClipboardSession::Result &result) {
			auto slot = getSelected();
			auto value = result ? data::read<Interface>(result.text()) : Value();
			Value ret;
			if (slot && value.isDictionary("system")) {
				applyJson(*slot, value);
				ret.setBool(true, "ok");
			} else {
				ret.setBool(false, "ok");
			}
			done(sp::move(ret));
		}, this);
	});

	addAsyncCommand("snapshot",
			"The first {n} particles of an emitter (the selected one, or {emitter}) after the next "
			"frame",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto index = args.isBasicType("emitter") ? size_t(args.getInteger("emitter")) : _selected;
		if (!_available || index >= _emitters.size()) {
			Value ret;
			ret.setBool(false, "ok");
			ret.setString(_available ? "no such emitter" : _reason, "reason");
			done(sp::move(ret));
			return;
		}

		auto &slot = _emitters[index];
		auto n = uint32_t(sprt::clamp(args.getInteger("n", 16), int64_t(1),
				int64_t(slot.system->getCount())));
		slot.node->requestSnapshot(n,
				[this, done = sp::move(done)](basic2d::ParticleSnapshot &&snapshot) {
			Value ret;
			ret.setBool(snapshot.success, "ok");
			if (snapshot.success) {
				ret.setValue(encodeFeedback(snapshot.feedback), "feedback");
				Value particles(Value::Type::ARRAY);
				for (uint32_t i = 0; i < snapshot.particles.size(); ++i) {
					particles.addValue(encodeParticle(i, snapshot.particles[i]));
				}
				ret.setValue(sp::move(particles), "particles");
			} else {
				ret.setString("the emitter left the scene before a frame", "reason");
			}
			done(sp::move(ret));
		});
	});

	addCommand("locale", "Switch the language: {name: en-us|ru-ru}", [this](const Value &args) {
		Value ret;
		ret.setBool(setParticleLocale(args.getString("name")), "ok");
		ret.setString(getParticleLocale(), "locale");
		ret.setString(_sceneMode, "mode");
		if (!_notice.empty()) {
			ret.setString(_notice, "notice");
		}
		return ret;
	});

	addCommand("move", "Put the emitter node at {x, y} in layout coordinates: {x, y, emitter?}",
			[this](const Value &args) {
		for (auto slot : getTargets(args)) {
			slot->center = Vec2(float(args.getDouble("x")), float(args.getDouble("y")));
			slot->placed = true;
			updateNodePosition(*slot);
		}
		return encodeStats();
	});

	addCommand("drive",
			"Move the emitter node around its position: {radius, period, emitter?}; period 0 stops",
			[this](const Value &args) {
		for (auto slot : getTargets(args)) {
			slot->driveRadius = float(args.getDouble("radius", 150.0));
			slot->drivePeriod = float(sprt::max(args.getDouble("period"), 0.0));
			slot->driveTime = 0.0f;
			updateNodePosition(*slot);
		}
		return encodeStats();
	});

	addCommand("seed", "Set a fixed seed for every system: {n}; {n: null} clears it",
			[this](const Value &args) {
		for (auto system : getSystems()) {
			if (args.isBasicType("n")) {
				system->setSeed(uint32_t(args.getInteger("n")));
			} else {
				system->clearSeed();
			}
		}
		return encodeStats();
	});
}

void ParticleDemoLayout::addCommand(StringView name, StringView description,
		Function<Value(const Value &)> &&handler) {
	if (!_inspectorScene) {
		return;
	}

	auto full = toString("particles.", name);
	if (inspector::addCommand(_inspectorScene->getContent(), full, description,
				[handler = sp::move(handler)](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		done(handler(in));
	})) {
		_inspectorCommands.emplace_back(sp::move(full));
	}
}

void ParticleDemoLayout::addAsyncCommand(StringView name, StringView description,
		Function<void(const Value &, Function<void(Value &&)> &&)> &&handler) {
	if (!_inspectorScene) {
		return;
	}

	auto full = toString("particles.", name);
	if (inspector::addCommand(_inspectorScene->getContent(), full, description,
				[handler = sp::move(handler)](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		handler(in, sp::move(done));
	})) {
		_inspectorCommands.emplace_back(sp::move(full));
	}
}

} // namespace stappler::xenolith::examples
