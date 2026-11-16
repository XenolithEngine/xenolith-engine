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

#include "../tests.h"

#include <string.h>
#include <zlib.h>
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#if SPRT_LINUX
#include <expat.h>
#include <ffi.h>
#endif

namespace thirdparty::test {

void performDependencyDataTest() {
	checkStr("zlib runtime/header version", zlibVersion(), ZLIB_VERSION);
	checkInt("Brotli encoder/decoder version", BrotliEncoderVersion(), BrotliDecoderVersion());
	const unsigned char input[] = "dependency refresh: compression and malformed input";
	unsigned char encoded[256] = {}, decoded[256] = {};
	size_t encodedSize = sizeof(encoded), decodedSize = sizeof(decoded);
	check(BrotliEncoderCompress(5, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC,
			sizeof(input), input, &encodedSize, encoded) == BROTLI_TRUE, "Brotli encode");
	check(BrotliDecoderDecompress(encodedSize, encoded, &decodedSize, decoded)
			== BROTLI_DECODER_RESULT_SUCCESS, "Brotli decode");
	check(decodedSize == sizeof(input) && memcmp(input, decoded, sizeof(input)) == 0,
			"Brotli round trip");
	decodedSize = sizeof(decoded);
	check(BrotliDecoderDecompress(encodedSize / 2, encoded, &decodedSize, decoded)
			!= BROTLI_DECODER_RESULT_SUCCESS, "Brotli rejects truncated stream");

	checkInt("SQLite runtime/header version", sqlite3_libversion_number(), SQLITE_VERSION_NUMBER);
	sqlite3 *db = nullptr;
	if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
		fail("SQLite open", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}
	check(sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, value BLOB);"
			"INSERT INTO t VALUES(1,x'000102ff'); BEGIN; DELETE FROM t; ROLLBACK;",
			nullptr, nullptr, nullptr) == SQLITE_OK, "SQLite transaction rollback");
	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, "SELECT hex(value), json_extract('{\"a\":[7]}','$.a[0]') FROM t",
			-1, &stmt, nullptr);
	checkInt("SQLite prepare", rc, SQLITE_OK);
	if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
		checkStr("SQLite blob survives rollback", (const char *)sqlite3_column_text(stmt, 0), "000102FF");
		checkInt("SQLite JSON", sqlite3_column_int(stmt, 1), 7);
	} else {
		fail("SQLite query", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);
	checkInt("SQLite close", sqlite3_close(db), SQLITE_OK);
}

// CVE-2026-88823: empty incremental AAD must preserve preceding AAD.
// CVE-2026-95692: AES-GCM-SIV accepts only its 12-byte nonce.
void performOpensslGcmSivRegressionTest() {
	unsigned char key[16] = {1}, iv[12] = {3}, aad[16] = {9}, plaintext[8] = {5};
	unsigned char ciphertext[8] = {}, reference[16] = {}, fragmented[16] = {};
	EVP_CIPHER *cipher = EVP_CIPHER_fetch(nullptr, "AES-128-GCM-SIV", nullptr);
	if (!cipher) { fail("AES-GCM-SIV", "default provider cipher unavailable"); return; }
	auto encrypt = [&](bool emptyUpdate, unsigned char *tag) {
		EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
		if (!ctx) { return false; }
		int len = 0;
		bool ok = EVP_EncryptInit_ex(ctx, cipher, nullptr, key, iv) == 1
				&& EVP_EncryptUpdate(ctx, nullptr, &len, aad, sizeof(aad)) == 1;
		if (emptyUpdate) {
			ok = ok && EVP_EncryptUpdate(ctx, nullptr, &len, aad, 0) == 1;
		}
		ok = ok && EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, sizeof(plaintext)) == 1
				&& EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) == 1
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1;
		EVP_CIPHER_CTX_free(ctx);
		return ok;
	};
	check(encrypt(false, reference) && encrypt(true, fragmented)
			&& memcmp(reference, fragmented, sizeof(reference)) == 0,
			"AES-GCM-SIV empty AAD update preserves authentication");
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	check(ctx && EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) == 1,
			"AES-GCM-SIV init");
	if (ctx) {
		check(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 8, nullptr) == 0,
				"AES-GCM-SIV rejects short IV");
		check(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 16, nullptr) == 0,
				"AES-GCM-SIV rejects long IV");
		check(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr) == 1,
				"AES-GCM-SIV accepts standard IV");
	}
	EVP_CIPHER_CTX_free(ctx);
	EVP_CIPHER_free(cipher);
	ERR_clear_error();
}

#if SPRT_LINUX
static double ffiMixedArguments(int a, double b, int c) { return a + b * c; }

// Upstream Expat PR #1492: an invalid declaration reaches the unknown decoder
// while producing diagnostic context. The callback must never receive a partial
// multibyte sequence at the end of the input.
struct DecoderBounds {
	const char *buffer;
	size_t size;
	bool overread = false;
};

static int decodeUnknown(void *data, const char *s) {
	auto &bounds = *static_cast<DecoderBounds *>(data);
	unsigned char lead = static_cast<unsigned char>(s[0]);
	size_t length = lead == 0xc3 ? 2 : (lead == 0xa9 ? 4 : 1);
	ptrdiff_t offset = s - bounds.buffer;
	if (offset < 0 || static_cast<size_t>(offset) + length > bounds.size) {
		bounds.overread = true;
		return -1;
	}
	return lead == 0xc3 && static_cast<unsigned char>(s[1]) == 0xa9 ? 0x49 : -1;
}

static int unknownEncoding(void *data, const XML_Char *, XML_Encoding *encoding) {
	for (int i = 0; i < 256; ++i) { encoding->map[i] = i < 128 ? i : -1; }
	encoding->map[0xc3] = -2;
	encoding->map[0xa9] = -4;
	encoding->convert = &decodeUnknown;
	encoding->data = data;
	encoding->release = nullptr;
	return XML_STATUS_OK;
}

void performDependencyLinuxTest() {
	XML_Expat_Version version = XML_ExpatVersionInfo();
	check(version.major == XML_MAJOR_VERSION && version.minor == XML_MINOR_VERSION
			&& version.micro == XML_MICRO_VERSION, "Expat runtime/header version");
	XML_Parser parser = XML_ParserCreate(nullptr);
	check(parser != nullptr, "Expat create parser");
	if (parser) {
		const char xml[] = "<root value='1'>&amp;</root>";
		check(XML_Parse(parser, xml, sizeof(xml) - 1, XML_TRUE) == XML_STATUS_OK,
				"Expat parses document");
		XML_ParserFree(parser);
	}
	parser = XML_ParserCreate(nullptr);
	if (parser) {
		// A lone high surrogate in UTF-16LE, followed by a closing element.
		const unsigned char xml[] = {0xff,0xfe,'<',0,'r',0,'>',0,0,0xd8,'<',0,'/',0,'r',0,'>',0};
		check(XML_Parse(parser, (const char *)xml, sizeof(xml), XML_TRUE) == XML_STATUS_ERROR,
				"Expat rejects lone UTF-16 surrogate");
		XML_ParserFree(parser);
	} else { fail("Expat malformed document", "parser allocation failed"); }

	parser = XML_ParserCreate("x-q5");
	check(parser != nullptr, "Expat custom encoding parser");
	if (parser) {
		const char doc[] = "<?xml \xc3\xa9?>";
		char *buffer = static_cast<char *>(XML_GetBuffer(parser, sizeof(doc) - 1));
		check(buffer != nullptr, "Expat custom encoding buffer");
		if (buffer) {
			memcpy(buffer, doc, sizeof(doc) - 1);
			DecoderBounds bounds{buffer, sizeof(doc) - 1};
			XML_SetUnknownEncodingHandler(parser, &unknownEncoding, &bounds);
			check(XML_ParseBuffer(parser, sizeof(doc) - 1, XML_TRUE) == XML_STATUS_ERROR,
					"Expat rejects malformed custom-encoding declaration");
			check(!bounds.overread, "Expat custom decoder stays within input (CVE-2026-124317)");
		}
		XML_ParserFree(parser);
	}

	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint, &ffi_type_double, &ffi_type_sint};
	int a = 3, c = 4;
	double b = 2.5, result = 0;
	void *values[] = {&a, &b, &c};
	ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_double, types);
	checkInt("libffi prepare mixed ABI", status, FFI_OK);
	if (status == FFI_OK) {
		ffi_call(&cif, FFI_FN(ffiMixedArguments), &result, values);
		check(result == 13.0, "libffi mixed integer/floating-point arguments");
	}
}
#endif

} // namespace thirdparty::test
