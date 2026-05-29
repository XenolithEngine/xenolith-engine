/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_DTOA_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_DTOA_H_

#include <sprt/cxx/cmath>
#include <sprt/cxx/detail/ctypes.h>

#include <sprt/thirdparty/dragonbox.h>

#include <sprt/runtime/detail/itoa.h>

namespace sprt::_dtoa {

static constexpr uint64_t POWERS_OF_10[] = {
	1,
	10,
	100,
	1'000,
	10'000,
	100'000,
	1'000'000,
	10'000'000,
	100'000'000,
	1'000'000'000,
	10'000'000'000,
	100'000'000'000,
	1'000'000'000'000,
	10'000'000'000'000,
	100'000'000'000'000,
	1'000'000'000'000'000,
	10'000'000'000'000'000,
	100'000'000'000'000'000,
	1'000'000'000'000'000'000,
};

struct dtoa_options {
	enum mode {
		fixed,
		scientific,
	};

	uint16_t precision = Max<uint16_t>;
	mode mode = fixed;
};

template <typename Char, floating_point T>
constexpr inline size_t dtoa(Char *buffer, T value, size_t bufferSize,
		dtoa_options opts = dtoa_options()) {
	if (isnan(value)) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			__constexpr_memcpy(buffer, "NaN", 3);
			return 3;
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			__constexpr_memcpy(buffer, u"NaN", 3);
			return 3;
		} else {
			return 0;
		}
	} else if (value == Infinity<T>) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			__constexpr_memcpy(buffer, "inf", 3);
			return 3;
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			__constexpr_memcpy(buffer, u"inf", 3);
			return 3;
		} else {
			return 0;
		}
	} else if (value == -Infinity<T>) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			__constexpr_memcpy(buffer, "-inf", 4);
			return 4;
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			__constexpr_memcpy(buffer, u"-inf", 4);
			return 4;
		} else {
			return 0;
		}
	} else if (value == T(0.0)) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			__constexpr_memcpy(buffer, "0.0", 3);
			return 3;
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			__constexpr_memcpy(buffer, u"0.0", 3);
			return 3;
		} else {
			return 0;
		}
	} else if (value == -T(0.0)) {
		if constexpr (sizeof(Char) == sizeof(char)) {
			__constexpr_memcpy(buffer, "-0.0", 3);
			return 3;
		} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
			__constexpr_memcpy(buffer, u"-0.0", 3);
			return 3;
		} else {
			return 0;
		}
	} else {
		auto result = jkj::dragonbox::to_decimal(value, jkj::dragonbox::policy::sign::return_sign,
				jkj::dragonbox::policy::cache::compact);

		auto intLen = intptr_t(_itoa::unsigned_to_decimal_len(result.significand));

		if (intLen > opts.precision) {
			auto off = intLen - opts.precision;
			result.significand /= POWERS_OF_10[off];
			result.exponent += off;
			intLen -= off;
		}

		size_t ret = 0;

		auto writeScientific = [&]() {
			auto pow10 = POWERS_OF_10[intLen - 1];
			auto exp = int(result.exponent) + intLen - 1;

			ret += _itoa::unsigned_to_decimal(buffer, exp >= 0 ? exp : -exp, bufferSize - ret);
			if (exp < 0) {
				buffer[bufferSize - ret++ - 1] = '-';
			}
			buffer[bufferSize - ret++ - 1] = 'e';

			ret += _itoa::unsigned_to_decimal(buffer, result.significand % pow10, bufferSize - ret);
			buffer[bufferSize - ret++ - 1] = '.';
			ret += _itoa::unsigned_to_decimal(buffer, result.significand / pow10, bufferSize - ret);
		};

		if (opts.mode == dtoa_options::scientific) {
			writeScientific();
		} else if (result.exponent >= 0) {
			if (result.exponent > 4) {
				writeScientific();
			} else {
				buffer[bufferSize - ret++ - 1] = '0';
				buffer[bufferSize - ret++ - 1] = '.';
				for (size_t i = 0; i < result.exponent; ++i) {
					buffer[bufferSize - ret++ - 1] = '0';
				}
				ret += _itoa::unsigned_to_decimal(buffer, result.significand, bufferSize - ret);
			}
		} else if (result.exponent <= -intLen) {
			if (-result.exponent - intLen > 4) {
				writeScientific();
			} else {
				ret += _itoa::unsigned_to_decimal(buffer, result.significand, bufferSize - ret);
				for (size_t i = 0; i < -result.exponent - intLen; ++i) {
					buffer[bufferSize - ret++ - 1] = '0';
				}
				buffer[bufferSize - ret++ - 1] = '.';
				buffer[bufferSize - ret++ - 1] = '0';
			}
		} else {
			// split value into two
			auto pow10 = POWERS_OF_10[-result.exponent];

			auto top = result.significand / pow10;
			auto bottom = result.significand % pow10;

			ret += _itoa::unsigned_to_decimal(buffer, bottom, bufferSize - ret);
			buffer[bufferSize - ret++ - 1] = '.';
			ret += _itoa::unsigned_to_decimal(buffer, top, bufferSize - ret);
		}

		if (result.is_negative) {
			buffer[bufferSize - ret++ - 1] = '-';
		}
		return ret;
	}
}

template <floating_point T>
constexpr size_t dtoa_len(T value, dtoa_options opts = dtoa_options()) {
	if (isnan(value)) {
		return 3;
	} else if (value == Infinity<T>) {
		return 3;
	} else if (value == -Infinity<T>) {
		return 4;
	} else if (value == T(0.0)) {
		return 3;
	} else if (value == -T(0.0)) {
		return 4;
	}

	auto result = jkj::dragonbox::to_decimal(value, jkj::dragonbox::policy::sign::return_sign,
			jkj::dragonbox::policy::cache::compact);

	auto intLen = intptr_t(_itoa::unsigned_to_decimal_len(result.significand));

	if (intLen > opts.precision) {
		auto off = intLen - opts.precision;
		result.significand /= POWERS_OF_10[off];
		result.exponent += off;
		intLen -= off;
	}

	size_t ret = 0;

	auto writeScientific = [&]() {
		auto pow10 = POWERS_OF_10[intLen - 1];
		auto exp = int(result.exponent) + intLen - 1;

		ret += _itoa::unsigned_to_decimal_len(exp >= 0 ? exp : -exp);
		if (exp < 0) {
			ret += 3;
		} else {
			ret += 2;
		}

		ret += _itoa::unsigned_to_decimal_len(result.significand % pow10);
		ret += _itoa::unsigned_to_decimal_len(result.significand / pow10);
	};

	if (opts.mode == dtoa_options::scientific) {
		writeScientific();
	} else if (result.exponent >= 0) {
		if (result.exponent > 4) {
			writeScientific();
		} else {
			ret += 2 + result.exponent;
			ret += _itoa::unsigned_to_decimal_len(result.significand);
		}
	} else if (result.exponent <= -intLen) {
		if (-result.exponent - intLen > 4) {
			writeScientific();
		} else {
			ret += 2 - result.exponent - intLen;
			ret += _itoa::unsigned_to_decimal_len(result.significand);
		}
	} else {
		// split value into two
		auto pow10 = POWERS_OF_10[-result.exponent];

		auto top = result.significand / pow10;
		auto bottom = result.significand % pow10;

		ret += _itoa::unsigned_to_decimal_len(bottom);
		ret++;
		ret += _itoa::unsigned_to_decimal_len(top);
	}

	if (result.is_negative) {
		ret++;
	}
	return ret;
}

} // namespace sprt::_dtoa

#endif
