/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_UTILS_SPHALFFLOAT_H_
#define STAPPLER_CORE_UTILS_SPHALFFLOAT_H_

#include "SPCore.h"

namespace STAPPLER_VERSIONIZED stappler::halffloat {

// see https://en.wikipedia.org/wiki/Half_precision_floating-point_format

constexpr uint16_t nan() { return (uint16_t)0x7e00; }
constexpr uint16_t posinf() { return (uint16_t)(31 << 10); }
constexpr uint16_t neginf() { return (uint16_t)(63 << 10); }

inline constexpr float decode(uint16_t half)  {
	uint16_t exp = (half >> 10) & 0x1f;
	uint16_t mant = half & 0x3ff;
    double val = (exp == 0) ? std::ldexp(mant, -24)
    		: ((exp != 31) ? std::ldexp(mant + 1024, exp - 25)
    				: (mant == 0 ? NumericLimits<float>::infinity() : stappler::nan()));

    return (half & 0x8000) ? -val : val;
}

inline constexpr uint16_t encode(float val)  {
    union {
        float f;
        uint32_t i;
    } u32 = { val };

    uint16_t bits = (u32.i >> 16) & 0x8000; /* Get the sign */
    uint16_t m = (u32.i >> 12) & 0x07ff; /* Keep one extra bit for rounding */
    uint32_t e = (u32.i >> 23) & 0xff; /* Using int is faster here */

    /* If zero, or denormal, or exponent underflows too much for a denormal
     * half, return signed zero. */
    if (e < 103) {
        return bits;
    }

    /* If NaN, return NaN. If Inf or exponent overflow, return Inf. */
    if (e > 142) {
        bits |= 0x7c00u;
        /* If exponent was 0xff and one mantissa bit was set, it means NaN,
         * not Inf, so make sure we set one mantissa bit too. */
        bits |= (e == 255) && (u32.i & 0x007fffffu);
        return bits;
    }

    /* If exponent underflows but not too much, return a denormal */
    if (e < 113) {
        m |= 0x0800u;
        /* Extra rounding may overflow and set mantissa to 0 and exponent
         * to 1, which is OK. */
        bits |= (m >> (114 - e)) + ((m >> (113 - e)) & 1);
        return bits;
    }

    bits |= ((e - 112) << 10) | (m >> 1);
    /* Extra rounding. An overflow will set mantissa to 0 and increment
     * the exponent, which is OK. */
    bits += m & 1;
    return bits;
}

}

#endif /* STAPPLER_CORE_UTILS_SPHALFFLOAT_H_ */
