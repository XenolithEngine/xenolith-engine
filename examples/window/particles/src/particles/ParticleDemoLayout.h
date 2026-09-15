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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEDEMOLAYOUT_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEDEMOLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dParticleEmitter.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** A scene with GPU particle emitters, driven by `particles.*` inspector commands.

Emitters own their systems, or share one (`particles.emitters {n: 2, shared: true}`). The status line names the backend and the queue, and a
banner replaces the scene when the particle pass cannot run there (not Vulkan, or not the Default
queue). A device without shaderStorageBufferArrayDynamicIndexing is reported only by the
vk::ParticlePass log line. */
class ParticleDemoLayout : public basic2d::SceneLayout2d {
public:
	virtual ~ParticleDemoLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	virtual void update(const UpdateTime &) override;

protected:
	struct EmitterSlot {
		Rc<basic2d::ParticleSystem> system;
		basic2d::ParticleEmitter *node = nullptr;
	};

	Rc<basic2d::ParticleSystem> makeBaselineSystem() const;

	// Every distinct system, once
	Vector<basic2d::ParticleSystem *> getSystems() const;

	void setEmitters(uint32_t count, bool shared);
	void rebuildEmitters();

	// encode -> init(Value) -> encode must be identical
	void checkRoundtrip();
	void placeEmitters();

	void updateAvailability();
	void refreshStatus();

	Value encodeStats() const;

	void registerCommands();
	void addCommand(StringView name, StringView description,
			Function<Value(const Value &)> &&handler);

	basic2d::Layer *_background = nullptr;
	basic2d::Label *_statusLabel = nullptr;
	basic2d::Label *_banner = nullptr;

	Rc<Texture> _texture;
	Vector<EmitterSlot> _emitters;
	uint32_t _emitterCount = 1;
	uint64_t _restarts = 0;
	bool _defaultSystems = false;
	bool _sharedSystem = false;
	uint64_t _updates = 0;

	bool _available = false;
	String _api;
	String _queue;
	String _reason;

	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;
};

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEDEMOLAYOUT_H_ */
