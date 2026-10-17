/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

/* A CHECK FOR THE SPRITE MESH PATH.

A polygon frame carries its outline in the package, and the sprite that draws it must submit the
outline's mesh - not a quad over the frame's bounds, which is a silent wrong picture. The two
halves of that sentence are checked by the two modes here:

    spritetest mesh     the vertex path: addPolygon's triangulation, its determinism, and the
                        corner mapping shared with the quad - a rectangle ring through the polygon
                        path must land on the quad's own corners, positions and texture
                        coordinates both, with flips and the quarter turn included
    spritetest raster   the picture: the same L frame as a quad and as a mesh, drawn beside each
                        other through the engine's own flat queue (software backend, no window).
                        The quad covers its whole rectangle, the mesh does not cover the corner
                        the outline cuts out, and where the L is solid the two agree pixel for
                        pixel - which is "a scene places one polygon frame and one quad frame, and
                        only the polygon's empty corner is missing".

With no arguments both run. The default is the check run-checks.py registers. */

#include "SPCommon.h"

#include "XL2d.h"
#include "XL2dCommandList.h"
#include "XL2dVertexArray.h"

#include "SpriteRaster.h"

#include <sprt/runtime/platform.h>

namespace STAPPLER_VERSIONIZED stappler {

using namespace stappler::spritebench;
using namespace stappler::xenolith;

static constexpr float CompareEpsilon = 1e-6f;

// ---- the mesh: triangulation and the corner mapping ----------------------------------------------

/* The L every check here uses. A 1x1 frame with the top-right 0.55x0.55 cut out, as a ring in
image space: x to the right, y down, open, six points. Its area is 1 - 0.55*0.55 = 0.6975. */
static Vector<Vec2> makeLRing() {
	return Vector<Vec2>{Vec2(0.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(0.45f, 1.0f), Vec2(0.45f, 0.45f),
		Vec2(1.0f, 0.45f), Vec2(1.0f, 0.0f)};
}

static float shoelaceHalf(SpanView<const Vec2> ring) {
	float sum = 0.0f;
	for (size_t i = 0; i < ring.size(); ++i) {
		const auto &a = ring[i];
		const auto &b = ring[(i + 1) % ring.size()];
		sum += a.x * b.y - b.x * a.y;
	}
	return sum * 0.5f;
}

static float triangleHalf(const Vec2 &a, const Vec2 &b, const Vec2 &c) {
	return ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
}

/* THE ORACLE SET of the mesh mode. Each is a property of the answer, not of the route to it:

  - the triangulation covers exactly the ring: summed triangle area equals the ring's area, and
    the triangles count is the ring's points minus two;
  - every triangle faces as the quad's own triangles do (negative in image space), or the raster
    would cull the mesh;
  - the answer is deterministic and walk-order independent: the ring reversed, and the ring
    closed by repeating its first point, give the same vertexes and the same indexes;
  - a rectangle ring and a quad built with the same arguments agree per corner - position
    exactly, texture coordinate to an epsilon of float arithmetic - for every combination of
    flips and the quarter turn. This is the whole drift-proofing of the two paths: they answer
    through one corner helper, and this says they still do;
  - a mid-edge point lands on the corner bilinear combination, for the turned frame too;
  - rings that cannot be a mesh append nothing and leave the array as it was. */

static uint32_t MeshFailures = 0;

// `what` is one stream expression: MESH_CHECK(cond, "x " << value << " y")
#define MESH_CHECK(cond, what) \
	do { \
		if (!(cond)) { \
			++MeshFailures; \
			sprt::cout << "  FAIL " << __func__ << ": " << what << "\n"; \
		} \
	} while (0)

static void checkTriangleCoverage(basic2d::VertexArray &array,
		const basic2d::VertexArray::Polygon &polygon, float expectedArea) {
	const auto data = array.pop();
	float area = 0.0f;
	const auto &vertexes = data->data;
	for (size_t t = 0; t < polygon.indexes.size(); t += 3) {
		const auto &a = vertexes[polygon.indexes[t]];
		const auto &b = vertexes[polygon.indexes[t + 1]];
		const auto &c = vertexes[polygon.indexes[t + 2]];
		const auto tri = triangleHalf(Vec2(a.pos.x, a.pos.y), Vec2(b.pos.x, b.pos.y),
				Vec2(c.pos.x, c.pos.y));
		area += tri;
		// Positions run y up, and there the quad's own triangles are counter-clockwise; the mesh
		// must face with them or a culling pipeline drops it.
		MESH_CHECK(tri > 0.0f, "треугольник " << t / 3 << " смотрит не туда, куда квад (площадь " << tri << ")");
	}
	MESH_CHECK(sprt::abs(area - expectedArea) < CompareEpsilon,
			"площадь меша " << area << " != кольца " << expectedArea);
	MESH_CHECK(polygon.indexes.size() == (polygon.ring.size() - 2) * 3,
			"треугольников " << (polygon.indexes.size() / 3) << " у кольца из "
			<< polygon.ring.size() << " точек");
}

static void checkMeshDeterminism() {
	auto ring = makeLRing();

	basic2d::VertexArray first;
	first.init(0, 0);
	auto p1 = first.addPolygon(SpanView<const Vec2>(ring.data(), ring.size()));
	p1.setGeometry(Vec4(0.0f, 0.0f, 0.0f, 1.0f), Size2(1.0f, 1.0f))
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
			.setColor(Color4F::WHITE);

	// The same shape walked backwards, and the same shape closed by repeating its first point.
	Vector<Vec2> reversed(ring.rbegin(), ring.rend());
	basic2d::VertexArray second;
	second.init(0, 0);
	auto p2 = second.addPolygon(SpanView<const Vec2>(reversed.data(), reversed.size()));
	p2.setGeometry(Vec4(0.0f, 0.0f, 0.0f, 1.0f), Size2(1.0f, 1.0f))
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
			.setColor(Color4F::WHITE);

	Vector<Vec2> closed = ring;
	closed.push_back(ring.front());
	basic2d::VertexArray third;
	third.init(0, 0);
	auto p3 = third.addPolygon(SpanView<const Vec2>(closed.data(), closed.size()));
	p3.setGeometry(Vec4(0.0f, 0.0f, 0.0f, 1.0f), Size2(1.0f, 1.0f))
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
			.setColor(Color4F::WHITE);

	MESH_CHECK(p1.valid && p2.valid && p3.valid,
			"L-кольцо отказано: " << p1.valid << p2.valid << p3.valid);
	MESH_CHECK(p1.indexes == p2.indexes, "обратный обход даёт другие треугольники");
	MESH_CHECK(p1.indexes == p3.indexes, "замкнутое кольцо даёт другие треугольники");

	auto d1 = first.pop();
	auto d2 = second.pop();
	MESH_CHECK(d1->indexes == d2->indexes, "обратный обход даёт другие индексы");
	MESH_CHECK(d1->data.size() == d2->data.size(), "обратный обход даёт другое число вершин");
	bool sameVertexes = d1->data.size() == d2->data.size();
	for (size_t i = 0; sameVertexes && i < d1->data.size(); ++i) {
		sameVertexes = d1->data[i].pos == d2->data[i].pos && d1->data[i].tex == d2->data[i].tex;
	}
	MESH_CHECK(sameVertexes, "обратный обход даёт другие вершины");

	checkTriangleCoverage(first, p1, 0.6975f);
}

static void checkMeshRefusals() {
	basic2d::VertexArray array;
	array.init(0, 0);
	array.addQuad().setGeometry(Vec4(0.0f, 0.0f, 0.0f, 1.0f), Size2(1.0f, 1.0f));
	const auto verticesBefore = array.getVertexCount();
	const auto indexesBefore = array.getIndexCount();

	Vector<Vec2> two{Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f)};
	auto few = array.addPolygon(SpanView<const Vec2>(two.data(), two.size()));
	MESH_CHECK(!few.valid, "кольцо из двух точек принято");
	MESH_CHECK(array.getVertexCount() == verticesBefore && array.getIndexCount() == indexesBefore,
			"отказ что-то дописал");

	Vector<Vec2> line{Vec2(0.0f, 0.0f), Vec2(0.5f, 0.5f), Vec2(1.0f, 1.0f)};
	auto collinear = array.addPolygon(SpanView<const Vec2>(line.data(), line.size()));
	MESH_CHECK(!collinear.valid, "кольцо по прямой принято");
	MESH_CHECK(array.getVertexCount() == verticesBefore && array.getIndexCount() == indexesBefore,
			"отказ по прямой что-то дописал");

	Vector<Vec2> degenerate{Vec2(0.25f, 0.25f), Vec2(0.25f, 0.25f), Vec2(0.25f, 0.25f)};
	auto same = array.addPolygon(SpanView<const Vec2>(degenerate.data(), degenerate.size()));
	MESH_CHECK(!same.valid, "вырожденное кольцо принято");
	MESH_CHECK(array.getVertexCount() == verticesBefore && array.getIndexCount() == indexesBefore,
			"вырожденный отказ что-то дописал");
}

// The image-space corner each quad vertex answers for, in the quad's own storage order
// (tl bl tr br): the top-left of the image, the bottom-left, the top-right, the bottom-right.
static Vec2 QuadCorners[4] = {Vec2(0.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(1.0f, 0.0f),
	Vec2(1.0f, 1.0f)};

static void checkRectAgainstQuad(bool flippedX, bool flippedY, bool rotated) {
	// A rectangle ring, in the same corner order the quad storage uses.
	Vector<Vec2> rect{QuadCorners[0], QuadCorners[1], QuadCorners[3], QuadCorners[2]};

	const Rect texRect(0.1f, 0.2f, 0.5f, 0.25f);
	const Vec4 origin(10.0f, 20.0f, 0.5f, 1.0f);
	const Size2 size(30.0f, 40.0f);

	basic2d::VertexArray quads;
	quads.init(0, 0);
	quads.addQuad()
			.setGeometry(origin, size)
			.setTextureRect(texRect, 1.0f, 1.0f, flippedX, flippedY, rotated)
			.setColor(Color4F::WHITE);

	basic2d::VertexArray meshes;
	meshes.init(0, 0);
	auto polygon = meshes.addPolygon(SpanView<const Vec2>(rect.data(), rect.size()));
	polygon.setGeometry(origin, size)
			.setTextureRect(texRect, 1.0f, 1.0f, flippedX, flippedY, rotated)
			.setColor(Color4F::WHITE);

	MESH_CHECK(polygon.valid, "прямоугольное кольцо отказано");
	if (!polygon.valid) {
		return;
	}

	const auto quadData = quads.pop();
	const auto meshData = meshes.pop();

	// Every ring point must be one of the four corners, and answer with the quad's own vertex for
	// that corner: position exactly (the same two adds), texture to a float epsilon (the polygon
	// reaches a corner through the bilinear form, the quad reads it directly).
	for (size_t i = 0; i < polygon.ring.size(); ++i) {
		const auto &p = polygon.ring[i];
		int corner = -1;
		for (int c = 0; c < 4; ++c) {
			if (QuadCorners[c] == p) {
				corner = c;
				break;
			}
		}
		MESH_CHECK(corner >= 0, "в прямоугольном кольце точка не из углов: " << p.x << " " << p.y);
		if (corner < 0) {
			continue;
		}
		const auto &meshVertex = meshData->data[polygon.firstVertex + i];
		const auto &quadVertex = quadData->data[corner];
		MESH_CHECK(meshVertex.pos == quadVertex.pos, "позиция угла " << corner << " не квадова");
		MESH_CHECK(sprt::abs(meshVertex.tex.x - quadVertex.tex.x) < CompareEpsilon
						&& sprt::abs(meshVertex.tex.y - quadVertex.tex.y) < CompareEpsilon, "текстурная координата угла " << corner << " не квадова");
	}

	// And the coverage: two triangles, the whole rectangle, facing as the quad faces.
	checkTriangleCoverage(meshes, polygon, size.width * size.height);
}

// A mid-edge point must land on the same bilinear combination of the corner answers, for a turned
// frame included: the mapping is affine and this pins that it stays so.
static void checkInteriorMapping(bool rotated) {
	Vector<Vec2> ring{Vec2(0.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(0.3f, 1.0f), Vec2(1.0f, 0.2f),
		Vec2(1.0f, 0.0f)};

	const Rect texRect(0.1f, 0.2f, 0.5f, 0.25f);
	const Vec4 origin(10.0f, 20.0f, 0.5f, 1.0f);
	const Size2 size(30.0f, 40.0f);

	basic2d::VertexArray quads;
	quads.init(0, 0);
	quads.addQuad()
			.setGeometry(origin, size)
			.setTextureRect(texRect, 1.0f, 1.0f, false, false, rotated)
			.setColor(Color4F::WHITE);

	basic2d::VertexArray meshes;
	meshes.init(0, 0);
	auto polygon = meshes.addPolygon(SpanView<const Vec2>(ring.data(), ring.size()));
	polygon.setGeometry(origin, size)
			.setTextureRect(texRect, 1.0f, 1.0f, false, false, rotated)
			.setColor(Color4F::WHITE);
	MESH_CHECK(polygon.valid, "кольцо с внутренней точкой отказано");
	if (!polygon.valid) {
		return;
	}

	const auto quadData = quads.pop();
	const auto meshData = meshes.pop();

	Vec2 quadTex[4];
	for (int c = 0; c < 4; ++c) {
		quadTex[c] = quadData->data[c].tex;
	}

	for (size_t i = 0; i < polygon.ring.size(); ++i) {
		const auto &p = polygon.ring[i];
		const auto &meshVertex = meshData->data[polygon.firstVertex + i];
		const Vec2 expected(quadTex[0].x + p.x * (quadTex[2].x - quadTex[0].x)
						+ p.y * (quadTex[1].x - quadTex[0].x),
				quadTex[0].y + p.x * (quadTex[2].y - quadTex[0].y)
						+ p.y * (quadTex[1].y - quadTex[0].y));
		MESH_CHECK(sprt::abs(meshVertex.tex.x - expected.x) < CompareEpsilon
						&& sprt::abs(meshVertex.tex.y - expected.y) < CompareEpsilon, "внутренняя точка (" << p.x << " " << p.y << ") не на билинейной форме");
		const Vec2 expectedPos(origin.x + p.x * size.width,
				origin.y + size.height - p.y * size.height);
		MESH_CHECK(sprt::abs(meshVertex.pos.x - expectedPos.x) < CompareEpsilon
						&& sprt::abs(meshVertex.pos.y - expectedPos.y) < CompareEpsilon, "внутренняя точка (" << p.x << " " << p.y << ") не на месте фрагмента");
	}
}

static int doMesh() {
	checkMeshDeterminism();
	checkMeshRefusals();
	for (bool rotated : {false, true}) {
		for (bool flippedY : {false, true}) {
			for (bool flippedX : {false, true}) {
				checkRectAgainstQuad(flippedX, flippedY, rotated);
			}
		}
	}
	for (bool rotated : {false, true}) {
		checkInteriorMapping(rotated);
	}

	sprt::cout << "mesh: проверок отказов " << MeshFailures << "\n";
	return MeshFailures == 0 ? 0 : 1;
}

// ---- the picture: one L as a quad, the same L as a mesh, beside each other ------------------------

/* Both frames sit in one 128x64 target: the quad's rectangle at x 4..60, the mesh's at x 68..124,
both y 4..60. The L's cut corner is the frame's top-right; with the projection's y flip that is
the raster's top-right of each rectangle. */

static constexpr uint32_t RasterExtentX = 128;
static constexpr uint32_t RasterExtentY = 64;
static constexpr float FrameOriginX[2] = {4.0f, 68.0f};
static constexpr float FrameOriginY = 4.0f;
static constexpr float FrameSize = 56.0f;
// The cut is 0.55 of the frame from the top-right corner; the probe boxes keep a margin from
// every edge so a fill rule on the boundary never decides the check.
static constexpr float Cut = 0.55f;

static uint32_t RasterFailures = 0;

// `what` is one stream expression: RASTER_CHECK(cond, "x " << value << " y")
#define RASTER_CHECK(cond, what) \
	do { \
		if (!(cond)) { \
			++RasterFailures; \
			sprt::cout << "  FAIL " << __func__ << ": " << what << "\n"; \
		} \
	} while (0)

static Rc<basic2d::VertexData> buildQuadFrame(size_t frame) {
	basic2d::VertexArray array;
	array.init(4, 6);
	array.addQuad()
			.setGeometry(Vec4(FrameOriginX[frame], FrameOriginY, 0.0f, 1.0f),
					Size2(FrameSize, FrameSize))
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
			.setColor(Color4F::WHITE);
	return array.pop();
}

static Rc<basic2d::VertexData> buildMeshFrame(size_t frame) {
	auto ring = makeLRing();
	basic2d::VertexArray array;
	array.init(0, 0);
	auto polygon = array.addPolygon(SpanView<const Vec2>(ring.data(), ring.size()));
	RASTER_CHECK(polygon.valid, "L-кольцо отказано в растровом режиме");
	polygon.setGeometry(Vec4(FrameOriginX[frame], FrameOriginY, 0.0f, 1.0f),
					Size2(FrameSize, FrameSize))
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
			.setColor(Color4F::WHITE);
	return array.pop();
}

static RasterImage renderFrame(Rc<basic2d::VertexData> &&mesh) {
	return rasterDraw([&](const Mat4 &view, core::MaterialId material,
					basic2d::CommandList &commands) {
		basic2d::CmdInfo info;
		info.material = material;
		info.renderingLevel = RenderingLevel::Transparent;
		commands.pushVertexArray(sp::move(mesh), view, sp::move(info));
	});
}

static int doRaster() {
	if (!rasterInit(Extent2(RasterExtentX, RasterExtentY))) {
		return 1;
	}

	// What each pixel of a frame's rectangle the check probes. Margin 4 pixels from every edge of
	// the rectangle and of the cut, so the answer never sits on a fill-rule boundary.
	const auto quadLeft = uint32_t(FrameOriginX[0] + 4.0f);
	const auto quadRight = uint32_t(FrameOriginX[0] + FrameSize - 4.0f);
	const auto meshLeft = uint32_t(FrameOriginX[1] + 4.0f);
	const auto top = uint32_t(FrameOriginY + 4.0f);
	const auto bottom = uint32_t(FrameOriginY + FrameSize - 4.0f);
	// The cut corner's box. The cut is the image's top-right (v < 1 - Cut, u > 1 - Cut); vertex
	// placement and the projection each flip y once, so in raster rows it is the frame's BOTTOM
	// right - probe there, kept off every edge.
	const auto cutLeft = uint32_t(FrameOriginX[1] + FrameSize * (1.0f - Cut) + 4.0f);
	const auto cutRight = uint32_t(FrameOriginX[1] + FrameSize - 4.0f);
	const auto cutTop = uint32_t(FrameOriginY + FrameSize * Cut + 4.0f);
	const auto cutBottom = uint32_t(FrameOriginY + FrameSize - 4.0f);

	// The scene: one frame with a quad beside the same frame with a mesh, one command list.
	RasterImage scene;
	{
		auto quad = buildQuadFrame(0);
		auto mesh = buildMeshFrame(1);
		scene = rasterDraw([&](const Mat4 &view, core::MaterialId material,
						basic2d::CommandList &commands) {
			basic2d::CmdInfo quadInfo;
			quadInfo.material = material;
			quadInfo.renderingLevel = RenderingLevel::Transparent;
			commands.pushVertexArray(sp::move(quad), view, sp::move(quadInfo));

			basic2d::CmdInfo meshInfo;
			meshInfo.material = material;
			meshInfo.renderingLevel = RenderingLevel::Transparent;
			commands.pushVertexArray(sp::move(mesh), view, sp::move(meshInfo));
		});
	}
	RASTER_CHECK(!scene.empty(), "сцена не отрисовалась");

	// Each frame alone, for the beside-it claim: the pair must equal the two singles.
	RasterImage quadAlone = renderFrame(buildQuadFrame(0));
	RasterImage meshAlone = renderFrame(buildMeshFrame(1));
	RASTER_CHECK(!quadAlone.empty() && !meshAlone.empty(), "одиночные кадры не отрисовались");

	if (!scene.empty() && !quadAlone.empty() && !meshAlone.empty()) {
		// Determinism: the same draw again is the same bytes.
		RasterImage sceneAgain;
		{
			auto quad = buildQuadFrame(0);
			auto mesh = buildMeshFrame(1);
			sceneAgain = rasterDraw([&](const Mat4 &view, core::MaterialId material,
							basic2d::CommandList &commands) {
				basic2d::CmdInfo quadInfo;
				quadInfo.material = material;
				quadInfo.renderingLevel = RenderingLevel::Transparent;
				commands.pushVertexArray(sp::move(quad), view, sp::move(quadInfo));

				basic2d::CmdInfo meshInfo;
				meshInfo.material = material;
				meshInfo.renderingLevel = RenderingLevel::Transparent;
				commands.pushVertexArray(sp::move(mesh), view, sp::move(meshInfo));
			});
		}
		RASTER_CHECK(scene.sameBytes(sceneAgain), "повторная сцена отличается байтами");

		// The quad frame: its whole rectangle is drawn.
		for (uint32_t y = top; y <= bottom; ++y) {
			for (uint32_t x = quadLeft; x <= quadRight; ++x) {
				RASTER_CHECK(scene.at(x, y) > 0, "квад пуст в (" << x << "," << y << ")");
			}
		}

		// The mesh frame: the cut corner is NOT drawn - the whole point of the mesh path.
		uint32_t cutLit = 0;
		for (uint32_t y = cutTop; y <= cutBottom; ++y) {
			for (uint32_t x = cutLeft; x <= cutRight; ++x) {
				cutLit += scene.at(x, y) > 0 ? 1 : 0;
			}
		}
		RASTER_CHECK(cutLit == 0, "меш закрасил вырезанный угол: " << cutLit << " пикселей");

		// The mesh frame's solid arm (the left of the L, full height) is drawn, and agrees with
		// the quad frame's own pixels there: same frame content, one as quad, one as mesh.
		uint32_t armLit = 0, armMismatch = 0;
		const auto armRight = uint32_t(FrameOriginX[1] + FrameSize * (1.0f - Cut) - 4.0f);
		for (uint32_t y = top; y <= bottom; ++y) {
			for (uint32_t x = meshLeft; x <= armRight; ++x) {
				const auto meshPixel = scene.at(x, y);
				armLit += meshPixel > 0 ? 1 : 0;
				// The same place of the quad frame, one frame width to the left.
				const auto quadPixel = scene.at(x - uint32_t(FrameOriginX[1] - FrameOriginX[0]), y);
				if (meshPixel != quadPixel) {
					++armMismatch;
				}
			}
		}
		RASTER_CHECK(armLit == (armRight - meshLeft + 1) * (bottom - top + 1), "меш не закрасил цельное крыло L: закрашено " << armLit);
		RASTER_CHECK(armMismatch == 0, "меш и квад расходятся на теле L: " << armMismatch << " пикселей");

		// The pair equals the singles: nothing about drawing beside another frame changed either.
		uint32_t besideMismatch = 0;
		for (uint32_t y = 0; y < RasterExtentY; ++y) {
			for (uint32_t x = 0; x < RasterExtentX; ++x) {
				const auto expected = quadAlone.at(x, y) > 0 || meshAlone.at(x, y) > 0;
				const auto actual = scene.at(x, y) > 0;
				if (expected != actual) {
					++besideMismatch;
				}
			}
		}
		RASTER_CHECK(besideMismatch == 0,
			"сцена не равна двум одиночным кадрам: " << besideMismatch << " пикселей");
	}

	rasterFinalize();

	sprt::cout << "raster: проверок отказов " << RasterFailures << "\n";
	return RasterFailures == 0 ? 0 : 1;
}

// ---- the modes ------------------------------------------------------------------------------------

static int run(int argc, const char *argv[]) {
	StringView cmd = argc > 1 ? StringView(argv[1]) : StringView("all");

	if (cmd == "mesh") {
		return doMesh();
	} else if (cmd == "raster") {
		return doRaster();
	} else if (cmd == "all") {
		const auto mesh = doMesh();
		const auto raster = doRaster();
		return mesh == 0 && raster == 0 ? 0 : 1;
	}

	sprt::cout << "spritetest mesh | raster | all\n";
	return 1;
}

} // namespace STAPPLER_VERSIONIZED stappler

int main(int argc, const char *argv[]) {
	return stappler::perform_main(argc, argv, [&] { return stappler::run(argc, argv); });
}
