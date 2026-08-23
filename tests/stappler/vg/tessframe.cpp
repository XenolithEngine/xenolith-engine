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

/* WHERE a shape sits decides whether the tesselator can tesselate it, and the two facts that
follow from that are what this file guards.

`Tesselator::Data::_mathTolerance` is `4 * FLT_EPSILON`, an ABSOLUTE number (SPTess.cc). It is one
ulp at 1.0, and one ulp of a float is already larger than it past |x| = 4. Every "is this the same
point as that one" in the sweep - `VertEq`, `FloatEq`, the intersection tests - is decided against
it, so a path that merely sits far from the origin is judged with a tolerance finer than its own
coordinates can express. The sweep then believes two ADJACENT REPRESENTABLE points are distinct,
splits an edge that has no length, and either drops the path or walks a released half-edge.

That is why `VectorCanvasPathDrawer::draw` hands the tesselator every path centred on its own
bounding box and puts the frame back in `pushVertex`. The graph editor is what found it: a wire
inside a forty-thousand-node atlas lives at coordinates of order 1e5, and nineteen of them failed
per frame - each failure erasing one whole wire, since a path is one tesselation.

Two things have to hold for that workaround to be right, and both are checked below:

  * A CENTRED PATH TESSELATES. The wire below is a real one, dumped out of the editor; centred, it
    comes out fine. This is the property the canvas now depends on.
  * THE ANTIALIAS FRINGE DOES NOT DEPEND ON POSITION. Centring may only translate the result. The
    relocation rule moves boundary vertexes it considers merged, and "merged" is decided by that
    same tolerance - so the fringe COULD have differed between a shape at the origin and the same
    shape far away. It does not, and that is asserted rather than assumed.

What is deliberately NOT exercised here is the uncentred path at large coordinates. It does not
merely fail - past about 1e5 it segfaults inside `splitEdgeLoops`, and a suite that runs it dies.
Fixing the tesselator to scale its tolerance with the coordinates would let that case be asserted;
until then the canvas keeps it from ever arising. */

#include "SPCommon.h"
#include "SPTessLine.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::geom;

struct Mesh {
	mem_std::Vector<Vec2> vertexes;
	mem_std::Vector<uint32_t> indexes;
	bool prepared = false;

	static void onVertex(void *t, uint32_t idx, const Vec2 &pt, float, const Vec2 &) {
		auto self = static_cast<Mesh *>(t);
		if (idx >= self->vertexes.size()) {
			self->vertexes.resize(idx + 1);
		}
		self->vertexes[idx] = pt;
	}
	static void onTriangle(void *t, uint32_t v[3]) {
		auto self = static_cast<Mesh *>(t);
		self->indexes.emplace_back(v[0]);
		self->indexes.emplace_back(v[1]);
		self->indexes.emplace_back(v[2]);
	}

	size_t triangles() const { return indexes.size() / 3; }

	/* Is this the same mesh as `other`, moved?
	
	Compared vertex by vertex with each mesh put back in its own frame, which is the only way to
	ask the question honestly: the tesselator returns coordinates in the caller's space, and a
	float at 1e6 steps by 0.0625, so a ribbon four units wide is QUANTIZED on the way out however
	exactly it was computed. `tolerance` is that step, not a fudge - it is what the output format
	can represent at that magnitude, and asking for less would be asking the impossible.

	Topology is compared exactly, because nothing about it is approximate. */
	bool matches(const Mesh &other, float tolerance) const {
		if (!prepared || !other.prepared || indexes.size() != other.indexes.size()
				|| vertexes.size() != other.vertexes.size()) {
			return false;
		}
		for (size_t i = 0; i < indexes.size(); ++i) {
			if (indexes[i] != other.indexes[i]) {
				return false;
			}
		}
		if (vertexes.empty()) {
			return true;
		}
		const Vec2 a0 = vertexes.front(), b0 = other.vertexes.front();
		for (size_t i = 0; i < vertexes.size(); ++i) {
			const Vec2 a = vertexes[i] - a0;
			const Vec2 b = other.vertexes[i] - b0;
			if (sprt::abs(a.x - b.x) > tolerance || sprt::abs(a.y - b.y) > tolerance) {
				return false;
			}
		}
		return true;
	}

	// The step between two adjacent floats around `k` - what the output can resolve there.
	static float outputStep(float k) {
		return sprt::max(sprt::Epsilon<float>, sprt::abs(k) * sprt::Epsilon<float> * 2.0f);
	}

	// Measured around the mesh's own first vertex, in double.
	//
	// Not fussiness: the cross product of three points at coordinates of 1e6 subtracts near-equal
	// numbers, and a float there is only good to 0.0625 - so a four-unit triangle would carry a
	// percent of error from THE MEASUREMENT, and the checks below would be reading their own
	// arithmetic instead of the tesselator's. The whole point here is comparing a shape against
	// itself moved, so the frame comes off first.
	double area() const {
		if (indexes.empty()) {
			return 0.0;
		}
		const double ox = vertexes[indexes[0]].x, oy = vertexes[indexes[0]].y;
		double ret = 0.0;
		for (size_t i = 0; i < indexes.size(); i += 3) {
			auto &a = vertexes[indexes[i]];
			auto &b = vertexes[indexes[i + 1]];
			auto &c = vertexes[indexes[i + 2]];
			const double ax = a.x - ox, ay = a.y - oy;
			const double bx = b.x - ox, by = b.y - oy;
			const double cx = c.x - ox, cy = c.y - oy;
			ret += sprt::abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) * 0.5;
		}
		return ret;
	}
};

// Runs one contour through LineDrawer and out of the tesselator. `width` of zero means fill.
static Mesh run(float width, float inset, float offset, const Callback<void(LineDrawer &)> &drawer) {
	Mesh ret;

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::perform([&] {
		auto tess = Rc<Tesselator>::create(pool);

		do {
			StrokeConfig cfg;
			cfg.lineWidth = width > 0.0f ? width : 1.0f;
			LineDrawer line(1.0f, width > 0.0f ? nullptr : Rc<Tesselator>(tess),
					width > 0.0f ? Rc<Tesselator>(tess) : nullptr, nullptr, cfg);
			drawer(line);
			line.drawClose(false);
		} while (0);

		if (inset > 0.0f || offset > 0.0f) {
			tess->setBoundariesTransform(inset, offset);
			tess->setRelocateRule(Tesselator::RelocateRule::Auto);
		}
		tess->setWindingRule(Winding::NonZero);

		TessResult res;
		res.target = &ret;
		res.pushVertex = &Mesh::onVertex;
		res.pushTriangle = &Mesh::onTriangle;

		ret.prepared = tess->prepare(res);
		if (ret.prepared) {
			ret.vertexes.resize(res.nvertexes);
			tess->write(res);
		}
	}, pool);
	memory::pool::destroy(pool);
	return ret;
}

// ---- the fringe may not depend on where the shape is --------------------------------------------

// An exec pin: a filled triangle about ten units across, drawn at a sweep of offsets. Area rather
// than vertex positions, because that is what "the same shape, translated" means for a mesh whose
// triangulation the sweep is free to choose.
static void checkFringeIsTranslationInvariant() {
	const auto pin = [](Vec2 at) {
		return [at](LineDrawer &line) {
			line.drawBegin(at.x, at.y);
			line.drawLine(at.x + 10.0f, at.y + 5.0f);
			line.drawLine(at.x, at.y + 10.0f);
			line.drawClose(true);
		};
	};

	for (float fringe : {0.0f, 1.0f, 2.0f}) {
		auto at0 = pin(Vec2(0.0f, 0.0f));
		auto base = run(0.0f, fringe, fringe, at0);
		check(base.prepared, mem_std::toString("a pin tesselates at the origin, fringe ", fringe));
		if (!base.prepared) {
			continue;
		}

		for (float k : {100.0f, 1'000.0f, 10'000.0f, 100'000.0f}) {
			auto atK = pin(Vec2(k, k));
			auto moved = run(0.0f, fringe, fringe, atK);
			check(moved.prepared && moved.triangles() == base.triangles(),
					mem_std::toString("... same triangle count at ", k, ", fringe ", fringe, ": ",
							base.triangles(), " vs ", moved.triangles()));
			// A part in a thousand. The claim is that the SHAPE is unchanged, not that the
			// arithmetic is exact - and out at 1e5 a float coordinate is only good to 0.008, so a
			// ten-unit triangle carries a relative error of about 1e-3 before the tesselator does
			// anything at all. A fringe that actually differed would move the area by percent.
			check(moved.prepared
							&& sprt::abs(moved.area() - base.area())
									<= sprt::abs(base.area()) * 1.0e-3,
					mem_std::toString("... same area at ", k, ", fringe ", fringe, ": ", base.area(),
							" vs ", moved.area()));
		}
	}
}

// ---- a centred path tesselates ------------------------------------------------------------------

// Dumped out of the graph editor at forty thousand nodes, verbatim:
//
//   M 77945.25,27803.5 C 103881.5,27803.5  136.5,27683.25  26072.75,27683.25
//
// A backwards wire: it ends 51 872 units to the left of where it starts and only 120 units below.
// `computeEdgeCurve` puts the control points `|dx| * 0.5` out from each end, so the curve is a flat
// ribbon fifty thousand wide folded through itself. None of that is what breaks it - see the file
// header - but it is the real input, so it is the one worth checking.
struct Wire {
	Vec2 a, c1, c2, b;
};

static constexpr Wire s_editorWire{Vec2(77'945.25f, 27'803.5f), Vec2(103'881.5f, 27'803.5f),
	Vec2(136.5f, 27'683.25f), Vec2(26'072.75f, 27'683.25f)};

static void checkCentredWire() {
	// What the canvas does now: the centre of the path's own bounding box becomes its origin.
	const Vec2 bmin(sprt::min(sprt::min(s_editorWire.a.x, s_editorWire.c1.x),
							 sprt::min(s_editorWire.c2.x, s_editorWire.b.x)),
			sprt::min(sprt::min(s_editorWire.a.y, s_editorWire.c1.y),
					sprt::min(s_editorWire.c2.y, s_editorWire.b.y)));
	const Vec2 bmax(sprt::max(sprt::max(s_editorWire.a.x, s_editorWire.c1.x),
							 sprt::max(s_editorWire.c2.x, s_editorWire.b.x)),
			sprt::max(sprt::max(s_editorWire.a.y, s_editorWire.c1.y),
					sprt::max(s_editorWire.c2.y, s_editorWire.b.y)));
	const Vec2 origin((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);

	auto draw = [&](LineDrawer &line) {
		line.drawBegin(s_editorWire.a.x - origin.x, s_editorWire.a.y - origin.y);
		line.drawCubicBezier(s_editorWire.c1.x - origin.x, s_editorWire.c1.y - origin.y,
				s_editorWire.c2.x - origin.x, s_editorWire.c2.y - origin.y,
				s_editorWire.b.x - origin.x, s_editorWire.b.y - origin.y);
	};

	auto mesh = run(2.0f, 25.0f, 25.0f, draw);

	check(mesh.prepared, "the editor's backwards wire tesselates once it is centred");
	check(mesh.triangles() > 0,
			mem_std::toString("... and it produces triangles: ", mesh.triangles()));
}

// ---- many disjoint contours in one path ---------------------------------------------------------

// The box layer's shape: one path holding a rectangle per node. Here to say that the count alone is
// not what breaks anything - forty thousand of them come out as forty thousand quads - and to pin
// the fact that subpaths of ONE path ARE resolved against each other, which is exactly what does
// not happen between two paths.
static void checkGrid() {
	const auto grid = [](uint32_t n, Vec2 step) {
		return [n, step](LineDrawer &line) {
			for (uint32_t r = 0; r < n; ++r) {
				for (uint32_t c = 0; c < n; ++c) {
					const float x = float(c) * step.x, y = float(r) * step.y;
					line.drawBegin(x, y);
					line.drawLine(x + 200.0f, y);
					line.drawLine(x + 200.0f, y + 100.0f);
					line.drawLine(x, y + 100.0f);
					line.drawClose(true);
				}
			}
		};
	};

	auto wide = grid(200, Vec2(300.0f, 200.0f));
	auto disjoint = run(0.0f, 0.0f, 0.0f, wide);
	check(disjoint.prepared && disjoint.triangles() == 200 * 200 * 2,
			mem_std::toString("forty thousand disjoint rectangles in one path are as many quads: ",
					disjoint.triangles()));

	// Sharing a corner is not sharing a vertex by accident: 16x16 touching rectangles have 17x17
	// corners between them, and the sweep is what merges them.
	auto tight = grid(16, Vec2(200.0f, 100.0f));
	auto touching = run(0.0f, 0.0f, 0.0f, tight);
	check(touching.prepared && touching.vertexes.size() == 17 * 17,
			mem_std::toString("touching rectangles of ONE path share their corners: ",
					touching.vertexes.size()));
}


// ---- the same shape, wherever it sits -----------------------------------------------------------

/* The invariant the normalization buys, stated so that only the SWEEP is on trial.

Coordinates are integers and the offsets are integers, so translating the input is exact to the last
bit - a float holds every integer up to 2^24 and the ulp at 1e6 is 0.0625, well under one. That
makes "the same shape, moved" mean literally the same numbers with a different exponent, and any
difference in the output is the sweep's own doing.

The shape is a bowtie: a closed contour that crosses itself, so the sweep has to find an
intersection, split two edges at it and merge the pieces - the machinery that used to walk a
released half-edge at large coordinates. NonZero winding, no fringe: what is measured is the
triangulation, not the antialias.

Before the tolerance split and the frame, this failed at 20 000 and segfaulted at 1e5. */
static void checkSweepIsTranslationInvariant() {
	const auto bowtie = [](float k) {
		return [k](LineDrawer &line) {
			line.drawBegin(k + 0.0f, k + 0.0f);
			line.drawLine(k + 300.0f, k + 200.0f);
			line.drawLine(k + 300.0f, k + 0.0f);
			line.drawLine(k + 0.0f, k + 200.0f);
			line.drawClose(true);
		};
	};

	auto b0 = bowtie(0.0f);
	auto base = run(0.0f, 0.0f, 0.0f, b0);
	check(base.prepared && base.triangles() > 0,
			mem_std::toString("a self-crossing contour tesselates at the origin: ",
					base.triangles(), " triangles"));
	if (!base.prepared) {
		return;
	}

	for (float k : {1'000.0f, 10'000.0f, 20'000.0f, 50'000.0f, 100'000.0f, 1'000'000.0f}) {
		auto bk = bowtie(k);
		auto moved = run(0.0f, 0.0f, 0.0f, bk);
		check(moved.prepared && moved.triangles() == base.triangles(),
				mem_std::toString("... same triangulation at ", k, ": ", base.triangles(), " vs ",
						moved.triangles()));
		check(moved.matches(base, Mesh::outputStep(k)),
				mem_std::toString("... same vertexes at ", k));
	}

	// The same contour STROKED. A stroke is a ribbon `StrokeWriter` builds by offsetting each
	// point along its normal, and that arithmetic happens in the coordinates it was handed - so
	// this asks whether the ribbon, too, survives being moved. The inner corners of a bowtie make
	// the ribbon cross itself, which is the case the sweep has to resolve.
	auto s0 = bowtie(0.0f);
	auto strokeBase = run(4.0f, 0.0f, 0.0f, s0);
	check(strokeBase.prepared && strokeBase.triangles() > 0,
			mem_std::toString("the same contour strokes at the origin: ", strokeBase.triangles(),
					" triangles"));
	if (!strokeBase.prepared) {
		return;
	}

	for (float k : {1'000.0f, 20'000.0f, 100'000.0f, 1'000'000.0f}) {
		auto sk = bowtie(k);
		auto moved = run(4.0f, 0.0f, 0.0f, sk);
		check(moved.prepared && moved.triangles() == strokeBase.triangles(),
				mem_std::toString("... stroke unchanged at ", k, ": ", strokeBase.triangles(),
						" vs ", moved.triangles()));
		check(moved.matches(strokeBase, Mesh::outputStep(k)),
				mem_std::toString("... stroke vertexes at ", k));
	}
}

/* And the same for the real wire, which is a CURVE - so this one also puts the FLATTENING on trial.

`LineDrawer` subdivides the bezier before any vertex exists, so if it subdivided in the coordinates
it was handed, each offset would produce a slightly different polyline and this could only ask that
the result be non-empty. It takes its own frame instead, so the polyline is the same one at every
offset and the triangle count can be demanded exactly. */
static void checkWireMagnitude() {
	const Vec2 span(s_editorWire.b.x - s_editorWire.a.x, s_editorWire.b.y - s_editorWire.a.y);
	const Vec2 c1(s_editorWire.c1.x - s_editorWire.a.x, s_editorWire.c1.y - s_editorWire.a.y);
	const Vec2 c2(s_editorWire.c2.x - s_editorWire.a.x, s_editorWire.c2.y - s_editorWire.a.y);

	auto at = [&](float k) {
		return [&, k](LineDrawer &line) {
			line.drawBegin(k, k);
			line.drawCubicBezier(k + c1.x, k + c1.y, k + c2.x, k + c2.y, k + span.x, k + span.y);
		};
	};

	auto a0 = at(0.0f);
	auto base = run(2.0f, 25.0f, 25.0f, a0);
	check(base.prepared && base.triangles() > 0,
			mem_std::toString("the editor's wire tesselates at the origin: ", base.triangles(),
					" triangles"));

	for (float k : {1.0e3f, 1.0e4f, 2.0e4f, 5.0e4f, 1.0e5f, 1.0e6f}) {
		auto ak = at(k);
		auto moved = run(2.0f, 25.0f, 25.0f, ak);
		check(moved.prepared && moved.triangles() == base.triangles(),
				mem_std::toString("... and at ", k, ": ", base.triangles(), " vs ",
						moved.triangles()));
	}
}

} // namespace

void performVgTessFrameTests() {
	checkFringeIsTranslationInvariant();
	checkCentredWire();
	checkSweepIsTranslationInvariant();
	checkWireMagnitude();
	checkGrid();
}

} // namespace STAPPLER_VERSIONIZED stappler
