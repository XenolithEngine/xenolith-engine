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

#include "XLSceneInspector.h"

#if defined(DEBUG) && !defined(SPRT_WINDOWS)
#include "XLSystem.h"
#include "XLContextInfo.h"
#include "XLInheritedStyle.h"

#include <unistd.h>
#include <thread>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <memory>
#include <cstdint>

// The engine ships a freestanding <sys/socket.h>/<sys/un.h> surface (only forwarded to the system
// headers under __SPRT_BUILD), and app/engine TUs are not built with it. The engine itself avoids
// raw sockets, so socket()/bind()/listen()/accept() are not declared anywhere else in a normal TU.
// Declare the minimal POSIX surface ourselves and define the AF_UNIX address layout inline.
extern "C" {
struct sockaddr; // opaque; only ever passed by pointer
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
}

#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {
namespace inspector {
namespace {

constexpr const char *kInspectorSock = "/tmp/xenolith-inspector.sock";

struct SockAddrUn {
#if defined(__APPLE__)
	uint8_t sun_len; // BSD address-length byte
	uint8_t sun_family;
#else
	uint16_t sun_family;
#endif
	char sun_path[108];
};

void appendNode(String &out, Node *n, int depth) {
	for (int i = 0; i < depth; ++i) { out += "  "; }
	if (auto tv = n->getType(); !tv.empty()) {
		out.append(tv.data(), tv.size());
	} else {
		out += "Node";
	}
	if (auto nv = n->getName(); !nv.empty()) {
		out += " #";
		out.append(nv.data(), nv.size());
	}
	if (auto id = n->getComponent<NodeIdentity>()) {
		for (auto &c : id->classes) {
			if (!c.empty()) {
				out += " .";
				out.append(c.data(), c.size());
			}
		}
	}
	out += n->isVisible() ? "  V" : "  H";
	auto cs = n->getContentSize();
	auto p = n->getPosition();
	auto col = n->getColor();
	char buf[200];
	snprintf(buf, sizeof(buf), "  sz=%.0fx%.0f pos=(%.0f,%.0f) z=%d color=(%.2f,%.2f,%.2f,%.2f)",
			cs.width, cs.height, p.x, p.y, (int)n->getLocalZOrder().get(), col.r, col.g, col.b,
			col.a);
	out.append(buf);
	if (auto ic = n->getComponent<InheritedColorStyle>()) {
		if (ic->defined & InheritedColorStyle::DefinedColor) {
			char b2[96];
			snprintf(b2, sizeof(b2), " inhColor=(%d,%d,%d)", ic->color.r, ic->color.g,
					ic->color.b);
			out.append(b2);
		}
	}
	if (auto ifs = n->getComponent<InheritedFontStyle>()) {
		if (ifs->defined & InheritedFontStyle::DefinedFontWeight) {
			char b3[64];
			snprintf(b3, sizeof(b3), " wght=%u", (unsigned)ifs->fontWeight.get());
			out.append(b3);
		}
	}
	out += "\n";
	for (auto &c : n->getChildren()) { appendNode(out, c.get(), depth + 1); }
}

struct State {
	std::mutex mu;
	String snapshot = "# inspector ready, waiting for first scene snapshot...\n";
	bool started = false;
};
State &state() {
	static State s;
	return s;
}

void start() {
	auto &s = state();
	std::lock_guard<std::mutex> lk(s.mu);
	if (s.started) { return; }
	s.started = true;
	std::thread([]() {
		::unlink(kInspectorSock);
		int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) { return; }
		SockAddrUn addr{};
#if defined(__APPLE__)
		addr.sun_len = (uint8_t)sizeof(SockAddrUn);
		addr.sun_family = (uint8_t)AF_UNIX;
#else
		addr.sun_family = (uint16_t)AF_UNIX;
#endif
		std::strncpy(addr.sun_path, kInspectorSock, sizeof(addr.sun_path) - 1);
		if (::bind(fd, (const struct sockaddr *)&addr, (unsigned int)sizeof(addr)) < 0
				|| ::listen(fd, 8) < 0) {
			::close(fd);
			return;
		}
		for (;;) {
			int c = ::accept(fd, nullptr, nullptr);
			if (c < 0) { continue; }
			String snap;
			{
				std::lock_guard<std::mutex> lk2(state().mu);
				snap = state().snapshot;
			}
			if (!snap.empty()) {
				const char *p = snap.data();
				size_t left = snap.size();
				while (left > 0) {
					auto w = ::write(c, p, left);
					if (w <= 0) { break; }
					p += w;
					left -= (size_t)w;
				}
			}
			::close(c);
		}
	}).detach();
}

} // namespace

void attach(Node *root) {
	if (!root) { return; }
	start();
	// Use the node's own scheduler (reliable per-frame dispatch) rather than a CallbackSystem,
	// whose update() is not driven by the director. Schedule on enter, unschedule on exit.
	struct Ctx {
		std::shared_ptr<uint64_t> next;
		bool scheduled = false;
	};
	auto ctx = std::make_shared<Ctx>();
	ctx->next = std::make_shared<uint64_t>(0);
	root->setEnterCallback([root, ctx](Scene *) {
		if (ctx->scheduled) { return; }
		ctx->scheduled = true;
		auto next = ctx->next;
		root->getScheduler()->schedulePerFrame(
				[root, next](const UpdateTime &t) {
					if (t.app < *next) { return; }
					*next = t.app + 200000; // refresh the snapshot ~5x/s
					char hdr[96];
					snprintf(hdr, sizeof(hdr), "# xenolith scene @ app=%llums\n",
							(unsigned long long)(t.app / 1000));
					String out = hdr;
					appendNode(out, root, 0);
					std::lock_guard<std::mutex> lk(state().mu);
					state().snapshot = sp::move(out);
				},
				root, 0, false);
	});
	root->setExitCallback([root, ctx]() {
		if (!ctx->scheduled) { return; }
		ctx->scheduled = false;
		root->getScheduler()->unschedule(root);
	});
}

} // namespace inspector
} // namespace stappler::xenolith
#endif
