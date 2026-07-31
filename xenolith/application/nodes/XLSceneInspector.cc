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

#include "SPLog.h"

#include <unistd.h>
#include <thread>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdarg>
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
constexpr const char *kInspectorLogsSock = "/tmp/xenolith-logs.sock";
constexpr size_t kLogBufferLimit = 4096;

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

	// Process-wide log ring buffer, fed by a CustomLog hook (registered lazily in startLogs()).
	// Served to MCP via a second UNIX socket (kInspectorLogsSock).
	std::mutex logMu;
	Vector<String> logBuffer;
	bool logsStarted = false;
	std::unique_ptr<stappler::log::CustomLog> logHook;
};
State &state() {
	static State s;
	return s;
}

// CustomLog sink: format every log entry as "[T][tag] message" and append it to the ring buffer.
// Returns true so the default sink (stderr/os_log) still runs — we only mirror, never replace.
bool logHookFn(stappler::log::LogType type, stappler::StringView tag,
		const sprt::source_location &, stappler::log::CustomLog::Type t,
		stappler::log::CustomLog::VA &va) {
	char tc = '?';
	switch (type) {
	case stappler::log::LogType::Verbose: tc = 'V'; break;
	case stappler::log::LogType::Debug: tc = 'D'; break;
	case stappler::log::LogType::Info: tc = 'I'; break;
	case stappler::log::LogType::Warn: tc = 'W'; break;
	case stappler::log::LogType::Error: tc = 'E'; break;
	case stappler::log::LogType::Fatal: tc = 'F'; break;
	}

	String line;
	line += '[';
	line += tc;
	line += ']';
	if (!tag.empty()) {
		line += '[';
		line.append(tag.data(), tag.size());
		line += ']';
	}
	line += ' ';

	if (t == stappler::log::CustomLog::Text) {
		line.append(va.text.data(), va.text.size());
	} else {
		char stackBuf[2048];
		__sprt_va_list tmp;
		va_copy(tmp, va.format.args);
		int n = ::vsnprintf(stackBuf, sizeof(stackBuf) - 1, va.format.format, tmp);
		va_end(tmp);
		if (n < 0) {
			n = 0;
		}
		if (size_t(n) > sizeof(stackBuf) - 1) {
			n = sizeof(stackBuf) - 1;
		}
		line.append(stackBuf, size_t(n));
	}

	auto &s = state();
	std::lock_guard<std::mutex> lk(s.logMu);
	s.logBuffer.push_back(sp::move(line));
	if (s.logBuffer.size() > kLogBufferLimit) {
		s.logBuffer.erase(s.logBuffer.begin());
	}
	return true;
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

void startLogs() {
	auto &s = state();
	std::lock_guard<std::mutex> lk(s.logMu);
	if (s.logsStarted) { return; }
	s.logsStarted = true;

	// Register the CustomLog sink so every log call is mirrored into the ring buffer.
	s.logHook = std::unique_ptr<stappler::log::CustomLog>(
			new stappler::log::CustomLog(logHookFn));
	{
		String boot = "[I][inspector] log capture started";
		s.logBuffer.push_back(sp::move(boot));
	}

	std::thread([]() {
		::unlink(kInspectorLogsSock);
		int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) { return; }
		SockAddrUn addr{};
#if defined(__APPLE__)
		addr.sun_len = (uint8_t)sizeof(SockAddrUn);
		addr.sun_family = (uint8_t)AF_UNIX;
#else
		addr.sun_family = (uint16_t)AF_UNIX;
#endif
		std::strncpy(addr.sun_path, kInspectorLogsSock, sizeof(addr.sun_path) - 1);
		if (::bind(fd, (const struct sockaddr *)&addr, (unsigned int)sizeof(addr)) < 0
				|| ::listen(fd, 8) < 0) {
			::close(fd);
			return;
		}
		for (;;) {
			int c = ::accept(fd, nullptr, nullptr);
			if (c < 0) { continue; }
			String dump;
			{
				std::lock_guard<std::mutex> lk2(state().logMu);
				for (const auto &l : state().logBuffer) {
					dump += l;
					dump += '\n';
				}
			}
			if (!dump.empty()) {
				const char *p = dump.data();
				size_t left = dump.size();
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
	startLogs();
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
