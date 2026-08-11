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

#ifndef TESTS_THIRDPARTY_LIB_CERTGEN_H
#define TESTS_THIRDPARTY_LIB_CERTGEN_H

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <string.h>

namespace thirdparty::test {

// A throwaway EC key plus the self-signed certificate over it. Both the pure
// libssl test (BIO pair) and the curl-over-loopback test need one, and building
// it exercises a good part of libcrypto's X.509 side on its own.
struct TestCredentials {
	EVP_PKEY *key = nullptr;
	X509 *cert = nullptr;

	~TestCredentials() { reset(); }

	void reset() {
		if (cert) {
			X509_free(cert);
			cert = nullptr;
		}
		if (key) {
			EVP_PKEY_free(key);
			key = nullptr;
		}
	}
};

// `cn` becomes the subject/issuer common name; `altNames` is an X509v3 SAN
// string ("DNS:localhost,IP:127.0.0.1") or nullptr for no SAN extension.
static inline bool makeSelfSignedCredentials(TestCredentials &out, const char *cn,
		const char *altNames) {
	out.reset();

	out.key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
	if (!out.key) {
		return false;
	}

	out.cert = X509_new();
	if (!out.cert) {
		return false;
	}

	// X509_set_version takes the wire value: 2 means "X.509 v3".
	if (X509_set_version(out.cert, 2) != 1) {
		return false;
	}
	if (ASN1_INTEGER_set(X509_get_serialNumber(out.cert), 1) != 1) {
		return false;
	}
	if (!X509_gmtime_adj(X509_getm_notBefore(out.cert), -3600)
			|| !X509_gmtime_adj(X509_getm_notAfter(out.cert), 24 * 3600)) {
		return false;
	}
	if (X509_set_pubkey(out.cert, out.key) != 1) {
		return false;
	}

	auto name = X509_get_subject_name(out.cert);
	if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)cn, -1, -1, 0)
			!= 1) {
		return false;
	}
	// Self-signed: issuer is the subject.
	if (X509_set_issuer_name(out.cert, name) != 1) {
		return false;
	}

	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, out.cert, out.cert, nullptr, nullptr, 0);

	// CA:TRUE so the same certificate can serve as its own trust anchor.
	auto addExt = [&](int nid, const char *value) {
		auto ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
		if (!ext) {
			return false;
		}
		auto ret = X509_add_ext(out.cert, ext, -1);
		X509_EXTENSION_free(ext);
		return ret == 1;
	};

	if (!addExt(NID_basic_constraints, "critical,CA:TRUE")) {
		return false;
	}
	if (altNames && !addExt(NID_subject_alt_name, altNames)) {
		return false;
	}

	return X509_sign(out.cert, out.key, EVP_sha256()) > 0;
}

// PEM-encodes a certificate into `buf`; returns the byte count (0 on failure).
static inline size_t pemEncodeCert(X509 *cert, char *buf, size_t bufLen) {
	auto bio = BIO_new(BIO_s_mem());
	if (!bio) {
		return 0;
	}
	size_t written = 0;
	if (PEM_write_bio_X509(bio, cert) == 1) {
		char *data = nullptr;
		auto len = BIO_get_mem_data(bio, &data);
		if (len > 0 && size_t(len) < bufLen) {
			memcpy(buf, data, size_t(len));
			buf[len] = 0;
			written = size_t(len);
		}
	}
	BIO_free(bio);
	return written;
}

} // namespace thirdparty::test

#endif // TESTS_THIRDPARTY_LIB_CERTGEN_H
