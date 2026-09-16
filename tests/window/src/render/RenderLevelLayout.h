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

#ifndef TESTS_WINDOW_SRC_RENDER_RENDERLEVELLAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_RENDERLEVELLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// What each RenderingLevel does to an OPAQUE node, and when the level is allowed to make it
// vanish.
//
// The renderer draws the solid geometry first, front to back, writing depth; surface and
// transparent geometry come afterwards and only depth-TEST against it. So the level is not a
// z-order: moving an opaque bar to Transparent does not lift it above the content, it moves it
// into a pass that the already-written depth can reject - which is what "the header disappeared
// when I set Transparent on it" really was.
//
// Row 1 puts one box per level IN FRONT of a solid backdrop (higher z): all four must be visible
// and identical.
// Row 2 puts the same four BEHIND an opaque cover (lower z): all four must be hidden - Transparent
// included, and that is correct, not a bug.
// Row 3 changes the level at runtime, after the boxes have been drawn once: each box must switch
// to the new level's behaviour instead of keeping the material it was first drawn with.
//
// Row 4 is the case the passes got WRONG: TRANSPARENT geometry behind a SURFACE box. Neither writes
// depth and the transparent pass is drawn after the surface one, so the picture behind painted over
// the box in front - a label over an image with an alpha channel simply vanished. The box that
// overlaps the translucent strip must be drawn over it; the box beside the strip, in front of it
// by zPath but covering none of it, must stay in the surface pass, and `render-level.apart` moves it
// over the strip to show that it is WHERE a surface draws, not only its zPath, that promotes it.
class RenderLevelLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void registerCommands() override;

protected:
	// one box per level, in the order Default, Solid, Surface, Transparent
	static constexpr size_t LevelCount = 4;

	void makeRow(basic2d::Layer *(&out)[LevelCount], ZOrder z, const Color4F &);
	void runPhase1();

	basic2d::Layer *_frontBackdrop = nullptr;
	basic2d::Layer *_behindCover = nullptr;
	basic2d::Layer *_front[LevelCount] = {};
	basic2d::Layer *_behind[LevelCount] = {};
	basic2d::Layer *_switched[LevelCount] = {};
	basic2d::Layer *_switchedBackdrop = nullptr;

	// row 4: a translucent strip, a surface box over it and a surface box beside it
	basic2d::Layer *_strip = nullptr;
	basic2d::Layer *_over = nullptr;
	basic2d::Layer *_apart = nullptr;
	bool _apartOverlaps = false;

	void placeApart();
	Value encodeState() const;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_RENDER_RENDERLEVELLAYOUT_H_
