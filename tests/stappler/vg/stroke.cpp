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

// Stroke geometry, asked of the tesselator directly.
//
// The renderer is the only thing that drives LineDrawer in production, and it needs a GPU, a
// surface and a frame graph to say anything at all. The geometry does not: LineDrawer writes
// into a Tesselator, and Tesselator hands its result back through two plain callbacks. So the
// questions that actually matter for dashes - how many separate ribbons came out, how much area
// they cover, how far the caps stick past the end of the line - can be asked from a CLI test
// that runs anywhere the module builds.
//
// Area and component count are used rather than vertex positions on purpose: the flattening of
// a curve, the miter clamp and the antialias pass are all free to move vertices around, but a
// dash pattern of `{10,10}` over a length of 100 covers 5 * 10 * width no matter how the
// tesselator got there.

#include "SPCommon.h"
#include "SPTessLine.h"
#include "SPVectorImage.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::geom;
using sprt::geom::Rect;

// Collects what Tesselator::write() emits, and derives the few scalars the checks below ask for.
struct StrokeResult {
	mem_std::Vector<Vec2> vertexes;
	mem_std::Vector<uint32_t> indexes;

	static void onVertex(void *target, uint32_t idx, const Vec2 &pt, float, const Vec2 &) {
		auto self = static_cast<StrokeResult *>(target);
		if (idx >= self->vertexes.size()) {
			self->vertexes.resize(idx + 1);
		}
		self->vertexes[idx] = pt;
	}

	static void onTriangle(void *target, uint32_t vert[3]) {
		auto self = static_cast<StrokeResult *>(target);
		self->indexes.emplace_back(vert[0]);
		self->indexes.emplace_back(vert[1]);
		self->indexes.emplace_back(vert[2]);
	}

	size_t triangles() const { return indexes.size() / 3; }

	float area() const {
		float ret = 0.0f;
		for (size_t i = 0; i < indexes.size(); i += 3) {
			auto &a = vertexes[indexes[i]];
			auto &b = vertexes[indexes[i + 1]];
			auto &c = vertexes[indexes[i + 2]];
			ret += sprt::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
		}
		return ret;
	}

	// Number of connected components over the triangle adjacency graph. Each dash is a ribbon
	// of its own, sharing no vertex with any other, so this counts dashes.
	//
	// Vertices are merged by position first: the tesselator can emit the same point as two
	// indexes (that is what the boundary/split machinery does), which would otherwise split one
	// ribbon into several components.
	uint32_t components() const {
		if (indexes.empty()) {
			return 0;
		}

		mem_std::Vector<uint32_t> parent(vertexes.size());
		for (uint32_t i = 0; i < parent.size(); ++i) { parent[i] = i; }

		auto find = [&](uint32_t v) {
			while (parent[v] != v) {
				parent[v] = parent[parent[v]];
				v = parent[v];
			}
			return v;
		};
		auto unite = [&](uint32_t a, uint32_t b) {
			a = find(a);
			b = find(b);
			if (a != b) {
				parent[b] = a;
			}
		};

		for (uint32_t i = 0; i < vertexes.size(); ++i) {
			for (uint32_t j = i + 1; j < vertexes.size(); ++j) {
				if (vertexes[i].fuzzyEquals(vertexes[j], 0.001f)) {
					unite(i, j);
				}
			}
		}

		for (size_t i = 0; i < indexes.size(); i += 3) {
			unite(indexes[i], indexes[i + 1]);
			unite(indexes[i], indexes[i + 2]);
		}

		mem_std::Vector<uint32_t> roots;
		for (size_t i = 0; i < indexes.size(); i += 3) {
			auto r = find(indexes[i]);
			bool known = false;
			for (auto &it : roots) {
				if (it == r) {
					known = true;
					break;
				}
			}
			if (!known) {
				roots.emplace_back(r);
			}
		}
		return uint32_t(roots.size());
	}

	Rect bbox() const {
		if (vertexes.empty()) {
			return Rect::ZERO;
		}

		// Only vertices that a triangle actually references: `write` may allocate a slot it
		// then leaves at the origin, and that would drag the box to (0,0).
		float minX = maxOf<float>(), minY = maxOf<float>();
		float maxX = -maxOf<float>(), maxY = -maxOf<float>();
		for (auto &idx : indexes) {
			auto &v = vertexes[idx];
			minX = sprt::min(minX, v.x);
			minY = sprt::min(minY, v.y);
			maxX = sprt::max(maxX, v.x);
			maxY = sprt::max(maxY, v.y);
		}
		return Rect(minX, minY, maxX - minX, maxY - minY);
	}
};

// Runs one path through the stroke half of LineDrawer. Antialiasing is deliberately left off:
// setBoundariesTransform adds a subpixel fringe whose vertex count depends on the relocation
// rule, which would make every area here approximate for no gain.
static StrokeResult strokePath(const StrokeConfig &cfg,
		const Callback<void(LineDrawer &)> &drawer) {
	StrokeResult ret;

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::perform([&] {
		auto tess = Rc<Tesselator>::create(pool);

		do {
			LineDrawer line(1.0f, nullptr, Rc<Tesselator>(tess), nullptr, cfg);
			drawer(line);
			line.drawClose(false);
		} while (0);

		tess->setWindingRule(Winding::NonZero);

		TessResult result;
		result.target = &ret;
		result.pushVertex = &StrokeResult::onVertex;
		result.pushTriangle = &StrokeResult::onTriangle;

		if (tess->prepare(result)) {
			ret.vertexes.resize(result.nvertexes);
			tess->write(result);
		}
	}, pool);
	memory::pool::destroy(pool);

	return ret;
}

// Same as strokePath, but with a fill tesselator attached as well - the stroke result is still
// what comes back. Filling changes how LineDrawer closes its contours, and that used to leak
// into the stroke.
static StrokeResult strokeFilledPath(const StrokeConfig &cfg,
		const Callback<void(LineDrawer &)> &drawer) {
	StrokeResult ret;

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::perform([&] {
		auto fill = Rc<Tesselator>::create(pool);
		auto stroke = Rc<Tesselator>::create(pool);

		do {
			LineDrawer line(1.0f, Rc<Tesselator>(fill), Rc<Tesselator>(stroke), nullptr, cfg);
			drawer(line);
			line.drawClose(false);
		} while (0);

		stroke->setWindingRule(Winding::NonZero);

		TessResult result;
		result.target = &ret;
		result.pushVertex = &StrokeResult::onVertex;
		result.pushTriangle = &StrokeResult::onTriangle;

		// Only the stroke is written out: `prepare` on the fill would renumber the vertexes the
		// stroke then writes against, so it is left alone entirely.
		if (stroke->prepare(result)) {
			ret.vertexes.resize(result.nvertexes);
			stroke->write(result);
		}
	}, pool);
	memory::pool::destroy(pool);

	return ret;
}

static StrokeResult strokeLine(const StrokeConfig &cfg, float length = 100.0f) {
	return strokePath(cfg, [&](LineDrawer &line) {
		line.drawBegin(0.0f, 0.0f);
		line.drawLine(length, 0.0f);
	});
}

static bool near(float value, float expected, float tolerance) {
	return sprt::abs(value - expected) <= tolerance;
}

static void checkSolid() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;

	auto res = strokeLine(cfg);

	check(res.components() == 1, "solid stroke is a single ribbon");
	check(near(res.area(), 400.0f, 1.0f), "solid stroke area is length * width");

	auto bbox = res.bbox();
	check(near(bbox.size.height, 4.0f, 0.01f), "solid stroke is as tall as it is wide");
	check(near(bbox.size.width, 100.0f, 0.01f), "butt cap does not extend the line");
}

// Caps are what makes a dotted line possible at all: a zero-length dash has no ribbon, so with
// a butt cap there is nothing to draw and the whole pattern renders as blank.
static void checkCaps() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;

	cfg.lineCup = LineCup::Square;
	auto square = strokeLine(cfg);
	check(near(square.bbox().size.width, 104.0f, 0.01f),
			"square cap extends the line by half a width at each end");
	check(near(square.area(), 400.0f + 16.0f, 1.0f), "square cap adds a half-width block per end");

	cfg.lineCup = LineCup::Round;
	auto round = strokeLine(cfg);
	const float discArea = float(M_PI) * 2.0f * 2.0f;
	check(near(round.bbox().size.width, 104.0f, 0.05f), "round cap reaches half a width past the end");
	check(round.area() < 400.0f + discArea && round.area() > 400.0f + discArea * 0.9f,
			"round cap adds a disc per end, minus the chord error of the polygon");
	check(round.components() == 1, "round caps merge into the ribbon rather than floating free");

	// A dot is a dash of zero length: no ribbon at all, only whatever the cap contributes.
	auto dot = strokePath(cfg, [](LineDrawer &line) { line.drawBegin(10.0f, 10.0f); });
	check(dot.triangles() > 0, "a round cap makes a zero-length dash visible");
	check(near(dot.area(), discArea, discArea * 0.1f), "a round dot is a disc of the stroke width");

	cfg.lineCup = LineCup::Butt;
	auto buttDot = strokePath(cfg, [](LineDrawer &line) { line.drawBegin(10.0f, 10.0f); });
	check(buttDot.triangles() == 0, "a zero-length dash with a butt cap draws nothing");
}

// The point of the whole exercise: a pattern cuts one ribbon into many, and the count and the
// covered area are what say whether it cut them in the right places.
static void checkDashes() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;

	float evenPattern[] = {10.0f, 10.0f};
	cfg.dashArray = SpanView<float>(evenPattern, 2);

	auto dashed = strokeLine(cfg);
	check(dashed.components() == 5, "a 10/10 pattern over a length of 100 draws five dashes");
	check(near(dashed.area(), 200.0f, 1.0f), "dashes cover half the line");

	// Shifting by exactly one period has to land back on the same figure.
	cfg.dashOffset = 20.0f;
	auto shifted = strokeLine(cfg);
	check(shifted.components() == dashed.components()
					&& near(shifted.area(), dashed.area(), 0.1f),
			"an offset of one full period reproduces the pattern");

	// Half a period swaps dashes for gaps. With this pattern the shift lands exactly on the
	// existing boundaries, so the count is unchanged and only the position moves.
	cfg.dashOffset = 10.0f;
	auto inverted = strokeLine(cfg);
	check(near(inverted.area(), 200.0f, 1.0f), "a half-period offset still covers half the line");
	check(inverted.components() == 5, "a half-period offset shifts the dashes without adding any");
	check(inverted.bbox().origin.x > dashed.bbox().origin.x,
			"a half-period offset moves the first dash off the start of the line");

	cfg.dashOffset = 0.0f;

	// SVG repeats an odd-length pattern, so {5} means {5,5}.
	float oddPattern[] = {5.0f};
	float evenEquivalent[] = {5.0f, 5.0f};
	cfg.dashArray = SpanView<float>(oddPattern, 1);
	auto odd = strokeLine(cfg);
	cfg.dashArray = SpanView<float>(evenEquivalent, 2);
	auto even = strokeLine(cfg);
	check(odd.components() == even.components() && near(odd.area(), even.area(), 0.1f),
			"an odd-length pattern repeats to an even one");

	// A pattern longer than the line leaves it solid, not blank.
	float longPattern[] = {1000.0f, 1000.0f};
	cfg.dashArray = SpanView<float>(longPattern, 2);
	auto single = strokeLine(cfg);
	check(single.components() == 1 && near(single.area(), 400.0f, 1.0f),
			"a dash longer than the line covers all of it");

	// Dotted: zero-length dashes are visible only through the cap.
	float dotPattern[] = {0.0f, 10.0f};
	cfg.dashArray = SpanView<float>(dotPattern, 2);
	cfg.lineCup = LineCup::Round;
	auto dotted = strokeLine(cfg);
	check(dotted.components() >= 10, "a dotted pattern draws one dot per period");
	const float discArea = float(M_PI) * 2.0f * 2.0f;
	const float dots = float(dotted.components());
	check(dotted.area() < discArea * dots && dotted.area() > discArea * dots * 0.9f,
			"each dot is a disc of the stroke width, minus the chord error of the polygon");

	cfg.lineCup = LineCup::Butt;
	check(strokeLine(cfg).triangles() == 0, "a dotted pattern with a butt cap draws nothing");
}

// A closed contour must not show a seam at the point the path happens to start from.
static void checkClosedContour() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;

	auto rect = [](LineDrawer &line) {
		line.drawBegin(0.0f, 0.0f);
		line.drawLine(100.0f, 0.0f);
		line.drawLine(100.0f, 100.0f);
		line.drawLine(0.0f, 100.0f);
		line.drawClose(true);
	};

	// A pattern that lines up with the corners: one dash per side.
	float sidePattern[] = {50.0f, 50.0f};
	cfg.dashArray = SpanView<float>(sidePattern, 2);
	auto dashed = strokePath(cfg, rect);
	check(dashed.components() == 4, "a 50/50 pattern around a 100x100 box draws four dashes");

	// Longer than the perimeter: the contour stays one closed ribbon, exactly as an undashed
	// stroke would be - no caps, so the corners keep their joins.
	float wholePattern[] = {1000.0f, 1000.0f};
	cfg.dashArray = SpanView<float>(wholePattern, 2);
	auto whole = strokePath(cfg, rect);

	cfg.dashArray = SpanView<float>();
	auto solid = strokePath(cfg, rect);

	check(whole.components() == 1, "a dash longer than the perimeter stays a single ribbon");
	check(near(whole.area(), solid.area(), 1.0f),
			"a dash longer than the perimeter matches the solid stroke");
}

// An open subpath is stroked open, whatever it is filled with. A fill has to close its contour
// to have an area at all, and that closing used to be handed to the stroke as well - which drew
// a segment the path never contained, straight across from the last point back to the moveTo.
static void checkOpenSubpathWithFill() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;

	// Two sides of a triangle, deliberately left open: the chord would be the third one.
	auto openV = [](LineDrawer &line) {
		line.drawBegin(0.0f, 0.0f);
		line.drawLine(50.0f, 50.0f);
		line.drawLine(100.0f, 0.0f);
	};

	auto strokeOnly = strokePath(cfg, openV);
	auto withFill = strokeFilledPath(cfg, openV);

	// Each side is 50*sqrt(2); the chord that must NOT appear would add another 100 * 4.
	const float sides = 2.0f * 50.0f * float(M_SQRT2) * 4.0f;
	check(near(strokeOnly.area(), sides, sides * 0.05f),
			"an open subpath strokes its two sides");
	check(near(withFill.area(), strokeOnly.area(), 1.0f),
			"filling an open subpath does not add a chord to its stroke");
	check(withFill.bbox().size.height > 40.0f,
			"the stroke of an open subpath still covers both sides");
}

// A dash pattern that is not really a pattern must render exactly like a solid stroke - this is
// the fallback every degenerate input funnels into.
static void checkSolidFallback() {
	StrokeConfig cfg;
	cfg.lineWidth = 4.0f;
	auto expected = strokeLine(cfg).area();

	float allZero[] = {0.0f, 0.0f, 0.0f};
	cfg.dashArray = SpanView<float>(allZero, 3);
	check(near(strokeLine(cfg).area(), expected, 1.0f), "all-zero dash pattern falls back to solid");

	// A negative entry invalidates the whole list, so this one is checked at the DashPattern
	// level - StrokeConfig itself takes whatever it is handed.
	vg::DashPattern pattern;
	float negative[] = {-1.0f, 2.0f};
	check(!pattern.set(SpanView<float>(negative, 2)), "negative dash length is rejected");
	check(pattern.isSolid(), "a rejected pattern leaves the stroke solid");

	float valid[] = {4.0f, 2.0f};
	check(pattern.set(SpanView<float>(valid, 2)), "a valid pattern is accepted");
	check(!pattern.isSolid() && pattern.count == 2, "an accepted pattern is stored");
	check(near(pattern.getPeriod(), 6.0f, 0.001f), "period is the sum of the pattern");

	float tooLong[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	check(pattern.set(SpanView<float>(tooLong, 10)), "an over-long pattern is accepted");
	check(pattern.count == vg::DashPattern::MaxCount, "an over-long pattern is truncated");
}

static void checkSvgParsing() {
	auto image = Rc<vg::VectorImage>::create(StringView(R"(<svg width="100" height="100">
		<path d="M0,0 L100,0" stroke="#000" stroke-dasharray="4 2" stroke-dashoffset="1"/>
		<path d="M0,10 L100,10" stroke="#000" style="stroke-dasharray:8,3;stroke-dashoffset:2"/>
		<path d="M0,20 L100,20" stroke="#000" stroke-dasharray="none"/>
		<path d="M0,30 L100,30" stroke="#000" stroke-dasharray="4 -2"/>
	</svg>)"));

	if (!image) {
		check(false, "SVG with dash attributes parses");
		return;
	}

	mem_std::Vector<vg::PathParams> params;
	for (auto &it : image->getPaths()) { params.emplace_back(it.second->getPath()->getParams()); }

	check(params.size() == 4, "every path in the document is read");
	if (params.size() != 4) {
		return;
	}

	// getPaths() is a map keyed by generated id, so the order is not the document order - find
	// each case by what makes it distinctive instead.
	uint32_t attrCount = 0, styleCount = 0, solidCount = 0;
	for (auto &it : params) {
		if (it.dash.count == 2 && near(it.dash.lengths[0], 4.0f, 0.001f)
				&& near(it.dash.offset, 1.0f, 0.001f)) {
			++attrCount;
		} else if (it.dash.count == 2 && near(it.dash.lengths[0], 8.0f, 0.001f)
				&& near(it.dash.offset, 2.0f, 0.001f)) {
			++styleCount;
		} else if (it.dash.isSolid()) {
			++solidCount;
		}
	}

	check(attrCount == 1, "stroke-dasharray as a presentation attribute is read");
	check(styleCount == 1, "stroke-dasharray inside style=\"\" is read");
	check(solidCount == 2, "\"none\" and a negative length both leave the stroke solid");
}

} // namespace

void performVgStrokeTests() {
	checkSolid();
	checkCaps();
	checkDashes();
	checkClosedContour();
	checkOpenSubpathWithFill();
	checkSolidFallback();
	checkSvgParsing();
}

} // namespace STAPPLER_VERSIONIZED stappler
