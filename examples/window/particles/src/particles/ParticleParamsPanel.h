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

#ifndef EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMSPANEL_H_
#define EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMSPANEL_H_

#include "particles/ParticleParams.h"
#include "XLNode.h"
#include "XLUiAccordionView.h"
#include "XLUiPanelRegistry.h"
#include "XLUiNumberField.h"
#include "XLUiSlider.h"
#include "XLUiVectorField.h"
#include "XLUiColorField.h"
#include "XLUiCheckbox.h"
#include "XLUiSelect.h"
#include "XLUiButton.h"
#include "particles/ParticleCurves.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// What the panel asks of its owner; every call comes from a widget callback
class ParticleParamsDelegate {
public:
	virtual ~ParticleParamsDelegate() = default;

	// A patch in the ParticleSystem::encode format; `texture` and `frameGrid` belong to the node
	virtual void handleParamsPatch(const Value &) = 0;
	virtual void handlePresetSelected(StringView) = 0;
	virtual void handleEmitterSelected(uint32_t) = 0;
	virtual void handleRestart() = 0;
	virtual void handleReset() = 0;

	// The scene tools: `move` drags the emitter and its origin, `points` edits emission points
	virtual void handleSceneMode(StringView) = 0;
	virtual void handleDrive(bool) = 0;
	virtual void handleClearPoints() = 0;
	virtual void handleCopy() = 0;
	virtual void handlePaste() = 0;
	virtual void handleSave() = 0;
	virtual void handleOpen() = 0;
};

/** Every parameter of getParticleParams as a live control, in an accordion of sections.

A widget change is turned into a patch right away and handed to the delegate; `refresh` puts the
system's state back into the widgets silently, so it never echoes as a patch. */
class ParticleParamsPanel : public Node {
public:
	virtual ~ParticleParamsPanel() = default;

	virtual bool init(ParticleParamsDelegate *);

	void refresh(const Value &system, const Value &node, const Value &editor);

	void setSceneMode(StringView);
	void setDrive(bool);

	void setEmitters(uint32_t count, uint32_t selected);
	void setPreset(StringView);

	// Opens or closes a section of the accordion
	bool setSectionOpen(StringView id, bool open);

	// Puts UI values into the parameter's widgets and applies them as a widget change would
	bool setWidgetValue(StringView key, SpanView<double>);

	// UI values of every parameter as its widgets hold them: {key: [values]}
	Value encodeUi() const;
	Vector<double> readUi(const ParticleParamInfo &) const;

protected:
	struct ParamWidgets {
		const ParticleParamInfo *info = nullptr;
		Vector<ui::NumberField *> numbers;
		Vector<ui::VectorField *> vectors;
		ui::Slider *slider = nullptr;
		ui::ColorField *color = nullptr;
		ui::Checkbox *check = nullptr;
		ui::Select *select = nullptr;
	};

	// The color curve editor: the stops, and the one being edited
	struct StopsEditor {
		Vector<ParticleColorStop> stops;
		size_t current = 0;
		ui::Checkbox *enabled = nullptr;
		ui::Select *select = nullptr;
		ui::Slider *position = nullptr;
		ui::NumberField *positionField = nullptr;
		ui::ColorField *color = nullptr;
		Node *preview = nullptr;
		Vector<basic2d::Layer *> previewCells;
	};

	void buildHeader();
	Rc<Node> buildSection(StringView section);
	int32_t buildRow(Node *body, const ParticleParamInfo &, int32_t z);
	int32_t buildStopsRows(Node *body, const ParticleParamInfo &, int32_t z);
	Node *makeRow(Node *body, StringView caption, int32_t z, StringView name);

	// Puts the current stop and the preview into the stops editor's widgets, silently
	void updateStopsWidgets();
	void editStops(const Callback<void(StopsEditor &)> &);
	void writeUi(ParamWidgets &, SpanView<double>);
	void emit(const ParamWidgets &);

	ParticleParamsDelegate *_delegate = nullptr;

	ui::Select *_presetSelect = nullptr;
	ui::Select *_emitterSelect = nullptr;
	ui::Button *_localeButton = nullptr;
	ui::Select *_modeSelect = nullptr;
	ui::Checkbox *_driveCheck = nullptr;

	StopsEditor _stops;

	Rc<ui::PanelRegistry> _registry;
	ui::AccordionView *_accordion = nullptr;

	Map<StringView, ParamWidgets> _widgets;

	Value _system;
	Value _node;
	Value _editor;
};

} // namespace stappler::xenolith::examples

#endif /* EXAMPLES_WINDOW_PARTICLES_SRC_PARTICLES_PARTICLEPARAMSPANEL_H_ */
