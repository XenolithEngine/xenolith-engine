/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "SPKdlValue.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

namespace {

int digitValue(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

// Digits of `radix` with `_` allowed after the first one; at least one digit.
bool readDigits(StringView &r, int radix, mem_std::String &out) {
	if (r.empty() || digitValue(r[0]) < 0 || digitValue(r[0]) >= radix) {
		return false;
	}
	while (!r.empty()) {
		auto c = r[0];
		if (c == '_') {
			++r;
			continue;
		}
		auto v = digitValue(c);
		if (v < 0 || v >= radix) {
			break;
		}
		out.push_back(c);
		++r;
	}
	return true;
}

// Digits of any radix as decimal text, by repeated multiply-add on base 10^9 limbs.
mem_std::String toDecimal(StringView digits, int radix) {
	constexpr uint32_t Base = 1'000'000'000;
	mem_std::Vector<uint32_t> limbs{0};
	for (auto c : digits) {
		uint64_t carry = uint64_t(digitValue(c));
		for (auto &limb : limbs) {
			auto v = uint64_t(limb) * uint64_t(radix) + carry;
			limb = uint32_t(v % Base);
			carry = v / Base;
		}
		while (carry > 0) {
			limbs.emplace_back(uint32_t(carry % Base));
			carry /= Base;
		}
	}

	mem_std::String out = mem_std::toString(limbs.back());
	for (size_t i = limbs.size() - 1; i > 0; --i) {
		auto part = mem_std::toString(limbs[i - 1]);
		out.append(9 - part.size(), '0');
		out.append(part);
	}
	return out;
}

} // namespace

namespace detail {

void appendHex(mem_std::String &out, uint32_t value, uint32_t minDigits) {
	char buf[8];
	uint32_t n = 0;
	do {
		buf[n++] = "0123456789ABCDEF"[value & 0xF];
		value >>= 4;
	} while (value != 0 && n < 8);
	while (n < minDigits && n < 8) { buf[n++] = '0'; }
	while (n > 0) { out.push_back(buf[--n]); }
}

// A number token as the grammar has it, into its canonical text; false when the token is not one.
bool readNumber(StringView token, ValueType &type, mem_std::String &text) {
	StringView r(token);
	bool negative = false;
	if (r.is('+') || r.is('-')) {
		negative = r.is('-');
		++r;
	}

	int radix = 0;
	if (r.starts_with("0x")) {
		radix = 16;
	} else if (r.starts_with("0o")) {
		radix = 8;
	} else if (r.starts_with("0b")) {
		radix = 2;
	}

	mem_std::String digits;
	if (radix != 0) {
		r += 2;
		if (!readDigits(r, radix, digits) || !r.empty()) {
			return false;
		}
		type = ValueType::Integer;
		auto value = toDecimal(digits, radix);
		text = (negative && value != "0") ? mem_std::toString("-", value) : value;
		return true;
	}

	if (!readDigits(r, 10, digits)) {
		return false;
	}

	mem_std::String fraction;
	bool hasFraction = false;
	if (r.is('.')) {
		++r;
		hasFraction = true;
		if (!readDigits(r, 10, fraction)) {
			return false;
		}
	}

	mem_std::String exponent;
	bool hasExponent = false;
	bool negativeExponent = false;
	if (r.is('e') || r.is('E')) {
		++r;
		hasExponent = true;
		if (r.is('+') || r.is('-')) {
			negativeExponent = r.is('-');
			++r;
		}
		if (!readDigits(r, 10, exponent)) {
			return false;
		}
	}

	if (!r.empty()) {
		return false;
	}

	if (!hasFraction && !hasExponent) {
		type = ValueType::Integer;
		auto value = toDecimal(digits, 10);
		text = (negative && value != "0") ? mem_std::toString("-", value) : value;
		return true;
	}

	type = ValueType::Float;
	text.clear();
	if (negative) {
		text.push_back('-');
	}
	text.append(digits);
	if (hasFraction) {
		text.push_back('.');
		text.append(fraction);
	}
	if (hasExponent) {
		text.append(negativeExponent ? "E-" : "E+");
		text.append(exponent);
	}
	return true;
}

sprt::Result<int64_t> readInteger(StringView canonical) {
	auto r = canonical;
	auto ret = r.readInteger(10);
	if (!ret || !r.empty()) {
		return sprt::Result<int64_t>();
	}
	// readInteger saturates, so the value has to print back to the same text.
	if (StringView(mem_std::toString(ret.get())) != canonical) {
		return sprt::Result<int64_t>();
	}
	return ret;
}

sprt::Result<uint64_t> readUnsigned(StringView canonical) {
	if (canonical.empty() || canonical[0] == '-' || canonical.size() > 20) {
		return sprt::Result<uint64_t>();
	}
	uint64_t value = 0;
	for (auto c : canonical) {
		auto d = uint64_t(c - '0');
		if (value > (sprt::Max<uint64_t> - d) / 10) {
			return sprt::Result<uint64_t>();
		}
		value = value * 10 + d;
	}
	return sprt::Result<uint64_t>(value);
}

sprt::Result<double> readDouble(StringView canonical) {
	auto r = canonical;
	auto ret = r.readDouble();
	if (!ret || !r.empty()) {
		return sprt::Result<double>();
	}
	return ret;
}

} // namespace detail

} // namespace stappler::kdl
