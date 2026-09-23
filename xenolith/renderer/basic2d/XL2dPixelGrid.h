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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DPIXELGRID_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DPIXELGRID_H_

#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

/** A grid of one-device-pixel lines over a zoomable plane, drawn as quads in surface pixels.

The plane is mapped into this node by `setMapping`: grid unit `i` is at `origin + i * unit` on both
axes. Level `k` is every `4^k` units. A level whose on-screen step is under `MinStep` device
pixels is not drawn, so zooming out moves drawing up the ladder 1, 4, 16, 64 and on. A line
belongs to the highest level dividing its index and is drawn once. Its colour runs from `minor`
to `major` by how far its level's step is above `MinStep`, and it fades in over the first half
of a step, so a zoom never makes a line jump.

Every line is snapped to a whole device pixel with `snapToPixels`. Geometry is rebuilt only when
the node's transform, size, mapping or colour changes; a still frame reuses it. */
class SP_PUBLIC PixelGrid : public Sprite {
public:
	static constexpr float DefaultMinStep = 4.0f;
	static constexpr uint32_t LevelRatio = 4;

	// What the last build decided.
	struct State {
		int32_t baseLevel = -1; // finest level drawn; -1 when nothing is
		float baseStep = 0.0f; // its step in device pixels
		uint32_t lines = 0;
		uint32_t rebuilds = 0;

		// Where unit zero is in surface pixels, before snapping, and how many pixels a unit is.
		// A line at unit `i` is at floor(origin + i * unit + 0.5).
		Vec2 surfaceOrigin;
		Vec2 surfaceUnit;

		// False when the transform has a rotation or skew: lines are then placed unsnapped.
		bool snapped = true;
	};

	virtual ~PixelGrid() = default;

	virtual bool init() override;

	void setMapping(const Vec2 &origin, float unit);
	const Vec2 &getOrigin() const { return _origin; }
	float getUnit() const { return _unit; }

	/* Read the mapping from a sibling each frame: its position is the origin and its x scale the
	unit. For a node that is panned and zoomed by writing its transform, like a canvas world; the
	sibling must outlive this node or be unset first. */
	void setMappingSource(Node *);
	Node *getMappingSource() const { return _mappingSource; }

	// Lines only inside this rect of the node; an empty rect means the whole content size.
	void setBounds(const Rect &);
	const Rect &getBounds() const { return _bounds; }

	// Draw the bounds' edges in the major colour whatever the zoom.
	void setEdgesVisible(bool);
	bool isEdgesVisible() const { return _edges; }

	// The finest level there is. Zero: a step never goes below one unit.
	void setMinLevel(int32_t);
	int32_t getMinLevel() const { return _minLevel; }

	void setMinStep(float);
	float getMinStep() const { return _minStep; }

	void setColors(const Color4F &minor, const Color4F &major);
	const Color4F &getMinorColor() const { return _minor; }
	const Color4F &getMajorColor() const { return _major; }

	const State &getState() const { return _state; }

	virtual void draw(FrameInfo &, NodeVisitFlags flags) override;

protected:
	using Sprite::init;

	virtual void pushCommands(FrameInfo &, NodeVisitFlags flags) override;
	virtual void updateVertexes(FrameInfo &frame) override;
	virtual void updateVertexesColor() override { }

	Vec2 _origin;
	float _unit = 1.0f;
	Node *_mappingSource = nullptr;
	Rect _bounds;
	bool _edges = false;
	int32_t _minLevel = 0;
	float _minStep = DefaultMinStep;
	Color4F _minor = Color4F(1.0f, 1.0f, 1.0f, 0.06f);
	Color4F _major = Color4F(1.0f, 1.0f, 1.0f, 0.2f);

	// The node-to-surface transform the vertices were built against.
	Mat4 _builtTransform;
	Color4F _builtColor;
	State _state;
};

// `point`, in the space `nodeToSurface` maps from, moved onto the nearest device pixel boundary.
// With a rotation or skew in the transform the point is returned as is.
SP_PUBLIC Vec2 snapToPixels(const Mat4 &nodeToSurface, const Vec2 &point);

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DPIXELGRID_H_ */
