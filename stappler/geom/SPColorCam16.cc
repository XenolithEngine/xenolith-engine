/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#include "SPColorCam16.h"
#include "SPColorHCT.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

struct Cam16Vec3 {
	Cam16Float a;
	Cam16Float b;
	Cam16Float c;
};

constexpr Cam16Float kScaledDiscountFromLinrgb[3][3] = {
	{
		0.001200833568784504,
		0.002389694492170889,
		0.0002795742885861124,
	},
	{
		0.0005891086651375999,
		0.0029785502573438758,
		0.0003270666104008398,
	},
	{
		0.00010146692491640572,
		0.0005364214359186694,
		0.0032979401770712076,
	},
};

constexpr Cam16Float kLinrgbFromScaledDiscount[3][3] = {
	{
		1373.2198709594231,
		-1100.4251190754821,
		-7.278681089101213,
	},
	{
		-271.815969077903,
		559.6580465940733,
		-32.46047482791194,
	},
	{
		1.9622899599665666,
		-57.173814538844006,
		308.7233197812385,
	},
};

constexpr Cam16Float kYFromLinrgb[3] = {0.2126, 0.7152, 0.0722};

constexpr Cam16Float kCriticalPlanes[255] = {
	0.015176349177441876,
	0.045529047532325624,
	0.07588174588720938,
	0.10623444424209313,
	0.13658714259697685,
	0.16693984095186062,
	0.19729253930674434,
	0.2276452376616281,
	0.2579979360165119,
	0.28835063437139563,
	0.3188300904430532,
	0.350925934958123,
	0.3848314933096426,
	0.42057480301049466,
	0.458183274052838,
	0.4976837250274023,
	0.5391024159806381,
	0.5824650784040898,
	0.6277969426914107,
	0.6751227633498623,
	0.7244668422128921,
	0.775853049866786,
	0.829304845476233,
	0.8848452951698498,
	0.942497089126609,
	1.0022825574869039,
	1.0642236851973577,
	1.1283421258858297,
	1.1946592148522128,
	1.2631959812511864,
	1.3339731595349034,
	1.407011200216447,
	1.4823302800086415,
	1.5599503113873272,
	1.6398909516233677,
	1.7221716113234105,
	1.8068114625156377,
	1.8938294463134073,
	1.9832442801866852,
	2.075074464868551,
	2.1693382909216234,
	2.2660538449872063,
	2.36523901573795,
	2.4669114995532007,
	2.5710888059345764,
	2.6777882626779785,
	2.7870270208169257,
	2.898822059350997,
	3.0131901897720907,
	3.1301480604002863,
	3.2497121605402226,
	3.3718988244681087,
	3.4967242352587946,
	3.624204428461639,
	3.754355295633311,
	3.887192587735158,
	4.022731918402185,
	4.160988767090289,
	4.301978482107941,
	4.445716283538092,
	4.592217266055746,
	4.741496401646282,
	4.893568542229298,
	5.048448422192488,
	5.20615066083972,
	5.3666897647573375,
	5.5300801301023865,
	5.696336044816294,
	5.865471690767354,
	6.037501145825082,
	6.212438385869475,
	6.390297286737924,
	6.571091626112461,
	6.7548350853498045,
	6.941541251256611,
	7.131223617812143,
	7.323895587840543,
	7.5195704746346665,
	7.7182615035334345,
	7.919981813454504,
	8.124744458384042,
	8.332562408825165,
	8.543448553206703,
	8.757415699253682,
	8.974476575321063,
	9.194643831691977,
	9.417930041841839,
	9.644347703669503,
	9.873909240696694,
	10.106627003236781,
	10.342513269534024,
	10.58158024687427,
	10.8238400726681,
	11.069304815507364,
	11.317986476196008,
	11.569896988756009,
	11.825048221409341,
	12.083451977536606,
	12.345119996613247,
	12.610063955123938,
	12.878295467455942,
	13.149826086772048,
	13.42466730586372,
	13.702830557985108,
	13.984327217668513,
	14.269168601521828,
	14.55736596900856,
	14.848930523210871,
	15.143873411576273,
	15.44220572664832,
	15.743938506781891,
	16.04908273684337,
	16.35764934889634,
	16.66964922287304,
	16.985093187232053,
	17.30399201960269,
	17.62635644741625,
	17.95219714852476,
	18.281524751807332,
	18.614349837764564,
	18.95068293910138,
	19.290534541298456,
	19.633915083172692,
	19.98083495742689,
	20.331304511189067,
	20.685334046541502,
	21.042933821039977,
	21.404114048223256,
	21.76888489811322,
	22.137256497705877,
	22.50923893145328,
	22.884842241736916,
	23.264076429332462,
	23.6469514538663,
	24.033477234264016,
	24.42366364919083,
	24.817520537484558,
	25.21505769858089,
	25.61628489293138,
	26.021211842414342,
	26.429848230738664,
	26.842203703840827,
	27.258287870275353,
	27.678110301598522,
	28.10168053274597,
	28.529008062403893,
	28.96010235337422,
	29.39497283293396,
	29.83362889318845,
	30.276079891419332,
	30.722335150426627,
	31.172403958865512,
	31.62629557157785,
	32.08401920991837,
	32.54558406207592,
	33.010999283389665,
	33.4802739966603,
	33.953417292456834,
	34.430438229418264,
	34.911345834551085,
	35.39614910352207,
	35.88485700094671,
	36.37747846067349,
	36.87402238606382,
	37.37449765026789,
	37.87891309649659,
	38.38727753828926,
	38.89959975977785,
	39.41588851594697,
	39.93615253289054,
	40.460400508064545,
	40.98864111053629,
	41.520882981230194,
	42.05713473317016,
	42.597404951718396,
	43.141702194811224,
	43.6900349931913,
	44.24241185063697,
	44.798841244188324,
	45.35933162437017,
	45.92389141541209,
	46.49252901546552,
	47.065252796817916,
	47.64207110610409,
	48.22299226451468,
	48.808024568002054,
	49.3971762874833,
	49.9904556690408,
	50.587870934119984,
	51.189430279724725,
	51.79514187861014,
	52.40501387947288,
	53.0190544071392,
	53.637271562750364,
	54.259673423945976,
	54.88626804504493,
	55.517063457223934,
	56.15206766869424,
	56.79128866487574,
	57.43473440856916,
	58.08241284012621,
	58.734331877617365,
	59.39049941699807,
	60.05092333227251,
	60.715611475655585,
	61.38457167773311,
	62.057811747619894,
	62.7353394731159,
	63.417162620860914,
	64.10328893648692,
	64.79372614476921,
	65.48848194977529,
	66.18756403501224,
	66.89098006357258,
	67.59873767827808,
	68.31084450182222,
	69.02730813691093,
	69.74813616640164,
	70.47333615344107,
	71.20291564160104,
	71.93688215501312,
	72.67524319850172,
	73.41800625771542,
	74.16517879925733,
	74.9167682708136,
	75.67278210128072,
	76.43322770089146,
	77.1981124613393,
	77.96744375590167,
	78.74122893956174,
	79.51947534912904,
	80.30219030335869,
	81.08938110306934,
	81.88105503125999,
	82.67721935322541,
	83.4778813166706,
	84.28304815182372,
	85.09272707154808,
	85.90692527145302,
	86.72564993000343,
	87.54890820862819,
	88.3767072518277,
	89.2090541872801,
	90.04595612594655,
	90.88742016217518,
	91.73345337380438,
	92.58406282226491,
	93.43925555268066,
	94.29903859396902,
	95.16341895893969,
	96.03240364439274,
	96.9059996312159,
	97.78421388448044,
	98.6670533535366,
	99.55452497210776,
};

static float Delinearized(const Cam16Float rgb_component) {
	Cam16Float normalized = rgb_component / 100;
	float ret;
	if (normalized <= 0.0031308) {
		ret = normalized * 12.92;
	} else {
		ret = 1.055 * std::pow(normalized, Cam16Float(1.0 / 2.4)) - 0.055;
	}
	return ret;
}

static Color4F Color4FFromLinrgb(Cam16Vec3 linrgb) {
	return Color4F(Delinearized(linrgb.a), Delinearized(linrgb.b), Delinearized(linrgb.c), 1.0f);
}

static Cam16Vec3 MatrixMultiply(Cam16Vec3 input, const Cam16Float matrix[3][3]) {
	Cam16Float a = input.a * matrix[0][0] + input.b * matrix[0][1] + input.c * matrix[0][2];
	Cam16Float b = input.a * matrix[1][0] + input.b * matrix[1][1] + input.c * matrix[1][2];
	Cam16Float c = input.a * matrix[2][0] + input.b * matrix[2][1] + input.c * matrix[2][2];
	return {a, b, c};
}

static Cam16Float GetAxis(Cam16Vec3 vector, int axis) {
	switch (axis) {
	case 0: return vector.a;
	case 1: return vector.b;
	case 2: return vector.c;
	default: return -1.0;
	}
}

/**
 * Solves the lerp equation.
 *
 * @param source The starting number.
 * @param mid The number in the middle.
 * @param target The ending number.
 * @return A number t such that lerp(source, target, t) = mid.
 */
Cam16Float Intercept(Cam16Float source, Cam16Float mid, Cam16Float target) {
	return (mid - source) / (target - source);
}

Cam16Vec3 LerpPoint(Cam16Vec3 source, Cam16Float t, Cam16Vec3 target) {
	return {
		source.a + (target.a - source.a) * t,
		source.b + (target.b - source.b) * t,
		source.c + (target.c - source.c) * t,
	};
}

/**
 * Intersects a segment with a plane.
 *
 * @param source The coordinates of point A.
 * @param coordinate The R-, G-, or B-coordinate of the plane.
 * @param target The coordinates of point B.
 * @param axis The axis the plane is perpendicular with. (0: R, 1: G, 2: B)
 * @return The intersection point of the segment AB with the plane R=coordinate,
 * G=coordinate, or B=coordinate
 */
static Cam16Vec3 SetCoordinate(Cam16Vec3 source, Cam16Float coordinate, Cam16Vec3 target,
		int axis) {
	Cam16Float t = Intercept(GetAxis(source, axis), coordinate, GetAxis(target, axis));
	return LerpPoint(source, t, target);
}

static bool IsBounded(Cam16Float x) { return 0.0 <= x && x <= 100.0; }

static Cam16Float ChromaticAdaptation(Cam16Float component) {
	Cam16Float af = std::pow(abs(component), Cam16Float(0.42));
	return Cam16::signum(component) * 400.0 * af / (af + 27.13);
}

/**
 * Returns the hue of a linear RGB color in CAM16.
 *
 * @param linrgb The linear RGB coordinates of a color.
 * @return The hue of the color in CAM16, in radians.
 */
static Cam16Float HueOf(Cam16Vec3 linrgb) {
	Cam16Vec3 scaledDiscount = MatrixMultiply(linrgb, kScaledDiscountFromLinrgb);
	Cam16Float r_a = ChromaticAdaptation(scaledDiscount.a);
	Cam16Float g_a = ChromaticAdaptation(scaledDiscount.b);
	Cam16Float b_a = ChromaticAdaptation(scaledDiscount.c);
	// redness-greenness
	Cam16Float a = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
	// yellowness-blueness
	Cam16Float b = (r_a + g_a - 2.0 * b_a) / 9.0;
	return std::atan2(b, a);
}

/**
 * Returns the nth possible vertex of the polygonal intersection.
 *
 * @param y The Y value of the plane.
 * @param n The zero-based index of the point. 0 <= n <= 11.
 * @return The nth possible vertex of the polygonal intersection of the y plane
 * and the RGB cube, in linear RGB coordinates, if it exists. If this possible
 * vertex lies outside of the cube,
 *     [-1.0, -1.0, -1.0] is returned.
 */
Cam16Vec3 NthVertex(Cam16Float y, int n) {
	const Cam16Float k_r = kYFromLinrgb[0];
	const Cam16Float k_g = kYFromLinrgb[1];
	const Cam16Float k_b = kYFromLinrgb[2];
	const Cam16Float coord_a = n % 4 <= 1 ? 0.0 : 100.0;
	const Cam16Float coord_b = n % 2 == 0 ? 0.0 : 100.0;
	if (n < 4) {
		const Cam16Float g = coord_a;
		const Cam16Float b = coord_b;
		const Cam16Float r = (y - g * k_g - b * k_b) / k_r;
		if (IsBounded(r)) {
			return {r, g, b};
		} else {
			return {-1.0, -1.0, -1.0};
		}
	} else if (n < 8) {
		const Cam16Float b = coord_a;
		const Cam16Float r = coord_b;
		const Cam16Float g = (y - r * k_r - b * k_b) / k_g;
		if (IsBounded(g)) {
			return {r, g, b};
		} else {
			return {-1.0, -1.0, -1.0};
		}
	} else {
		const Cam16Float r = coord_a;
		const Cam16Float g = coord_b;
		const Cam16Float b = (y - r * k_r - g * k_g) / k_b;
		if (IsBounded(b)) {
			return {r, g, b};
		} else {
			return {-1.0, -1.0, -1.0};
		}
	}
}

/**
 * Sanitizes a small enough angle in radians.
 *
 * @param angle An angle in radians; must not deviate too much from 0.
 * @return A coterminal angle between 0 and 2pi.
 */
static Cam16Float SanitizeRadians(Cam16Float angle) {
	return std::fmod(angle + numbers::pi * 8, numbers::pi * 2);
}

static bool AreInCyclicOrder(Cam16Float a, Cam16Float b, Cam16Float c) {
	Cam16Float delta_a_b = SanitizeRadians(b - a);
	Cam16Float delta_a_c = SanitizeRadians(c - a);
	return delta_a_b < delta_a_c;
}

/**
 * Finds the segment containing the desired color.
 *
 * @param y The Y value of the color.
 * @param target_hue The hue of the color.
 * @param out return a list of two sets of linear RGB coordinates, each corresponding to
 * an endpoint of the segment containing the desired color.
 */
void BisectToSegment(Cam16Float y, Cam16Float target_hue, Cam16Vec3 out[2]) {
	Cam16Vec3 left = {-1.0, -1.0, -1.0};
	Cam16Vec3 right = left;
	Cam16Float left_hue = 0.0;
	Cam16Float right_hue = 0.0;
	bool initialized = false;
	bool uncut = true;
	for (int n = 0; n < 12; n++) {
		Cam16Vec3 mid = NthVertex(y, n);
		if (mid.a < 0) {
			continue;
		}
		Cam16Float mid_hue = HueOf(mid);
		if (!initialized) {
			left = mid;
			right = mid;
			left_hue = mid_hue;
			right_hue = mid_hue;
			initialized = true;
			continue;
		}
		if (uncut || AreInCyclicOrder(left_hue, mid_hue, right_hue)) {
			uncut = false;
			if (AreInCyclicOrder(left_hue, target_hue, mid_hue)) {
				right = mid;
				right_hue = mid_hue;
			} else {
				left = mid;
				left_hue = mid_hue;
			}
		}
	}
	out[0] = left;
	out[1] = right;
}

/**
 * Delinearizes an RGB component, returning a floating-point number.
 *
 * @param rgb_component 0.0 <= rgb_component <= 100.0, represents linear R/G/B
 * channel
 * @return 0.0 <= output <= 255.0, color channel converted to regular RGB space
 */
static Cam16Float TrueDelinearized(Cam16Float rgb_component) {
	Cam16Float normalized = rgb_component / 100.0;
	Cam16Float delinearized = 0.0;
	if (normalized <= 0.0031308) {
		delinearized = normalized * 12.92;
	} else {
		delinearized = 1.055 * std::pow(normalized, Cam16Float(1.0 / 2.4)) - 0.055;
	}
	return delinearized * 255.0;
}

static int CriticalPlaneBelow(Cam16Float x) { return (int)std::floor(x - Cam16Float(0.5)); }

static int CriticalPlaneAbove(Cam16Float x) { return (int)std::ceil(x - Cam16Float(0.5)); }

static Cam16Vec3 Midpoint(Cam16Vec3 a, Cam16Vec3 b) {
	return {
		(a.a + b.a) / 2,
		(a.b + b.b) / 2,
		(a.c + b.c) / 2,
	};
}


/**
 * Finds a color with the given Y and hue on the boundary of the cube.
 *
 * @param y The Y value of the color.
 * @param target_hue The hue of the color.
 * @return The desired color, in linear RGB coordinates.
 */
static Cam16Vec3 BisectToLimit(Cam16Float y, Cam16Float target_hue) {
	Cam16Vec3 segment[2];
	BisectToSegment(y, target_hue, segment);
	Cam16Vec3 left = segment[0];
	Cam16Float left_hue = HueOf(left);
	Cam16Vec3 right = segment[1];
	for (int axis = 0; axis < 3; axis++) {
		if (GetAxis(left, axis) != GetAxis(right, axis)) {
			int l_plane = -1;
			int r_plane = 255;
			if (GetAxis(left, axis) < GetAxis(right, axis)) {
				l_plane = CriticalPlaneBelow(TrueDelinearized(GetAxis(left, axis)));
				r_plane = CriticalPlaneAbove(TrueDelinearized(GetAxis(right, axis)));
			} else {
				l_plane = CriticalPlaneAbove(TrueDelinearized(GetAxis(left, axis)));
				r_plane = CriticalPlaneBelow(TrueDelinearized(GetAxis(right, axis)));
			}
			for (int i = 0; i < 8; i++) {
				if (abs(r_plane - l_plane) <= 1) {
					break;
				} else {
					int m_plane = (int)floor((l_plane + r_plane) / 2.0);
					Cam16Float mid_plane_coordinate = kCriticalPlanes[m_plane];
					Cam16Vec3 mid = SetCoordinate(left, mid_plane_coordinate, right, axis);
					Cam16Float mid_hue = HueOf(mid);
					if (AreInCyclicOrder(left_hue, target_hue, mid_hue)) {
						right = mid;
						r_plane = m_plane;
					} else {
						left = mid;
						left_hue = mid_hue;
						l_plane = m_plane;
					}
				}
			}
		}
	}
	return Midpoint(left, right);
}

static Cam16Float InverseChromaticAdaptation(Cam16Float adapted) {
	Cam16Float adapted_abs = abs(adapted);
	Cam16Float base =
			std::fmax(Cam16Float(0), Cam16Float(27.13 * adapted_abs / (400.0 - adapted_abs)));
	return Cam16::signum(adapted) * std::pow(base, Cam16Float(1.0 / 0.42));
}

static Color4F FindResultByJ(Cam16Float hue_radians, Cam16Float chroma, Cam16Float y) {
	// Initial estimate of j.
	Cam16Float j = std::sqrt(y) * 11.0;
	// ===========================================================
	// Operations inlined from Cam16 to avoid repeated calculation
	// ===========================================================
	const Cam16Float t_inner_coeff = 1
			/ std::pow(Cam16Float(1.64)
							- std::pow(Cam16Float(0.29),
									ViewingConditions::DEFAULT.background_y_to_white_point_y),
					Cam16Float(0.73));
	const Cam16Float e_hue = 0.25 * (std::cos(hue_radians + Cam16Float(2.0)) + 3.8);
	const Cam16Float p1 = e_hue * (50000.0 / 13.0) * ViewingConditions::DEFAULT.n_c
			* ViewingConditions::DEFAULT.ncb;
	const Cam16Float h_sin = std::sin(hue_radians);
	const Cam16Float h_cos = std::cos(hue_radians);
	for (int iteration_round = 0; iteration_round < 5; ++iteration_round) {
		// ===========================================================
		// Operations inlined from Cam16 to avoid repeated calculation
		// ===========================================================
		Cam16Float j_normalized = j / 100.0;
		Cam16Float alpha = chroma == 0.0 || j == 0.0 ? 0.0 : chroma / sqrt(j_normalized);
		Cam16Float t = std::pow(alpha * t_inner_coeff, Cam16Float(1.0 / 0.9));
		Cam16Float ac = ViewingConditions::DEFAULT.aw
				* std::pow(j_normalized,
						Cam16Float(1.0) / ViewingConditions::DEFAULT.c
								/ ViewingConditions::DEFAULT.z);
		Cam16Float p2 = ac / ViewingConditions::DEFAULT.nbb;
		Cam16Float gamma =
				23.0 * (p2 + 0.305) * t / (23.0 * p1 + 11 * t * h_cos + 108.0 * t * h_sin);
		Cam16Float a = gamma * h_cos;
		Cam16Float b = gamma * h_sin;
		Cam16Float r_a = (460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0;
		Cam16Float g_a = (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0;
		Cam16Float b_a = (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0;
		Cam16Float r_c_scaled = InverseChromaticAdaptation(r_a);
		Cam16Float g_c_scaled = InverseChromaticAdaptation(g_a);
		Cam16Float b_c_scaled = InverseChromaticAdaptation(b_a);
		Cam16Vec3 scaled{r_c_scaled, g_c_scaled, b_c_scaled};
		Cam16Vec3 linrgb = MatrixMultiply(scaled, kLinrgbFromScaledDiscount);
		// ===========================================================
		// Operations inlined from Cam16 to avoid repeated calculation
		// ===========================================================
		if (linrgb.a < 0 || linrgb.b < 0 || linrgb.c < 0) {
			return Color4F::BLACK;
		}
		Cam16Float k_r = kYFromLinrgb[0];
		Cam16Float k_g = kYFromLinrgb[1];
		Cam16Float k_b = kYFromLinrgb[2];
		Cam16Float fnj = k_r * linrgb.a + k_g * linrgb.b + k_b * linrgb.c;
		if (fnj <= 0) {
			return Color4F::BLACK;
		}
		if (iteration_round == 4 || abs(fnj - y) < 0.002) {
			if (linrgb.a > 100.01 || linrgb.b > 100.01 || linrgb.c > 100.01) {
				return Color4F::BLACK;
			}
			return Color4FFromLinrgb(linrgb);
		}
		// Iterates with Newton method,
		// Using 2 * fn(j) / j as the approximation of fn'(j)
		j = j - (fnj - y) * j / (2 * fnj);
	}
	return Color4F::BLACK;
}

static Color4F Color4FFromLstar(const Cam16Float lstar) {
	auto y = ViewingConditions::YFromLstar(lstar);
	auto component = Delinearized(y);
	return Color4F(component, component, component, 1.0f);
}

static float logn(float x, float k) {
	float answer;
	answer = std::log10(x) / std::log10(k);
	return answer;
}

static void fixTone(Cam16Float &h, Cam16Float &c, Cam16Float &t, Color4F &color) {
	const Cam16Float hueOffset = 109.0f;
	const Cam16Float hueRange = 30.0f;
	const Cam16Float toneOffset = 97.0f;

	if (h >= hueOffset && h <= hueOffset + hueRange && t > toneOffset) {
		const float tone = (t - toneOffset) / (100.0f - toneOffset);
		const float val = (h - hueOffset) / hueRange * 5.0;
		const float log = logn(val, 2.0f);
		const float p = std::pow(2.0f, -(log * log));
		const float q = (p * 0.95f * std::sqrt(tone));

		color.r = std::pow(color.r, 1.0f - q);
		color.g = std::pow(color.g, 1.0f - q);
		color.b = std::pow(color.b, 1.0f - q);
	}
}

static Color4F SolveToColor4F(Cam16Float hue_degrees, Cam16Float chroma, Cam16Float lstar) {
	if (chroma < 0.0001 || lstar < 0.0001 || lstar > 99.9999) {
		return Color4FFromLstar(lstar);
	}

	hue_degrees = Cam16::sanitizeDegrees(hue_degrees);
	//fixTone(hue_degrees, chroma, lstar);
	Cam16Float hue_radians = hue_degrees / 180 * numbers::pi;
	Cam16Float y = ViewingConditions::YFromLstar(lstar);
	Color4F exact_answer = FindResultByJ(hue_radians, chroma, y);
	if (exact_answer != Color4F::BLACK) {
		return exact_answer;
	}
	Cam16Vec3 linrgb = BisectToLimit(y, hue_radians);
	auto ret = Color4FFromLinrgb(linrgb);
	fixTone(hue_degrees, chroma, lstar, ret);
	return ret;
}

ColorHCT ColorHCT::progress(const ColorHCT &a, const ColorHCT &b, float p) {
	return ColorHCT((a.data.hue * (1.0f - p) + b.data.hue * p),
			(a.data.chroma * (1.0f - p) + b.data.chroma * p),
			(a.data.tone * (1.0f - p) + b.data.tone * p),
			(a.data.alpha * (1.0f - p) + b.data.alpha * p));
}

ColorHCT ColorHCT::solveColorHCT(Cam16Float h, Cam16Float c, Cam16Float t, float a) {
	auto tmp = SolveToColor4F(h, c, t);
	tmp.a = a;
	return ColorHCT(tmp);
}

Color4F ColorHCT::solveColor4F(Cam16Float h, Cam16Float c, Cam16Float t, float a) {
	auto tmp = SolveToColor4F(h, c, t);
	tmp.a = a;
	return tmp;
}

std::ostream &operator<<(std::ostream &stream, const ColorHCT &obj) {
	stream << "ColorHCT(h:" << obj.data.hue << " c:" << obj.data.chroma << " t:" << obj.data.tone
		   << " a:" << obj.data.alpha << ");";
	return stream;
}

} // namespace stappler::geom
