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
#include "XLInteractiveComponent.h"
#include "XLFocusWithin.h"

#include "XLNode.h"
#include "XLAction.h" // RenderContinuously, for the `render` command
#include "XLInheritedStyle.h"
#include "XLDirector.h"
#include "XLAppThread.h"
#include "XLCoreRenderSession.h"
#include "XLTextInputManager.h"

#if MODULE_XENOLITH_FONT
// Downstream module: reached only through the font::FontController application extension, for the
// `fonts` command.
#include "XLFontController.h"
#endif

#include "SPLog.h"
#include "SPData.h"
#include "SPBitmap.h"

#include <sprt/runtime/dispatch/looper.h>

#include <sprt/cxx/mutex>
#include <sprt/c/__sprt_stdarg.h> // va_copy/va_end for the printf-style log hook
#include <sprt/c/__sprt_stdio.h> // vsnprintf for the printf-style log hook
#include <sprt/c/__sprt_stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace {

constexpr size_t LOG_BUFFER_LIMIT = 4'096;
constexpr size_t MAX_COMMAND_LEN = 64; // a handshake line is "scene\n" / "logs\n" / "xenolith/1\n"
constexpr size_t LOG_FORMAT_BUFFER = 2'048;

// A single frame is a scene dump or a PNG; anything past this is a broken or hostile peer.
constexpr uint32_t MAX_FRAME_SIZE = 64u * 1'024u * 1'024u;

constexpr StringView PROTOCOL_HANDSHAKE = StringView("xenolith/1");

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
	// Index that buffer[0] would have in an unbounded log. Lets a client resume with `since`
	// instead of re-reading the whole ring on every poll.
	uint64_t firstIndex = 0;
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

// Every inspector attached to a running scene, in attach order. SceneContent attaches one per
// scene, so this is one entry per window - which is what lets the single session that owns the
// socket reach a popup's or a dialog's scene, not only the one it happens to live on. Same thread
// discipline as s_listenerOwner; function-local so it is built on first use rather than at static
// init, like logCapture() above.
Vector<SceneInspector *> &inspectors() {
	static Vector<SceneInspector *> s;
	return s;
}

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
		++capture.firstIndex;
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

// { "lines": [...], "next": <cursor to pass as `since` next time> }
Value getLogDump(uint64_t since) {
	auto &capture = logCapture();
	sprt::lock_guard<sprt::mutex> lock(capture.mutex);

	Value lines(Value::Type::ARRAY);
	// entries older than `since` are already known to the client; entries dropped from the ring
	// since then are simply gone, so clamp instead of failing
	auto start = since > capture.firstIndex ? size_t(since - capture.firstIndex) : size_t(0);
	for (size_t i = start; i < capture.buffer.size(); ++i) { lines.addString(capture.buffer[i]); }

	Value ret;
	ret.setValue(sp::move(lines), "lines");
	ret.setInteger(int64_t(capture.firstIndex + capture.buffer.size()), "next");
	return ret;
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

	/* ...and the STATE, written the way a stylesheet asks for it. A control used to publish
	`:checked` and `:disabled` as style classes, so a dump showed them for free; now they are bits
	in InteractiveComponent, and a dump that showed only classes would show a checked row exactly
	like an unchecked one. */
	if (auto ic = node->getComponent<InteractiveComponent>()) {
		auto state = ic->state;
		auto put = [&](InteractiveState flag, StringView name) {
			if (sprt::hasFlag(state, flag)) {
				out << " :" << name;
			}
		};
		// `:disabled` is the ABSENCE of Enabled, which is why it is printed by hand
		if (!sprt::hasFlag(state, InteractiveState::Enabled)) {
			out << " :disabled";
		}
		put(InteractiveState::Focus, "focus");
		put(InteractiveState::Hover, "hover");
		put(InteractiveState::Active, "active");
		put(InteractiveState::Checked, "checked");
		put(InteractiveState::Invalid, "invalid");
		put(InteractiveState::ReadOnly, "read-only");
		put(InteractiveState::Indeterminate, "indeterminate");
		put(InteractiveState::Required, "required");
		put(InteractiveState::Default, "default");
		put(InteractiveState::FocusVisible, "focus-visible");
	}
	if (hasFocusWithin(node)) {
		out << " :focus-within";
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

sprt::dispatch::SocketAddress resolveAddress() {
	if (auto env = __sprt_getenv("XENOLITH_INSPECTOR_ADDRESS")) {
		auto addr = sprt::dispatch::SocketAddress::parse(StringView(env));
		if (!addr.isValid()) {
			slog().warn("SceneInspector", "invalid XENOLITH_INSPECTOR_ADDRESS: ", env);
		}
		return addr;
	}
	return sprt::dispatch::SocketAddress::parse(DEFAULT_ADDRESS);
}

// The listener is not free (a socket, a path in /tmp), so it is armed only where it is wanted:
// during development, when explicitly addressed, and always in headless mode - there the socket is
// the only interface the process has.
bool isInspectorEnabled(const ContextInfo *info) {
#if defined(DEBUG)
	return true;
#else
	if (__sprt_getenv("XENOLITH_INSPECTOR_ADDRESS")) {
		return true;
	}
	if (info && hasFlag(info->flags, sprt::window::ContextFlags::Headless)) {
		return true;
	}
	return false;
#endif
}

// Reverse lookups for the enum names the protocol speaks. Derived from the same name tables the
// engine prints with, so they can not drift.
sprt::window::InputEventName parseEventName(StringView name) {
	for (uint32_t i = 0; i < toInt(sprt::window::InputEventName::Max); ++i) {
		auto value = sprt::window::InputEventName(i);
		if (sprt::window::getInputEventName(value) == name) {
			return value;
		}
	}
	return sprt::window::InputEventName::None;
}

sprt::window::InputMouseButton parseButtonName(StringView name) {
	for (uint32_t i = 0; i < toInt(sprt::window::InputMouseButton::Max); ++i) {
		auto value = sprt::window::InputMouseButton(i);
		if (sprt::window::getInputButtonName(value) == name) {
			return value;
		}
	}
	return sprt::window::InputMouseButton::None;
}

sprt::window::InputKeyCode parseKeyCodeName(StringView name) {
	for (uint32_t i = 0; i < toInt(sprt::window::InputKeyCode::Max); ++i) {
		auto value = sprt::window::InputKeyCode(i);
		if (sprt::window::getInputKeyCodeName(value) == name) {
			return value;
		}
	}
	return sprt::window::InputKeyCode::Unknown;
}

// One protocol event -> one InputEventData. Returns false for an event name the engine does not
// know, so the caller can report which entry was rejected instead of injecting garbage.
bool readInputEvent(const Value &src, core::InputEventData &out) {
	auto name = src.isInteger("event") ? sprt::window::InputEventName(src.getInteger("event"))
									   : parseEventName(src.getString("event"));
	if (name == sprt::window::InputEventName::None
			|| toInt(name) >= toInt(sprt::window::InputEventName::Max)) {
		return false;
	}

	out = core::InputEventData();
	out.event = name;
	out.id = uint32_t(src.getInteger("id", 0));

	out.input.button = src.isInteger("button")
			? sprt::window::InputMouseButton(src.getInteger("button"))
			: parseButtonName(src.getString("button"));
	out.input.modifiers = sprt::window::InputModifier(src.getInteger("modifiers", 0));
	out.input.x = float(src.getDouble("x", 0.0));
	out.input.y = float(src.getDouble("y", 0.0));

	if (out.isKeyEvent()) {
		out.key.keycode = src.isInteger("keycode")
				? sprt::window::InputKeyCode(src.getInteger("keycode"))
				: parseKeyCodeName(src.getString("keycode"));
		out.key.compose = sprt::window::InputKeyComposeState(src.getInteger("compose", 0));
		out.key.keysym = uint32_t(src.getInteger("keysym", 0));
		// A codepoint is easier to write as the character itself than as a number; only the first
		// one is taken, since one key event carries one character.
		if (src.isString("keychar")) {
			auto str = src.getString("keychar");
			out.key.keychar =
					str.empty() ? char32_t(0) : sprt::unicode::utf8Decode32(str.data(), str.size());
		} else {
			out.key.keychar = char32_t(src.getInteger("keychar", 0));
		}
	} else {
		out.point.valueX = float(src.getDouble("valueX", 0.0));
		out.point.valueY = float(src.getDouble("valueY", 0.0));
		out.point.density = float(src.getDouble("density", 1.0));
	}
	return true;
}

Value encodeGeometry(const sprt::window::WindowGeometry &g) {
	Value ret;
	// Logical units, the same space WindowInfo::rect takes - so what is read here can be handed
	// straight back to createWindow with WindowCreationFlags::UsePosition.
	ret.setInteger(int64_t(g.rect.x), "x");
	ret.setInteger(int64_t(g.rect.y), "y");
	ret.setInteger(int64_t(g.rect.width), "width");
	ret.setInteger(int64_t(g.rect.height), "height");
	// Without this the two zeroes above are indistinguishable from a window at the top-left corner.
	ret.setBool(g.hasPosition, "hasPosition");
	return ret;
}

Value encodeConstraints(const core::FrameConstraints &c) {
	Value ret;
	ret.setInteger(int64_t(c.extent.width), "width");
	ret.setInteger(int64_t(c.extent.height), "height");
	ret.setDouble(double(c.density), "density");
	ret.setDouble(double(c.surfaceDensity), "surfaceDensity");
	ret.setInteger(int64_t(c.frameInterval), "frameInterval");
	return ret;
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

#if MODULE_XENOLITH_FONT
// The controller is an extension of the APPLICATION, not of the window, so every window's inspector
// reports the same one - a font set is shared by every scene the app thread drives.
static font::FontController *getFontController(const Node *owner) {
	auto director = owner ? owner->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	return app ? app->getExtension<font::FontController>() : nullptr;
}
#endif

Value SceneInspector::getFontInfo() const {
#if MODULE_XENOLITH_FONT
	auto controller = getFontController(_owner);
	if (!controller) {
		return Value();
	}

	auto info = controller->getControllerInfo();

	Value totals;
	totals.setString(info.name, "name");
	totals.setBool(info.loaded, "loaded");
	totals.setBool(info.dirty, "dirty");
	totals.setInteger(int64_t(info.layouts), "layouts");
	totals.setInteger(int64_t(info.faces), "faces");
	totals.setInteger(int64_t(info.families), "families");
	totals.setInteger(int64_t(info.aliases), "aliases");
	totals.setInteger(int64_t(info.chars), "chars");
	totals.setInteger(int64_t(info.charsMemory), "charsMemory");
	totals.setInteger(int64_t(info.kerningPairs), "kerningPairs");
	totals.setInteger(int64_t(info.requiredChars), "requiredChars");
	totals.setInteger(int64_t(info.glyphGeneration), "glyphGeneration");
	totals.setInteger(int64_t(info.submittedGeneration), "submittedGeneration");
	totals.setInteger(int64_t(info.uploadedGeneration), "uploadedGeneration");
	totals.setInteger(int64_t(info.uploadsInFlight), "uploadsInFlight");
	totals.setInteger(int64_t(info.atlasWidth), "atlasWidth");
	totals.setInteger(int64_t(info.atlasHeight), "atlasHeight");
	totals.setInteger(int64_t(info.atlasBytes), "atlasBytes");
	totals.setInteger(int64_t(info.atlasBudget), "atlasBudget");
	totals.setDouble(double(info.cachePressure), "cachePressure");
	totals.setDouble(double(info.atlasOccupancy), "atlasOccupancy");
	totals.setDouble(double(info.evictionThreshold), "evictionThreshold");
	totals.setBool(info.evictAlways, "evictAlways");

	Value layouts(Value::Type::ARRAY);
	controller->enumerateLayouts([&](const font::FontController::LayoutInfo &layout) {
		Value entry;
		entry.setString(layout.name, "name");
		entry.setString(layout.family, "family");
		// The size is already multiplied by the density - that is what the layout key holds and
		// what the faces were opened at.
		entry.setDouble(double(layout.spec.fontSize.val()), "size");
		entry.setDouble(double(layout.spec.density), "density");
		entry.setInteger(int64_t(layout.spec.fontStyle.get()), "style");
		entry.setInteger(int64_t(layout.spec.fontWeight.get()), "weight");
		entry.setInteger(int64_t(layout.spec.fontStretch.get()), "stretch");
		entry.setInteger(int64_t(layout.spec.fontGrade.get()), "grade");
		entry.setInteger(int64_t(layout.metrics.height), "height");
		entry.setInteger(int64_t(layout.metrics.ascender), "ascender");
		entry.setInteger(int64_t(layout.metrics.descender), "descender");
		entry.setInteger(int64_t(layout.users), "users");
		entry.setBool(layout.persistent, "persistent");
		entry.setInteger(int64_t(layout.idleTime), "idleTime");

		size_t chars = 0, charsMemory = 0, kerningPairs = 0, requiredChars = 0;
		Value faces(Value::Type::ARRAY);
		for (auto &face : layout.faces) {
			chars += face.usage.chars;
			charsMemory += face.usage.charsMemory;
			kerningPairs += face.usage.kerningPairs;
			requiredChars += face.usage.requiredChars;

			Value f;
			f.setString(face.name, "name");
			f.setString(face.source, "source");
			f.setInteger(int64_t(face.id), "id");
			f.setInteger(int64_t(face.plane), "plane");
			f.setInteger(int64_t(face.usage.chars), "chars");
			f.setInteger(int64_t(face.usage.charsMemory), "charsMemory");
			f.setInteger(int64_t(face.usage.kerningPairs), "kerningPairs");
			f.setInteger(int64_t(face.usage.requiredChars), "requiredChars");
			f.setInteger(int64_t(face.usage.submittedChars), "submittedChars");
			f.setBool(face.usage.pendingChars, "pending");
			faces.addValue(sp::move(f));
		}

		entry.setInteger(int64_t(chars), "chars");
		entry.setInteger(int64_t(charsMemory), "charsMemory");
		entry.setInteger(int64_t(kerningPairs), "kerningPairs");
		entry.setInteger(int64_t(requiredChars), "requiredChars");
		entry.setValue(sp::move(faces), "faces");
		layouts.addValue(sp::move(entry));
	});

	Value ret;
	ret.setValue(sp::move(totals), "controller");
	ret.setValue(sp::move(layouts), "layouts");
	return ret;
#else
	return Value();
#endif
}

void SceneInspector::writeFontDump(const Callback<void(StringView)> &out) const {
#if MODULE_XENOLITH_FONT
	auto controller = getFontController(_owner);
	if (!controller) {
		out << "# no font controller\n";
		return;
	}

	auto info = controller->getControllerInfo();
	out << "controller '" << info.name << "' " << (info.loaded ? "loaded" : "NOT LOADED")
		<< (info.dirty ? " dirty" : "") << "\n";
	out << "  layouts=" << info.layouts << " faces=" << info.faces << " families=" << info.families
		<< " aliases=" << info.aliases << "\n";
	out << "  glyphs=" << info.requiredChars << " shaped=" << info.chars
		<< " kerning=" << info.kerningPairs << " memory=" << info.charsMemory << "b\n";
	out << "  atlas=" << info.atlasWidth << "x" << info.atlasHeight
		<< " generation=" << info.glyphGeneration << " submitted=" << info.submittedGeneration
		<< " uploaded=" << info.uploadedGeneration << " inFlight=" << info.uploadsInFlight << "\n";
	out << "  cache: pressure=" << info.cachePressure << " threshold=" << info.evictionThreshold
		<< (info.evictAlways ? " EVICT-ALWAYS" : "") << " atlas=" << info.atlasBytes << "b/"
		<< info.atlasBudget << "b occupancy=" << info.atlasOccupancy << "\n";

	controller->enumerateLayouts([&](const font::FontController::LayoutInfo &layout) {
		size_t chars = 0, kerningPairs = 0, requiredChars = 0, charsMemory = 0;
		for (auto &face : layout.faces) {
			chars += face.usage.chars;
			kerningPairs += face.usage.kerningPairs;
			requiredChars += face.usage.requiredChars;
			charsMemory += face.usage.charsMemory;
		}

		out << "\n"
			<< layout.name << "  family=" << layout.family << " size=" << layout.spec.fontSize.val()
			<< " density=" << layout.spec.density << " weight=" << layout.spec.fontWeight.get()
			<< " users=" << layout.users << (layout.persistent ? " persistent" : "")
			<< " idle=" << layout.idleTime << "us\n";
		out << "  glyphs=" << requiredChars << " shaped=" << chars << " kerning=" << kerningPairs
			<< " memory=" << charsMemory << "b height=" << layout.metrics.height << "\n";

		for (auto &face : layout.faces) {
			out << "    face " << face.name << " id=" << face.id << " plane=" << face.plane
				<< " source=" << face.source << " glyphs=" << face.usage.requiredChars
				<< " shaped=" << face.usage.chars << " kerning=" << face.usage.kerningPairs
				<< (face.usage.pendingChars ? " PENDING" : "") << "\n";
		}
	});
#else
	out << "# built without the font module\n";
#endif
}

void SceneInspector::addCommand(StringView name, StringView description,
		CommandCallback &&callback) {
	if (name.empty() || !callback) {
		return;
	}
	_commands[name.str<Interface>()] = Command{description.str<Interface>(), sp::move(callback)};
}

bool SceneInspector::removeCommand(StringView name) {
	auto it = _commands.find(name);
	if (it == _commands.end()) {
		return false;
	}
	_commands.erase(it);
	return true;
}

Value SceneInspector::getCommandList() const {
	Value commands(Value::Type::ARRAY);
	for (auto &it : _commands) {
		Value entry;
		entry.setString(it.first, "name");
		entry.setString(it.second.description, "description");
		commands.addValue(sp::move(entry));
	}

	Value ret;
	ret.setValue(sp::move(commands), "commands");
	return ret;
}

core::RenderServerChannel *SceneInspector::getRenderServer() const {
	auto director = _owner ? _owner->getDirector() : nullptr;
	return director ? director->getRenderServer() : nullptr;
}

StringView SceneInspector::getWindowId() const {
	auto server = getRenderServer();
	auto info = server ? server->getInfo() : nullptr;

	// `id` is fixed before the window exists (the runtime re-uniques a collision then) and is one
	// of the few WindowInfo fields the app thread may read - see AppWindow::getInfo.
	return info ? StringView(info->id) : StringView();
}

Value SceneInspector::getWindowList() const {
	Value windows(Value::Type::ARRAY);
	for (auto *it : inspectors()) {
		auto server = it->getRenderServer();
		auto info = server ? server->getInfo() : nullptr;
		if (!info) {
			continue; // the scene is attached but the window is not there yet
		}

		Value entry;
		entry.setString(info->id, "id");
		entry.setString(sprt::window::getWindowTypeName(info->type), "type");
		if (!info->parent.empty()) {
			entry.setString(info->parent, "parent");
		}
		entry.setString(info->title, "title");

		auto &c = server->getConstraints();
		entry.setInteger(int64_t(c.extent.width), "width");
		entry.setInteger(int64_t(c.extent.height), "height");
		entry.setDouble(double(c.density), "density");

		// Where the window is, when the platform knows. Reported alongside the surface extent
		// rather than instead of it: the two are in different units, and a caller that wants to
		// reopen a window where it was needs the logical rect, not the pixel one.
		auto &g = server->getWindowGeometry();
		if (g.hasPosition) {
			entry.setInteger(int64_t(g.rect.x), "x");
			entry.setInteger(int64_t(g.rect.y), "y");
		}
		entry.setBool(g.hasPosition, "hasPosition");
		entry.setBool(it == this, "default");
		windows.addValue(sp::move(entry));
	}

	Value ret;
	ret.setValue(sp::move(windows), "windows");
	return ret;
}

SceneInspector *SceneInspector::resolveTarget(NotNull<Session> session, int64_t serial,
		const Value &req) {
	// Absent `window` means "the scene this session lives on", which is what every client that
	// predates auxiliary windows expects.
	if (!req.isString("window")) {
		return this;
	}

	auto id = req.getString("window");
	for (auto *it : inspectors()) {
		if (it->getWindowId() == id) {
			return it;
		}
	}

	sendError(session, serial, toString("unknown window: ", id, "; see the `windows` command"));
	return nullptr;
}

void SceneInspector::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	startLogCapture();

	// Before any of the early returns below: a window whose inspector never takes the listener is
	// exactly the one that has to stay reachable through the `window` argument.
	inspectors().emplace_back(this);

	if (s_listenerOwner) {
		return; // another inspector already holds the process-wide listener
	}

	auto director = _owner ? _owner->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}

	if (!isInspectorEnabled(app->getContextInfo())) {
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
	auto &list = inspectors();
	for (auto it = list.begin(); it != list.end(); ++it) {
		if (*it == this) {
			list.erase(it);
			break;
		}
	}

	if (_listener) {
		_listener->cancel();
		_listener = nullptr;

		if (s_listenerOwner == this) {
			s_listenerOwner = nullptr;
		}
	}

	// Drop every open connection too. The scene is going away, so there is nothing left to serve,
	// and any handle still armed on the app looper would keep it running - Context::handleDidStop
	// blocks in AppThread::waitStopped() until that looper returns. Copied out first: cancelling
	// runs the close callback, which erases from _sessions.
	auto sessions = sp::move(_sessions);
	_sessions.clear();
	for (auto &it : sessions) { it->handle->cancel(Status::Done); }

	System::handleExit();
}

// One accepted connection: read the first line, then either serve a single legacy text command or
// upgrade to the framed protocol and keep serving until the peer disconnects.
void SceneInspector::serveConnection(Rc<sprt::dispatch::StreamHandle> &&stream) {
	auto session = Rc<Session>::alloc();
	session->handle = sp::move(stream);
	// the reader closure holds the session, which holds the handle and this system alive; the
	// dispatch layer breaks the handle <-> closure cycle when the connection finalizes
	session->inspector = this;

	// Tracked so scene teardown can drop it: an armed handle keeps the app looper spinning, and
	// Context::handleDidStop blocks in AppThread::waitStopped() until that looper returns.
	_sessions.emplace(session);
	session->handle->setCloseCallback(
			[this, session = Rc<Session>(session)](Status) mutable { _sessions.erase(session); });

	session->handle->read([session](BytesView data) {
		if (session->framed) {
			return session->inspector->readSession(session, data);
		}
		return session->inspector->readHandshake(session, data);
	});
}

// The peer will send nothing more. Stop reading AND release the handle: a reader that keeps
// returning Ok on an EOF'd socket leaves it armed forever, and the app looper can then never
// drain - which wedges shutdown in waitStopped().
Status SceneInspector::finishSession(NotNull<Session> session) {
	session->handle->cancel(Status::Done);
	return Status::Done;
}

Status SceneInspector::readHandshake(NotNull<Session> session, BytesView data) {
	if (data.empty()) {
		return finishSession(session); // EOF before a full command: nothing to serve
	}

	for (size_t i = 0; i < data.size(); ++i) {
		auto c = char(data[i]);
		if (c == '\n') {
			auto line = StringView(session->line);
			line.trimChars<StringView::Chars<'\r'>>();

			if (line.starts_with(PROTOCOL_HANDSHAKE)) {
				// "xenolith/1" or "xenolith/1 json" - JSON keeps the protocol reachable from
				// clients that have no CBOR decoder (binary payloads become base64url strings).
				auto rest = line.sub(PROTOCOL_HANDSHAKE.size());
				rest.trimChars<StringView::WhiteSpace>();
				session->json = (rest == "json");
				session->framed = true;

				auto greeting = toString("# ", PROTOCOL_HANDSHAKE, " ok ",
						session->json ? "json" : "cbor", "\n");
				session->handle->write(BytesView(reinterpret_cast<const uint8_t *>(greeting.data()),
						greeting.size()));

				// whatever follows the handshake line in this same read is already frame data
				auto tail = data.sub(i + 1);
				session->line.clear();
				if (!tail.empty()) {
					return readSession(session, tail);
				}
				return Status::Ok;
			}

			// legacy one-shot text protocol
			StringStream reply;
			if (line == "scene") {
				reply << "# xenolith scene\n";
				writeSceneDump([&](StringView str) { reply << str; });
			} else if (line == "logs") {
				writeLogDump([&](StringView str) { reply << str; });
			} else if (line == "fonts") {
				reply << "# xenolith fonts\n";
				writeFontDump([&](StringView str) { reply << str; });
			} else {
				reply << "# unknown command; expected 'scene', 'logs', 'fonts' or '"
					  << PROTOCOL_HANDSHAKE << "'\n";
			}

			auto text = reply.weak();
			session->handle->write(
					BytesView(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
			session->handle->shutdownWrite();
			return Status::Done; // stop reading; wait for the peer to close
		}

		session->line.push_back(c);
		if (session->line.size() > MAX_COMMAND_LEN) {
			session->handle->cancel();
			return Status::Done;
		}
	}
	return Status::Ok; // command incomplete: keep reading
}

Status SceneInspector::readSession(NotNull<Session> session, BytesView data) {
	if (data.empty()) {
		return finishSession(session); // peer is done sending
	}

	session->buffer.insert(session->buffer.end(), data.data(), data.data() + data.size());

	// a read can deliver any number of whole or partial frames
	size_t offset = 0;
	while (session->buffer.size() - offset >= sizeof(uint32_t)) {
		auto header = BytesView(session->buffer.data() + offset, sizeof(uint32_t));
		auto size = header.readUnsigned32();

		if (size > MAX_FRAME_SIZE) {
			slog().warn("SceneInspector", "frame size ", size, " exceeds the limit, dropping peer");
			session->handle->cancel();
			return Status::Done;
		}

		if (session->buffer.size() - offset - sizeof(uint32_t) < size) {
			break; // frame incomplete: keep reading
		}

		auto payload = BytesView(session->buffer.data() + offset + sizeof(uint32_t), size);
		// data::read sniffs the format, so a JSON session and a CBOR one both parse here
		handleRequest(session, data::read<Interface>(payload));

		offset += sizeof(uint32_t) + size;
	}

	if (offset > 0) {
		session->buffer.erase(session->buffer.begin(), session->buffer.begin() + offset);
	}
	return Status::Ok;
}

void SceneInspector::sendFrame(NotNull<Session> session, const Value &value) {
	auto payload = data::write<Interface>(value,
			session->json ? EncodeFormat(EncodeFormat::Json, EncodeFormat::NoCompression)
						  : EncodeFormat(EncodeFormat::Cbor, EncodeFormat::NoCompression));

	uint8_t header[sizeof(uint32_t)];
	auto size = uint32_t(payload.size());
	header[0] = uint8_t(size & 0xFF);
	header[1] = uint8_t((size >> 8) & 0xFF);
	header[2] = uint8_t((size >> 16) & 0xFF);
	header[3] = uint8_t((size >> 24) & 0xFF);

	session->handle->write(BytesView(header, sizeof(header)));
	session->handle->write(BytesView(payload.data(), payload.size()));
}

void SceneInspector::sendResponse(NotNull<Session> session, int64_t serial, Value &&result) {
	Value response;
	response.setInteger(serial, "serial");
	response.setString("ok", "status");
	response.setValue(sp::move(result), "result");
	sendFrame(session, response);
}

void SceneInspector::sendError(NotNull<Session> session, int64_t serial, StringView error) {
	Value response;
	response.setInteger(serial, "serial");
	response.setString("error", "status");
	response.setString(error, "error");
	sendFrame(session, response);
}

void SceneInspector::handleRequest(NotNull<Session> session, Value &&request) {
	// Every read of `request` below goes through this const reference. The request is untrusted
	// socket input, so a key may simply be absent; a NON-const get*() on a missing key hands out
	// the shared null container and aborts the debug build (assertMutableNullAccess). The const
	// accessors are the sanctioned defensive read and return the sentinel harmlessly — without
	// them a single malformed frame (`{"command":…}` instead of `{"cmd":…}`) kills the app.
	const Value &req = request;

	auto serial = req.getInteger("serial", 0);
	auto cmd = req.getString("cmd");

	// Everything below "logs" (which is a process-wide ring buffer, not a window's) acts on ONE
	// window's scene. `window: "<id>"` picks which; without it, the one this inspector lives on.
	// That is what makes an auxiliary window - a menu, a dialog - reachable at all: it has an
	// inspector of its own, but only the first one to attach owns the socket.
	auto target = resolveTarget(session, serial, req);
	if (!target) {
		return; // resolveTarget answered the request
	}

	if (cmd == "scene") {
		StringStream dump;
		target->writeSceneDump([&](StringView str) { dump << str; });

		Value result;
		result.setString(dump.str(), "text");
		sendResponse(session, serial, sp::move(result));
	} else if (cmd == "logs") {
		sendResponse(session, serial, getLogDump(uint64_t(req.getInteger("since", 0))));
	} else if (cmd == "fonts") {
		auto fonts = target->getFontInfo();
		if (!fonts) {
			sendError(session, serial, "no font controller");
			return;
		}
		if (req.getBool("text")) {
			StringStream dump;
			target->writeFontDump([&](StringView str) { dump << str; });
			fonts.setString(dump.str(), "text");
		}
		sendResponse(session, serial, sp::move(fonts));
	} else if (cmd == "windows") {
		sendResponse(session, serial, getWindowList());
	} else if (cmd == "commands") {
		sendResponse(session, serial, target->getCommandList());
	} else if (cmd == "invoke") {
		target->handleInvoke(session, serial, sp::move(request));
	} else if (cmd == "screenshot") {
		target->handleScreenshot(session, serial, sp::move(request));
	} else if (cmd == "input") {
		target->handleInput(session, serial, sp::move(request));
	} else if (cmd == "text") {
		target->handleText(session, serial, sp::move(request));
	} else if (cmd == "frame") {
		auto server = target->getRenderServer();
		if (!server) {
			sendError(session, serial, "no render session");
			return;
		}
		// The headless window renders on demand, so this is what actually produces frames. Each
		// window has its own presentation engine, so each one has to be stepped on its own.
		auto count = sprt::max(req.getInteger("count", 1), int64_t(1));
		for (int64_t i = 0; i < count; ++i) { server->setReadyForNextFrame(); }

		Value result;
		result.setInteger(count, "count");
		sendResponse(session, serial, sp::move(result));
	} else if (cmd == "render") {
		/* Hold the render loop OPEN, for everything that changes without anyone touching it.

		A window is drawn when something dirties it, and plenty of what a check wants to look at
		dirties nothing an outside caller can see: a deferred style pass, an action's step, a probe
		landing, a load finishing. Nobody moves the mouse during an automated run, so those changes
		are computed and never drawn - which reads from the outside as "the fix did nothing", and
		reads in a screenshot as the frame before it. This makes the scene redraw regardless.

		It does not produce frames by itself: in headless mode `frame` is still what advances the
		presentation engine, and this is what makes each of those frames redraw the scene rather
		than re-present the last one. The pair is the idiom - `render` once, `frame` as needed.

		{seconds: N} bounds it; without one it runs until the window closes. {stop: true} takes it
		off. Re-invoking replaces the previous one: the action is tagged, and per window, so an
		auxiliary window is held open through its own `window` id. */
		if (!target->_owner) {
			sendError(session, serial, "no scene");
			return;
		}

		static constexpr uint32_t RenderTag = "XLInspectorRender"_tag;

		const auto seconds = req.getDouble("seconds");
		const auto stop = req.getBool("stop");

		target->_owner->stopAllActionsByTag(RenderTag);
		if (!stop) {
			if (seconds > 0.0) {
				target->_owner->runAction(Rc<RenderContinuously>::create(float(seconds)),
						RenderTag);
			} else {
				target->_owner->runAction(Rc<RenderContinuously>::create(), RenderTag);
			}
		}

		Value result;
		result.setBool(!stop, "running");
		result.setDouble(seconds, "seconds");
		sendResponse(session, serial, sp::move(result));
	} else if (cmd == "window") {
		target->handleWindow(session, serial, sp::move(request));
	} else if (cmd == "quit") {
		// Not routed: this shuts the process down, so it always means the root window - closing a
		// popup here would just dismiss the menu.
		auto server = getRenderServer();
		if (!server) {
			sendError(session, serial, "no render session");
			return;
		}

		// Answer before closing: once the window is gone the context tears down (headless sets
		// ContextFlags::DestroyWhenAllWindowsClosed) and this socket dies with the app thread.
		Value result;
		result.setBool(true, "closing");
		sendResponse(session, serial, sp::move(result));
		session->handle->shutdownWrite();

		server->close(!req.hasValue("graceful") || req.getBool("graceful"));
	} else {
		sendError(session, serial, toString("unknown command: ", cmd));
	}
}

void SceneInspector::handleInvoke(NotNull<Session> session, int64_t serial, Value &&request) {
	const Value &req = request;

	auto name = req.getString("name");
	auto it = _commands.find(name);
	if (it == _commands.end()) {
		sendError(session, serial, toString("unknown scene command: ", name));
		return;
	}

	// The callback may complete later; `done` carries the session (and through it this system)
	// alive until it does.
	// `args` is optional: take it only when it is really there, or the non-const getValue would
	// hand back (and move from) the shared null container.
	it->second.callback(req.hasValue("args") ? sp::move(request.getValue("args")) : Value(),
			[this, session = Rc<Session>(session), serial](
					Value &&result) { sendResponse(session, serial, sp::move(result)); });
}

void SceneInspector::handleScreenshot(NotNull<Session> session, int64_t serial, Value &&request) {
	auto server = getRenderServer();
	if (!server) {
		sendError(session, serial, "no render session");
		return;
	}

	auto raw = static_cast<const Value &>(request).getString("format") == "raw";
	auto director = _owner ? _owner->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	if (!app) {
		sendError(session, serial, "no application thread");
		return;
	}

	server->captureScreenshot(
			[this, session = Rc<Session>(session), serial, raw, app = Rc<AppThread>(app)](
					const core::ImageInfoData &info, BytesView view) mutable {
		// This runs on the device-task thread and `view` points into a mapped staging buffer that
		// dies with the task, so the pixels must be consumed right here.
		Value result;
		if (view.empty()) {
			result.setString("capture failed", "error");
		} else {
			result.setInteger(int64_t(info.extent.width), "width");
			result.setInteger(int64_t(info.extent.height), "height");

			if (raw) {
				result.setString("raw", "format");
				result.setInteger(int64_t(toInt(info.format)), "pixelFormat");
				result.setBytes(view.bytes<Interface>(), "data");
			} else {
				// getBitmap already un-swizzles the BGRA family the swapchain formats use
				auto bmp = core::getBitmap(info, view);
				if (bmp.empty()) {
					result.setString(toString("unsupported pixel format: ", toInt(info.format)),
							"error");
				} else {
					result.setString("png", "format");
					result.setBytes(bmp.write<Bytes>(bitmap::FileFormat::Png), "data");
				}
			}
		}

		// the socket lives on the app looper, so the reply must be posted back to it
		app->performOnAppThread(
				[this, session = sp::move(session), serial, result = sp::move(result)]() mutable {
			if (result.isString("error")) {
				sendError(session, serial, result.getString("error"));
			} else {
				sendResponse(session, serial, sp::move(result));
			}
		}, this);
	});
}

void SceneInspector::handleInput(NotNull<Session> session, int64_t serial, Value &&request) {
	auto server = getRenderServer();
	if (!server) {
		sendError(session, serial, "no render session");
		return;
	}

	const Value &events = static_cast<const Value &>(request).getValue("events");
	if (!events.isArray()) {
		sendError(session, serial, "'events' must be an array");
		return;
	}

	Vector<core::InputEventData> data;
	data.reserve(events.size());
	for (const auto &it : events.asArray()) {
		core::InputEventData event;
		if (!readInputEvent(it, event)) {
			sendError(session, serial, toString("unknown input event: ", it.getString("event")));
			return;
		}
		data.emplace_back(event);
	}

	// "native" routes through the OS window instead of straight into the client, which is what puts
	// the events in front of the text-input processor. Without it a typed character never becomes
	// text, it stays a key event nobody consumes.
	auto native = static_cast<const Value &>(request).getBool("native");

	auto count = int64_t(data.size());
	if (native) {
		server->handleNativeInputEvents(sp::move(data));
	} else {
		server->handleInputEvents(sp::move(data));
	}

	Value result;
	result.setInteger(count, "accepted");
	result.setBool(native, "native");
	sendResponse(session, serial, sp::move(result));
}

void SceneInspector::handleText(NotNull<Session> session, int64_t serial, Value &&request) {
	const Value &req = request;
	auto op = req.getString("op");

	if (op == "state") {
		// The application-side mirror of what the platform last reported. It lives on the app
		// thread, which is this thread, so it can be read directly.
		auto director = _owner ? _owner->getDirector() : nullptr;
		auto manager = director ? director->getTextInputManager() : nullptr;
		if (!manager) {
			sendError(session, serial, "no text input manager");
			return;
		}

		const auto &state = manager->getState();

		Value result;
		result.setBool(state.enabled, "enabled");
		result.setString(string::toUtf8<Interface>(state.getStringView()), "text");
		result.setInteger(int64_t(state.cursor.start), "cursorStart");
		result.setInteger(int64_t(state.cursor.length), "cursorLength");
		result.setInteger(int64_t(state.marked.start), "markedStart");
		result.setInteger(int64_t(state.marked.length), "markedLength");
		result.setInteger(int64_t(toInt(state.type)), "type");
		result.setInteger(int64_t(toInt(state.compose)), "compose");
		result.setBool(manager->getHandler() != nullptr, "hasHandler");
		sendResponse(session, serial, sp::move(result));
		return;
	}

	auto server = getRenderServer();
	if (!server) {
		sendError(session, serial, "no render session");
		return;
	}

	core::TextInputCommand cmd;
	if (op == "insert") {
		cmd.op = core::TextInputCommandOp::Insert;
	} else if (op == "marked") {
		cmd.op = core::TextInputCommandOp::SetMarked;
	} else if (op == "unmark") {
		cmd.op = core::TextInputCommandOp::Unmark;
	} else if (op == "delete-backward") {
		cmd.op = core::TextInputCommandOp::DeleteBackward;
	} else if (op == "delete-forward") {
		cmd.op = core::TextInputCommandOp::DeleteForward;
	} else if (op == "cancel") {
		cmd.op = core::TextInputCommandOp::Cancel;
	} else {
		sendError(session, serial,
				toString("unknown text op: ", op,
						"; expected insert, marked, unmark, delete-backward, delete-forward, " "can" "cel" " or" " st" "at" "e"));
		return;
	}

	if (req.hasValue("text")) {
		auto utf8 = req.getString("text");
		size_t size = sprt::unicode::getUtf16Length(utf8);
		cmd.text.resize(size);
		sprt::unicode::toUtf16(cmd.text.data(), cmd.text.size(), utf8, &size);
		cmd.text.resize(size);
	}
	if (req.hasValue("replaceStart")) {
		cmd.replacement = core::TextCursor(uint32_t(req.getInteger("replaceStart")),
				uint32_t(req.getInteger("replaceLength", 0)));
	}
	if (req.hasValue("markedStart")) {
		cmd.marked = core::TextCursor(uint32_t(req.getInteger("markedStart")),
				uint32_t(req.getInteger("markedLength", 0)));
	}
	cmd.compose = core::InputKeyComposeState(req.getInteger("compose", 0));

	server->performTextInput(sp::move(cmd));

	// The edit is applied on the context thread and comes back as a propagate, so a caller that
	// wants to observe it must let a frame pass before reading the state again.
	Value result;
	result.setString(op, "op");
	result.setBool(true, "applied");
	sendResponse(session, serial, sp::move(result));
}

void SceneInspector::handleWindow(NotNull<Session> session, int64_t serial, Value &&request) {
	auto server = getRenderServer();
	if (!server) {
		sendError(session, serial, "no render session");
		return;
	}

	const Value &req = request;

	auto op = req.getString("op");
	if (op == "constraints") {
		sendResponse(session, serial, encodeConstraints(server->getConstraints()));
	} else if (op == "geometry") {
		sendResponse(session, serial, encodeGeometry(server->getWindowGeometry()));
	} else if (op == "resize") {
		auto width = uint32_t(req.getInteger("width", 0));
		auto height = uint32_t(req.getInteger("height", 0));
		if (width == 0 || height == 0) {
			sendError(session, serial, "'width' and 'height' are required and must be non-zero");
			return;
		}

		server->setWindowExtent(Extent2(width, height),
				[this, session = Rc<Session>(session), serial](Status st) {
			if (!sprt::status::isSuccessful(st)) {
				sendError(session, serial, toString("resize failed: ", st));
				return;
			}
			Value result;
			result.setBool(true, "resized");
			sendResponse(session, serial, sp::move(result));
		}, this);
	} else if (op == "close") {
		Value result;
		result.setBool(true, "closing");
		sendResponse(session, serial, sp::move(result));
		server->close(!req.hasValue("graceful") || req.getBool("graceful"));
	} else {
		sendError(session, serial,
				toString("unknown window op: ", op,
						"; expected 'resize', 'constraints', 'geometry' or 'close'"));
	}
}

namespace inspector {

void attach(Node *root) {
	if (!root || root->getSystemByType<SceneInspector>()) {
		return;
	}
	root->addSystem(Rc<SceneInspector>::create());
}

SceneInspector *get(Node *root) { return root ? root->getSystemByType<SceneInspector>() : nullptr; }

bool addCommand(Node *root, StringView name, StringView description,
		SceneInspector::CommandCallback &&cb) {
	if (auto i = get(root)) {
		i->addCommand(name, description, sp::move(cb));
		return true;
	}
	return false;
}

} // namespace inspector

} // namespace stappler::xenolith
