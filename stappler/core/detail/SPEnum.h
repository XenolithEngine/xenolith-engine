/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef CORE_CORE_UTILS_SPENUM_H_
#define CORE_CORE_UTILS_SPENUM_H_

#include "SPPlatformInit.h" // IWYU pragma: keep
#include <sprt/runtime/enum.h>

// A part of SPCore.h, DO NOT include this directly

namespace stappler {

// Enumeration utilities, usage like:
//
// for (auto value : each<EnumClass>()>) { }
//
// EnumClass should contains value with name Max
//

using sprt::toInt;
using sprt::each;
using sprt::flags;

} // namespace stappler

/** SP_DEFINE_ENUM_AS_MASK is utility to make a bitwise-mask from typed enum
 * It defines a set of overloaded operators, that allow some bitwise operations
 * on this enum class
 *
 * Type should be unsigned, and SDK code style suggests to make it sized (uint32_t, uint64_t)
 */
#define SP_DEFINE_ENUM_AS_MASK(Type) SPRT_DEFINE_ENUM_AS_MASK(Type)

/** SP_DEFINE_ENUM_AS_INCREMENTABLE adds operator++/operator-- for enumerations */
#define SP_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last) SPRT_DEFINE_ENUM_AS_INCREMENTABLE(Type, First, Last)

#endif /* CORE_CORE_UTILS_SPENUM_H_ */
