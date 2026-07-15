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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___NEW_NOTHROW_H_
#define RUNTIME_INCLUDE_SPRT_CXX___NEW_NOTHROW_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_config.h>
#include <sprt/c/bits/__sprt_size_t.h>

#if __SPRT_STD_EXTERNAL // std-owned types: project when an external STL owns std

#include <memory>
#include <new>

namespace sprt {
using align_val_t = std::align_val_t;
} // namespace sprt

#else

namespace std {
// The TYPES stay canonical: they participate in the replaceable operator new/delete
// manglings, which must match the platform ABI (libc++abi's weak set). Only the
// nothrow OBJECT is a symbol — always versioned (__SPRT_STD_OWNED_*, unified ABI),
// so the runtime never defines a canonical-namespace symbol.
struct nothrow_t {
	explicit nothrow_t() = default;
};

enum class align_val_t : __SPRT_ID(size_t){};

__SPRT_STD_OWNED_BEGIN
SPRT_API extern const nothrow_t nothrow;
__SPRT_STD_OWNED_END
} // namespace std

#endif // __SPRT_USE_STL

namespace sprt {
using align_val_t = std::align_val_t;
using std::nothrow_t;
using std::nothrow;
} // namespace sprt


#endif // RUNTIME_INCLUDE_SPRT_CXX___NEW_NOTHROW_H_
