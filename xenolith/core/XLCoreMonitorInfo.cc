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

#include "XlCoreMonitorInfo.h"

extern "C" {
#include "XLCorePNPID.cc"
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

static String getManufacturerName(const unsigned char *x) {
	String name;
	name.resize(3);

	name[0] = ((x[0] & 0x7c) >> 2) + '@';
	name[1] = ((x[0] & 0x03) << 3) + ((x[1] & 0xe0) >> 5) + '@';
	name[2] = (x[1] & 0x1f) + '@';

	return name;
}

static String extractString(const unsigned char *x, unsigned len) {
	String ret;
	bool seen_newline = false;
	unsigned i;

	for (i = 0; i < len; i++) {
		// The encoding is cp437, so any character is allowed,
		// but in practice it is unwise to use a non-ASCII character.
		bool non_ascii = (x[i] >= 1 && x[i] < 0x20 && x[i] != 0x0a) || x[i] >= 0x7f;

		if (seen_newline) {
			if (x[i] != 0x20) {
				return ret;
			}
		} else if (x[i] == 0x0a) {
			seen_newline = true;
		} else if (!x[i]) {
			return ret;
		} else if (x[i] == 0xff) {
			return ret;
		} else if (!non_ascii) {
			ret.push_back(x[i]);
		} else {
			ret.push_back('.');
		}
	}
	return ret;
}

static void parseDetailedBlock(const unsigned char *x, EdidInfo &info) {
	static const unsigned char zero_descr[18] = {0};

	if (!memcmp(x, zero_descr, sizeof(zero_descr))) {
		return;
	}

	switch (x[3]) {
	case 0xfc: info.model = extractString(x + 5, 13); return;
	case 0xff: info.serial = extractString(x + 5, 13); return;
	default: return;
	}
}

EdidInfo EdidInfo::parse(BytesView data) {
	// std::cout << base16::encode<Interface>(data) << "\n";

	EdidInfo ret;
	auto x = data.data();

	ret.vendorId = getManufacturerName(x + 0x08);
	if (ret.vendorId == "CID") {
		ret.vendorId.clear();
	} else {
		ret.vendor = StringView(pnp_name(ret.vendorId.data()));
		if (ret.vendor.empty()) {
			ret.vendor = ret.vendorId;
		}
	}

	parseDetailedBlock(x + 0x36, ret);
	parseDetailedBlock(x + 0x48, ret);
	parseDetailedBlock(x + 0x5a, ret);
	parseDetailedBlock(x + 0x6c, ret);

	return ret;
}

StringView EdidInfo::getVendorName(StringView data) { return pnp_name(data.data()); }

const ModeInfo ModeInfo::Preferred{maxOf<uint16_t>(), maxOf<uint16_t>(), 0};
const ModeInfo ModeInfo::Current{maxOf<uint16_t>(), maxOf<uint16_t>(), maxOf<uint16_t>()};
const MonitorId MonitorId::Primary{"__primary__"};
const MonitorId MonitorId::None;

const FullscreenInfo FullscreenInfo::None{MonitorId::None, ModeInfo::Current,
	FullscreenFlags::None};

const FullscreenInfo FullscreenInfo::Current{MonitorId::None, ModeInfo::Current,
	FullscreenFlags::Current};

} // namespace stappler::xenolith::core
