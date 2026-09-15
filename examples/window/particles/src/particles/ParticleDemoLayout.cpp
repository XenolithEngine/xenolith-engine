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
#include "XLScene.h"
#include "XL2dSceneContent.h"
#include "XLSceneInspector.h"
#include "XLDirector.h"
#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "XLCoreQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

static constexpr uint32_t s_maxEmitters = 2;

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

bool ParticleDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.08f, 0.08f, 0.10f, 1.0f)), ZOrder(0));
	_background->setName("particles-background");

	_statusLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(10));
	_statusLabel->setName("particles-status");
	_statusLabel->setAnchorPoint(Anchor::TopLeft);
	_statusLabel->setFontSize(14);
	_statusLabel->setColor(Color::White);

	_banner = addChild(Rc<basic2d::Label>::create(), ZOrder(11));
	_banner->setName("particles-banner");
	_banner->setAnchorPoint(Anchor::Middle);
	_banner->setAlignment(font::TextAlign::Center);
	_banner->setFontSize(20);
	_banner->setColor(Color::Red_200);
	_banner->setVisible(false);

	scheduleUpdate();
	return true;
}

void ParticleDemoLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	_texture = makeSoftCircleTexture(_director);

	updateAvailability();
	rebuildEmitters();
	refreshStatus();

	_inspectorScene = scene;
	registerCommands();

	checkRoundtrip();
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

	auto size = getContentSize();
	_background->setContentSize(size);
	_statusLabel->setPosition(Vec2(12.0f, size.height - 10.0f));
	_banner->setPosition(Vec2(size.width / 2.0f, size.height / 2.0f));
	_banner->setWidth(sprt::max(size.width - 48.0f, 1.0f));

	placeEmitters();
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
}

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

void ParticleDemoLayout::updateNodePosition(EmitterSlot &slot) {
	auto pos = slot.center;
	if (slot.drivePeriod > 0.0f) {
		auto a = float(M_PI * 2.0) * slot.driveTime / slot.drivePeriod;
		pos += Vec2(sprt::cos(a), sprt::sin(a)) * slot.driveRadius;
	}
	slot.node->setPosition(pos);
}

Rc<basic2d::ParticleSystem> ParticleDemoLayout::makeBaselineSystem(uint32_t index) const {
	auto system = Rc<basic2d::ParticleSystem>::create();
	if (_defaultSystems) {
		return system;
	}

	if (index > 0) {
		system->setCount(32);
		system->setLifetime(1.0f);
		system->setExplosiveness(1.0f);
		system->setRandomness(1.0f);
		system->setParticleSize(Size2(16.0f, 16.0f));
		system->setVelocity(60.0f, 100.0f);
		system->setNormal(0.0f, float(M_PI * 2.0));
		system->setEmissionPoints(
				makeSpanView({Vec2(-60.0f, -40.0f), Vec2(60.0f, -40.0f), Vec2(0.0f, 60.0f)}));
		return system;
	}

	system->setCount(64);
	system->setLifetime(1.5f);
	system->setParticleSize(Size2(24.0f, 24.0f));
	system->setVelocity(120.0f, 40.0f);
	system->setNormal(float(M_PI_4), float(M_PI_2));
	system->setLinearAcceleration(Vec2(0.0f, -60.0f));
	return system;
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

	if (!_available) {
		return;
	}

	for (uint32_t i = 0; i < _emitterCount; ++i) {
		EmitterSlot slot;
		slot.system = (_sharedSystem && i > 0) ? _emitters.front().system : makeBaselineSystem(i);

		Rc<basic2d::ParticleEmitter> emitter;
		if (_texture) {
			emitter = Rc<basic2d::ParticleEmitter>::create(slot.system, Rc<Texture>(_texture));
		} else {
			emitter = Rc<basic2d::ParticleEmitter>::create(slot.system);
		}
		emitter->setName(toString("particles-emitter-", i));
		emitter->setAnchorPoint(Anchor::Middle);
		emitter->setContentSize(Size2(24.0f, 24.0f));

		slot.node = addChild(move(emitter), ZOrder(1 + int16_t(i)));
		_emitters.emplace_back(sp::move(slot));
	}

	placeEmitters();
}

void ParticleDemoLayout::checkRoundtrip() {
	auto system = makeBaselineSystem();
	system->setColor(Color4F(1.0f, 0.5f, 0.25f, 0.75f));
	system->setOrigin(Vec2(4.0f, -8.0f));
	system->setHue(-0.25f, 0.5f);
	system->setEmissionPoints(makeSpanView({Vec2(-10.0f, 0.0f), Vec2(10.0f, 5.0f)}));
	system->addFlags(basic2d::ParticleSystemFlags::LocalCoords
			| basic2d::ParticleSystemFlags::UseLifetimeMax);
	system->setSeed(42);
	auto fade = [](float t) { return Vec4(1.0f, 1.0f - t, 0.0f, 1.0f - t); };
	system->setColorCurve(Rc<CurveBuffer>::create(uint32_t(4), Callback<Vec4(float)>(fade)));
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

void ParticleDemoLayout::placeEmitters() {
	auto size = getContentSize();
	const auto n = float(_emitters.size());
	for (size_t i = 0; i < _emitters.size(); ++i) {
		auto &slot = _emitters[i];
		if (!slot.placed) {
			slot.center = Vec2(size.width * (float(i) + 1.0f) / (n + 1.0f), size.height / 2.0f);
		}
		updateNodePosition(slot);
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
	if (_available && !_emitters.empty()) {
		auto &system = _emitters.front().system;
		out << " · emitters " << _emitters.size() << " · count " << system->getCount()
			<< " · lifetime " << system->getLifetimeMin() << "s · step "
			<< system->getFrameInterval() << "us";
	} else if (!_available) {
		out << " · particles unavailable";
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

	Value emitters(Value::Type::ARRAY);
	for (auto &it : _emitters) {
		Value e;
		e.setInteger(int64_t(it.node->getEmitterId()), "emitterId");
		e.setInteger(int64_t(it.system->getId()), "systemId");
		e.setString(it.node->getName(), "name");
		e.setInteger(int64_t(it.system->getParamsGeneration()), "paramsGeneration");
		e.setInteger(int64_t(it.system->getRestartGeneration()), "restartGeneration");
		e.setValue(it.system->encode(), "system");

		auto pos = it.node->getPosition();
		Value p(Value::Type::ARRAY);
		p.addDouble(pos.x);
		p.addDouble(pos.y);
		e.setValue(sp::move(p), "position");

		emitters.addValue(sp::move(e));
	}
	ret.setValue(sp::move(emitters), "emitters");
	return ret;
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
			"Apply parameters in the ParticleSystem::encode format to every system, or to the "
			"system of {emitter: index}",
			[this](const Value &args) {
		if (args.isBasicType("emitter")) {
			auto index = size_t(args.getInteger("emitter"));
			if (index < _emitters.size()) {
				_emitters[index].system->apply(args);
			}
		} else {
			for (auto system : getSystems()) { system->apply(args); }
		}
		refreshStatus();
		return encodeStats();
	});

	addCommand("preset",
			"Apply a preset (fountain, snow, vortex) to every system or to {emitter: index}",
			[this](const Value &args) {
		auto preset = getParticlePreset(args.getString("name"));
		if (!preset.isDictionary()) {
			Value error;
			error.setString("unknown preset", "error");
			return error;
		}

		Vector<basic2d::ParticleSystem *> systems;
		for (auto slot : getTargets(args)) {
			if (sprt::find(systems.begin(), systems.end(), slot->system.get()) == systems.end()) {
				systems.emplace_back(slot->system.get());
			}
		}
		for (auto system : systems) { system->apply(preset); }
		refreshStatus();
		return encodeStats();
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

} // namespace stappler::xenolith::examples
