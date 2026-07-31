#include "SPITransport.h"
#include "SPNetworkHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

TransportResult fetch_text(StringView url, String &out) {
	mem_std::NetworkHandle h;
	if (!h.init(network::Method::Get, toString(url))) {
		return {Status::ErrorNotPermitted, 0, toString("init failed")};
	}

	h.setReceiveCallback([&out](char *data, size_t size) -> size_t {
		out.append(data, size);
		return size;
	});

	if (!h.perform()) {
		return {Status::ErrorNotPermitted, 0, toString("perform failed")};
	}

	long code = h.getResponseCode();
	// HTTP 200 or FTP 226 (Transfer complete) are OK.
	if (code >= 200 && code < 300) {
		return {Status::Ok, code, String()};
	}
	char buf[96];
	snprintf(buf, sizeof(buf), "unexpected response code %ld", code);
	return {Status::ErrorNotSupported, code, toString(buf)};
}

TransportResult fetch_bytes(StringView url, Bytes &out,
		const Function<void(int64_t, int64_t)> &progress) {
	mem_std::NetworkHandle h;
	if (!h.init(network::Method::Get, toString(url))) {
		return {Status::ErrorNotPermitted, 0, toString("init failed")};
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
		return {Status::ErrorNotPermitted, 0, toString("perform failed")};
	}

	long code = h.getResponseCode();
	if (code >= 200 && code < 300) {
		return {Status::Ok, code, String()};
	}
	char buf[96];
	snprintf(buf, sizeof(buf), "unexpected response code %ld", code);
	return {Status::ErrorNotSupported, code, toString(buf)};
}

} // namespace stappler::xenolith::installer
