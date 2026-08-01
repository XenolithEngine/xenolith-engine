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

#ifndef TESTS_WINDOW_SRC_HOVERLAYOUT_H_
#define TESTS_WINDOW_SRC_HOVERLAYOUT_H_

#include "TestLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XLUiStyleResolver.h"
#include "XLUiInteractiveComponent.h" // ui::InteractiveState (= document::InteractiveFlags)

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for interactive pseudo-classes (:hover/:active/:checked/:disabled) in
// the document CSS core, wired to InteractiveComponent through StyleResolver. Each swatch has
// a fixed InteractiveComponent state; its background-color comes from the matching pseudo rule
// (grey base, red :hover, blue :active, green :checked, purple :disabled). A sixth swatch flips
// to :hover at runtime through StyleResolver::handleInteractiveState, exercising the mask cache.
class HoverLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	struct Row {
		basic2d::Label *name = nullptr;
		basic2d::Layer *swatch = nullptr;
	};

	basic2d::Layer *makeSwatch(Node *parent, ui::InteractiveState state, ui::StyleResolver **outResolver = nullptr);

	Vector<Row> _rows;
	basic2d::Layer *_transition = nullptr;
	ui::StyleResolver *_transitionResolver = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_HOVERLAYOUT_H_
