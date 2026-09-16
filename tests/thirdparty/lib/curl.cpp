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

// Functional tests for the shipped libcurl, used through <curl/curl.h> alone.
// Three layers are covered:
//   * pure API surface (version/url/escape/slist/date/mime) - deterministic and
//     hermetic;
//   * the transfer engine, driven over file:// so no network is involved;
//   * a real TLS transfer against an in-process OpenSSL server on 127.0.0.1,
//     which is what proves the curl <-> OpenSSL pairing in the toolchain works.
// The public-internet test is opt-in (--network).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <threads.h>

#include <curl/curl.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "../tests.h"
#include "certgen.h"

// The host uses plain POSIX descriptors; on Windows the SPRT <sys/socket.h>
// carries winsock's 64-bit SOCKET, exactly as tests/libc/c/socket.cpp does.
#ifndef __SPRT_PLATFORM_ID
typedef int SOCKET;
#define closesocket(fd) close(fd)
#endif

namespace thirdparty::test {

//
// A growable sink for CURLOPT_WRITEFUNCTION
//

namespace {

struct Buffer {
	char data[64 * 1024];
	size_t size = 0;

	void clear() {
		size = 0;
		data[0] = 0;
	}

	const char *c_str() {
		data[size < sizeof(data) ? size : sizeof(data) - 1] = 0;
		return data;
	}
};

size_t writeToBuffer(char *ptr, size_t chunk, size_t count, void *userdata) {
	auto buf = (Buffer *)userdata;
	auto len = chunk * count;
	if (buf->size + len >= sizeof(buf->data)) {
		return 0; // signals an error to curl, which is what we want here
	}
	memcpy(buf->data + buf->size, ptr, len);
	buf->size += len;
	return len;
}

} // namespace

//
// curl: build identity
//

void performCurlVersionTest() {
	auto version = curl_version();
	printf("       %s\n", version ? version : "(null)");
	check(version != nullptr && strlen(version) > 0, "curl_version() is non-empty");

	auto info = curl_version_info(CURLVERSION_NOW);
	if (!info) {
		fail("curl_version_info", "returned null");
		return;
	}
	check(true, "curl_version_info");

	checkStr("libcurl version matches the header", info->version, LIBCURL_VERSION);
	checkInt("version_num matches the header", (long long)info->version_num,
			(long long)LIBCURL_VERSION_NUM);

	printf("       ssl_version: %s\n", info->ssl_version ? info->ssl_version : "(none)");
	printf("       libz: %s zstd: %s brotli: %s\n", info->libz_version ? info->libz_version : "-",
			info->zstd_version ? info->zstd_version : "-",
			info->brotli_version ? info->brotli_version : "-");

	// The whole point of the openssl flavour: it must be linked against the same
	// OpenSSL the openssl_* tests exercise.
	check(info->ssl_version != nullptr && strstr(info->ssl_version, "OpenSSL") != nullptr,
			"the TLS backend is OpenSSL");

	struct {
		int bit;
		const char *name;
	} features[] = {
		{CURL_VERSION_SSL, "SSL"},
		{CURL_VERSION_LIBZ, "libz"},
		{CURL_VERSION_BROTLI, "brotli"},
		{CURL_VERSION_ZSTD, "zstd"},
		{CURL_VERSION_HTTP3, "HTTP3"},
		{CURL_VERSION_IPV6, "IPv6"},
		{CURL_VERSION_ASYNCHDNS, "AsynchDNS"},
	};
	for (auto &f : features) { check((info->features & f.bit) != 0, f.name); }

	// The protocol table drives every transfer this file makes.
	bool haveHttp = false, haveHttps = false, haveFile = false;
	printf("       protocols:");
	for (auto p = info->protocols; p && *p; ++p) {
		printf(" %s", *p);
		if (strcmp(*p, "http") == 0) {
			haveHttp = true;
		} else if (strcmp(*p, "https") == 0) {
			haveHttps = true;
		} else if (strcmp(*p, "file") == 0) {
			haveFile = true;
		}
	}
	printf("\n");
	check(haveHttp, "http protocol");
	check(haveHttps, "https protocol");
	check(haveFile, "file protocol");
}

//
// curl: easy handle plumbing, no transfer
//

void performCurlEasyOptionsTest() {
	checkInt("curl_global_init", (long long)curl_global_init(CURL_GLOBAL_DEFAULT),
			(long long)CURLE_OK);

	auto curl = curl_easy_init();
	if (!curl) {
		fail("curl_easy_init", "returned null");
		return;
	}
	check(true, "curl_easy_init");

	checkInt("setopt CURLOPT_URL",
			(long long)curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/"),
			(long long)CURLE_OK);
	checkInt("setopt CURLOPT_FOLLOWLOCATION",
			(long long)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L), (long long)CURLE_OK);
	checkInt("setopt CURLOPT_TIMEOUT_MS",
			(long long)curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L), (long long)CURLE_OK);
	checkInt("setopt CURLOPT_USERAGENT",
			(long long)curl_easy_setopt(curl, CURLOPT_USERAGENT, "thirdparty-test/1.0"),
			(long long)CURLE_OK);
	checkInt("setopt CURLOPT_PROTOCOLS_STR",
			(long long)curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https,file"),
			(long long)CURLE_OK);
	// Content encodings only work if the codecs are actually linked in.
	checkInt("setopt CURLOPT_ACCEPT_ENCODING",
			(long long)curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""), (long long)CURLE_OK);

	// An option number no build knows must be reported, not silently accepted.
	checkInt("setopt with an unknown option",
			(long long)curl_easy_setopt(curl, (CURLoption)999999, 1L),
			(long long)CURLE_UNKNOWN_OPTION);

	long responseCode = -1;
	checkInt("getinfo CURLINFO_RESPONSE_CODE",
			(long long)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode),
			(long long)CURLE_OK);
	checkInt("response code before a transfer", responseCode, 0);

	// The option metadata table (curl_easy_option_*) must be consistent.
	auto opt = curl_easy_option_by_name("URL");
	check(opt != nullptr, "curl_easy_option_by_name(\"URL\")");
	if (opt) {
		checkInt("option id for URL", (long long)opt->id, (long long)CURLOPT_URL);
		checkInt("option type for URL", (long long)opt->type, (long long)CURLOT_STRING);
		auto byId = curl_easy_option_by_id(CURLOPT_URL);
		check(byId == opt, "curl_easy_option_by_id agrees with by_name");
	}

	auto dup = curl_easy_duphandle(curl);
	check(dup != nullptr, "curl_easy_duphandle");
	curl_easy_cleanup(dup);

	curl_easy_reset(curl);
	check(true, "curl_easy_reset");

	checkStr("curl_easy_strerror(CURLE_OK)", curl_easy_strerror(CURLE_OK), "No error");
	check(strlen(curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL)) > 0,
			"curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL)");

	curl_easy_cleanup(curl);
}

//
// curl: the URL parser
//

void performCurlUrlApiTest() {
	auto url = curl_url();
	if (!url) {
		fail("curl_url", "returned null");
		return;
	}
	check(true, "curl_url");

	checkInt("curl_url_set(full URL)",
			(long long)curl_url_set(url, CURLUPART_URL,
					"https://user:secret@example.com:8443/a/b?x=1&y=2#frag", 0),
			(long long)CURLUE_OK);

	struct {
		CURLUPart part;
		const char *name;
		const char *expect;
	} parts[] = {
		{CURLUPART_SCHEME, "scheme", "https"},
		{CURLUPART_USER, "user", "user"},
		{CURLUPART_PASSWORD, "password", "secret"},
		{CURLUPART_HOST, "host", "example.com"},
		{CURLUPART_PORT, "port", "8443"},
		{CURLUPART_PATH, "path", "/a/b"},
		{CURLUPART_QUERY, "query", "x=1&y=2"},
		{CURLUPART_FRAGMENT, "fragment", "frag"},
	};
	for (auto &p : parts) {
		char *value = nullptr;
		if (curl_url_get(url, p.part, &value, 0) == CURLUE_OK) {
			checkStr(p.name, value, p.expect);
			curl_free(value);
		} else {
			fail(p.name, "curl_url_get failed");
		}
	}

	// Relative resolution against the URL already held by the handle.
	auto relative = curl_url();
	check(curl_url_set(relative, CURLUPART_URL, "https://example.com/a/b", 0) == CURLUE_OK,
			"curl_url_set(base)");
	check(curl_url_set(relative, CURLUPART_URL, "../c", 0) == CURLUE_OK,
			"curl_url_set(relative reference)");
	char *resolved = nullptr;
	if (curl_url_get(relative, CURLUPART_URL, &resolved, 0) == CURLUE_OK) {
		checkStr("relative reference resolved", resolved, "https://example.com/c");
		curl_free(resolved);
	} else {
		fail("relative reference resolved", "curl_url_get failed");
	}
	curl_url_cleanup(relative);

	// A missing scheme is an error unless the caller opts into guessing.
	auto guess = curl_url();
	checkInt("curl_url_set without a scheme",
			(long long)curl_url_set(guess, CURLUPART_URL, "example.com/x", 0),
			(long long)CURLUE_BAD_SCHEME);
	checkInt("curl_url_set with CURLU_DEFAULT_SCHEME",
			(long long)curl_url_set(guess, CURLUPART_URL, "example.com/x", CURLU_DEFAULT_SCHEME),
			(long long)CURLUE_OK);
	curl_url_cleanup(guess);

	// Every error code must have a message.
	check(strlen(curl_url_strerror(CURLUE_BAD_SCHEME)) > 0, "curl_url_strerror");

	curl_url_cleanup(url);
}

//
// curl: percent-encoding
//

void performCurlEscapeTest() {
	auto escaped = curl_easy_escape(nullptr, "a b/c?d=e&f", 0);
	checkStr("curl_easy_escape", escaped, "a%20b%2Fc%3Fd%3De%26f");

	int unescapedLen = 0;
	auto unescaped = curl_easy_unescape(nullptr, escaped, 0, &unescapedLen);
	checkStr("curl_easy_unescape round trip", unescaped, "a b/c?d=e&f");
	checkInt("curl_easy_unescape length", unescapedLen, 11);
	curl_free(unescaped);
	curl_free(escaped);

	// Binary input: an embedded NUL must survive both directions.
	static const char binary[] = {'a', 0, 'b'};
	escaped = curl_easy_escape(nullptr, binary, int(sizeof(binary)));
	checkStr("curl_easy_escape over binary input", escaped, "a%00b");
	unescaped = curl_easy_unescape(nullptr, escaped, 0, &unescapedLen);
	checkInt("curl_easy_unescape over binary input", unescapedLen, 3);
	check(unescaped && memcmp(unescaped, binary, sizeof(binary)) == 0,
			"binary round trip is byte exact");
	curl_free(unescaped);
	curl_free(escaped);

	// Unreserved characters must not be touched (RFC 3986).
	escaped = curl_easy_escape(nullptr, "AZaz09-._~", 0);
	checkStr("unreserved characters are left alone", escaped, "AZaz09-._~");
	curl_free(escaped);
}

//
// curl: string lists
//

void performCurlSlistTest() {
	struct curl_slist *list = nullptr;
	list = curl_slist_append(list, "X-First: 1");
	list = curl_slist_append(list, "X-Second: 2");
	list = curl_slist_append(list, "X-Third: 3");
	check(list != nullptr, "curl_slist_append");

	int count = 0;
	const char *expect[] = {"X-First: 1", "X-Second: 2", "X-Third: 3"};
	for (auto it = list; it; it = it->next) {
		if (count < 3) {
			checkStr("slist entry keeps insertion order", it->data, expect[count]);
		}
		++count;
	}
	checkInt("slist length", count, 3);

	curl_slist_free_all(list);

	// A header list is only useful if a handle takes it.
	auto curl = curl_easy_init();
	struct curl_slist *headers = curl_slist_append(nullptr, "Accept: application/json");
	checkInt("setopt CURLOPT_HTTPHEADER",
			(long long)curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers), (long long)CURLE_OK);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
}

//
// curl: date parsing
//

void performCurlDateTest() {
	// The three formats RFC 9110 requires an HTTP client to accept, all naming
	// the same instant.
	checkInt("curl_getdate(IMF-fixdate)",
			(long long)curl_getdate("Sun, 06 Nov 1994 08:49:37 GMT", nullptr), 784111777LL);
	checkInt("curl_getdate(RFC 850)",
			(long long)curl_getdate("Sunday, 06-Nov-94 08:49:37 GMT", nullptr), 784111777LL);
	checkInt("curl_getdate(asctime)", (long long)curl_getdate("Sun Nov  6 08:49:37 1994", nullptr),
			784111777LL);

	checkInt("curl_getdate(epoch)", (long long)curl_getdate("Thu, 01 Jan 1970 00:00:00 GMT",
										   nullptr),
			0LL);
	checkInt("curl_getdate(garbage)", (long long)curl_getdate("not a date at all", nullptr), -1LL);
}

//
// curl: multipart bodies
//

void performCurlMimeTest() {
	auto curl = curl_easy_init();
	if (!curl) {
		fail("curl_easy_init", "returned null");
		return;
	}

	auto mime = curl_mime_init(curl);
	check(mime != nullptr, "curl_mime_init");

	auto field = curl_mime_addpart(mime);
	check(field != nullptr, "curl_mime_addpart");
	checkInt("curl_mime_name", (long long)curl_mime_name(field, "field"), (long long)CURLE_OK);
	checkInt("curl_mime_data", (long long)curl_mime_data(field, "value", CURL_ZERO_TERMINATED),
			(long long)CURLE_OK);

	auto file = curl_mime_addpart(mime);
	checkInt("curl_mime_name (file part)", (long long)curl_mime_name(file, "upload"),
			(long long)CURLE_OK);
	checkInt("curl_mime_filename", (long long)curl_mime_filename(file, "payload.txt"),
			(long long)CURLE_OK);
	checkInt("curl_mime_data (file part)",
			(long long)curl_mime_data(file, "contents", CURL_ZERO_TERMINATED), (long long)CURLE_OK);
	checkInt("curl_mime_type", (long long)curl_mime_type(file, "text/plain"), (long long)CURLE_OK);

	checkInt("setopt CURLOPT_MIMEPOST", (long long)curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime),
			(long long)CURLE_OK);

	curl_mime_free(mime);
	curl_easy_cleanup(curl);
}

//
// curl: the transfer engine over file://
//

namespace {

// Writes `content` into the current directory, returning the path used to open
// it (`path`, always POSIX - that is what the runtime speaks on every target)
// and a file:// URL for it (`url`).
//
// The URL is NOT simply "file://" + path on Windows: libcurl is a Windows build
// and applies DOS rules to a file: path - it drops the leading slash when the
// path starts with a drive spec ("/c:/...") and rewrites '/' as '\'. The runtime
// spells the same drive POSIX-style ("/c/..."), so the drive letter has to be
// written the way curl expects for the two conventions to meet in open().
bool writeTempFile(const char *name, const char *content, char *url, size_t urlLen,
		char *path, size_t pathLen) {
	char cwd[1024];
	if (!getcwd(cwd, sizeof(cwd))) {
		return false;
	}
	snprintf(path, pathLen, "%s/%s", cwd, name);

	auto f = fopen(path, "wb");
	if (!f) {
		return false;
	}
	auto len = strlen(content);
	auto written = fwrite(content, 1, len, f);
	fclose(f);
	if (written != len) {
		return false;
	}

#if defined(_WIN32)
	// "/c/dir/file" -> "file:///c:/dir/file"
	if (path[0] == '/' && path[1] && path[2] == '/') {
		snprintf(url, urlLen, "file:///%c:%s", path[1], path + 2);
		return true;
	}
#endif
	snprintf(url, urlLen, "file://%s", path);
	return true;
}

} // namespace

void performCurlFileTransferTest() {
	static const char *content = "third-party file transfer payload\n";
	char url[1200], path[1100];
	if (!writeTempFile("thirdparty-curl-file.txt", content, url, sizeof(url), path, sizeof(path))) {
		fail("temporary file", "could not be written");
		return;
	}
	printf("       %s\n", url);

	auto curl = curl_easy_init();
	if (!curl) {
		fail("curl_easy_init", "returned null");
		return;
	}

	Buffer body;
	body.clear();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToBuffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "file");

	auto res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		printf("       curl_easy_perform: %s\n", curl_easy_strerror(res));
	}
	checkInt("curl_easy_perform over file://", (long long)res, (long long)CURLE_OK);
	checkStr("transferred body", body.c_str(), content);

	curl_off_t downloaded = -1;
	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloaded);
	checkInt("CURLINFO_SIZE_DOWNLOAD_T", (long long)downloaded, (long long)strlen(content));

	// Not compared to `url` verbatim: a Windows libcurl normalises the drive form
	// ("file:///c:/x" -> "file://c:/x"), so only the parts that must survive are
	// checked.
	char *effective = nullptr;
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
	check(effective && strncmp(effective, "file://", 7) == 0
					&& strstr(effective, "thirdparty-curl-file.txt") != nullptr,
			"CURLINFO_EFFECTIVE_URL names the file that was fetched");

	// A file that is not there must fail with the dedicated code.
	char missing[1300];
	snprintf(missing, sizeof(missing), "%s.missing", url);
	body.clear();
	curl_easy_setopt(curl, CURLOPT_URL, missing);
	res = curl_easy_perform(curl);
	checkInt("a missing file:// target fails", (long long)res,
			(long long)CURLE_FILE_COULDNT_READ_FILE);

	// A protocol the handle was told not to allow must be refused before any I/O.
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	checkInt("a disallowed protocol is refused", (long long)res,
			(long long)CURLE_UNSUPPORTED_PROTOCOL);

	curl_easy_cleanup(curl);
	unlink(path);
}

//
// curl: the multi interface, driven over file://
//

void performCurlMultiTest() {
	static const char *content = "multi interface payload\n";
	char url[1200], path[1100];
	if (!writeTempFile("thirdparty-curl-multi.txt", content, url, sizeof(url), path,
				sizeof(path))) {
		fail("temporary file", "could not be written");
		return;
	}

	auto multi = curl_multi_init();
	if (!multi) {
		fail("curl_multi_init", "returned null");
		return;
	}
	check(true, "curl_multi_init");

	constexpr int kTransfers = 3;
	CURL *handles[kTransfers] = {};
	Buffer bodies[kTransfers];

	for (int i = 0; i < kTransfers; ++i) {
		handles[i] = curl_easy_init();
		bodies[i].clear();
		curl_easy_setopt(handles[i], CURLOPT_URL, url);
		curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, &writeToBuffer);
		curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &bodies[i]);
		curl_easy_setopt(handles[i], CURLOPT_PROTOCOLS_STR, "file");
		checkInt("curl_multi_add_handle", (long long)curl_multi_add_handle(multi, handles[i]),
				(long long)CURLM_OK);
	}

	int running = 0;
	CURLMcode mc = CURLM_OK;
	// Bounded so a stuck transfer fails the test instead of hanging the run.
	for (int iterations = 0; iterations < 1000; ++iterations) {
		mc = curl_multi_perform(multi, &running);
		if (mc != CURLM_OK || running == 0) {
			break;
		}
		int numfds = 0;
		mc = curl_multi_poll(multi, nullptr, 0, 100, &numfds);
		if (mc != CURLM_OK) {
			break;
		}
	}
	checkInt("curl_multi_perform", (long long)mc, (long long)CURLM_OK);
	checkInt("every transfer finished", running, 0);

	int done = 0, succeeded = 0;
	CURLMsg *msg = nullptr;
	int queued = 0;
	while ((msg = curl_multi_info_read(multi, &queued)) != nullptr) {
		if (msg->msg == CURLMSG_DONE) {
			++done;
			if (msg->data.result == CURLE_OK) {
				++succeeded;
			} else {
				printf("       transfer failed: %s\n", curl_easy_strerror(msg->data.result));
			}
		}
	}
	checkInt("CURLMSG_DONE for every handle", done, kTransfers);
	checkInt("every transfer reported CURLE_OK", succeeded, kTransfers);

	for (int i = 0; i < kTransfers; ++i) {
		checkStr("multi transfer body", bodies[i].c_str(), content);
		curl_multi_remove_handle(multi, handles[i]);
		curl_easy_cleanup(handles[i]);
	}

	checkInt("curl_multi_cleanup", (long long)curl_multi_cleanup(multi), (long long)CURLM_OK);
	unlink(path);
}

//
// curl over TLS against an in-process OpenSSL server on loopback
//

namespace {

struct LoopbackServer {
	SOCKET listenFd;
	SSL_CTX *ctx = nullptr;
	const char *body = nullptr;
	// 0 = served, otherwise the step that failed
	int result = -1;
};

// Accepts exactly one connection, completes the handshake, reads the request
// head and answers with a fixed 200. Any failure is recorded in `result` and
// the thread returns - the client side then fails its own checks.
int serveOnce(void *arg) {
	auto server = (LoopbackServer *)arg;

	struct pollfd pfd;
	pfd.fd = int(server->listenFd);
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, 15000) <= 0) {
		server->result = 1;
		return 1;
	}

	struct sockaddr_in peer;
	socklen_t peerLen = sizeof(peer);
	auto client = accept(server->listenFd, (struct sockaddr *)&peer, &peerLen);
	if (int(client) < 0) {
		server->result = 2;
		return 1;
	}

	auto ssl = SSL_new(server->ctx);
	if (!ssl) {
		closesocket(client);
		server->result = 3;
		return 1;
	}
	SSL_set_fd(ssl, int(client));

	if (SSL_accept(ssl) <= 0) {
		server->result = 4;
	} else {
		char request[2048];
		int total = 0;
		while (total < int(sizeof(request)) - 1) {
			auto n = SSL_read(ssl, request + total, int(sizeof(request)) - 1 - total);
			if (n <= 0) {
				break;
			}
			total += n;
			request[total] = 0;
			if (strstr(request, "\r\n\r\n")) {
				break;
			}
		}

		char response[1024];
		auto bodyLen = strlen(server->body);
		auto len = snprintf(response, sizeof(response),
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: %zu\r\n"
				"Connection: close\r\n"
				"\r\n"
				"%s",
				bodyLen, server->body);
		server->result = (SSL_write(ssl, response, len) == len) ? 0 : 5;
		SSL_shutdown(ssl);
	}

	SSL_free(ssl);
	closesocket(client);
	return 0;
}

} // namespace

void performCurlTlsLoopbackTest() {
	TestCredentials creds;
	if (!makeSelfSignedCredentials(creds, "localhost", "DNS:localhost,IP:127.0.0.1")) {
		fail("TLS credentials", "makeSelfSignedCredentials failed");
		return;
	}
	check(true, "TLS credentials");

	char pem[4096];
	auto pemLen = pemEncodeCert(creds.cert, pem, sizeof(pem));
	check(pemLen > 0, "PEM encoding of the server certificate");
	if (!pemLen) {
		return;
	}

	auto serverCtx = SSL_CTX_new(TLS_server_method());
	if (!serverCtx || SSL_CTX_use_certificate(serverCtx, creds.cert) != 1
			|| SSL_CTX_use_PrivateKey(serverCtx, creds.key) != 1) {
		fail("server SSL_CTX", "could not be configured");
		ERR_print_errors_fp(stdout);
		SSL_CTX_free(serverCtx);
		return;
	}
	check(true, "server SSL_CTX");

	// A listening socket on an ephemeral loopback port.
	LoopbackServer server;
	server.ctx = serverCtx;
	server.body = "hello from the loopback TLS server";
	server.listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (int(server.listenFd) < 0) {
		fail("socket(AF_INET, SOCK_STREAM)", "failed");
		SSL_CTX_free(serverCtx);
		return;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(server.listenFd, (struct sockaddr *)&addr, sizeof(addr)) != 0
			|| listen(server.listenFd, 1) != 0) {
		fail("bind/listen on 127.0.0.1", "failed");
		closesocket(server.listenFd);
		SSL_CTX_free(serverCtx);
		return;
	}
	check(true, "bind/listen on 127.0.0.1");

	struct sockaddr_in bound;
	socklen_t boundLen = sizeof(bound);
	if (getsockname(server.listenFd, (struct sockaddr *)&bound, &boundLen) != 0) {
		fail("getsockname", "failed");
		closesocket(server.listenFd);
		SSL_CTX_free(serverCtx);
		return;
	}
	auto port = int(ntohs(bound.sin_port));
	check(port != 0, "an ephemeral port was assigned");

	thrd_t thread;
	if (thrd_create(&thread, &serveOnce, &server) != thrd_success) {
		fail("thrd_create", "failed");
		closesocket(server.listenFd);
		SSL_CTX_free(serverCtx);
		return;
	}

	char url[64];
	snprintf(url, sizeof(url), "https://127.0.0.1:%d/", port);
	printf("       %s\n", url);

	auto curl = curl_easy_init();
	Buffer body;
	body.clear();

	// The generated certificate is the only trust anchor, and the IP SAN is what
	// the host check has to match - nothing here depends on the system CA store.
	struct curl_blob caBlob;
	caBlob.data = pem;
	caBlob.len = pemLen;
	caBlob.flags = CURL_BLOB_COPY;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &caBlob);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToBuffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);

	auto res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		printf("       curl_easy_perform: %s\n", curl_easy_strerror(res));
	}
	checkInt("curl_easy_perform over TLS", (long long)res, (long long)CURLE_OK);

	long responseCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
	checkInt("HTTP response code", responseCode, 200);
	checkStr("response body", body.c_str(), server.body);

	char *contentType = nullptr;
	curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
	checkStr("CURLINFO_CONTENT_TYPE", contentType, "text/plain");

	long verifyResult = -1;
	curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verifyResult);
	checkInt("CURLINFO_SSL_VERIFYRESULT", verifyResult, 0);

	curl_easy_cleanup(curl);

	int threadResult = 0;
	thrd_join(thread, &threadResult);
	checkInt("the server thread served the request", server.result, 0);

	closesocket(server.listenFd);
	SSL_CTX_free(serverCtx);
	ERR_clear_error();
}

//
// curl against the public internet (opt-in)
//

void performCurlHttpsTest() {
	auto curl = curl_easy_init();
	if (!curl) {
		fail("curl_easy_init", "returned null");
		return;
	}

	static const char *url = "https://example.com/";
	Buffer body;
	body.clear();

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToBuffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "thirdparty-test/1.0");
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

	// The CA bundle curl was configured with lives in the toolchain sources; let
	// the environment point at one when the baked-in path is not reachable.
	if (auto caFile = getenv("THIRDPARTY_CAINFO")) {
		curl_easy_setopt(curl, CURLOPT_CAINFO, caFile);
	}

	auto res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		printf("       curl_easy_perform: %s\n", curl_easy_strerror(res));
	}
	checkInt("curl_easy_perform over https", (long long)res, (long long)CURLE_OK);

	long responseCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
	checkInt("HTTP response code", responseCode, 200);
	check(body.size > 0, "a body was received");
	check(strstr(body.c_str(), "<html") != nullptr || strstr(body.c_str(), "<HTML") != nullptr,
			"the body looks like HTML");

	char *scheme = nullptr;
	curl_easy_getinfo(curl, CURLINFO_SCHEME, &scheme);
	checkStr("CURLINFO_SCHEME", scheme, "https");

	long verifyResult = -1;
	curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verifyResult);
	checkInt("CURLINFO_SSL_VERIFYRESULT", verifyResult, 0);

	curl_easy_cleanup(curl);
}

} // namespace thirdparty::test
