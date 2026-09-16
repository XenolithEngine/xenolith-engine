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

#ifndef TESTS_TESS_SRC_TESSBENCH_H_
#define TESTS_TESS_SRC_TESSBENCH_H_

#include "SPCommon.h"
#include "SPTess.h"
#include "SPTessLine.h"
#include "SPVectorImage.h"

namespace STAPPLER_VERSIONIZED stappler::tessbench {

using namespace stappler::geom;

/* WHAT one path came out as, in numbers a regression can be stated in.

Deliberately NOT the vertexes themselves. A tesselator is free to choose a different triangulation
of the same region - a fan instead of a strip, a different diagonal - and a golden file made of
vertex positions would go red on every such change while saying nothing about whether the picture
moved. What may not change is what the picture IS: how much area is covered, where it sits, and how
many separate pieces it is in. Those are properties of the RESULT, not of the route to it.

The counts are kept beside them anyway, because a change in triangle count with the area unchanged
is exactly the interesting case - it means the tesselation got better or worse without being wrong,
and that is what a performance change looks like. */
struct PathDigest {
	uint32_t vertexes = 0;
	uint32_t triangles = 0;

	double area = 0.0; // sum of |cross| / 2 over triangles, in path units
	float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;

	// Both the file and the comparison work in THOUSANDTHS of a path unit - see `encode`.

	bool failed = false; // the tesselator refused the path

	// Digests agree when the SHAPE agrees. Counts are compared exactly; geometry to a tolerance,
	// because the icons are 24 units across and a change of a thousandth of a unit is arithmetic
	// noise, not a different picture.
	// `geomEpsilon` is in path units; an icon is 24 across, so 0.002 is a change nobody can see.
	bool sameShape(const PathDigest &, float geomEpsilon) const;
	bool sameCounts(const PathDigest &other) const {
		return failed == other.failed && vertexes == other.vertexes && triangles == other.triangles;
	}

	mem_std::String encode() const;
	static bool decode(StringView, PathDigest &);
};

// One icon, tesselated. `antialias` adds the fringe the renderer asks for at scale 1 - the second
// variant, and the one that exercises `computeBoundary` and the relocation rule.
struct IconCase {
	mem_std::String name;
	bool antialias = false;
};

// Everything the bench knows how to do to one icon: fill it, digest it, and hand back the time.
struct IconResult {
	PathDigest digest;
	uint64_t microseconds = 0;
	uint32_t paths = 0; // an icon is one or more paths
};

/* The mesh itself, for the caller that wants to LOOK at the icon rather than measure it.

Optional, and off by default, because the bench tesselates four thousand icons twice and keeping
every mesh would be gigabytes for a run that only ever needed seven numbers per icon. The raster
side asks for it one icon at a time. */
struct RawMesh {
	mem_std::Vector<Vec2> vertexes;
	mem_std::Vector<uint32_t> indexes;
	mem_std::Vector<float> values; // per-vertex antialias intensity, 1.0 inside the shape
};

SP_PUBLIC IconResult tessellateIcon(StringView name, bool antialias, RawMesh *out = nullptr);

// Every icon the renderer has, in declaration order, skipping the ones with no geometry.
SP_PUBLIC void forEachIcon(const Callback<void(StringView)> &);

} // namespace stappler::tessbench

#endif /* TESTS_TESS_SRC_TESSBENCH_H_ */
