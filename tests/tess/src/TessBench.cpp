/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#include "TessBench.h"
#include "XL2dIcons.h"
#include "XL2dConfig.h"

namespace STAPPLER_VERSIONIZED stappler::tessbench {

using stappler::xenolith::basic2d::IconName;

// ---- the digest ---------------------------------------------------------------------------------

namespace {

// Thousandths, rounded. The golden file holds INTEGERS and no floating point at all.
//
// Not fastidiousness: a text form of a float is the one part of a golden that has to round-trip
// exactly, and going through the general number formatter did not - it wrote 4.0 as "4.5" and the
// file could not be read back as what was written. An integer count of thousandths has no such
// question, prints the same on every platform, and makes the file's own resolution explicit: the
// icons are 24 units across, so a thousandth is a change nobody can see and a hundredth is one
// worth failing over.
int64_t toMilli(double v) { return int64_t(v < 0.0 ? (v * 1000.0 - 0.5) : (v * 1000.0 + 0.5)); }

} // namespace

bool PathDigest::sameShape(const PathDigest &other, float eps) const {
	if (failed != other.failed) {
		return false;
	}
	if (failed) {
		return true; // both refused: nothing else to compare
	}
	// Area relative, box absolute: an icon is 24 units across, so an absolute epsilon means the
	// same thing everywhere on it, while area spans four orders of magnitude between a dot and a
	// filled square.
	// Compared as the file holds them, so what a run asserts is exactly what the file can say.
	// One thousandth of slack on top of that is the rounding itself.
	const int64_t slack = int64_t(double(eps) * 1000.0) + 1;
	if (sprt::abs(toMilli(area) - toMilli(other.area))
			> sprt::abs(toMilli(area)) * slack / 1'000 + slack) {
		return false;
	}
	return sprt::abs(toMilli(minX) - toMilli(other.minX)) <= slack
			&& sprt::abs(toMilli(minY) - toMilli(other.minY)) <= slack
			&& sprt::abs(toMilli(maxX) - toMilli(other.maxX)) <= slack
			&& sprt::abs(toMilli(maxY) - toMilli(other.maxY)) <= slack;
}

mem_std::String PathDigest::encode() const {
	if (failed) {
		return mem_std::String("FAILED");
	}
	return mem_std::toString(vertexes, " ", triangles, " ", toMilli(area), " ", toMilli(minX), " ",
			toMilli(minY), " ", toMilli(maxX), " ", toMilli(maxY));
}

bool PathDigest::decode(StringView str, PathDigest &out) {
	str.skipChars<StringView::WhiteSpace>();
	if (str.starts_with("FAILED")) {
		out = PathDigest();
		out.failed = true;
		return true;
	}
	const auto next = [&]() -> int64_t {
		str.skipChars<StringView::WhiteSpace>();
		bool neg = false;
		if (str.is('-')) {
			neg = true;
			++str;
		}
		auto v = int64_t(str.readInteger(10).get(0));
		return neg ? -v : v;
	};

	out = PathDigest();
	out.vertexes = uint32_t(next());
	out.triangles = uint32_t(next());
	out.area = double(next()) / 1000.0;
	out.minX = float(next()) / 1000.0f;
	out.minY = float(next()) / 1000.0f;
	out.maxX = float(next()) / 1000.0f;
	out.maxY = float(next()) / 1000.0f;
	return true;
}

// ---- tesselating one icon -----------------------------------------------------------------------

namespace {

// Collects what Tesselator::write emits and derives the digest from it. Area is summed in double
// around the mesh's own first vertex: the cross product of three nearby points subtracts near-equal
// numbers, and doing it in float at the icon's coordinates would put noise in the golden file.
struct Collector {
	mem_std::Vector<Vec2> vertexes;
	mem_std::Vector<uint32_t> indexes;

	mem_std::Vector<float> values;

	static void onVertex(void *t, uint32_t idx, const Vec2 &pt, float value, const Vec2 &) {
		auto self = static_cast<Collector *>(t);
		if (idx >= self->vertexes.size()) {
			self->vertexes.resize(idx + 1);
			self->values.resize(idx + 1, 1.0f);
		}
		self->vertexes[idx] = pt;
		self->values[idx] = value;
	}

	static void onTriangle(void *t, uint32_t v[3]) {
		auto self = static_cast<Collector *>(t);
		self->indexes.emplace_back(v[0]);
		self->indexes.emplace_back(v[1]);
		self->indexes.emplace_back(v[2]);
	}

	void fill(PathDigest &out) const {
		out.triangles = uint32_t(indexes.size() / 3);
		if (indexes.empty()) {
			return;
		}

		// Only vertexes a triangle references: `write` may allocate a slot it never touches, and
		// an untouched slot sits at the origin and would drag the box there.
		bool first = true;
		double ox = vertexes[indexes[0]].x, oy = vertexes[indexes[0]].y;
		double area = 0.0;
		uint32_t used = 0;
		mem_std::Vector<bool> seen(vertexes.size(), false);

		for (size_t i = 0; i < indexes.size(); i += 3) {
			auto &a = vertexes[indexes[i]];
			auto &b = vertexes[indexes[i + 1]];
			auto &c = vertexes[indexes[i + 2]];

			const double ax = a.x - ox, ay = a.y - oy;
			const double bx = b.x - ox, by = b.y - oy;
			const double cx = c.x - ox, cy = c.y - oy;
			area += sprt::abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) * 0.5;

			for (uint32_t k = 0; k < 3; ++k) {
				auto idx = indexes[i + k];
				if (!seen[idx]) {
					seen[idx] = true;
					++used;
				}
				auto &v = vertexes[idx];
				if (first) {
					out.minX = out.maxX = v.x;
					out.minY = out.maxY = v.y;
					first = false;
				} else {
					out.minX = sprt::min(out.minX, v.x);
					out.minY = sprt::min(out.minY, v.y);
					out.maxX = sprt::max(out.maxX, v.x);
					out.maxY = sprt::max(out.maxY, v.y);
				}
			}
		}

		out.vertexes = used;
		out.area = area;
	}
};

// What the renderer asks for at scale 1. `VGAntialiasFactor` is the engine's own number; the bench
// uses it rather than a round one so the antialiased variant is the geometry the engine really
// produces, not a plausible imitation of it.
constexpr float BenchAntialias = xenolith::config::VGAntialiasFactor;

} // namespace

IconResult tessellateIcon(StringView name, bool antialias, RawMesh *out) {
	IconResult ret;

	auto iconName = xenolith::basic2d::IconName::None;
	for (uint32_t i = 0; i < toInt(IconName::Max); ++i) {
		if (xenolith::basic2d::getIconName(IconName(i)) == name) {
			iconName = IconName(i);
			break;
		}
	}
	if (iconName == IconName::None) {
		ret.digest.failed = true;
		return ret;
	}

	auto image = Rc<vg::VectorImage>::create(vg::Size2(24.0f, 24.0f));
	if (!image) {
		ret.digest.failed = true;
		return ret;
	}
	xenolith::basic2d::drawIcon(*image, iconName, 0.0f);

	Collector collector;
	geom::TessResult res;
	res.target = &collector;
	res.pushVertex = &Collector::onVertex;
	res.pushTriangle = &Collector::onTriangle;

	const auto start = sprt::platform::clock(ClockType::Monotonic);

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::perform([&] {
		for (auto &it : image->getPaths()) {
			auto path = it.second->getPath();
			if (!path) {
				continue;
			}
			++ret.paths;

			auto tess = Rc<geom::Tesselator>::create(pool);

			do {
				geom::StrokeConfig cfg;
				geom::LineDrawer line(1.0f, Rc<geom::Tesselator>(tess), nullptr, nullptr, cfg);

				// The path's own transform is part of its geometry - `drawIcon` flips y and
				// shifts, and a bench that ignored it would tesselate a mirrored icon.
				auto &t = path->getTransform();
				auto d = path->getPoints().data();
				const auto pt = [&](uint32_t i) {
					return t.transformPoint(Vec2(d[i].p.x, d[i].p.y));
				};

				for (auto &cmd : path->getCommands()) {
					switch (cmd) {
					case vg::Command::MoveTo:
						line.drawBegin(pt(0).x, pt(0).y);
						++d;
						break;
					case vg::Command::LineTo:
						line.drawLine(pt(0).x, pt(0).y);
						++d;
						break;
					case vg::Command::QuadTo:
						line.drawQuadBezier(pt(0).x, pt(0).y, pt(1).x, pt(1).y);
						d += 2;
						break;
					case vg::Command::CubicTo:
						line.drawCubicBezier(pt(0).x, pt(0).y, pt(1).x, pt(1).y, pt(2).x, pt(2).y);
						d += 3;
						break;
					case vg::Command::ArcTo: {
						// Radii and flags are not positions - only the endpoint moves.
						auto end = pt(1);
						line.drawArc(d[0].p.x, d[0].p.y, d[2].f.v, d[2].f.a, d[2].f.b, end.x,
								end.y);
						d += 3;
						break;
					}
					case vg::Command::ClosePath: line.drawClose(true); break;
					default: break;
					}
				}
				line.drawClose(false);
			} while (0);

			if (antialias) {
				tess->setBoundariesTransform(BenchAntialias, BenchAntialias);
				tess->setRelocateRule(geom::Tesselator::RelocateRule::Auto);
			}
			tess->setWindingRule(path->getWindingRule());

			if (!tess->prepare(res)) {
				ret.digest.failed = true;
				continue;
			}
			collector.vertexes.resize(res.nvertexes);
			collector.values.resize(res.nvertexes, 1.0f);
			tess->write(res);
		}
	}, pool);
	memory::pool::destroy(pool);

	ret.microseconds = sprt::platform::clock(ClockType::Monotonic) - start;
	if (!ret.digest.failed) {
		collector.fill(ret.digest);
		if (out) {
			out->vertexes = collector.vertexes;
			out->indexes = collector.indexes;
			out->values = collector.values;
			out->values.resize(out->vertexes.size(), 1.0f);
		}
	}
	return ret;
}

void forEachIcon(const Callback<void(StringView)> &cb) {
	for (uint32_t i = 0; i < toInt(IconName::Max); ++i) {
		const auto name = IconName(i);
		if (name == IconName::None || name == IconName::Empty) {
			continue;
		}
		auto str = xenolith::basic2d::getIconName(name);
		if (!str.empty()) {
			cb(str);
		}
	}
}

} // namespace stappler::tessbench
