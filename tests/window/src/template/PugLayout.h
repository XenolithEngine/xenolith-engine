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

#ifndef TESTS_WINDOW_SRC_TEMPLATE_PUGLAYOUT_H_
#define TESTS_WINDOW_SRC_TEMPLATE_PUGLAYOUT_H_

#include "app/TestLayout.h"
#include "XLPugSystem.h"
#include "XLSimpleStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration layout for the pug -> scene-graph pipeline (xenolith_renderer_pug)
// and the simpleui CSS styling subsystem:
// - a pug template builds the node tree (flex, labels, buttons);
// - a CSS stylesheet is attached to this layout node via StyleSheetSystem, styles
//   resolve by selectors (tag/.class/#id) and auto-apply through StyleApplier;
// - "Toggle accent" flips a css class on a label (subtree re-resolution);
// - "Toggle theme" swaps the whole stylesheet (light <-> dark);
// - "Rebuild" re-runs the template with updated context data.
class PugLayout : public TestLayout {
public:
	virtual ~PugLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void toggleTheme();
	void toggleAccent();
	void rebuildTemplate();
	Node *findByName(Node *, StringView name) const;

	pugui::TemplateSystem *_template = nullptr;
	Node *_tree = nullptr;
	Rc<simpleui::StyleSheet> _lightSheet;
	Rc<simpleui::StyleSheet> _darkSheet;
	simpleui::StyleSheetSystem *_styles = nullptr;
	bool _dark = false;
	uint32_t _taps = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_TEMPLATE_PUGLAYOUT_H_
