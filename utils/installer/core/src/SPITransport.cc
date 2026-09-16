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

#include "SPITransport.h"
#include "SPNetworkHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

// HTTP 2xx or FTP 226 (Transfer complete) are OK; anything else is reported verbatim.
void finishTransport(TransportResult &result, long code) {
	result.responseCode = code;
	if (code < 200 || code >= 300) {
		result.setError(Status::ErrorNotSupported, "unexpected response code ", code);
	}
}

} // namespace

TransportResult fetchText(StringView url, String &out) {
	TransportResult result;

	mem_std::NetworkHandle h;
	if (!h.init(network::Method::Get, url)) {
		result.setError(Status::ErrorNotPermitted, "init failed");
		return result;
	}

	h.setReceiveCallback([&out](char *data, size_t size) -> size_t {
		out.append(data, size);
		return size;
	});

	if (!h.perform()) {
		result.setError(Status::ErrorNotPermitted, "perform failed");
		return result;
	}

	finishTransport(result, h.getResponseCode());
	return result;
}

TransportResult fetchBytes(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress) {
	TransportResult result;

	mem_std::NetworkHandle h;
	if (!h.init(network::Method::Get, url)) {
		result.setError(Status::ErrorNotPermitted, "init failed");
		return result;
	}

	h.setReceiveCallback([&out](char *data, size_t size) -> size_t {
		out.insert(out.end(), reinterpret_cast<uint8_t *>(data),
				reinterpret_cast<uint8_t *>(data) + size);
		return size;
	});

	if (progress) {
		h.setDownloadProgress([&progress](int64_t total, int64_t now) -> int {
			progress(now, total);
			return 0;
		});
	}

	if (!h.perform()) {
		result.setError(Status::ErrorNotPermitted, "perform failed");
		return result;
	}

	finishTransport(result, h.getResponseCode());
	return result;
}

} // namespace stappler::xenolith::installer
