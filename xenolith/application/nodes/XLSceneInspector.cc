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
#include "XLNode.h"
#include "XLInheritedStyle.h"
#include "XLDirector.h"
#include "XLAppThread.h"

#include "SPLog.h"

#include <sprt/runtime/dispatch/looper.h>

#include <sprt/cxx/mutex>
#include <sprt/c/__sprt_stdarg.h> // va_copy/va_end for the printf-style log hook
#include <sprt/c/__sprt_stdio.h> // vsnprintf for the printf-style log hook

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace {

constexpr size_t LOG_BUFFER_LIMIT = 4'096;
constexpr size_t MAX_COMMAND_LEN = 64; // a command line is "scene\n" / "logs\n"
constexpr size_t LOG_FORMAT_BUFFER = 2'048;

// The default listener address per platform; overridden by the XENOLITH_INSPECTOR_ADDRESS
// environment variable ("unix:/path", "unix:@abstract", "host:port" or ":port").
#if SPRT_WINDOWS
// Python on Windows has no practical AF_UNIX support - TCP loopback is pragmatic
constexpr StringView DEFAULT_ADDRESS = StringView("127.0.0.1:4490");
#elif SPRT_ANDROID
// no /tmp in the app sandbox; the abstract namespace works with
// `adb forward tcp:4490 localabstract:xenolith-inspector`
constexpr StringView DEFAULT_ADDRESS = StringView("unix:@xenolith-inspector");
#else
constexpr StringView DEFAULT_ADDRESS = StringView("unix:/tmp/xenolith-inspector.sock");
#endif

// Process-wide log ring buffer, fed by a CustomLog hook installed with the first inspector.
// Written from arbitrary threads, hence the mutex; the scene dump needs none, it is built on
// the app thread inside the serve callback.
struct LogCapture {
	sprt::mutex mutex;
	Vector<String> buffer;
	bool started = false;
	log::CustomLog *hook = nullptr;

	~LogCapture() {
		if (hook) {
			sprt::__delete(hook);
			hook = nullptr;
		}
	}
};

LogCapture &logCapture() {
	static LogCapture s;
	return s;
}

// The inspector that currently owns the process-wide listener. Only ever read and written from
// handleEnter/handleExit, i.e. on the app thread, so it needs no synchronization of its own.
SceneInspector *s_listenerOwner = nullptr;

// CustomLog sink: format every entry as "[T][tag] message" and append it to the ring buffer.
// Returns true so the default sink (stderr/os_log) still runs - we only mirror, never replace.
bool logHookFn(log::LogType type, StringView tag, const sprt::source_location &,
		log::CustomLog::Type t, log::CustomLog::VA &va) {
	StringView typeChar;
	switch (type) {
	case log::LogType::Verbose: typeChar = StringView("V"); break;
	case log::LogType::Debug: typeChar = StringView("D"); break;
	case log::LogType::Info: typeChar = StringView("I"); break;
	case log::LogType::Warn: typeChar = StringView("W"); break;
	case log::LogType::Error: typeChar = StringView("E"); break;
	case log::LogType::Fatal: typeChar = StringView("F"); break;
	}

	StringStream line;
	line << "[" << typeChar << "]";
	if (!tag.empty()) {
		line << "[" << tag << "]";
	}
	line << " ";

	if (t == log::CustomLog::Text) {
		line << va.text;
	} else {
		// a caller-supplied printf format string: only vsnprintf can expand it
		char formatBuffer[LOG_FORMAT_BUFFER];
		__sprt_va_list tmp;
		__sprt_va_copy(tmp, va.format.args);
		auto n = __sprt_vsnprintf(formatBuffer, sizeof(formatBuffer) - 1, va.format.format, tmp);
		__sprt_va_end(tmp);
		if (n > 0) {
			line << StringView(formatBuffer, sprt::min(size_t(n), sizeof(formatBuffer) - 1));
		}
	}

	auto &capture = logCapture();
	sprt::lock_guard<sprt::mutex> lock(capture.mutex);
	capture.buffer.emplace_back(line.str());
	if (capture.buffer.size() > LOG_BUFFER_LIMIT) {
		capture.buffer.erase(capture.buffer.begin());
	}
	return true;
}

void startLogCapture() {
	auto &capture = logCapture();
	{
		sprt::lock_guard<sprt::mutex> lock(capture.mutex);
		if (capture.started) {
			return;
		}
		capture.started = true;
		capture.buffer.emplace_back(String("[I][SceneInspector] log capture started"));
	}
	// installed outside the lock: registration makes the hook reachable, and the first entry it
	// receives takes the same mutex
	capture.hook = sprt::__new<log::CustomLog>(logHookFn);
}

void writeLogDump(const Callback<void(StringView)> &out) {
	auto &capture = logCapture();
	sprt::lock_guard<sprt::mutex> lock(capture.mutex);
	for (auto &it : capture.buffer) { out << it << "\n"; }
}

void writeNode(const Callback<void(StringView)> &out, Node *node, uint32_t depth) {
	for (uint32_t i = 0; i < depth; ++i) { out << "  "; }

	auto identity = node->getComponent<NodeIdentity>();
	out << (identity && !identity->type.empty() ? StringView(identity->type) : StringView("Node"));
	if (identity && !identity->name.empty()) {
		out << " #" << identity->name;
	}
	if (identity) {
		for (auto &it : identity->classes) {
			if (!it.empty()) {
				out << " ." << it;
			}
		}
	}

	auto size = node->getContentSize();
	auto position = node->getPosition();
	auto color = node->getColor();

	// io_fixed's second argument is the number of SIGNIFICANT digits (not decimal places): 6 keeps
	// sub-pixel layout values readable, 2 is the useful resolution of a 0..1 colour channel
	out << (node->isVisible() ? "  V" : "  H") << "  sz=" << sprt::io_fixed(size.width, 6) << "x"
		<< sprt::io_fixed(size.height, 6) << " pos=(" << sprt::io_fixed(position.x, 6) << ","
		<< sprt::io_fixed(position.y, 6) << ") z=" << node->getLocalZOrder().get() << " color=("
		<< sprt::io_fixed(color.r, 2) << "," << sprt::io_fixed(color.g, 2) << ","
		<< sprt::io_fixed(color.b, 2) << "," << sprt::io_fixed(color.a, 2) << ")";

	if (auto style = node->getComponent<InheritedColorStyle>()) {
		if (style->defined & InheritedColorStyle::DefinedColor) {
			out << " inhColor=(" << style->color.r << "," << style->color.g << "," << style->color.b
				<< ")";
		}
	}
	if (auto style = node->getComponent<InheritedFontStyle>()) {
		if (style->defined & InheritedFontStyle::DefinedFontWeight) {
			out << " wght=" << style->fontWeight.get();
		}
	}
	out << "\n";

	for (auto &it : node->getChildren()) { writeNode(out, it.get(), depth + 1); }
}

// Accumulates a command line across reads (a read may deliver a partial line)
struct InspectorCommand : Ref {
	String line;
};

sprt::dispatch::SocketAddress resolveAddress() {
	if (auto env = ::getenv("XENOLITH_INSPECTOR_ADDRESS")) {
		auto addr = sprt::dispatch::SocketAddress::parse(StringView(env));
		if (!addr.isValid()) {
			slog().warn("SceneInspector", "invalid XENOLITH_INSPECTOR_ADDRESS: ", env);
		}
		return addr;
	}
	return sprt::dispatch::SocketAddress::parse(DEFAULT_ADDRESS);
}

} // namespace

SceneInspector::~SceneInspector() { }

bool SceneInspector::init() {
	if (!System::init()) {
		return false;
	}

	// enter/exit is all this system needs: the listener is armed while the owner is on a scene,
	// and everything else happens inside the socket callbacks
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
	return true;
}

void SceneInspector::writeSceneDump(const Callback<void(StringView)> &out) const {
	if (!_owner) {
		out << "# no owner\n";
		return;
	}
	writeNode(out, _owner, 0);
}

void SceneInspector::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	startLogCapture();

	if (s_listenerOwner) {
		return; // another inspector already holds the process-wide listener
	}

	auto director = _owner ? _owner->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}

	auto address = resolveAddress();
	if (!address.isValid()) {
		return;
	}

	// The listener lives on the app looper, so every accept/read callback below runs on the app
	// thread - the same thread that owns the scene graph. That is what lets the dump be built on
	// demand instead of being snapshotted on a timer.
	_listener = looper->listenSocket(address, [this](Rc<sprt::dispatch::StreamHandle> &&stream) {
		serveConnection(sp::move(stream));
	}, this);
	if (!_listener) {
		// no socket support on this backend (wasm), or bind failure (logged by the dispatch
		// layer) - the inspector just stays off
		slog().debug("SceneInspector", "listener not started on '", address.description(), "'");
		return;
	}

	s_listenerOwner = this;
	slog().debug("SceneInspector", "listening on '", _listener->getAddress().description(), "'");
}

void SceneInspector::handleExit() {
	if (_listener) {
		_listener->cancel();
		_listener = nullptr;

		if (s_listenerOwner == this) {
			s_listenerOwner = nullptr;
		}
	}

	System::handleExit();
}

// One accepted connection: read a command line, reply with the dump, shut the write side down;
// the connection finalizes when the client closes.
void SceneInspector::serveConnection(Rc<sprt::dispatch::StreamHandle> &&stream) {
	auto command = Rc<InspectorCommand>::alloc();
	auto handle = sp::move(stream);
	auto target = handle.get();

	// the reader closure holds the handle and this system alive; the dispatch layer breaks the
	// handle <-> closure cycle when the connection finalizes
	target->read([self = Rc<SceneInspector>(this), handle, command](BytesView data) {
		if (data.empty()) {
			return Status::Ok; // EOF before a full command: nothing to serve
		}

		for (size_t i = 0; i < data.size(); ++i) {
			auto c = char(data[i]);
			if (c == '\n') {
				auto line = StringView(command->line);
				line.trimChars<StringView::Chars<'\r'>>();

				StringStream reply;
				if (line == "scene") {
					reply << "# xenolith scene\n";
					self->writeSceneDump([&](StringView str) { reply << str; });
				} else if (line == "logs") {
					writeLogDump([&](StringView str) { reply << str; });
				} else {
					reply << "# unknown command; expected 'scene' or 'logs'\n";
				}

				auto text = reply.weak();
				handle->write(
						BytesView(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
				handle->shutdownWrite();
				return Status::Done; // stop reading; wait for the peer to close
			}

			command->line.push_back(c);
			if (command->line.size() > MAX_COMMAND_LEN) {
				handle->cancel();
				return Status::Done;
			}
		}
		return Status::Ok; // command incomplete: keep reading
	});
}

namespace inspector {

void attach(Node *root) {
	if (!root || root->getSystemByType<SceneInspector>()) {
		return;
	}
	root->addSystem(Rc<SceneInspector>::create());
}

} // namespace inspector

} // namespace stappler::xenolith

#endif
