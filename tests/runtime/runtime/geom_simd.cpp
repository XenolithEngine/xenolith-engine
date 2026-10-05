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

#include <sprt/runtime/stream.h>
#include <sprt/runtime/geom/simd.h>

#include "../tests.h"

// Every geometry SIMD backend built for this arch against the scalar one. Lane-wise arithmetic,
// shuffles and comparisons must be bit-identical; a sum of products is too, unless the backend
// fuses it (NEON64 fmla), and rsqrt is held to the precision of the hardware estimate.

namespace sprt {

namespace {

namespace simd = sprt::geom::simd;

struct alignas(16) F4 {
	float v[4];
};

struct alignas(16) F16 {
	float v[16];
};

struct SimdBackend {
	const char *name;
	bool fusedProducts; // multiply-add without the intermediate rounding
	bool fusedCross;
	float rsqrtTolerance; // relative

	void (*load4)(float, float, float, float, float *);
	void (*broadcast)(float, float *);
	void (*load1)(float, float *);
	void (*binary[4])(const float *, const float *, float *); // add, sub, mul, div
	void (*binary1[3])(const float *, const float *, float *); // add1, sub1, mul1
	void (*rsqrt)(const float *, float *);
	void (*rsqrt1)(const float *, float *);

	void (*matScalar[2])(const float *, float, float *); // addMat4Scalar, multiplyMat4Scalar
	void (*mat[3])(const float *, const float *, float *); // addMat4, subtractMat4, multiplyMat4
	void (*matUnary[2])(const float *, float *); // negateMat4, transposeMat4
	void (*transformComponents)(const float *, const float *, float *);
	void (*transform)(const float *, const float *, float *);
	void (*cross)(const float *, const float *, float *);
	bool (*bbox)(const float *, const float *, float *);
};

// One table per namespace; every operand goes through the backend's own load and store
#define SP_SIMD_BACKEND(NS, FUSED, FUSED_CROSS, RSQRT_TOL) \
	SimdBackend{ \
		#NS, \
		FUSED, \
		FUSED_CROSS, \
		RSQRT_TOL, \
		[](float a, float b, float c, float d, float *dst) { \
			simd::NS::store(dst, simd::NS::load(a, b, c, d)); \
		}, \
		[](float a, float *dst) { simd::NS::store(dst, simd::NS::load(a)); }, \
		[](float a, float *dst) { simd::NS::store(dst, simd::NS::load1(a)); }, \
		{ \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::add(simd::NS::load(a), simd::NS::load(b))); \
			}, \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::sub(simd::NS::load(a), simd::NS::load(b))); \
			}, \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::mul(simd::NS::load(a), simd::NS::load(b))); \
			}, \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::div(simd::NS::load(a), simd::NS::load(b))); \
			}, \
		}, \
		{ \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::add1(simd::NS::load(a), simd::NS::load(b))); \
			}, \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::sub1(simd::NS::load(a), simd::NS::load(b))); \
			}, \
			[](const float *a, const float *b, float *dst) { \
				simd::NS::store(dst, simd::NS::mul1(simd::NS::load(a), simd::NS::load(b))); \
			}, \
		}, \
		[](const float *a, float *dst) { simd::NS::store(dst, simd::NS::rsqrt(simd::NS::load(a))); }, \
		[](const float *a, float *dst) { \
			simd::NS::store(dst, simd::NS::rsqrt1(simd::NS::load(a))); \
		}, \
		{ \
			[](const float *m, float s, float *dst) { simd::NS::addMat4Scalar(m, s, dst); }, \
			[](const float *m, float s, float *dst) { simd::NS::multiplyMat4Scalar(m, s, dst); }, \
		}, \
		{ \
			[](const float *a, const float *b, float *dst) { simd::NS::addMat4(a, b, dst); }, \
			[](const float *a, const float *b, float *dst) { simd::NS::subtractMat4(a, b, dst); }, \
			[](const float *a, const float *b, float *dst) { simd::NS::multiplyMat4(a, b, dst); }, \
		}, \
		{ \
			[](const float *m, float *dst) { simd::NS::negateMat4(m, dst); }, \
			[](const float *m, float *dst) { simd::NS::transposeMat4(m, dst); }, \
		}, \
		[](const float *m, const float *v, float *dst) { \
			simd::NS::transformVec4Components(m, v[0], v[1], v[2], v[3], dst); \
		}, \
		[](const float *m, const float *v, float *dst) { simd::NS::transformVec4(m, v, dst); }, \
		[](const float *a, const float *b, float *dst) { simd::NS::crossVec3(a, b, dst); }, \
		[](const float *a, const float *b, float *isect) { \
			auto r = simd::NS::f32x4(); \
			auto ret = simd::NS::isVec2BboxIntersects(simd::NS::load(a), simd::NS::load(b), r); \
			simd::NS::store(isect, r); \
			return ret; \
		}, \
	}

// The plain-C++ cross product of the SSE and NEON namespaces is contracted into an FMA wherever the
// target has one (__FP_FAST_FMAF does not say so on LoongArch or RISC-V); vector arithmetic is not
#if __aarch64__ || __loongarch__ || __riscv
static constexpr bool ScalarCodeFused = true;
#else
static constexpr bool ScalarCodeFused = false;
#endif

#define SP_SIMD_STRINGIFY_(X) #X
#define SP_SIMD_STRINGIFY(X) SP_SIMD_STRINGIFY_(X)

// An estimate instruction is 12 bits on x86, NEON's vrsqrte 8; LSX and the scalar code are exact
static constexpr float RsqrtEstimate = 1.0f / 256.0f;

static unsigned s_checks = 0;
static unsigned s_failures = 0;

static void check(bool ok, const SimdBackend &b, const char *what) {
	++s_checks;
	if (!ok) {
		++s_failures;
		sprt::cerr << sprt::test::failed("  FAIL: ") << b.name << ": " << what << "\n";
	}
}

struct Random {
	uint32_t state = 0x9E37'79B9;

	uint32_t next() {
		state = state * 1'664'525u + 1'013'904'223u;
		return state;
	}

	// Mostly ordinary magnitudes, with zeros, signed zeros and wide exponents mixed in
	float value() {
		auto r = next();
		switch (r % 16) {
		case 0: return 0.0f;
		case 1: return -0.0f;
		case 2: return float(int32_t(next() % 2001) - 1000) * 1e-6f;
		case 3: return float(int32_t(next() % 2001) - 1000) * 1e6f;
		default: return float(int32_t(next() % 200'001) - 100'000) / 1000.0f;
		}
	}

	float nonZero() {
		float v = value();
		return v == 0.0f ? 1.5f : v;
	}

	float positive() { return float(next() % 1'000'000 + 1) / 1000.0f; }
};

static bool bitEqual(float a, float b) {
	if (a != a && b != b) {
		return true;
	}
	return __builtin_memcmp(&a, &b, sizeof(float)) == 0;
}

static bool bitEqual(const float *a, const float *b, int n) {
	for (int i = 0; i < n; ++i) {
		if (!bitEqual(a[i], b[i])) {
			return false;
		}
	}
	return true;
}

static bool sameValue(const float *a, const float *b, int n) {
	for (int i = 0; i < n; ++i) {
		if (a[i] != b[i] && (a[i] == a[i] || b[i] == b[i])) {
			return false;
		}
	}
	return true;
}

static float absf(float v) { return v < 0.0f ? -v : v; }

// For a sum of products: the difference against the scalar result, relative to the magnitude of
// the terms, since the sum itself may cancel to anything
static bool closeSum(float subject, float reference, float magnitude, float tolerance) {
	return absf(subject - reference) <= tolerance * magnitude + 1e-30f;
}

static void checkLanes(const SimdBackend &b, const SimdBackend &ref, Random &rnd) {
	bool loads = true, broadcast = true, load1 = true;
	bool binary[4] = {true, true, true, true};
	bool binary1[3] = {true, true, true};
	bool rsqrt = true, rsqrt1 = true;

	for (int iter = 0; iter < 512; ++iter) {
		F4 a, c, x, y, rx, ry;
		for (int i = 0; i < 4; ++i) {
			a.v[i] = rnd.value();
			c.v[i] = rnd.nonZero();
		}

		b.load4(a.v[0], a.v[1], a.v[2], a.v[3], x.v);
		loads = loads && bitEqual(x.v, a.v, 4);

		b.broadcast(a.v[0], x.v);
		ref.broadcast(a.v[0], rx.v);
		broadcast = broadcast && bitEqual(x.v, rx.v, 4);

		b.load1(a.v[1], x.v);
		ref.load1(a.v[1], rx.v);
		load1 = load1 && bitEqual(x.v, rx.v, 4);

		for (int op = 0; op < 4; ++op) {
			b.binary[op](a.v, c.v, x.v);
			ref.binary[op](a.v, c.v, rx.v);
			binary[op] = binary[op] && bitEqual(x.v, rx.v, 4);
		}

		for (int op = 0; op < 3; ++op) {
			b.binary1[op](a.v, c.v, y.v);
			ref.binary1[op](a.v, c.v, ry.v);
			binary1[op] = binary1[op] && bitEqual(y.v, ry.v, 4);
		}

		F4 p;
		for (int i = 0; i < 4; ++i) { p.v[i] = rnd.positive(); }
		b.rsqrt(p.v, x.v);
		ref.rsqrt(p.v, rx.v);
		for (int i = 0; i < 4; ++i) {
			rsqrt = rsqrt && absf(x.v[i] - rx.v[i]) <= b.rsqrtTolerance * rx.v[i];
		}

		b.rsqrt1(p.v, y.v);
		ref.rsqrt1(p.v, ry.v);
		rsqrt1 = rsqrt1 && absf(y.v[0] - ry.v[0]) <= b.rsqrtTolerance * ry.v[0]
				&& bitEqual(&y.v[1], &ry.v[1], 3);
	}

	check(loads, b, "load(a, b, c, d) keeps the lane order");
	check(broadcast, b, "load(float) broadcasts");
	check(load1, b, "load1 fills lane 0 and clears the rest");
	check(binary[0], b, "add matches scalar");
	check(binary[1], b, "sub matches scalar");
	check(binary[2], b, "mul matches scalar");
	check(binary[3], b, "div matches scalar");
	check(binary1[0], b, "add1 changes lane 0 only");
	check(binary1[1], b, "sub1 changes lane 0 only");
	check(binary1[2], b, "mul1 changes lane 0 only");
	check(rsqrt, b, "rsqrt within the estimate's precision");
	check(rsqrt1, b, "rsqrt1 changes lane 0 only");
}

static void checkMatrices(const SimdBackend &b, const SimdBackend &ref, Random &rnd) {
	bool matScalar[2] = {true, true};
	bool mat[2] = {true, true};
	bool matUnary[2] = {true, true};
	bool product = true, productAliased = true, transform = true, components = true, cross = true;
	float worstProduct = 0.0f;

	const float productTolerance = b.fusedProducts ? 4.0f / float(1 << 23) : 0.0f;
	const float crossTolerance = b.fusedCross ? 4.0f / float(1 << 23) : 0.0f;

	auto sameSum = [&](const float *subject, const float *reference, const float *magnitude, int n,
						   float tolerance) {
		for (int i = 0; i < n; ++i) {
			if (tolerance == 0.0f) {
				if (!bitEqual(subject[i], reference[i])) {
					return false;
				}
			} else if (!closeSum(subject[i], reference[i], magnitude[i], tolerance)) {
				return false;
			}
			if (magnitude[i] > 0.0f) {
				auto err = absf(subject[i] - reference[i]) / magnitude[i];
				worstProduct = worstProduct < err ? err : worstProduct;
			}
		}
		return true;
	};

	for (int iter = 0; iter < 256; ++iter) {
		F16 m1, m2, x, rx;
		F4 v, vx, rv;
		for (int i = 0; i < 16; ++i) {
			m1.v[i] = rnd.value();
			m2.v[i] = rnd.value();
		}
		for (int i = 0; i < 4; ++i) { v.v[i] = rnd.value(); }
		const float s = rnd.value();

		for (int op = 0; op < 2; ++op) {
			b.matScalar[op](m1.v, s, x.v);
			ref.matScalar[op](m1.v, s, rx.v);
			matScalar[op] = matScalar[op] && bitEqual(x.v, rx.v, 16);

			b.mat[op](m1.v, m2.v, x.v);
			ref.mat[op](m1.v, m2.v, rx.v);
			mat[op] = mat[op] && bitEqual(x.v, rx.v, 16);

			b.matUnary[op](m1.v, x.v);
			ref.matUnary[op](m1.v, rx.v);
			// negate of a zero: NEON64 fneg gives -0, a subtraction from zero +0; both are right
			matUnary[op] = matUnary[op] && (op == 0 ? sameValue(x.v, rx.v, 16) : bitEqual(x.v, rx.v, 16));
		}

		F16 mag;
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k) { sum += absf(m1.v[k * 4 + row] * m2.v[col * 4 + k]); }
				mag.v[col * 4 + row] = sum;
			}
		}

		b.mat[2](m1.v, m2.v, x.v);
		ref.mat[2](m1.v, m2.v, rx.v);
		product = product && sameSum(x.v, rx.v, mag.v, 16, productTolerance);

		// in place, the way Mat4::multiply(*this, m, this) calls it
		x = m1;
		b.mat[2](x.v, m2.v, x.v);
		productAliased = productAliased && sameSum(x.v, rx.v, mag.v, 16, productTolerance);

		F4 vmag;
		for (int row = 0; row < 4; ++row) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) { sum += absf(m1.v[k * 4 + row] * v.v[k]); }
			vmag.v[row] = sum;
		}

		b.transform(m1.v, v.v, vx.v);
		ref.transform(m1.v, v.v, rv.v);
		transform = transform && sameSum(vx.v, rv.v, vmag.v, 4, productTolerance);

		b.transformComponents(m1.v, v.v, vx.v);
		ref.transformComponents(m1.v, v.v, rv.v);
		components = components && sameSum(vx.v, rv.v, vmag.v, 4, productTolerance);

		// three floats in, three out: a fourth lane must be neither read nor written
		float c1[3] = {v.v[0], v.v[1], v.v[2]};
		float c2[3] = {s, m2.v[0], m2.v[1]};
		float cx[4] = {0.0f, 0.0f, 0.0f, 42.0f};
		float rc[3];
		b.cross(c1, c2, cx);
		ref.cross(c1, c2, rc);
		const float cmag[3] = {absf(c1[1] * c2[2]) + absf(c1[2] * c2[1]),
			absf(c1[2] * c2[0]) + absf(c1[0] * c2[2]), absf(c1[0] * c2[1]) + absf(c1[1] * c2[0])};
		cross = cross && sameSum(cx, rc, cmag, 3, crossTolerance) && cx[3] == 42.0f;
	}

	check(matScalar[0], b, "addMat4Scalar matches scalar");
	check(matScalar[1], b, "multiplyMat4Scalar matches scalar");
	check(mat[0], b, "addMat4 matches scalar");
	check(mat[1], b, "subtractMat4 matches scalar");
	check(matUnary[0], b, "negateMat4 matches scalar");
	check(matUnary[1], b, "transposeMat4 matches scalar");
	check(product, b, b.fusedProducts ? "multiplyMat4 within the fused rounding of scalar"
									  : "multiplyMat4 matches scalar");
	check(productAliased, b, "multiplyMat4 into its own first operand");
	check(transform, b, "transformVec4 against scalar");
	check(components, b, "transformVec4Components against scalar");
	check(cross, b, "crossVec3 against scalar, three lanes only");

	if (b.fusedProducts) {
		sprt::cout << "  " << b.name << ": worst fused product error " << worstProduct
				   << " of the terms' magnitude\n";
	}
}

static void checkBbox(const SimdBackend &b, const SimdBackend &ref, Random &rnd) {
	bool same = true, isect = true;
	unsigned hits = 0;
	for (int iter = 0; iter < 2'048; ++iter) {
		// small integer grid: plenty of touching and overlapping segments
		F4 s1, s2, x, rx;
		for (int i = 0; i < 4; ++i) {
			s1.v[i] = float(rnd.next() % 9);
			s2.v[i] = float(rnd.next() % 9);
		}
		auto r = b.bbox(s1.v, s2.v, x.v);
		auto rr = ref.bbox(s1.v, s2.v, rx.v);
		hits += rr ? 1 : 0;
		same = same && r == rr;
		isect = isect && bitEqual(x.v, rx.v, 4);
	}
	check(same, b, "isVec2BboxIntersects answers as scalar");
	check(isect, b, "isVec2BboxIntersects direction vectors match scalar");
	check(hits > 0 && hits < 2'048, b, "isVec2BboxIntersects: the cases cover both answers");
}

} // namespace

void performGeomSimdTests() {
	const SimdBackend reference = SP_SIMD_BACKEND(scalar, false, false, 0.0f);

	const SimdBackend backends[] = {
	// SSE through SIMDe: native on x86, NEON on ARM, LSX on LoongArch, portable elsewhere
		SP_SIMD_BACKEND(sse, false, ScalarCodeFused, RsqrtEstimate),
		SP_SIMD_BACKEND(neon, false, ScalarCodeFused, RsqrtEstimate),
#if SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_NEON64
		SP_SIMD_BACKEND(neon64, true, true, RsqrtEstimate),
#else
		SP_SIMD_BACKEND(neon64, false, ScalarCodeFused, RsqrtEstimate),
#endif
#if __loongarch_sx
		SP_SIMD_BACKEND(lsx, false, false, 0.0f),
#endif
	};

	sprt::cout << "geom simd: default backend "
			   << SP_SIMD_STRINGIFY(SP_GEOM_DEFAULT_SIMD_NAMESPACE) << "\n";

#if __loongarch_sx
	// LSX is the default on LoongArch unless the build forces the scalar geometry
	++s_checks;
	if (SP_GEOM_DEFAULT_SIMD != SP_GEOM_DEFAULT_SIMD_LSX
			&& SP_GEOM_DEFAULT_SIMD != SP_GEOM_DEFAULT_SIMD_SCALAR) {
		++s_failures;
		sprt::cerr << sprt::test::failed("  FAIL: ") << "LSX is not the default on LoongArch\n";
	}
#endif

	for (auto &b : backends) {
		Random rnd;
		checkLanes(b, reference, rnd);
		checkMatrices(b, reference, rnd);
		checkBbox(b, reference, rnd);
	}

	sprt::cout << "geom simd tests: " << s_checks << " checks, " << s_failures << " failures\n";
}

} // namespace sprt
