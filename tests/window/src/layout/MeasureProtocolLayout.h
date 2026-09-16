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

#ifndef TESTS_WINDOW_SRC_LAYOUT_MEASUREPROTOCOLLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_MEASUREPROTOCOLLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The flex engine measures whatever can answer, not a fixed list of classes.
//
// `Label` and a nested flex container are simply the two answerers the engine ships. Anything
// that carries a `HandleMeasure` system, or the precomputed `MeasureComponent` fallback, takes
// part in content sizing on exactly the same terms - including plain `flex-basis: auto`, which in
// CSS falls through to the size property and then to the content.
//
// The boxes in the row are, left to right:
//   1. a node with an application-written measure system, `flex-basis: auto`   (must be measured)
//   2. the same node with `flex-basis: fit-content`                            (must be measured)
//   3. the same node clamped by `max-width` - it must reflow, not just clip
//   4. a node with no system, only a `MeasureComponent`                        (fallback answers)
//   5. a `Label`                                                               (the shipped case)
//   6. a `Layer` with an explicit CSS width                                    (never measured)
class MeasureProtocolLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();

	void expectNear(StringView what, float actual, float expected);
	void expect(bool, StringView what);

	basic2d::Layer *_row = nullptr;

	Node *_autoBasis = nullptr;
	Node *_fitBasis = nullptr;
	Node *_clamped = nullptr;
	Node *_component = nullptr;
	basic2d::Label *_label = nullptr;
	basic2d::Layer *_fixed = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_MEASUREPROTOCOLLAYOUT_H_
