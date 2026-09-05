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

#include "XLRemoteTransport.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

TransportStream::~TransportStream() { }
TransportConnection::~TransportConnection() { }
TransportListener::~TransportListener() { }
Transport::~Transport() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

String PeerIdentity::getDescription() const {
	if (!description.empty()) {
		return description;
	}
	StringStream s;
	if (uid >= 0) {
		s << "uid=" << uid;
		if (pid >= 0) {
			s << " pid=" << pid;
		}
	} else if (!spki.empty()) {
		s << "spki=" << base16::encode<Interface>(BytesView(spki.data(), spki.size()));
	} else {
		s << "<anonymous>";
	}
	return s.str();
}

// Defined by the per-transport translation units. Each is a no-op where its dependencies do not
// exist, so the set of schemes a build understands is decided by what actually compiled -- asking
// for a missing one fails with a message (see logUnknownScheme) instead of a link error.
void registerMemTransport();
void registerQuicTransport();
void registerUnixTransport();

void initializeTransports() {
	static bool s_done = [] {
		registerMemTransport();
		registerQuicTransport();
		registerUnixTransport();
		return true;
	}();
	(void)s_done;
}

// --- registry ---

// One slot per scheme rather than a map: the set is closed and tiny, and an array keeps the lookup
// free of allocation on a path that runs during connection setup.
namespace {

struct RegistryData {
	Rc<Transport> transports[4];

	static RegistryData &get() {
		static RegistryData s_data;
		return s_data;
	}
};

static size_t schemeIndex(AddressScheme s) { return size_t(toInt(s)); }

} // namespace

void TransportRegistry::registerTransport(Rc<Transport> &&t) {
	if (!t) {
		return;
	}
	auto idx = schemeIndex(t->getScheme());
	auto &slot = RegistryData::get().transports[idx];
	if (slot) {
		log::source().warn("remote::Transport", "scheme '", getSchemeName(t->getScheme()),
				"' is already registered; keeping the first registration");
		return;
	}
	slot = sp::move(t);
}

Transport *TransportRegistry::get(AddressScheme s) {
	return RegistryData::get().transports[schemeIndex(s)].get();
}

bool TransportRegistry::has(AddressScheme s) { return get(s) != nullptr; }

Vector<AddressScheme> TransportRegistry::getSchemes() {
	Vector<AddressScheme> out;
	auto &data = RegistryData::get();
	for (size_t i = 0; i < sizeof(data.transports) / sizeof(data.transports[0]); ++i) {
		if (data.transports[i]) {
			out.emplace_back(AddressScheme(i));
		}
	}
	return out;
}

// Report what this build DOES understand alongside the refusal: a scheme missing here is a build
// configuration, not a typo, and the difference is invisible without the list.
static void logUnknownScheme(const Address &addr, StringView what) {
	String available;
	for (auto s : TransportRegistry::getSchemes()) {
		if (!available.empty()) {
			available.append(", ");
		}
		auto name = getSchemeName(s);
		available.append(name.data(), name.size());
	}
	if (available.empty()) {
		available = "none";
	}
	log::source().error("remote::Transport", "cannot ", what, " ", addr.description(),
			": scheme '", getSchemeName(addr.scheme),
			"' is not registered in this build (available: ", available, ")");
}

Rc<TransportConnection> TransportRegistry::connect(const Address &addr,
		const TransportClientConfig &cfg) {
	auto t = get(addr.scheme);
	if (!t) {
		logUnknownScheme(addr, "connect to");
		return nullptr;
	}
	return t->connect(addr, cfg);
}

Rc<TransportListener> TransportRegistry::listen(const Address &addr,
		const TransportServerConfig &cfg) {
	auto t = get(addr.scheme);
	if (!t) {
		logUnknownScheme(addr, "listen on");
		return nullptr;
	}
	return t->listen(addr, cfg);
}

} // namespace stappler::xenolith::remote
