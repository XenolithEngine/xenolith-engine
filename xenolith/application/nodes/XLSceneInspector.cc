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

#if defined(DEBUG)
#include "XLSystem.h"
#include "XLInheritedStyle.h"
#include "XLDirector.h"
#include "XLAppThread.h"

#include "SPLog.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <memory>
#include <cstdint>

namespace STAPPLER_VERSIONIZED stappler::xenolith {
namespace inspector {
namespace {

constexpr size_t kLogBufferLimit = 4096;
constexpr size_t kMaxCommandLen = 64; // a command line is "scene\n" / "logs\n"

// The default listener address per platform; overridden by the
// XENOLITH_INSPECTOR_ADDRESS environment variable ("unix:/path",
// "unix:@abstract", "host:port" or ":port").
#if defined(SPRT_WINDOWS)
// Python on Windows has no practical AF_UNIX support - TCP loopback is pragmatic
constexpr const char *kDefaultAddress = "127.0.0.1:4490";
#elif defined(SPRT_ANDROID)
// no /tmp in the app sandbox; the abstract namespace works with
// `adb forward tcp:4490 localabstract:xenolith-inspector`
constexpr const char *kDefaultAddress = "unix:@xenolith-inspector";
#else
constexpr const char *kDefaultAddress = "unix:/tmp/xenolith-inspector.sock";
#endif

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
	bool started = false; // listener start attempted (process-wide, once)

	// Process-wide log ring buffer, fed by a CustomLog hook (registered on the
	// first attach()). Served through the same listener via the "logs" command.
	std::mutex logMu;
	Vector<String> logBuffer;
	bool logsStarted = false;
	std::unique_ptr<stappler::log::CustomLog> logHook;

	Rc<sprt::dispatch::ListenHandle> listener;
};
State &state() {
	static State s;
	return s;
}

// CustomLog sink: format every log entry as "[T][tag] message" and append it to
// the ring buffer. Called from arbitrary threads - hence the mutex. Returns
// true so the default sink (stderr/os_log) still runs — we only mirror, never
// replace.
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

void startLogCapture() {
	auto &s = state();
	std::lock_guard<std::mutex> lk(s.logMu);
	if (s.logsStarted) {
		return;
	}
	s.logsStarted = true;
	s.logHook =
			std::unique_ptr<stappler::log::CustomLog>(new stappler::log::CustomLog(logHookFn));
	s.logBuffer.push_back(String("[I][inspector] log capture started"));
}

// Build the reply for one command line ("scene" | "logs").
String makeReply(StringView cmd) {
	auto &s = state();
	if (cmd == "scene") {
		std::lock_guard<std::mutex> lk(s.mu);
		return s.snapshot;
	}
	if (cmd == "logs") {
		String dump;
		std::lock_guard<std::mutex> lk(s.logMu);
		for (const auto &l : s.logBuffer) {
			dump += l;
			dump += '\n';
		}
		return dump;
	}
	return String("# unknown command; expected 'scene' or 'logs'\n");
}

// One accepted connection: read a command line, reply with the dump, shut the
// write side down; the connection finalizes when the client closes.
void serveConnection(Rc<sprt::dispatch::StreamHandle> &&stream) {
	auto held = sp::move(stream);
	auto cmd = std::make_shared<String>();
	auto s = held.get();
	// the reader closure holds the handle alive; the dispatch layer breaks the
	// handle <-> closure cycle when the connection finalizes
	s->read([held, cmd](BytesView data) {
		using Status = stappler::Status;
		if (data.empty()) {
			return Status::Ok; // EOF before a full command: nothing to serve
		}
		for (size_t i = 0; i < data.size(); ++i) {
			const char c = char(data[i]);
			if (c == '\n') {
				// strip an optional trailing '\r'
				auto line = StringView(*cmd);
				if (line.ends_with("\r")) {
					line = StringView(line.data(), line.size() - 1);
				}
				auto reply = makeReply(line);
				held->write(BytesView(reinterpret_cast<const uint8_t *>(reply.data()),
						reply.size()));
				held->shutdownWrite();
				return Status::Done; // stop reading; wait for the peer to close
			}
			cmd->push_back(c);
			if (cmd->size() > kMaxCommandLen) {
				held->cancel();
				return Status::Done;
			}
		}
		return Status::Ok; // command incomplete: keep reading
	});
}

// Start the process-wide listener on the app thread's looper (called from the
// root's enter callback, which runs on the app thread).
void startServer(Node *root) {
	auto &s = state();
	{
		std::lock_guard<std::mutex> lk(s.mu);
		if (s.started) {
			return;
		}
		s.started = true;
	}

	auto director = root->getDirector();
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}

	sprt::dispatch::SocketAddress addr;
	if (auto env = ::getenv("XENOLITH_INSPECTOR_ADDRESS")) {
		addr = sprt::dispatch::SocketAddress::parse(StringView(env));
		if (!addr.isValid()) {
			log::warn("SceneInspector", "invalid XENOLITH_INSPECTOR_ADDRESS: ", env);
			return;
		}
	} else {
		addr = sprt::dispatch::SocketAddress::parse(StringView(kDefaultAddress));
	}

	s.listener = looper->listenSocket(addr,
			[](Rc<sprt::dispatch::StreamHandle> &&stream) { serveConnection(sp::move(stream)); });
	if (!s.listener) {
		// no socket support on this backend (wasm), or bind failure (logged by
		// the dispatch layer) - the inspector just stays off
		log::debug("SceneInspector", "listener not started on '", addr.description(), "'");
	} else {
		log::debug("SceneInspector", "listening on '", s.listener->getAddress().description(),
				"'");
	}
}

} // namespace

void attach(Node *root) {
	if (!root) {
		return;
	}
	startLogCapture();
	// Use the node's own scheduler (reliable per-frame dispatch) rather than a CallbackSystem,
	// whose update() is not driven by the director. Schedule on enter, unschedule on exit.
	struct Ctx {
		std::shared_ptr<uint64_t> next;
		bool scheduled = false;
	};
	auto ctx = std::make_shared<Ctx>();
	ctx->next = std::make_shared<uint64_t>(0);
	root->setEnterCallback([root, ctx](Scene *) {
		startServer(root); // app thread; idempotent
		if (ctx->scheduled) {
			return;
		}
		ctx->scheduled = true;
		auto next = ctx->next;
		root->getScheduler()->schedulePerFrame(
				[root, next](const UpdateTime &t) {
			if (t.app < *next) {
				return;
			}
			*next = t.app + 200'000; // refresh the snapshot ~5x/s
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
		if (!ctx->scheduled) {
			return;
		}
		ctx->scheduled = false;
		root->getScheduler()->unschedule(root);
	});
}

} // namespace inspector
} // namespace stappler::xenolith
#endif
