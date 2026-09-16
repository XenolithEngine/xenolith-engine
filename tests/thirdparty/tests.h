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

#ifndef TESTS_THIRDPARTY_TESTS_H
#define TESTS_THIRDPARTY_TESTS_H

#include <stddef.h>

namespace thirdparty::test {

//
// Check harness (defined in main.cpp). Every check prints one line, so a run is
// readable as-is and greppable for FAIL; the process exit code is the number of
// failed checks (clamped to 125).
//

// Records a check and prints "[ OK ] <name>" or "[FAIL] <name>".
void check(bool ok, const char *name);

// Compares `got` against `expect`, printing both on mismatch.
void checkInt(const char *name, long long got, long long expect);
void checkStr(const char *name, const char *got, const char *expect);

// Compares a buffer against a lowercase hex string ("a9993e36...").
void checkHex(const char *name, const void *buf, size_t len, const char *expectHex);

// Unconditional failure with a reason - for a library call that failed where no
// meaningful comparison is left to make.
void fail(const char *name, const char *reason);

// Notes something the run could not exercise here; not a failure.
void skip(const char *name, const char *reason);

// Hex-decodes a test vector into `out` (which must hold strlen(hex) / 2 bytes)
// and returns the byte count.
size_t fromHex(const char *hex, unsigned char *out, size_t outLen);

// A test either works everywhere, or needs something the environment may not
// provide. Tests that need it are skipped unless explicitly requested, so a
// default run stays hermetic and deterministic.
enum TestFlags {
	TestDefault = 0,
	// needs a route to the public internet (opt in with --network)
	TestNetwork = 1 << 0,
	// needs loopback TCP sockets (opt out with --no-sockets)
	TestSockets = 1 << 1,
};

struct TestCase {
	const char *name;
	void (*fn)();
	unsigned flags;
};

// Returned list is terminated by a {nullptr, nullptr, 0} entry.
const TestCase *getTests();

//
// openssl: libcrypto
//
void performOpensslVersionTest();
void performOpensslDigestTest();
void performOpensslHmacTest();
void performOpensslCipherTest();
void performOpensslKdfTest();
void performOpensslBignumTest();
void performOpensslRandomTest();
void performOpensslEncodingTest();
void performOpensslRsaTest();
void performOpensslEcTest();
void performOpensslX509Test();
void performOpensslErrorTest();

// openssl: libssl (handshake over a BIO pair, no sockets involved)
void performOpensslTlsMemoryTest();

//
// curl
//
void performCurlVersionTest();
void performCurlEasyOptionsTest();
void performCurlUrlApiTest();
void performCurlEscapeTest();
void performCurlSlistTest();
void performCurlDateTest();
void performCurlMimeTest();
void performCurlFileTransferTest();
void performCurlMultiTest();

// curl over a loopback TLS server built from the OpenSSL that curl links to
void performCurlTlsLoopbackTest();

// curl against the public internet
void performCurlHttpsTest();

} // namespace thirdparty::test

#endif // TESTS_THIRDPARTY_TESTS_H
