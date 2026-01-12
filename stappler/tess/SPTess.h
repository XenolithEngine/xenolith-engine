/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_TESS_SPTESS_H_
#define STAPPLER_TESS_SPTESS_H_

#include "SPVec2.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

struct HalfEdge;

enum class VerboseFlag : long {
	None,
	General,
	Full
};

enum class Winding {
	EvenOdd,
	NonZero,
	Positive,
	Negative,
	AbsGeqTwo
};

struct SP_PUBLIC TessResult {
	uint32_t nvertexes = 0;
	uint32_t nfaces = 0;

	void *target = nullptr;
	void (*pushVertex)(void *, uint32_t, const Vec2 &pt, float vertexValue, const Vec2 &norm);
	void (*pushTriangle)(void *, uint32_t[3]);
};

class SP_PUBLIC Tesselator : public Ref {
public:
	struct Cursor {
		HalfEdge *edge = nullptr;
		HalfEdge *root = nullptr;
		uint32_t count = 0;
		bool isClockwise = false; // CCW by default
		bool closed = false;
		Vec2 origin;
	};

	virtual ~Tesselator();

	bool init(memory::pool_t *);

	// Tesselator can generate subpixel border around contour to reduce aliasing effects
	// Antialiasing value is the width of this border (typically - 0.5 of screen pixel)
	// Border for an edge is a single quad (two triangles) with additional intensity
	// attribute for its vertexes. You should multiply original color alpha component with
	// intensity to achieve correct antialiasing.
	// Extra triangles will not intersect actual contour triangles, no color artifacts
	// should be observed on modern graphic hardware
	// When antialiasing is enabled (value > 0.0f), original vertexes will be displaced
	// (or even split into multiple vertexes) for visually accurate output
	void setAntialiasValue(float);

	// Same as above, but controls inset and offset separately
	// For RelocateRule::None and Auto inset can not always be added, it will be summed with offset
	void setBoundariesTransform(float inset, float offset);

	float getBoundaryInset() const;
	float getBoundaryOffset() const;

	// Content scale used only in DistanceField mode, to generate extra vertexes on boundary
	void setContentScale(float);

	float getContentScale() const;

	// Rule, how to relocate origin vertex for antialiasing purposes
	// Antialiasing algorithm can relocate vertexes to reduce visual extension effect,
	// caused by subpixel border. This can produce transparency artifacts on complex form,
	// when vertex displacement should change it's priority in processing).
	// If relocation is disabled completely, artifacts can be observed on complex vertexes
	// with more then two edges (usually - self-intersecions).
	enum class RelocateRule {
		Never, // do not relocate vertexes, image in result can be slightly bolder then original
		Auto, // relocate only self-intersects and merged vertexes, default
		Always, // relocate all vertexes
		Monotonize, // force to remonotonize after relocation (expensive, but best quality)
		DistanceField, // create distance field instead of antialiasing
	};

	void setRelocateRule(RelocateRule);
	RelocateRule getRelocateRule() const;

	// Winding rule used to determine, what region is interior for the output.
	// Since tesselator uses sweepline algorithm, winding is calculated as number of
	// sweepline intersections with significant (not helper/external) edges.
	// Intersection with CCW-edge adds 1 to winding, with CW-edge - subtracts 1.
	// Then, winding rule used to determine if region should be tesselated as interior.
	// Region with winding number 0 always treated as exterior.
	void setWindingRule(Winding);
	Winding getWindingRule() const;

	void preallocate(uint32_t n); // allocate n vertexes and n edges;

	// In contrast with GLU tesselator, you can write multiple contours via different cursors.
	// For example - if you generate multiple symmetric contours with same function - you can write
	// all contours in place, instead of buffering results or run generation process multiple times
	Cursor beginContour(bool clockwise = false);
	bool pushVertex(Cursor &, const Vec2 &);
	bool pushStrokeVertex(Cursor &, const Vec2 &pt, const Vec2 &offset);
	bool pushStrokeTop(Cursor &, const Vec2 &pt);
	bool pushStrokeBottom(Cursor &, const Vec2 &pt);
	bool closeContour(Cursor &);
	bool closeStrokeContour(Cursor &);

	// Output process split into two phases:
	// 1. Prepare - calculate and tesselate interior regions, assign indexes for faces and vertexes,
	// write new nvertexes and nfaces to result
	// 2. Write - write actual vertexes and indexes into buffers
	//
	// So, you can use one large single buffer for vertexes/indexes like:
	// 1. run `prepare` for all tesselators, to calculate required buffers size
	// 2. allocate single vertex buffer and single index buffer
	// 3. run `write ` for all tesselators to fill buffer
	// To do this - pass single TessResult to all calls
	bool prepare(TessResult &);
	bool write(TessResult &);

protected:
	struct Data;

	Data *_data = nullptr;
};

} // namespace stappler::geom

#endif /* STAPPLER_TESS_SPTESS_H_ */
