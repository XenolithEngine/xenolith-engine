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

#ifndef TESTS_WINDOW_SRC_PUGCASCADELAYOUT_H_
#define TESTS_WINDOW_SRC_PUGCASCADELAYOUT_H_

#include "TestLayout.h"
#include "XLPugSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstrates cascading variable/function resolution between nested pugui::TemplateSystem's.
// Two nodes are arranged manually (outer -> inner); the OUTER system defines a function brand()
// and a variable year, the INNER system's template references #{brand()} and #{year} without
// defining them - they resolve through the ancestor system's Context via the VarScope chain.
class PugCascadeLayout : public TestLayout {
public:
	virtual ~PugCascadeLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	Node *_outer = nullptr;
	Node *_inner = nullptr;
	Node *_outerTree = nullptr;
	Node *_innerTree = nullptr;
	pugui::TemplateSystem *_outerSys = nullptr;
	pugui::TemplateSystem *_innerSys = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_PUGCASCADELAYOUT_H_
