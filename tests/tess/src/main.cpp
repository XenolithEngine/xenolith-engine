/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

/* A BENCH FOR THE TESSELATOR, and a regression net for it.

The tesselator has no test of its own that exercises real content. `tests/stappler`'s `vg-tess-frame`
states invariants about hand-made shapes - the right thing for the precision work it guards - but
nothing there says "the four thousand icons the engine ships still come out the same". That is what
this is: a fixed, large, real corpus, run through the tesselator on a CLI thread with no device, no
window and no frame, and reduced to something a golden file can hold.

    tesstest bench                  time the whole corpus, both variants, and break it down
    tesstest golden                 compare against tests/tess/golden/icons.txt
    tesstest golden --write         rewrite that file (do this ONLY when the change is intended)
    tesstest one <icon> [--aa]      one icon, in full

TWO VARIANTS, and the second is not a formality. `fill` runs the sweep and the triangulation. `fill
+aa` additionally runs `computeBoundary` and the relocation rule - a completely separate half of the
tesselator, the half that displaces boundary vertexes, and the one that produced the failures the
graph editor hit. A corpus that only ever asked for fills would call that half untested.

WHAT THE GOLDEN HOLDS is deliberately not vertexes: see PathDigest. A tesselator may pick a
different diagonal and still be right, and a file made of positions would go red on every such
change while saying nothing about whether the picture moved. */

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPTess.h"
#include "TessBench.h"
#include "TessRaster.h"

#include <sprt/runtime/platform.h>


namespace STAPPLER_VERSIONIZED stappler {

using namespace stappler::tessbench;

static constexpr float GoldenEpsilon = 0.002f;

// The icons are authored in a 24-unit box; 64 gives the antialias fringe several pixels to live in
// without making a per-icon golden raster large.
static constexpr uint32_t RasterExtent = 64;

static mem_std::String getGoldenPath() {
	return mem_std::toString(filesystem::currentDir<mem_std::Interface>("golden/icons.txt"));
}

// ---- the corpus ---------------------------------------------------------------------------------

struct CorpusRun {
	uint32_t icons = 0;
	uint32_t paths = 0;
	uint32_t failed = 0;
	uint64_t microseconds = 0;
	uint64_t vertexes = 0;
	uint64_t triangles = 0;
};

static CorpusRun runCorpus(bool antialias,
		const Callback<void(StringView, const IconResult &)> &each = nullptr) {
	CorpusRun run;
	forEachIcon([&](StringView name) {
		auto r = tessellateIcon(name, antialias);
		++run.icons;
		run.paths += r.paths;
		run.microseconds += r.microseconds;
		if (r.digest.failed) {
			++run.failed;
		} else {
			run.vertexes += r.digest.vertexes;
			run.triangles += r.digest.triangles;
		}
		if (each) {
			each(name, r);
		}
	});
	return run;
}

static int doBench() {
	for (auto antialias : {false, true}) {
		auto run = runCorpus(antialias);

		sprt::cout << "\n== " << (antialias ? "заливка + кайма" : "заливка") << " ==\n";
		sprt::cout << "  иконок " << run.icons << ", путей " << run.paths << ", отказов "
				   << run.failed << "\n";
		sprt::cout << "  вершин " << run.vertexes << ", треугольников " << run.triangles << "\n";
		sprt::cout << "  всего " << run.microseconds << " мкс, на иконку "
				   << (run.icons ? run.microseconds / run.icons : 0) << " мкс\n";
	}
	return 0;
}

// ---- the golden ---------------------------------------------------------------------------------

static mem_std::String buildGolden() {
	mem_std::StringStream out;
	out << "# tesstest golden: <icon> <variant> <vertexes> <triangles> <area> <minX> <minY> "
		   "<maxX> <maxY>\n";
	out << "# rebuild with: tesstest golden --write\n";
	for (auto antialias : {false, true}) {
		runCorpus(antialias, [&](StringView name, const IconResult &r) {
			out << name << " " << (antialias ? "aa" : "fill") << " " << r.digest.encode() << "\n";
		});
	}
	return out.str();
}

static int doGolden(bool write) {
	auto path = getGoldenPath();

	if (write) {
		auto data = buildGolden();
		if (!filesystem::write(FileInfo{path},
					BytesView((const uint8_t *)data.data(), data.size()))) {
			sprt::cout << "не удалось записать " << path << "\n";
			return 1;
		}
		sprt::cout << "записано: " << path << "\n";
		return 0;
	}

	auto stored = filesystem::readTextFile<mem_std::Interface>(FileInfo{path});
	if (stored.empty()) {
		sprt::cout << "нет эталона: " << path << "\n  создайте его: tesstest golden --write\n";
		return 1;
	}

	// name+variant -> digest. Read whole, then compare: a run is four thousand icons twice, and
	// walking the file in step with the corpus would tie the two orders together forever.
	mem_std::Map<mem_std::String, PathDigest> expected;
	StringView r(stored);
	while (!r.empty()) {
		auto line = r.readUntil<StringView::Chars<'\n'>>();
		r.skipChars<StringView::Chars<'\n'>>();
		line.skipChars<StringView::WhiteSpace>();
		if (line.empty() || line.is('#')) {
			continue;
		}
		auto name = line.readUntil<StringView::WhiteSpace>();
		line.skipChars<StringView::WhiteSpace>();
		auto variant = line.readUntil<StringView::WhiteSpace>();
		PathDigest d;
		PathDigest::decode(line, d);
		expected.emplace(mem_std::toString(name, " ", variant), d);
	}

	uint32_t checked = 0, shapeDiff = 0, countDiff = 0, missing = 0;
	for (auto antialias : {false, true}) {
		runCorpus(antialias, [&](StringView name, const IconResult &res) {
			auto key = mem_std::toString(name, " ", antialias ? "aa" : "fill");
			auto it = expected.find(key);
			if (it == expected.end()) {
				++missing;
				return;
			}
			++checked;
			if (!res.digest.sameShape(it->second, GoldenEpsilon)) {
				++shapeDiff;
				if (shapeDiff <= 20) {
					sprt::cout << "  ФОРМА  " << key << "\n    было:  " << it->second.encode()
							   << "\n    стало: " << res.digest.encode() << "\n";
				}
			} else if (!res.digest.sameCounts(it->second)) {
				// The picture is the same and the mesh is not. Not a failure - a tesselation
				// change - but it is exactly what a performance change looks like, so it is
				// reported separately rather than passed over in silence.
				++countDiff;
				if (countDiff <= 20) {
					sprt::cout << "  сетка  " << key << ": " << it->second.vertexes << "/"
							   << it->second.triangles << " -> " << res.digest.vertexes << "/"
							   << res.digest.triangles << "\n";
				}
			}
		});
	}

	sprt::cout << "\nсверено " << checked << ", форма разошлась " << shapeDiff
			   << ", сетка изменилась " << countDiff << ", нет в эталоне " << missing << "\n";
	return shapeDiff > 0 ? 1 : 0;
}

// ---- the raster golden ---------------------------------------------------------------------------

static mem_std::String getRasterGoldenPath() {
	return mem_std::toString(filesystem::currentDir<mem_std::Interface>("golden/raster.txt"));
}

/* Signature bits that may differ before the picture is called moved.
 
Zero would be the honest number and is the wrong one in practice: the hash thresholds each cell
against the image's own mean, so a cell sitting exactly on the mean can flip on a change of one
intensity level anywhere in the icon. One bit of slack absorbs that; two would start hiding a
genuinely shifted stroke. */
static constexpr uint32_t RasterSignatureSlack = 1;

// Coverage is a sum over thousands of pixels, so it moves by a few counts on any change to the
// fringe. A part in five hundred is below what a reviewer would call a different icon.
static constexpr double RasterCoverageTolerance = 0.002;

static int doRasterGolden(bool write) {
	if (!rasterInit(Extent2(RasterExtent, RasterExtent))) {
		sprt::cout << "растеризатор не поднялся\n";
		return 1;
	}

	auto path = getRasterGoldenPath();
	mem_std::Map<mem_std::String, RasterImage::Digest> expected;

	if (!write) {
		auto stored = filesystem::readTextFile<mem_std::Interface>(FileInfo{path});
		if (stored.empty()) {
			sprt::cout << "нет растрового эталона: " << path
					   << "\n  создайте его: tesstest raster-golden --write\n";
			rasterFinalize();
			return 1;
		}
		StringView r(stored);
		while (!r.empty()) {
			auto line = r.readUntil<StringView::Chars<'\n'>>();
			r.skipChars<StringView::Chars<'\n'>>();
			line.skipChars<StringView::WhiteSpace>();
			if (line.empty() || line.is('#')) {
				continue;
			}
			auto name = line.readUntil<StringView::WhiteSpace>();
			line.skipChars<StringView::WhiteSpace>();
			auto variant = line.readUntil<StringView::WhiteSpace>();
			RasterImage::Digest d;
			RasterImage::Digest::decode(line, d);
			expected.emplace(mem_std::toString(name, " ", variant), d);
		}
	}

	mem_std::StringStream out;
	out << "# tesstest raster golden: <icon> <variant> <nonZero> <coverage> <sigHi> <sigLo>\n";
	out << "# rendered through the flat queue at " << RasterExtent << "x" << RasterExtent
		<< ", white on black - see TessRaster.h\n";
	out << "# rebuild with: tesstest raster-golden --write\n";

	uint32_t rendered = 0, empty = 0, moved = 0, drifted = 0, missing = 0;
	for (auto antialias : {false, true}) {
		forEachIcon([&](StringView name) {
			auto img = rasterIcon(name, antialias);
			auto key = mem_std::toString(name, " ", antialias ? "aa" : "fill");
			if (img.empty()) {
				++empty;
				if (write) {
					out << key << " EMPTY\n";
				}
				return;
			}
			++rendered;
			auto d = img.digest();
			if (write) {
				out << key << " " << d.encode() << "\n";
				return;
			}

			auto it = expected.find(key);
			if (it == expected.end()) {
				++missing;
				return;
			}
			const auto dist = d.distance(it->second);
			if (dist > RasterSignatureSlack) {
				++moved;
				if (moved <= 20) {
					sprt::cout << "  КАРТИНКА  " << key << ": бит расходится " << dist << "\n";
					// The image itself, because a hash distance is not something anybody can look
					// at and decide about.
					img.writePng(
							mem_std::toString(name, "-", antialias ? "aa" : "fill", "-actual.png"));
				}
			} else {
				const auto e = double(it->second.coverage);
				if (sprt::abs(double(d.coverage) - e) > e * RasterCoverageTolerance + 1.0) {
					++drifted;
					if (drifted <= 20) {
						sprt::cout << "  покрытие  " << key << ": " << it->second.coverage << " -> "
								   << d.coverage << "\n";
					}
				}
			}
		});
	}

	rasterFinalize();

	if (write) {
		auto data = out.str();
		if (!filesystem::write(FileInfo{path},
					BytesView((const uint8_t *)data.data(), data.size()))) {
			sprt::cout << "не удалось записать " << path << "\n";
			return 1;
		}
		sprt::cout << "записано: " << path << " (отрисовано " << rendered << ", пусто " << empty
				   << ")\n";
		return 0;
	}

	sprt::cout << "\nотрисовано " << rendered << ", пусто " << empty << ", картинка сдвинулась "
			   << moved << ", покрытие уплыло " << drifted << ", нет в эталоне " << missing << "\n";
	return moved > 0 ? 1 : 0;
}

// ---- against the icons Google ships ---------------------------------------------------------------

/* A DIFFERENT question from the golden, and worth keeping separate.

The golden asks "did we change". This asks "were we ever right" - the same icon, rendered by
somebody else's pipeline, shipped alongside the SVG our own generator reads. A regression net cannot
see an error that was always there; this can.

It cannot be an exact comparison and is not meant to be. Their rasterizer is not ours, their
antialiasing is not ours, and their PNG is black-on-transparent at 24dp while ours is white-on-black
at whatever size was asked for. What survives all of that is the SHAPE, so the comparison is the
same 8x8 perceptual hash the raster golden uses: insensitive to a pixel of fringe, and it moves the
moment an icon is missing a stroke, mirrored, or shifted.

The mapping is the generator's own naming read backwards: `Action_3d_rotation_solid` is category
`action`, icon `3d_rotation`, and `_solid` means their `materialicons` directory while `_outline`
means `materialiconsoutlined`. */
static constexpr uint32_t ReferenceExtent = 24;

// Their coverage is the alpha channel: the icon is black on transparent, so alpha IS the shape.
static bool loadReference(StringView root, StringView iconName, RasterImage &out) {
	auto underscore = iconName.rfind('_');
	if (underscore == maxOf<size_t>()) {
		return false;
	}
	auto variant = iconName.sub(underscore + 1);
	auto rest = iconName.sub(0, underscore);
	auto dot = rest.find('_');
	if (dot == maxOf<size_t>()) {
		return false;
	}
	auto category = rest.sub(0, dot);
	auto name = rest.sub(dot + 1);

	mem_std::String lowerCategory;
	for (auto c : category) { lowerCategory.push_back(::tolower(c)); }

	StringView dir;
	if (variant == "solid") {
		dir = StringView("materialicons");
	} else if (variant == "outline") {
		dir = StringView("materialiconsoutlined");
	} else {
		return false;
	}

	auto path = mem_std::toString(root, "/", lowerCategory, "/", name, "/", dir,
			"/24dp/1x/baseline_", name, "_black_24dp.png");
	if (!RasterImage::readPng(path, out)) {
		return false;
	}
	// Alpha to intensity, so both sides speak the same language before the hash sees them.
	for (size_t i = 0; i + 3 < out.data.size(); i += 4) {
		const auto a = out.data[i + 3];
		out.data[i] = out.data[i + 1] = out.data[i + 2] = a;
		out.data[i + 3] = 255;
	}
	return true;
}

/* Icons this far from their reference are DUMPED, not just counted.

Eight bits of a sixty-four-bit hash is where the two pictures stop being the same picture with
different antialiasing. A number at that point is useless on its own - nobody can look at "eleven
bits" and decide anything - so the bench writes the pair out and lets a person look. One directory
per icon, both images rendered the same way (white on black coverage) so they can be flipped
between, plus their difference, which is where the eye goes first. */
static constexpr uint32_t ReferenceDumpThreshold = 8;

static constexpr auto ReferenceDumpDir = "reference-diff";

static bool dumpReferencePair(StringView name, const RasterImage &fill, const RasterImage &aa,
		const RasterImage &ref, uint32_t distFill, uint32_t distAa) {
	auto base = filesystem::currentDir<mem_std::Interface>(ReferenceDumpDir);
	auto dir = mem_std::toString(base, "/", name);
	filesystem::mkdir(FileInfo{base});
	if (!filesystem::mkdir(FileInfo{dir})) {
		return false;
	}

	/* BOTH of our variants, beside theirs.
	
	One image cannot answer the question the directory exists to answer. Our antialias fringe is an
	OFFSET - it displaces the boundary outward - so at 24 units across it visibly fattens a shape
	and closes thin gaps, and an icon that differs only because of that is a different problem from
	one whose geometry is wrong. With both variants in the directory the distinction is visible at
	a glance: if `ours-fill` matches and `ours-aa` does not, it is the fringe; if neither does, it
	is the shape. */
	fill.writePng(mem_std::toString(dir, "/ours-fill.png"));
	aa.writePng(mem_std::toString(dir, "/ours-aa.png"));
	ref.writePng(mem_std::toString(dir, "/google.png"));

	const auto writeDiff = [&](const RasterImage &ours, StringView suffix) {
		if (ours.width != ref.width || ours.height != ref.height
				|| ours.data.size() != ref.data.size()) {
			return;
		}
		RasterImage diff;
		diff.width = ours.width;
		diff.height = ours.height;
		diff.data.resize(ours.data.size());
		for (size_t i = 0; i + 3 < ours.data.size(); i += 4) {
			const auto d = uint8_t(sprt::abs(int32_t(ours.data[i]) - int32_t(ref.data[i])));
			diff.data[i] = diff.data[i + 1] = diff.data[i + 2] = d;
			diff.data[i + 3] = 255;
		}
		diff.writePng(mem_std::toString(dir, "/diff-", suffix, ".png"));
	};
	writeDiff(fill, "fill");
	writeDiff(aa, "aa");

	// A line of text beside the images, because five PNGs do not say how far apart they were or
	// which way to read them.
	auto note = mem_std::toString("icon: ", name, "\n", "signature distance (fill): ", distFill,
			" bits of 64\n", "signature distance (aa):   ", distAa, " bits of 64\n\n",
			"ours-fill.png - this tesselator, no antialias fringe, white on black coverage\n",
			"ours-aa.png   - the same with the fringe the renderer asks for at scale 1\n",
			"google.png    - material-design-icons/png .../24dp/1x, alpha as coverage\n",
			"diff-fill.png - |ours-fill - google| per pixel\n",
			"diff-aa.png   - |ours-aa - google| per pixel\n");
	filesystem::write(FileInfo{mem_std::toString(dir, "/about.txt")},
			BytesView((const uint8_t *)note.data(), note.size()));
	return true;
}

static int doReference(StringView root) {
	if (root.empty()) {
		sprt::cout << "нужен путь: tesstest reference <material-design-icons>/png\n";
		return 1;
	}
	if (!rasterInit(Extent2(ReferenceExtent, ReferenceExtent))) {
		sprt::cout << "растеризатор не поднялся\n";
		return 1;
	}

	uint32_t compared = 0, noReference = 0, notDrawn = 0, dumped = 0;
	uint32_t histFill[65] = {};
	uint32_t histAa[65] = {};
	mem_std::Vector<mem_std::String> worst;

	forEachIcon([&](StringView name) {
		RasterImage ref;
		if (!loadReference(root, name, ref)) {
			++noReference;
			return;
		}

		// BOTH variants, every time. The pair is the measurement - see dumpReferencePair.
		auto fill = rasterIcon(name, false);
		auto aa = rasterIcon(name, true);
		if (fill.empty() && aa.empty()) {
			++notDrawn;
			return;
		}

		const auto refDigest = ref.digest();
		const auto dFill = fill.empty() ? 64u : fill.digest().distance(refDigest);
		const auto dAa = aa.empty() ? 64u : aa.digest().distance(refDigest);
		++compared;
		++histFill[sprt::min(dFill, uint32_t(64))];
		++histAa[sprt::min(dAa, uint32_t(64))];

		if (dFill >= ReferenceDumpThreshold || dAa >= ReferenceDumpThreshold) {
			++dumped;
			dumpReferencePair(name, fill, aa, ref, dFill, dAa);
			worst.emplace_back(mem_std::toString(name, " (заливка ", dFill, ", с каймой ", dAa,
					dFill < ReferenceDumpThreshold ? " - кайма" : " - форма", ")"));
		}
	});

	rasterFinalize();

	sprt::cout << "\nсверено с эталонами Google: " << compared << ", нет эталона " << noReference
			   << ", не отрисовалось " << notDrawn << "\n";
	sprt::cout << "расхождение подписи, бит -> иконок (заливка / с каймой):\n";
	for (uint32_t i = 0; i <= 64; ++i) {
		if (histFill[i] || histAa[i]) {
			sprt::cout << "  " << i << ": " << histFill[i] << " / " << histAa[i] << "\n";
		}
	}

	if (!worst.empty()) {
		sprt::cout << "\nвыложено " << dumped << " пар (>= " << ReferenceDumpThreshold
				   << " бит хотя бы в одном варианте) в " << ReferenceDumpDir << "/<иконка>/\n";
		uint32_t shown = 0;
		for (auto &it : worst) {
			if (shown++ >= 25) {
				break;
			}
			sprt::cout << "  " << it << "\n";
		}
	}
	return 0;
}

// ---- the case the graph editor makes ------------------------------------------------------------

/* A deterministic stand-in for a graph of wires, because the graph itself is not one.

The icons are the wrong corpus for the sweep: they are 24 units across with a dozen open edges at a
time, and the sweepline never holds more than about fourteen. The graph editor holds four hundred -
long, near-horizontal, overlapping strokes crossing the whole canvas - and that is the case where
the dictionary walk on every event is the cost.

Driving the editor to measure it does not work: its zoom, its level of detail and which frame gets
captured all move between runs, and the numbers move with them. This makes the same SHAPE of input
directly - wires laid across a wide box, each spanning most of it, so a great many are open at once -
and it makes it the same way every time.

    tesstest wires [count]
*/
/* The same wires, but shaped the way the EDITOR shapes them: one path, one tesselator, one contour
each, with the antialias fringe the editor asks for.

`doWires` below puts every wire into ONE tesselator, which is the sweep benchmark it was written to
be - four hundred overlapping ribbons is exactly the case the dictionary walk is measured on. It is
not the case the graph editor has: `EdEdgeLayer` creates a VectorPath per wire and the canvas gives
each path its own tesselator with a single open contour. Those two shapes answer different
questions, so this is a second mode rather than an edit to the first. */
static int doWiresPerPath(uint32_t count, bool bypass) {
	if (count == 0) {
		count = 400;
	}

	constexpr float Width = 6'000.0f;
	constexpr float Height = 2'000.0f;

	uint32_t vertexes = 0, triangles = 0, bypassed = 0;

	const auto start = sprt::platform::clock(sprt::platform::ClockType::Monotonic);

	for (uint32_t i = 0; i < count; ++i) {
		auto pool = memory::pool::create(memory::pool::acquire());
		memory::perform([&] {
			auto tess = Rc<geom::Tesselator>::create(pool);
			tess->setStrokeBypassEnabled(bypass);

			const float t0 = float((i * 7919u) % 1'000u) / 1'000.0f;
			const float t1 = float((i * 104'729u) % 1'000u) / 1'000.0f;
			const float ax = Width * 0.7f * t0;
			const float bx = ax + Width * 0.25f;
			const float ay = Height * t1;
			const float by = Height * float((i * 15'485'863u) % 1'000u) / 1'000.0f;
			const float spread = sprt::max(50.0f, sprt::abs(bx - ax) * 0.5f);

			do {
				geom::StrokeConfig cfg;
				cfg.lineWidth = 2.0f;
				geom::LineDrawer line(1.0f, nullptr, Rc<geom::Tesselator>(tess), nullptr, cfg);
				line.drawBegin(ax, ay);
				line.drawCubicBezier(ax + spread, ay, bx - spread, by, bx, by);
				line.drawClose(false);
			} while (0);

			tess->setWindingRule(geom::Winding::NonZero);

			// What the canvas does for an antialiased path: VGAntialiasFactor on both, and the
			// default relocation rule. The editor's wires are all antialiased, so a benchmark
			// without this would be measuring a case that does not occur.
			tess->setBoundariesTransform(0.5f, 0.5f);
			tess->setRelocateRule(geom::Tesselator::RelocateRule::Auto);

			struct Sink {
				uint32_t vertexes = 0;
				uint32_t triangles = 0;
				static void v(void *t, uint32_t, const Vec2 &, float, const Vec2 &) {
					++reinterpret_cast<Sink *>(t)->vertexes;
				}
				static void t3(void *t, uint32_t[3]) { ++reinterpret_cast<Sink *>(t)->triangles; }
			};

			Sink sink;
			geom::TessResult res;
			res.target = &sink;
			res.pushVertex = &Sink::v;
			res.pushTriangle = &Sink::t3;

			if (tess->prepare(res)) {
				tess->write(res);
			}

			vertexes += sink.vertexes;
			triangles += sink.triangles;
			bypassed += tess->getStrokeBypassCount();
		}, pool);
		memory::pool::destroy(pool);
	}

	const auto total = sprt::platform::clock(sprt::platform::ClockType::Monotonic) - start;

	sprt::cout << "проводов " << count << " (по тесселятору на путь, обход "
			   << (bypass ? "вкл" : "выкл") << "), всего " << total << " мкс\n";
	sprt::cout << "  вершин " << vertexes << ", треугольников " << triangles << ", обойдено "
			   << bypassed << " из " << count << "\n";
	return 0;
}

static int doWires(uint32_t count) {
	if (count == 0) {
		count = 400;
	}

	// The editor's own numbers: a wire is a cubic whose control points sit `|dx| * 0.5` out from
	// its ends (doc::computeEdgeCurve), stroked two units wide.
	constexpr float Width = 6'000.0f;
	constexpr float Height = 2'000.0f;

	const auto start = sprt::platform::clock(sprt::platform::ClockType::Monotonic);

	uint32_t vertexes = 0;
	uint32_t triangles = 0;

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::perform([&] {
		auto tess = Rc<geom::Tesselator>::create(pool);

		do {
			geom::StrokeConfig cfg;
			cfg.lineWidth = 2.0f;
			geom::LineDrawer line(1.0f, nullptr, Rc<geom::Tesselator>(tess), nullptr, cfg);

			for (uint32_t i = 0; i < count; ++i) {
				// Deterministic pseudo-scatter: a multiplicative step around the box, so the
				// wires overlap heavily without any two being identical and without a RNG.
				const float t0 = float((i * 7'919u) % 1'000u) / 1'000.0f;
				const float t1 = float((i * 104'729u) % 1'000u) / 1'000.0f;

				/* A wire spans a QUARTER of the box, not all of it.
				
				A full-width wire with the editor's own tension folds so far outside its own ends
				that the sweep drowns - four hundred of those do not finish in eight minutes, which
				is a finding about the editor's geometry rather than a benchmark. A quarter-width
				span keeps the shape (a forward cubic with horizontal control points) and keeps a
				few hundred edges open at any sweep position, which is the property being
				measured. */
				const float ax = Width * 0.7f * t0;
				const float bx = ax + Width * 0.25f;
				const float ay = Height * t1;
				const float by = Height * float((i * 15'485'863u) % 1'000u) / 1'000.0f;
				const float spread = sprt::max(50.0f, sprt::abs(bx - ax) * 0.5f);

				line.drawBegin(ax, ay);
				line.drawCubicBezier(ax + spread, ay, bx - spread, by, bx, by);
			}
			line.drawClose(false);
		} while (0);

		tess->setWindingRule(geom::Winding::NonZero);

		// The sink counts what came out: a run whose time changes while these do not is a run
		// that tesselated something else, and that is the only cross-check this bench has.
		struct Sink {
			uint32_t vertexes = 0;
			uint32_t triangles = 0;

			static void v(void *t, uint32_t, const Vec2 &, float, const Vec2 &) {
				++reinterpret_cast<Sink *>(t)->vertexes;
			}
			static void t3(void *t, uint32_t[3]) { ++reinterpret_cast<Sink *>(t)->triangles; }
		};

		Sink sink;

		geom::TessResult res;
		res.target = &sink;
		res.pushVertex = &Sink::v;
		res.pushTriangle = &Sink::t3;
		if (tess->prepare(res)) {
			tess->write(res);
		}

		vertexes = sink.vertexes;
		triangles = sink.triangles;
	}, pool);
	memory::pool::destroy(pool);

	const auto total = sprt::platform::clock(sprt::platform::ClockType::Monotonic) - start;

	sprt::cout << "проводов " << count << ", всего " << total << " мкс\n";
	sprt::cout << "  вершин " << vertexes << ", треугольников " << triangles << "\n";
	return 0;
}

// ---- one icon -----------------------------------------------------------------------------------

static int doOne(StringView name, bool antialias) {
	auto r = tessellateIcon(name, antialias);
	sprt::cout << name << " (" << (antialias ? "aa" : "fill") << "): путей " << r.paths << ", "
			   << r.microseconds << " мкс\n  " << r.digest.encode() << "\n";
	// The digest through its own text form and back. Kept as a standing check rather than a
	// debugging leftover: a golden file is worth exactly nothing if what it holds does not read
	// back as what was written, and that is a property of THIS pair of functions, which an
	// earlier version of them did not have.
	auto text = r.digest.encode();
	PathDigest back;
	PathDigest::decode(text, back);
	sprt::cout << "  эталонная строка: \"" << text << "\""
			   << (back.sameCounts(r.digest) && back.sameShape(r.digest, GoldenEpsilon)
								  ? " (круг сходится)"
								  : " (КРУГ НЕ СХОДИТСЯ)")
			   << "\n";

	return r.digest.failed ? 1 : 0;
}

static int run(int argc, const char *argv[]) {
	StringView cmd = argc > 1 ? StringView(argv[1]) : StringView("bench");

	bool antialias = false;
	bool write = false;
	bool perPath = false;
	bool noBypass = false;
	StringView icon;
	for (int i = 2; i < argc; ++i) {
		StringView a(argv[i]);
		if (a == "--aa") {
			antialias = true;
		} else if (a == "--per-path") {
			perPath = true;
		} else if (a == "--no-bypass") {
			noBypass = true;
		} else if (a == "--write") {
			write = true;

		} else if (!a.starts_with("--")) {
			icon = a;
		}
	}

	if (cmd == "raster") {
		if (!rasterInit(Extent2(RasterExtent, RasterExtent))) {
			sprt::cout << "растеризатор не поднялся\n";
			return 1;
		}
		if (icon.empty()) {
			sprt::cout << "растеризатор поднят: программный движок и плоская очередь без окна\n";
			rasterFinalize();
			return 0;
		}
		auto img = rasterIcon(icon, antialias);
		if (img.empty()) {
			sprt::cout << "не отрисовалось: " << icon << "\n";
			rasterFinalize();
			return 1;
		}
		auto out = mem_std::toString(icon, antialias ? "-aa" : "-fill", ".png");
		sprt::cout << icon << ": " << img.width << "x" << img.height << " -> " << out
				   << (img.writePng(out) ? "" : " (не записалось)") << "\n";
		rasterFinalize();
		return 0;
	} else if (cmd == "wires") {
		const auto count = icon.empty() ? 0 : uint32_t(StringView(icon).readInteger(10).get(400));
		if (perPath) {
			return doWiresPerPath(count, !noBypass);
		}
		return doWires(count);
	} else if (cmd == "reference") {
		return doReference(icon);
	} else if (cmd == "raster-golden") {
		return doRasterGolden(write);
	} else if (cmd == "bench") {
		return doBench();
	} else if (cmd == "golden") {
		return doGolden(write);
	} else if (cmd == "one") {
		if (icon.empty()) {
			sprt::cout << "нужно имя иконки\n";
			return 1;
		}
		return doOne(icon, antialias);
	}

	sprt::cout << "tesstest bench | golden [--write] | raster-golden [--write] | one <icon> [--aa]"
				  " | raster [<icon>] [--aa]\n"
				  "         | reference <material-design-icons>/png | wires [count]\n";
	return 1;
}

} // namespace STAPPLER_VERSIONIZED stappler

int main(int argc, const char *argv[]) {
	return stappler::perform_main(argc, argv, [&] { return stappler::run(argc, argv); });
}
