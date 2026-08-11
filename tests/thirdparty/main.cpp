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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests.h"

namespace thirdparty::test {

static const TestCase s_tests[] = {
	// libcrypto: primitives with published test vectors
	{"openssl_version", &performOpensslVersionTest, TestDefault},
	{"openssl_digest", &performOpensslDigestTest, TestDefault},
	{"openssl_hmac", &performOpensslHmacTest, TestDefault},
	{"openssl_cipher", &performOpensslCipherTest, TestDefault},
	{"openssl_kdf", &performOpensslKdfTest, TestDefault},
	{"openssl_bignum", &performOpensslBignumTest, TestDefault},
	{"openssl_random", &performOpensslRandomTest, TestDefault},
	{"openssl_encoding", &performOpensslEncodingTest, TestDefault},
	// libcrypto: public key
	{"openssl_rsa", &performOpensslRsaTest, TestDefault},
	{"openssl_ec", &performOpensslEcTest, TestDefault},
	{"openssl_x509", &performOpensslX509Test, TestDefault},
	{"openssl_error", &performOpensslErrorTest, TestDefault},
	// libssl: a complete TLS 1.3 handshake, no sockets
	{"openssl_tls_memory", &performOpensslTlsMemoryTest, TestDefault},

	// curl: the parts that need no transfer at all
	{"curl_version", &performCurlVersionTest, TestDefault},
	{"curl_easy_options", &performCurlEasyOptionsTest, TestDefault},
	{"curl_url_api", &performCurlUrlApiTest, TestDefault},
	{"curl_escape", &performCurlEscapeTest, TestDefault},
	{"curl_slist", &performCurlSlistTest, TestDefault},
	{"curl_date", &performCurlDateTest, TestDefault},
	{"curl_mime", &performCurlMimeTest, TestDefault},
	// curl: the transfer engine, driven over file://
	{"curl_file_transfer", &performCurlFileTransferTest, TestDefault},
	{"curl_multi", &performCurlMultiTest, TestDefault},
	// curl over TLS, against an in-process OpenSSL server on loopback
	{"curl_tls_loopback", &performCurlTlsLoopbackTest, TestSockets},
	// curl over TLS, against the public internet
	{"curl_https", &performCurlHttpsTest, TestNetwork},

	{nullptr, nullptr, 0},
};

const TestCase *getTests() { return s_tests; }

static int s_checks = 0;
static int s_failures = 0;

void check(bool ok, const char *name) {
	++s_checks;
	if (!ok) {
		++s_failures;
	}
	printf("[%s] %s\n", ok ? " OK " : "FAIL", name);
}

void checkInt(const char *name, long long got, long long expect) {
	bool ok = (got == expect);
	check(ok, name);
	if (!ok) {
		printf("       got %lld, expected %lld\n", got, expect);
	}
}

void checkStr(const char *name, const char *got, const char *expect) {
	bool ok = got && expect && strcmp(got, expect) == 0;
	check(ok, name);
	if (!ok) {
		printf("       got \"%s\", expected \"%s\"\n", got ? got : "(null)",
				expect ? expect : "(null)");
	}
}

void checkHex(const char *name, const void *buf, size_t len, const char *expectHex) {
	auto p = (const unsigned char *)buf;
	size_t expectLen = strlen(expectHex) / 2;
	bool ok = (len == expectLen) && p != nullptr;
	if (ok) {
		for (size_t i = 0; i < len; ++i) {
			char pair[3] = {expectHex[i * 2], expectHex[i * 2 + 1], 0};
			if ((unsigned)strtoul(pair, nullptr, 16) != p[i]) {
				ok = false;
				break;
			}
		}
	}
	check(ok, name);
	if (!ok) {
		printf("       got ");
		if (p) {
			for (size_t i = 0; i < len; ++i) { printf("%02x", p[i]); }
		} else {
			printf("(null)");
		}
		printf("\n       expected %s\n", expectHex);
	}
}

void fail(const char *name, const char *reason) {
	++s_checks;
	++s_failures;
	printf("[FAIL] %s: %s\n", name, reason);
}

void skip(const char *name, const char *reason) { printf("[SKIP] %s: %s\n", name, reason); }

size_t fromHex(const char *hex, unsigned char *out, size_t outLen) {
	size_t n = strlen(hex) / 2;
	if (n > outLen) {
		n = outLen;
	}
	for (size_t i = 0; i < n; ++i) {
		char pair[3] = {hex[i * 2], hex[i * 2 + 1], 0};
		out[i] = (unsigned char)strtoul(pair, nullptr, 16);
	}
	return n;
}

} // namespace thirdparty::test

using namespace thirdparty::test;

static void printUsage(const char *argv0) {
	printf("usage: %s [--list] [--network] [--no-sockets] [test ...]\n"
		   "  --list        print every test name (one per line) and exit\n"
		   "  --network     also run tests that need the public internet\n"
		   "  --no-sockets  skip tests that need loopback TCP sockets\n"
		   "  test...       run only the named tests (flags above still apply)\n",
			argv0);
}

int main(int argc, const char *argv[]) {
	unsigned enabled = TestDefault | TestSockets;
	bool listOnly = false;
	const char *selected[64];
	int selectedCount = 0;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--list") == 0) {
			listOnly = true;
		} else if (strcmp(argv[i], "--network") == 0) {
			enabled |= TestNetwork;
		} else if (strcmp(argv[i], "--no-sockets") == 0) {
			enabled &= ~unsigned(TestSockets);
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printUsage(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return 2;
		} else if (selectedCount < int(sizeof(selected) / sizeof(selected[0]))) {
			selected[selectedCount++] = argv[i];
		}
	}

	const TestCase *tests = getTests();

	if (listOnly) {
		for (const TestCase *t = tests; t->name; ++t) { printf("%s\n", t->name); }
		return 0;
	}

	// An explicitly named test runs whatever its flags say - naming it is the
	// opt-in. Without names, the flag mask decides.
	for (const TestCase *t = tests; t->name; ++t) {
		bool named = false;
		for (int i = 0; i < selectedCount; ++i) {
			if (strcmp(t->name, selected[i]) == 0) {
				named = true;
				break;
			}
		}
		if (selectedCount > 0 && !named) {
			continue;
		}
		if (!named && (t->flags & ~enabled) != 0) {
			printf("==== %s (skipped) ====\n", t->name);
			continue;
		}
		printf("==== %s ====\n", t->name);
		t->fn();
	}

	if (selectedCount > 0) {
		// Report unknown names rather than silently passing.
		for (int i = 0; i < selectedCount; ++i) {
			bool found = false;
			for (const TestCase *t = tests; t->name; ++t) {
				if (strcmp(t->name, selected[i]) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				fprintf(stderr, "Test not found: %s\n", selected[i]);
				return 2;
			}
		}
	}

	printf("----------------------------------------\n");
	printf("%d checks, %d failures\n", s_checks, s_failures);
	return s_failures > 125 ? 125 : s_failures;
}
