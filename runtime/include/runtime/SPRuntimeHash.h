/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEHASH_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEHASH_H_

#include "SPRuntimeInt.h"

namespace sprt::sha1 {

struct Ctx {
	uint32_t digest[5];
	uint32_t count_lo, count_hi;
	uint32_t data[16];
	int32_t local;
};

constexpr static uint32_t Length = 20;

SPRT_API void sha_init(Ctx &md);
SPRT_API void sha_process(Ctx &md, const uint8_t *src, uint32_t inlen);
SPRT_API void sha_done(Ctx &md, uint8_t out[Length]);

} // namespace sprt::sha1


namespace sprt::sha256 {

struct Ctx {
	uint64_t length;
	uint32_t state[8];
	uint32_t curlen;
	uint8_t buf[64];
};

constexpr static uint32_t Length = 32;

SPRT_API void sha_init(Ctx &md);
SPRT_API void sha_process(Ctx &md, const uint8_t *src, uint32_t inlen);
SPRT_API void sha_done(Ctx &md, uint8_t out[Length]);

} // namespace sprt::sha256


namespace sprt::sha512 {

struct Ctx {
	uint64_t length;
	uint64_t state[8];
	uint32_t curlen;
	uint8_t buf[128];
};

constexpr static uint32_t Length = 64;

SPRT_API void sha_init(Ctx &md);
SPRT_API void sha_process(Ctx &md, const uint8_t *src, uint32_t inlen);
SPRT_API void sha_done(Ctx &md, uint8_t out[Length]);

} // namespace sprt::sha512

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEHASH_H_
