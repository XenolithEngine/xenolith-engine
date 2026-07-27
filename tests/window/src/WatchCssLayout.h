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

#ifndef TESTS_WINDOW_SRC_WATCHCSSLAYOUT_H_
#define TESTS_WINDOW_SRC_WATCHCSSLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for ui::StyleSystem live CSS reload (watchFile). A stylesheet is
// loaded from a real on-disk file; a swatch Layer gets its background-color from it. The
// app then REWRITES the file on the director thread (red -> green); the file watch should
// fire, rebuild the sheet and re-apply the style, turning the swatch green. Logs a
// PASS/FAIL by re-resolving the swatch's color after the rewrite.
//
// Reach via XL_WATCH_CSS_TEST. Optional XL_WATCH_CSS_FILE overrides the temp path.
class WatchCssLayout : public basic2d::SceneLayout2d {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void writeCss(StringView color);

	String _cssPath;
	basic2d::Layer *_swatch = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WATCHCSSLAYOUT_H_
