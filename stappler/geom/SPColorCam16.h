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

#ifndef CORE_GEOM_SPCOLORCAM16_H_
#define CORE_GEOM_SPCOLORCAM16_H_

#include "SPColor.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

// Использовать float или double для расчётов Cam16
using Cam16Float = float;

struct SP_PUBLIC ViewingConditions {
	static const ViewingConditions DEFAULT;

	using Float = Cam16Float;

	static constexpr Float YFromLstar(Float lstar) {
		if (lstar > 8.0) {
			Float cube_root = (lstar + 16.0) / 116.0;
			Float cube = cube_root * cube_root * cube_root;
			return cube * 100.0;
		} else {
			return lstar / (24389.0 / 27.0) * 100.0;
		}
	}

	static constexpr ViewingConditions create(const Float _white_point[3],
			const Float _adapting_luminance, const Float _background_lstar, const Float _surround,
			const bool _discounting_illuminant) {
		const Float background_lstar_corrected =
				(_background_lstar < 30.0) ? 30.0 : _background_lstar;
		const Float rgb_w[3] = {Float(0.401288 * _white_point[0] + 0.650173 * _white_point[1]
										- 0.051461 * _white_point[2]),
			Float(-0.250268 * _white_point[0] + 1.204414 * _white_point[1]
					+ 0.045854 * _white_point[2]),
			Float(-0.002079 * _white_point[0] + 0.048952 * _white_point[1]
					+ 0.953127 * _white_point[2])};
		const Float f = 0.8 + (_surround / 10.0);
		const Float _c = f >= 0.9 ? std::lerp(Float(0.59), Float(0.69), Float((f - 0.9) * 10.0))
								  : std::lerp(Float(0.525), Float(0.59), Float((f - 0.8) * 10.0));
		Float d = _discounting_illuminant
				? 1.0
				: f * (1.0 - ((1.0 / 3.6) * std::exp(Float((-_adapting_luminance - 42.0) / 92.0))));
		d = d > 1.0 ? 1.0 : d < 0.0 ? 0.0 : d;
		const Float nc = f;
		const Float _rgb_d[3] = {Float(d * (100.0 / rgb_w[0]) + 1.0 - d),
			Float(d * (100.0 / rgb_w[1]) + 1.0 - d), Float(d * (100.0 / rgb_w[2]) + 1.0 - d)};

		const Float k = 1.0 / (5.0 * _adapting_luminance + 1.0);
		const Float k4 = k * k * k * k;
		const Float k4f = 1.0 - k4;
		const Float _fl = (k4 * _adapting_luminance)
				+ (0.1 * k4f * k4f * std::pow(Float(5.0 * _adapting_luminance), Float(1.0 / 3.0)));
		const Float _fl_root = std::pow(Float(_fl), Float(0.25));
		const Float n = YFromLstar(background_lstar_corrected) / _white_point[1];
		const Float _z = 1.48 + std::sqrt(Float(n));
		const Float _nbb = 0.725 / std::pow(Float(n), Float(0.2));
		const Float _ncb = _nbb;
		const Float rgb_a_factors[3] = {
			std::pow(Float(_fl * _rgb_d[0] * rgb_w[0] / 100.0), Float(0.42)),
			std::pow(Float(_fl * _rgb_d[1] * rgb_w[1] / 100.0), Float(0.42)),
			std::pow(Float(_fl * _rgb_d[2] * rgb_w[2] / 100.0), Float(0.42))};
		const Float rgb_a[3] = {Float(400.0 * rgb_a_factors[0] / (rgb_a_factors[0] + 27.13)),
			Float(400.0 * rgb_a_factors[1] / (rgb_a_factors[1] + 27.13)),
			Float(400.0 * rgb_a_factors[2] / (rgb_a_factors[2] + 27.13))};
		const Float _aw = (40.0 * rgb_a[0] + 20.0 * rgb_a[1] + rgb_a[2]) / 20.0 * _nbb;
		return ViewingConditions{_adapting_luminance, background_lstar_corrected, _surround,
			_discounting_illuminant, n, _aw, _nbb, _ncb, _c, nc, _fl, _fl_root, _z,
			{_white_point[0], _white_point[1], _white_point[2]}, {_rgb_d[0], _rgb_d[1], _rgb_d[2]}};
	}

	Float adapting_luminance = 0.0;
	Float background_lstar = 0.0;
	Float surround = 0.0;
	bool discounting_illuminant = false;
	Float background_y_to_white_point_y = 0.0;
	Float aw = 0.0;
	Float nbb = 0.0;
	Float ncb = 0.0;
	Float c = 0.0;
	Float n_c = 0.0;
	Float fl = 0.0;
	Float fl_root = 0.0;
	Float z = 0.0;
	Float white_point[3] = {0.0, 0.0, 0.0};
	Float rgb_d[3] = {0.0, 0.0, 0.0};
};

constexpr ViewingConditions ViewingConditions::DEFAULT = {
	11.725676537,
	50.000000000,
	2.000000000,
	false,
	0.184186503,
	29.981000900,
	1.016919255,
	1.016919255,
	0.689999998,
	1.000000000,
	0.388481468,
	0.789482653,
	1.909169555,
	{95.047, 100.0, 108.883},
	{1.021177769, 0.986307740, 0.933960497},
};

struct SP_PUBLIC Cam16 {
	using Float = ViewingConditions::Float;

	static constexpr Float linearized(const int rgb_component) {
		Float normalized = rgb_component / 255.0;
		if (normalized <= 0.040449936) {
			return normalized / 12.92 * 100.0;
		} else {
			return std::pow(Float((normalized + 0.055) / 1.055), Float(2.4)) * 100.0;
		}
	}

	static constexpr Float linearized(const Float normalized) {
		if (normalized <= 0.040449936) {
			return normalized / 12.92 * 100.0;
		} else {
			return std::pow(Float((normalized + 0.055) / 1.055), Float(2.4)) * 100.0;
		}
	}

	static constexpr Float sanitizeDegrees(const Float degrees) {
		if (degrees < 0.0) {
			return std::fmod(degrees, Float(360.0)) + 360;
		} else if (degrees >= 360.0) {
			return std::fmod(degrees, Float(360.0));
		} else {
			return degrees;
		}
	}

	static constexpr int signum(Float num) {
		if (num < 0) {
			return -1;
		} else if (num == 0) {
			return 0;
		} else {
			return 1;
		}
	}

	static constexpr Cam16 create(const Color4F &color,
			const ViewingConditions &viewing_conditions) {
		const Float red_l = linearized(color.a);
		const Float green_l = linearized(color.g);
		const Float blue_l = linearized(color.b);

		const Float x = 0.41233895 * red_l + 0.35762064 * green_l + 0.18051042 * blue_l;
		const Float y = 0.2126 * red_l + 0.7152 * green_l + 0.0722 * blue_l;
		const Float z = 0.01932141 * red_l + 0.11916382 * green_l + 0.95034478 * blue_l;

		// Convert XYZ to 'cone'/'rgb' responses
		const Float r_c = 0.401288 * x + 0.650173 * y - 0.051461 * z;
		const Float g_c = -0.250268 * x + 1.204414 * y + 0.045854 * z;
		const Float b_c = -0.002079 * x + 0.048952 * y + 0.953127 * z;

		// Discount illuminant.
		const Float r_d = viewing_conditions.rgb_d[0] * r_c;
		const Float g_d = viewing_conditions.rgb_d[1] * g_c;
		const Float b_d = viewing_conditions.rgb_d[2] * b_c;

		// Chromatic adaptation.
		const Float r_af =
				std::pow(Float(viewing_conditions.fl * std::fabs(r_d) / 100.0), Float(0.42));
		const Float g_af =
				std::pow(Float(viewing_conditions.fl * std::fabs(g_d) / 100.0), Float(0.42));
		const Float b_af =
				std::pow(Float(viewing_conditions.fl * std::fabs(b_d) / 100.0), Float(0.42));
		const Float r_a = signum(r_d) * 400.0 * r_af / (r_af + 27.13);
		const Float g_a = signum(g_d) * 400.0 * g_af / (g_af + 27.13);
		const Float b_a = signum(b_d) * 400.0 * b_af / (b_af + 27.13);

		// Redness-greenness
		const Float a = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
		const Float b = (r_a + g_a - 2.0 * b_a) / 9.0;
		const Float u = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
		const Float p2 = (40.0 * r_a + 20.0 * g_a + b_a) / 20.0;

		const Float radians = std::atan2(b, a);
		const Float degrees = radians * 180.0 / numbers::pi;
		const Float _hue = sanitizeDegrees(degrees);
		const Float hue_radians = _hue * numbers::pi / 180.0;
		const Float ac = p2 * viewing_conditions.nbb;

		const Float _j = 100.0
				* std::pow(ac / viewing_conditions.aw, viewing_conditions.c * viewing_conditions.z);
		const Float _q = (4.0 / viewing_conditions.c) * std::sqrt(Float(_j / 100.0))
				* (viewing_conditions.aw + 4.0) * viewing_conditions.fl_root;
		const Float hue_prime = _hue < 20.14 ? _hue + 360 : _hue;
		const Float e_hue = 0.25 * (std::cos(Float(hue_prime * numbers::pi / 180.0 + 2.0) + 3.8));
		const Float p1 = 50000.0 / 13.0 * e_hue * viewing_conditions.n_c * viewing_conditions.ncb;
		const Float t = p1 * std::sqrt(a * a + b * b) / (u + Float(0.305));
		const Float alpha = std::pow(t, Float(0.9))
				* std::pow(Float(1.64)
								- std::pow(Float(0.29),
										viewing_conditions.background_y_to_white_point_y),
						Float(0.73));
		const Float c = alpha * std::sqrt(_j / Float(100.0));
		const Float _m = c * viewing_conditions.fl_root;
		const Float _s =
				50.0 * sqrt((alpha * viewing_conditions.c) / (viewing_conditions.aw + 4.0));
		const Float _jstar = (1.0 + 100.0 * 0.007) * _j / (1.0 + 0.007 * _j);
		const Float mstar = 1.0 / 0.0228 * std::log(Float(1.0 + 0.0228 * _m));
		const Float _astar = mstar * std::cos(hue_radians);
		const Float _bstar = mstar * std::sin(hue_radians);
		return Cam16{_hue, c, _j, _q, _m, _s, _jstar, _astar, _bstar};
	}

	static constexpr Float LstarFromY(Float y) {
		Float yNormalized = y / 100.0;
		if (yNormalized <= 216.0 / 24389.0) {
			return (24389.0 / 27.0) * yNormalized;
		} else {
			return 116.0 * std::pow(Float(yNormalized), Float(1.0 / 3.0)) - 16.0;
		}
	}

	static constexpr Float LstarFromColor4F(const Color4F &color) {
		const Float red_l = linearized(color.r);
		const Float green_l = linearized(color.g);
		const Float blue_l = linearized(color.b);
		const Float y = 0.2126 * red_l + 0.7152 * green_l + 0.0722 * blue_l;
		return LstarFromY(y);
	}

	// ViewingConditions::DEFAULT is constexpr, some calculation may be optimized

	static Cam16 create(const Color4F &color) {
		const Float red_l = linearized(color.r);
		const Float green_l = linearized(color.g);
		const Float blue_l = linearized(color.b);

		const Float x = 0.41233895 * red_l + 0.35762064 * green_l + 0.18051042 * blue_l;
		const Float y = 0.2126 * red_l + 0.7152 * green_l + 0.0722 * blue_l;
		const Float z = 0.01932141 * red_l + 0.11916382 * green_l + 0.95034478 * blue_l;

		// Convert XYZ to 'cone'/'rgb' responses
		const Float r_c = 0.401288 * x + 0.650173 * y - 0.051461 * z;
		const Float g_c = -0.250268 * x + 1.204414 * y + 0.045854 * z;
		const Float b_c = -0.002079 * x + 0.048952 * y + 0.953127 * z;

		// Discount illuminant.
		const Float r_d = ViewingConditions::DEFAULT.rgb_d[0] * r_c;
		const Float g_d = ViewingConditions::DEFAULT.rgb_d[1] * g_c;
		const Float b_d = ViewingConditions::DEFAULT.rgb_d[2] * b_c;

		// Chromatic adaptation.
		const Float r_af = std::pow(Float(ViewingConditions::DEFAULT.fl * std::fabs(r_d) / 100.0),
				Float(0.42));
		const Float g_af = std::pow(Float(ViewingConditions::DEFAULT.fl * std::fabs(g_d) / 100.0),
				Float(0.42));
		const Float b_af = std::pow(Float(ViewingConditions::DEFAULT.fl * std::fabs(b_d) / 100.0),
				Float(0.42));
		const Float r_a = signum(r_d) * 400.0 * r_af / (r_af + 27.13);
		const Float g_a = signum(g_d) * 400.0 * g_af / (g_af + 27.13);
		const Float b_a = signum(b_d) * 400.0 * b_af / (b_af + 27.13);

		// Redness-greenness
		const Float a = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
		const Float b = (r_a + g_a - 2.0 * b_a) / 9.0;
		const Float u = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
		const Float p2 = (40.0 * r_a + 20.0 * g_a + b_a) / 20.0;

		const Float radians = std::atan2(b, a);
		const Float degrees = radians * 180.0 / numbers::pi;
		const Float _hue = sanitizeDegrees(degrees);
		const Float hue_radians = _hue * numbers::pi / 180.0;
		const Float ac = p2 * ViewingConditions::DEFAULT.nbb;

		const Float _j = 100.0
				* std::pow(ac / ViewingConditions::DEFAULT.aw,
						ViewingConditions::DEFAULT.c * ViewingConditions::DEFAULT.z);
		const Float _q = (4.0 / ViewingConditions::DEFAULT.c) * std::sqrt(Float(_j / 100.0))
				* (ViewingConditions::DEFAULT.aw + 4.0) * ViewingConditions::DEFAULT.fl_root;
		const Float hue_prime = _hue < 20.14 ? _hue + 360 : _hue;
		const Float e_hue = 0.25 * (std::cos(Float(hue_prime * numbers::pi / 180.0 + 2.0)) + 3.8);
		const Float p1 = 50000.0 / 13.0 * e_hue * ViewingConditions::DEFAULT.n_c
				* ViewingConditions::DEFAULT.ncb;
		const Float t = p1 * std::sqrt(a * a + b * b) / (u + Float(0.305));
		const Float tmpA = std::pow(Float(1.64)
						- std::pow(Float(0.29),
								ViewingConditions::DEFAULT.background_y_to_white_point_y),
				Float(0.73));
		const Float tmpB = std::pow(t, Float(0.9));
		const Float alpha = tmpB * tmpA;
		const Float c = alpha * std::sqrt(_j / Float(100.0));
		const Float _m = c * ViewingConditions::DEFAULT.fl_root;
		const Float _s = 50.0
				* sqrt((alpha * ViewingConditions::DEFAULT.c)
						/ (ViewingConditions::DEFAULT.aw + 4.0));
		const Float _jstar = (1.0 + 100.0 * 0.007) * _j / (1.0 + 0.007 * _j);
		const Float mstar = 1.0 / 0.0228 * std::log(Float(1.0 + 0.0228 * _m));
		const Float _astar = mstar * std::cos(hue_radians);
		const Float _bstar = mstar * std::sin(hue_radians);
		return Cam16{_hue, c, _j, _q, _m, _s, _jstar, _astar, _bstar};
	}

	Float hue = 0.0;
	Float chroma = 0.0;
	Float j = 0.0;
	Float q = 0.0;
	Float m = 0.0;
	Float s = 0.0;

	Float jstar = 0.0;
	Float astar = 0.0;
	Float bstar = 0.0;
};

} // namespace stappler::geom

#endif /* CORE_GEOM_SPCOLORCAM16_H_ */
