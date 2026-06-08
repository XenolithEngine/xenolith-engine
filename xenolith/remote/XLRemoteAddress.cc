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

#include "XLRemoteAddress.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

Address Address::parse(StringView str) {
	Address addr;
	if (str.starts_with("unix:")) {
		addr.unixDomain = true;
		addr.path = str.sub(5).str<Interface>();
		return addr;
	}

	// network "host:port" or ":port" -- split on the last ':'
	size_t colon = maxOf<size_t>();
	for (size_t i = str.size(); i > 0; --i) {
		if (str[i - 1] == ':') {
			colon = i - 1;
			break;
		}
	}

	if (colon != maxOf<size_t>()) {
		addr.host = str.sub(0, colon).str<Interface>();
		addr.port = uint16_t(str.sub(colon + 1).readInteger(10).get(0));
	} else {
		addr.port = uint16_t(str.readInteger(10).get(0));
	}
	return addr;
}

String Address::description() const {
	StringStream s;
	if (unixDomain) {
		s << "unix:" << path;
	} else {
		s << host << ":" << port;
	}
	return s.str();
}

} // namespace stappler::xenolith::remote
