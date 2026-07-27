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

#ifndef TESTS_WINDOW_SRC_INHERITEDSTYLELAYOUT_H_
#define TESTS_WINDOW_SRC_INHERITEDSTYLELAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for CSS inherited properties flowing through the Inherited*Style
// components (see XLInheritedStyle.h). A styled `.box` container gets the components from
// ui::StyleResolver; programmatically-created child labels (matching no selectors, with
// explicit 14px/black settings) must render with the inherited 28px/bold/green/uppercase
// style while their stored explicit values stay untouched. Two consumption paths are
// covered: a recursive resolver (label gets its own components) and an ancestor walk
// (non-recursive resolver, label reads the parent chain). Rewriting the CSS to drop the
// rule must remove the components and revert the labels to their explicit style.
// Reach via XL_INHERITED_TEST.
class InheritedStyleLayout : public basic2d::SceneLayout2d {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void writeCss(bool styled);
	void nudgeAncestorLabel();
	void runPhase1();
	void runPhase2();

	String _cssPath;
	basic2d::Layer *_containerRecursive = nullptr;
	basic2d::Layer *_containerAncestor = nullptr;
	basic2d::Label *_labelRecursive = nullptr;
	basic2d::Label *_labelAncestor = nullptr;
	basic2d::Label *_labelReference = nullptr;

	uint32_t _nudges = 0;
	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_INHERITEDSTYLELAYOUT_H_
