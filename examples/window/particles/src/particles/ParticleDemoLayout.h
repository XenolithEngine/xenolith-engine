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

#include "particles/ParticleParamsPanel.h"
#include "XL2dSceneLayout.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dParticleEmitter.h"
#include "XLClipboard.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// The demo's stylesheet; it goes on the scene content, so the in-scene popups are styled too
StringView getParticleDemoStylesheet();

/** GPU particle emitters on the left, every parameter of the selected one on the right.

The panel and the `particles.*` inspector commands edit the same systems: a command changes the
system, and the panel picks the change up on the next update from the system's generation. The
status line names the backend and the queue; a banner replaces the scene when the particle pass
cannot run there. */
class ParticleDemoLayout : public basic2d::SceneLayout2d, protected ParticleParamsDelegate {
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

		String preset;
		Value nodeValue; // {texture, frameGrid}
		Value editorValue; // {colorStops, animCurve}

		// Handles over the node, shown for the selected emitter
		Node *markers = nullptr;
		basic2d::Layer *originMarker = nullptr;
		Vector<Node *> pointMarkers;
		Vec2 dragOffset;

		// Where the node stands, or the center it circles around while driven
		Vec2 center;
		bool placed = false; // moved by a command: layout changes leave it alone

		float driveRadius = 0.0f;
		float drivePeriod = 0.0f; // seconds per circle, 0 - not driven
		float driveTime = 0.0f;
	};

	virtual void handleParamsPatch(const Value &) override;
	virtual void handlePresetSelected(StringView) override;
	virtual void handleEmitterSelected(uint32_t) override;
	virtual void handleRestart() override;
	virtual void handleReset() override;
	virtual void handleSceneMode(StringView) override;
	virtual void handleDrive(bool) override;
	virtual void handleClearPoints() override;
	virtual void handleCopy() override;
	virtual void handlePaste() override;
	virtual void handleSave() override;
	virtual void handleOpen() override;

	void buildScene();

	// Every distinct system, once
	Vector<basic2d::ParticleSystem *> getSystems() const;

	// The emitter named by {emitter: index}, or every emitter
	Vector<EmitterSlot *> getTargets(const Value &args);
	EmitterSlot *getSelected();

	void applyPreset(EmitterSlot &, StringView name);

	// The one way a change reaches an emitter: node keys, editor keys (which resample the curves),
	// then ParticleSystem::apply
	void applyPatch(EmitterSlot &, const Value &);
	void applyNodeValue(EmitterSlot &, const Value &);

	void buildMarkers(size_t index);
	void updateMarkers();
	void handleSceneTap(Vec2 world, bool remove);

	// {version, preset, system, node, editor}
	Value encodeJson(const EmitterSlot &) const;
	void applyJson(EmitterSlot &, const Value &);
	bool saveJson(StringView path);
	bool loadJson(StringView path);

	void setNotice(StringView);
	void updateNodePosition(EmitterSlot &);

	void setEmitters(uint32_t count, bool shared);
	void rebuildEmitters();
	void placeScene();

	void refreshPanel();
	void checkPanel();

	// encode -> init(Value) -> encode must be identical
	void checkRoundtrip();

	void updateAvailability();
	void refreshStatus();

	Value encodeStats() const;
	Value encodeFeedback(const EmitterSlot &) const;
	Value encodeFeedback(const basic2d::ParticleFeedback &) const;

	void registerCommands();
	void addCommand(StringView name, StringView description,
			Function<Value(const Value &)> &&handler);
	void addAsyncCommand(StringView name, StringView description,
			Function<void(const Value &, Function<void(Value &&)> &&)> &&handler);

	basic2d::Layer *_background = nullptr;
	basic2d::Layer *_sceneBackground = nullptr;
	Node *_root = nullptr;
	Node *_sceneArea = nullptr;
	basic2d::Label *_statusLabel = nullptr;
	basic2d::Label *_banner = nullptr;
	ParticleParamsPanel *_panel = nullptr;

	Map<String, Rc<Texture>> _textures;
	Vector<EmitterSlot> _emitters;
	uint32_t _emitterCount = 1;
	uint32_t _selected = 0;
	uint64_t _restarts = 0;
	bool _defaultSystems = false;
	bool _sharedSystem = false;
	uint64_t _updates = 0;

	// The feedback the status line shows, refreshed at most every 0.1 s
	uint64_t _statusFeedbackSequence = 0;
	float _statusTime = 0.0f;

	// Feedback totals of the selected emitter at the start of the rate window
	uint64_t _rateEmitterId = 0;
	uint64_t _rateBirths = 0;
	uint64_t _rateSteps = 0;
	float _rateTime = 0.0f;
	float _birthsRate = 0.0f;
	float _stepsRate = 0.0f;

	String _sceneMode = "move";
	String _notice;
	Rc<ClipboardSession> _clipboard;
	Rc<sprt::window::DialogRequest> _dialog;

	// The selected system's state the panel shows
	uint64_t _panelSystemId = 0;
	uint64_t _panelGeneration = maxOf<uint64_t>();

	bool _available = false;
	String _api;
	String _queue;
	String _reason;

	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;
};

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEDEMOLAYOUT_H_ */
