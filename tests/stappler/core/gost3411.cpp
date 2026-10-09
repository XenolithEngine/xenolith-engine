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

// GOST R 34.11-2012 (Streebog): both compression functions - SSE2 through SIMDe, which is native
// SSE2, NEON or LSX depending on the target, and the 64-bit scalar one - against the examples of
// the standard (RFC 6986, section 10) and against each other.

#include "SPCommon.h"
#include "SPCoreCrypto.h"
#include "SPString.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

template <size_t N>
static memory::StandardInterface::StringType toHex(const sprt::array<uint8_t, N> &buf) {
	static constexpr char digits[] = "0123456789abcdef";
	memory::StandardInterface::StringType ret;
	for (auto b : buf) {
		ret.push_back(digits[b >> 4]);
		ret.push_back(digits[b & 0xF]);
	}
	return ret;
}

static memory::StandardInterface::BytesType fromHex(StringView hex) {
	auto nibble = [](char c) -> uint8_t {
		return (c >= 'a') ? uint8_t(c - 'a' + 10) : uint8_t(c - '0');
	};
	memory::StandardInterface::BytesType ret;
	for (size_t i = 0; i + 1 < hex.size(); i += 2) {
		ret.push_back(uint8_t((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
	}
	return ret;
}

static const char *backendName(crypto::Gost3411Backend b) {
	return b == crypto::Gost3411Backend::Simd ? "simd" : "scalar";
}

struct Example {
	const char *name;
	memory::StandardInterface::BytesType message;
	const char *digest512;
	const char *digest256;
};

static void checkExamples(crypto::Gost3411Backend backend) {
	// M1 is 63 ASCII digits, M2 a line of the Lay of Igor's Campaign in CP1251 - one block and a
	// partial one
	StringView m1("012345678901234567890123456789012345678901234567890123456789012");
	const Example examples[] = {
		Example{"M1", memory::StandardInterface::BytesType((const uint8_t *)m1.data(),
							  (const uint8_t *)m1.data() + m1.size()),
			"1b54d01a4af5b9d5cc3d86d68d285462b19abc2475222f35c085122be4ba1ffa00ad30f8767b3a8238"
			"4c6574f024c311e2a481332b08ef7f41797891c1646f48",
			"9d151eefd8590b89daa6ba6cb74af9275dd051026bb149a452fd84e5e57b5500"},
		Example{"M2",
			fromHex("d1e520e2e5f2f0e82c20d1f2f0e8e1eee6e820e2edf3f6e82c20e2e5fef2fa20f120eceef0ff20f1"
					"f2f0e5ebe0ece820ede020f5f0e0e1f0fbff20efebfaeafb20c8e3eef0e5e2fb"),
			"1e88e62226bfca6f9994f1f2d51569e0daf8475a3b0fe61a5300eee46d961376035fe83549ada2b862"
			"0fcd7c496ce5b33f0cb9dddc2b6460143b03dabac9fb28",
			"9dd2fe4e90409e5da87f53976d7405b0c0cac628fc669a741d50063c557e8f50"},
	};

	for (auto &it : examples) {
		auto d512 = crypto::Gost3411_512(backend)
							.update(it.message.data(), it.message.size())
							.final();
		auto d256 = crypto::Gost3411_256(backend)
							.update(it.message.data(), it.message.size())
							.final();
		checkEq(StringView(toHex(d512)), StringView(it.digest512),
				string::toString<memory::StandardInterface>("gost3411 ", backendName(backend), ": 512-bit digest of ", it.name));
		checkEq(StringView(toHex(d256)), StringView(it.digest256),
				string::toString<memory::StandardInterface>("gost3411 ", backendName(backend), ": 256-bit digest of ", it.name));
	}
}

// Every length across several blocks, fed in irregular pieces: the backends must agree on every
// state the compression function can be called in, not only on the two examples
static void checkBackendsAgree() {
	uint32_t state = 0x2545'F491;
	auto next = [&] {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	};

	memory::StandardInterface::BytesType data(400);
	for (auto &b : data) { b = uint8_t(next()); }

	bool same512 = true, same256 = true;
	for (size_t len = 0; len <= data.size(); ++len) {
		crypto::Gost3411_512 simd512(crypto::Gost3411Backend::Simd);
		crypto::Gost3411_512 scalar512(crypto::Gost3411Backend::Scalar);
		crypto::Gost3411_256 simd256(crypto::Gost3411Backend::Simd);
		crypto::Gost3411_256 scalar256(crypto::Gost3411Backend::Scalar);

		size_t pos = 0;
		while (pos < len) {
			auto piece = sprt::min(size_t(next() % 97 + 1), len - pos);
			simd512.update(data.data() + pos, piece);
			scalar512.update(data.data() + pos, piece);
			simd256.update(data.data() + pos, piece);
			scalar256.update(data.data() + pos, piece);
			pos += piece;
		}

		same512 = same512 && simd512.final() == scalar512.final();
		same256 = same256 && simd256.final() == scalar256.final();
	}

	check(same512, "gost3411: simd and scalar 512-bit digests agree for 0..400 bytes");
	check(same256, "gost3411: simd and scalar 256-bit digests agree for 0..400 bytes");
}

} // namespace

void performGost3411Tests() {
	checkExamples(crypto::Gost3411Backend::Simd);
	checkExamples(crypto::Gost3411Backend::Scalar);
	checkBackendsAgree();

	check(crypto::Gost3411_512().ctx.backend == crypto::Gost3411DefaultBackend,
			string::toString<memory::StandardInterface>("gost3411: the default backend is ", backendName(crypto::Gost3411DefaultBackend)));
}

} // namespace stappler
