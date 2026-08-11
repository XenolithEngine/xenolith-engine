/**
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include <sprt/runtime/geom/geom.h>
#include <sprt/runtime/geom/mat4.h>

namespace sprt::geom {

namespace {

using WhiteSpaceGroup = StringView::CharGroup<CharGroupId::WhiteSpace>;

// Matches a css unit suffix at the START of `str`, advancing it past the suffix. `scale` is what
// the number must be multiplied by to reach the returned unit: the absolute units (pt/pc/mm/cm/in)
// and `%` all normalise onto another one. False when no known unit follows.
//
// Longest match first where suffixes share a prefix (dppx/dpcm before dpi), so a unit is never
// half-consumed.
bool readMetricUnitPrefix(StringView &str, bool resolutionMetric, Metric::Units &unit,
		float &scale) {
	auto take = [&](StringView suffix, Metric::Units u, float s) {
		str += suffix.size();
		unit = u;
		scale = s;
		return true;
	};

	if (!resolutionMetric) {
		if (str.starts_with("%")) {
			return take("%", Metric::Units::Percent, 1.0f / 100.0f);
		} else if (str.starts_with("rem")) {
			return take("rem", Metric::Units::Rem, 1.0f);
		} else if (str.starts_with("em")) {
			return take("em", Metric::Units::Em, 1.0f);
		} else if (str.starts_with("px")) {
			return take("px", Metric::Units::Px, 1.0f);
		} else if (str.starts_with("pt")) {
			return take("pt", Metric::Units::Px, 4.0f / 3.0f);
		} else if (str.starts_with("pc")) {
			return take("pc", Metric::Units::Px, 15.0f);
		} else if (str.starts_with("mm")) {
			return take("mm", Metric::Units::Px, 3.543307f);
		} else if (str.starts_with("cm")) {
			return take("cm", Metric::Units::Px, 35.43307f);
		} else if (str.starts_with("in")) {
			return take("in", Metric::Units::Px, 90.0f);
		} else if (str.starts_with("vmin")) {
			return take("vmin", Metric::Units::VMin, 1.0f);
		} else if (str.starts_with("vmax")) {
			return take("vmax", Metric::Units::VMax, 1.0f);
		} else if (str.starts_with("vw")) {
			return take("vw", Metric::Units::Vw, 1.0f);
		} else if (str.starts_with("vh")) {
			return take("vh", Metric::Units::Vh, 1.0f);
		}
	} else {
		if (str.starts_with("dppx")) {
			return take("dppx", Metric::Units::Dppx, 1.0f);
		} else if (str.starts_with("dpcm")) {
			return take("dpcm", Metric::Units::Dpi, 1.0f / 2.54f);
		} else if (str.starts_with("dpi")) {
			return take("dpi", Metric::Units::Dpi, 1.0f);
		}
	}
	return false;
}

// One operand of a calc() expression while it is being folded. A css calc term is either a plain
// number or a dimension, and the two obey different arithmetic — hence the flag rather than a
// dedicated "unitless" member of Units.
struct CalcTerm {
	float value = 0.0f;
	Metric::Units unit = Metric::Units::Px;
	bool unitless = true;
};

// Parenthesis nesting bound: a cheap guard against a pathological expression, mirroring the depth
// limit expandCssVariables uses for var() recursion.
constexpr uint32_t CalcMaxDepth = 16;

bool calcExpr(StringView &r, bool resolutionMetric, uint32_t depth, CalcTerm &out);

bool calcFactor(StringView &r, bool resolutionMetric, uint32_t depth, CalcTerm &out) {
	r.skipChars<WhiteSpaceGroup>();

	if (r.is('(')) {
		if (depth >= CalcMaxDepth) {
			return false;
		}
		++r;
		if (!calcExpr(r, resolutionMetric, depth + 1, out)) {
			return false;
		}
		r.skipChars<WhiteSpaceGroup>();
		if (!r.is(')')) {
			return false;
		}
		++r;
		return true;
	}

	auto num = r.readFloat();
	if (!num.valid()) {
		return false;
	}

	out.value = num.get();
	out.unit = Metric::Units::Px;
	out.unitless = true;

	// No whitespace skip before the unit: `16 px` is two tokens in css, not a dimension.
	Metric::Units unit = Metric::Units::Px;
	float scale = 1.0f;
	if (readMetricUnitPrefix(r, resolutionMetric, unit, scale)) {
		out.value *= scale;
		out.unit = unit;
		out.unitless = false;
	}
	return true;
}

bool calcTerm(StringView &r, bool resolutionMetric, uint32_t depth, CalcTerm &out) {
	if (!calcFactor(r, resolutionMetric, depth, out)) {
		return false;
	}

	for (;;) {
		r.skipChars<WhiteSpaceGroup>();
		const bool mul = r.is('*');
		const bool div = r.is('/');
		if (!mul && !div) {
			return true;
		}
		++r;

		CalcTerm rhs;
		if (!calcFactor(r, resolutionMetric, depth, rhs)) {
			return false;
		}

		if (mul) {
			// css: at least one side of a product must be a plain number, so that the result has
			// a single unit — `2px * 3px` has no meaning this Metric could hold
			if (!out.unitless && !rhs.unitless) {
				return false;
			}
			if (out.unitless && !rhs.unitless) {
				out.unit = rhs.unit;
				out.unitless = false;
			}
			out.value *= rhs.value;
		} else {
			// css: the divisor must be a plain number
			if (!rhs.unitless || rhs.value == 0.0f) {
				return false;
			}
			out.value /= rhs.value;
		}
	}
}

bool calcExpr(StringView &r, bool resolutionMetric, uint32_t depth, CalcTerm &out) {
	if (!calcTerm(r, resolutionMetric, depth, out)) {
		return false;
	}

	for (;;) {
		r.skipChars<WhiteSpaceGroup>();
		const bool plus = r.is('+');
		const bool minus = r.is('-');
		if (!plus && !minus) {
			return true;
		}
		++r;

		CalcTerm rhs;
		if (!calcTerm(r, resolutionMetric, depth, rhs)) {
			return false;
		}

		// A sum only combines like with like. A Metric carries ONE value and ONE unit, so a mixed
		// sum (`100% - 20px`) is not representable and the whole declaration is rejected rather
		// than silently losing a term — the caller drops it, exactly as for a bad unit.
		if (out.unitless != rhs.unitless || (!out.unitless && out.unit != rhs.unit)) {
			return false;
		}
		out.value += plus ? rhs.value : -rhs.value;
	}
}

// `r` starts at "calc(". Folds the expression to a single value+unit and advances `r` past the
// closing paren. var() substitution has already happened by the time a value reaches here (see
// ResolvedStyle::expandPendingRule), so the expression is plain text.
bool readCalcValue(StringView &r, bool resolutionMetric, bool allowEmptyMetric, Metric &out) {
	auto body = r;
	body += 5; // past "calc("

	uint32_t nest = 1;
	size_t i = 0;
	for (; i < body.size() && nest > 0; ++i) {
		if (body[i] == '(') {
			++nest;
		} else if (body[i] == ')') {
			--nest;
		}
	}
	if (nest != 0) {
		return false; // unbalanced
	}

	auto inner = body.sub(0, i - 1);
	const auto rest = body.sub(i);

	CalcTerm term;
	if (!calcExpr(inner, resolutionMetric, 0, term)) {
		return false;
	}

	inner.skipChars<WhiteSpaceGroup>();
	if (!inner.empty()) {
		return false; // trailing garbage: an operator we do not implement, most likely
	}

	if (term.unitless && !allowEmptyMetric) {
		return false;
	}

	out.value = term.value;
	if (!term.unitless) {
		out.metric = term.unit;
	}
	r = rest;
	return true;
}

} // namespace

bool Metric::readStyleValue(StringView &r, bool resolutionMetric, bool allowEmptyMetric) {
	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (!resolutionMetric && r.starts_with("auto")) {
		r += 4;
		this->metric = Metric::Units::Auto;
		this->value = 0.0f;
		return true;
	}
	if (!resolutionMetric && r.starts_with("fit-content")) {
		r += 11;
		this->metric = Metric::Units::FitContent;
		this->value = 0.0f;
		return true;
	}
	if (r.starts_with("calc(")) {
		return readCalcValue(r, resolutionMetric, allowEmptyMetric, *this);
	}

	auto fRes = r.readFloat();
	if (!fRes.valid()) {
		return false;
	}

	auto fvalue = fRes.get();
	if (fvalue == 0.0f) {
		this->value = fvalue;
		this->metric = Metric::Units::Px;
		return true;
	}

	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	auto str = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	// The unit is matched against the WHOLE token: a dimension is one token, and a trailing
	// remainder ("50%x") is not a unit this parser knows. Same table calc() operands read, so the
	// two can never drift apart.
	Metric::Units unit = Metric::Units::Px;
	float scale = 1.0f;
	if (auto tail = str; readMetricUnitPrefix(tail, resolutionMetric, unit, scale) && tail.empty()) {
		this->value = fvalue * scale;
		this->metric = unit;
		return true;
	}

	if (allowEmptyMetric) {
		this->value = fvalue;
		return true;
	}

	return false;
}

bool Rect::containsPoint(const Vec2 &point, float padding) const {
	bool bRet = false;

	if (point.x >= getMinX() - padding && point.x <= getMaxX() + padding
			&& point.y >= getMinY() - padding && point.y <= getMaxY() + padding) {
		bRet = true;
	}

	return bRet;
}

bool Rect::intersectsRect(const Rect &rect) const {
	return !(getMaxX() < rect.getMinX() || rect.getMaxX() < getMinX() || getMaxY() < rect.getMinY()
			|| rect.getMaxY() < getMinY());
}

bool Rect::intersectsCircle(const Vec2 &center, float radius) const {
	Vec2 rectangleCenter((origin.x + size.width / 2), (origin.y + size.height / 2));

	float w = size.width / 2;
	float h = size.height / 2;

	float dx = fabs(center.x - rectangleCenter.x);
	float dy = fabs(center.y - rectangleCenter.y);

	if (dx > (radius + w) || dy > (radius + h)) {
		return false;
	}

	Vec2 circleDistance(fabs(center.x - origin.x - w), fabs(center.y - origin.y - h));

	if (circleDistance.x <= (w)) {
		return true;
	}

	if (circleDistance.y <= (h)) {
		return true;
	}

	float cornerDistanceSq = powf(circleDistance.x - w, 2) + powf(circleDistance.y - h, 2);

	return (cornerDistanceSq <= (powf(radius, 2)));
}

void Rect::merge(const Rect &rect) {
	float top1 = getMaxY();
	float left1 = getMinX();
	float right1 = getMaxX();
	float bottom1 = getMinY();

	float top2 = rect.getMaxY();
	float left2 = rect.getMinX();
	float right2 = rect.getMaxX();
	float bottom2 = rect.getMinY();
	origin.x = sprt::min(left1, left2);
	origin.y = sprt::min(bottom1, bottom2);
	size.width = sprt::max(right1, right2) - origin.x;
	size.height = sprt::max(top1, top2) - origin.y;
}

Rect Rect::unionWithRect(const Rect &rect) const {
	float thisLeftX = origin.x;
	float thisRightX = origin.x + size.width;
	float thisTopY = origin.y + size.height;
	float thisBottomY = origin.y;

	if (thisRightX < thisLeftX) {
		sprt::swap(thisRightX, thisLeftX); // This rect has negative width
	}

	if (thisTopY < thisBottomY) {
		sprt::swap(thisTopY, thisBottomY); // This rect has negative height
	}

	float otherLeftX = rect.origin.x;
	float otherRightX = rect.origin.x + rect.size.width;
	float otherTopY = rect.origin.y + rect.size.height;
	float otherBottomY = rect.origin.y;

	if (otherRightX < otherLeftX) {
		sprt::swap(otherRightX, otherLeftX); // Other rect has negative width
	}

	if (otherTopY < otherBottomY) {
		sprt::swap(otherTopY, otherBottomY); // Other rect has negative height
	}

	float combinedLeftX = sprt::min(thisLeftX, otherLeftX);
	float combinedRightX = sprt::max(thisRightX, otherRightX);
	float combinedTopY = sprt::max(thisTopY, otherTopY);
	float combinedBottomY = sprt::min(thisBottomY, otherBottomY);

	return Rect(combinedLeftX, combinedBottomY, combinedRightX - combinedLeftX,
			combinedTopY - combinedBottomY);
}

bool URect::containsPoint(const UVec2 &point) const {
	bool bRet = false;

	if (point.x >= getMinX() && point.x <= getMaxX() && point.y >= getMinY()
			&& point.y <= getMaxY()) {
		bRet = true;
	}

	return bRet;
}

bool URect::intersectsRect(const URect &rect) const {
	return !(getMaxX() < rect.getMinX() || rect.getMaxX() < getMinX() || getMaxY() < rect.getMinY()
			|| rect.getMaxY() < getMinY());
}

bool IRect::containsPoint(const IVec2 &point) const {
	bool bRet = false;

	if (point.x >= getMinX() && point.x <= getMaxX() && point.y >= getMinY()
			&& point.y <= getMaxY()) {
		bRet = true;
	}

	return bRet;
}

bool IRect::intersectsRect(const IRect &rect) const {
	return !(getMaxX() < rect.getMinX() || rect.getMaxX() < getMinX() || getMaxY() < rect.getMinY()
			|| rect.getMaxY() < getMinY());
}

Rect TransformRect(const Rect &rect, const Mat4 &transform) {
	const float top = rect.getMinY();
	const float left = rect.getMinX();
	const float right = rect.getMaxX();
	const float bottom = rect.getMaxY();

	Vec2 topLeft(left, top);
	Vec2 topRight(right, top);
	Vec2 bottomLeft(left, bottom);
	Vec2 bottomRight(right, bottom);

	transform.transformPoint(&topLeft);
	transform.transformPoint(&topRight);
	transform.transformPoint(&bottomLeft);
	transform.transformPoint(&bottomRight);

	const float minX = min(min(topLeft.x, topRight.x), min(bottomLeft.x, bottomRight.x));
	const float maxX = max(max(topLeft.x, topRight.x), max(bottomLeft.x, bottomRight.x));
	const float minY = min(min(topLeft.y, topRight.y), min(bottomLeft.y, bottomRight.y));
	const float maxY = max(max(topLeft.y, topRight.y), max(bottomLeft.y, bottomRight.y));

	return Rect(minX, minY, (maxX - minX), (maxY - minY));
}

} // namespace sprt::geom
