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

#ifndef TESTS_WINDOW_SRC_RENDER_SCALE9LAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_SCALE9LAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dScale9Sprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A Scale9Sprite that can be asked what it actually put in the vertex buffer.
//
// The claim about a nine-slice is a set of numbers - this corner is 64 wide at every content size,
// that edge grew only along x, these nine texture rects tile the fragment exactly - and every one
// of them is invisible in a picture: a stretched corner and an unstretched one differ by a few
// pixels of gradient. So the check reads the vertices.
//
// It reads the REAL ones. A second copy of the geometry, computed here for the check to compare
// against, would only prove that two pieces of arithmetic agree; VertexArray::getQuad reports what
// the sprite wrote, which is what the GPU will draw.
class Scale9Probe : public basic2d::Scale9Sprite {
public:
	virtual ~Scale9Probe() { }

	// One entry per quad, in the order the sprite emitted them: the rect in the sprite's own
	// coordinates and the normalized rect in the texture. Not const: VertexArray hands out a quad
	// through a mutable accessor, and copying the array to read it would be worse.
	Value encodePieces();

	Value encodeState();
};

// Scale9Sprite: the nine-slice geometry.
//
// Three sprites, and each one exists for a claim the others cannot make:
//
//   * `full`   the whole picture, sliced on all four sides. The reference: corner sizes must equal
//              the four numbers exactly at any content size, and the nine texture rects must tile
//              the fragment with no gap and no overlap;
//   * `atlas`  the same four numbers over a SUB-RECT of the same texture. The slice is measured in
//              pixels of the FRAGMENT, so this one must come out with the same view geometry and
//              DIFFERENT texture coordinates - the one thing that breaks if the code divides by
//              the texture instead;
//   * `zero`   one side left at zero. That column must not be emitted at all: six pieces rather
//              than nine, and the tiling still exact.
//
// Nothing here is checked by looking at it, and nothing here needs a screenshot: a PNG comparison
// would be testing the rasterizer rather than the slicing.
class Scale9Layout : public TestLayout {
public:
	// The stand's own geometry, duplicated by scale9-check.py on purpose: a check that reads its
	// expectations out of the thing it is checking cannot fail.
	static constexpr float SpriteWidth = 300.0f;
	static constexpr float SpriteHeight = 200.0f;

	// Four DIFFERENT numbers, none of them zero and none of them equal: three of the four ways to
	// get the order wrong produce the same picture when two sides match.
	static constexpr float SliceTop = 12.0f;
	static constexpr float SliceRight = 16.0f;
	static constexpr float SliceBottom = 8.0f;
	static constexpr float SliceLeft = 20.0f;

	// The texture this stand draws is BUILT HERE rather than bundled: a nine-slice claim is a claim
	// about pixel arithmetic, and the numbers it is checked against must not depend on what a .png
	// in resources/ happens to be. It is also the only way to have the texture ready when the
	// sprites enter - a queue resource is compiled asynchronously, and Sprite acquires its texture
	// exactly once, on enter.
	static constexpr uint32_t TextureWidth = 128;
	static constexpr uint32_t TextureHeight = 96;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(Scene *) override;

protected:
	virtual void registerCommands() override;

	// The generated frame: an orange border of exactly the four slice widths over a blue middle,
	// so that a person looking at the stand sees what the check asserts.
	Rc<Texture> makeTexture();

	Scale9Probe *getTarget(const Value &args) const;

	Scale9Probe *_full = nullptr;
	Scale9Probe *_atlas = nullptr;
	Scale9Probe *_zero = nullptr;
};

} // namespace stappler::xenolith::app

#endif /* TESTS_WINDOW_SRC_RENDER_SCALE9LAYOUT_H_ */
