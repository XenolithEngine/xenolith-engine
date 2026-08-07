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

#ifndef TESTS_WINDOW_SRC_WIDGETS_PANELLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_PANELLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiPanel.h"
#include "XLUiCheckbox.h"
#include "XLUiBadge.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The Panel family (panel / checkbox / badge) takes its fill and its corners from CSS through the
// per-type appliers registered by each atom, not through the generic node-colour path a plain
// Layer uses. This test is what says whether that registration actually reaches them: a fill that
// never arrives leaves the atom at Panel's construction-time white, which is exactly what the
// installer saw.
//
// It also covers the state flip - checking a checkbox adds the "checked" class, so the resolver
// must restyle it - and the badge's Label child, which is styled by the same recursive resolver
// one level below a node whose own attributes were consumed by a type applier.
//
// The last phase covers the reset command (`ParameterName::CmdReset`): the resolver hands it to
// every type applier BEFORE the parameters, so a widget can drop styling that the new resolve is
// no longer going to mention. Without it, a declaration that stopped matching would stay applied
// forever, because a style pass only ever sees the parameters that ARE present.
class PanelLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();
	void runPhase3();

	void expectColor(StringView phase, StringView what, const Color4B &actual,
			const Color4B &expected);
	void expect(bool, StringView phase, StringView what);

	ui::Panel *_panel = nullptr;
	ui::Checkbox *_checkbox = nullptr;
	ui::Badge *_badge = nullptr;
	// same CSS, no type applier involved: the control that tells a broken applier apart from a
	// stylesheet that never matched anything
	basic2d::Layer *_plainLayer = nullptr;
	// styled by exactly one rule, which is taken away in the last phase - see CmdReset above
	ui::Button *_resettable = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_PANELLAYOUT_H_
