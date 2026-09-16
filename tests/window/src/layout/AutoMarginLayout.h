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

#ifndef TESTS_WINDOW_SRC_LAYOUT_AUTOMARGINLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_AUTOMARGINLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// `margin: auto` on a flex item, which is what CSS gives you instead of an alignment property
// for a single item.
//
// An auto margin is free space, not a distance. Along the main axis every auto margin on a line
// splits the space left after flexing, and `justify-content` gets nothing — so `margin-left: auto`
// on the last item pushes it to the end, and `margin: 0 auto` on a lone item centres it. Along
// the cross axis an item's own auto margins split what is left of its line, and they outrank
// `align-self`/`align-items`, `stretch` included.
//
// The rows are: push-to-end, centre-one, cross-centre against `align-items: flex-start`, and an
// auto cross margin defeating `align-items: stretch`.
class AutoMarginLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();

	void expectNear(StringView what, float actual, float expected);

	basic2d::Layer *makeRow(StringView cls);

	basic2d::Layer *_pushRow = nullptr;
	basic2d::Layer *_pushFirst = nullptr;
	basic2d::Layer *_pushLast = nullptr;

	basic2d::Layer *_centreRow = nullptr;
	basic2d::Layer *_centreOnly = nullptr;

	basic2d::Layer *_crossRow = nullptr;
	basic2d::Layer *_crossPlain = nullptr; // aligned by align-items: flex-start
	basic2d::Layer *_crossAuto = nullptr; // centred by its own auto margins

	basic2d::Layer *_stretchRow = nullptr;
	basic2d::Layer *_stretchPlain = nullptr; // stretches to the line
	basic2d::Layer *_stretchAuto = nullptr; // keeps its height, auto margin wins

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_AUTOMARGINLAYOUT_H_
