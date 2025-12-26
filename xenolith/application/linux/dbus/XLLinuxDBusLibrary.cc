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

#include "XLLinuxDBusLibrary.h"
#include "SPBytesReader.h"
#include "SPLog.h"
#include "SPMemory.h"
#include "SPString.h"
#include "XLContextInfo.h"
#include "dbus/dbus.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

BasicValue BasicValue::makeBool(bool val) { return BasicValue(val); }
BasicValue BasicValue::makeByte(uint8_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(int16_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(uint16_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(int32_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(uint32_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(int64_t val) { return BasicValue(val); }
BasicValue BasicValue::makeInteger(uint64_t val) { return BasicValue(val); }
BasicValue BasicValue::makeDouble(double val) { return BasicValue(val); }
BasicValue BasicValue::makeString(StringView val) { return BasicValue(val); }
BasicValue BasicValue::makePath(StringView val) {
	auto ret = BasicValue(val);
	ret.type = Type::Path;
	return ret;
}
BasicValue BasicValue::makeSignature(StringView val) {
	auto ret = BasicValue(val);
	ret.type = Type::Signature;
	return ret;
}
BasicValue BasicValue::makeFd(int val) {
	BasicValue ret;
	ret.type = Type::Path;
	ret.value.fd = val;
	return ret;
}

static const char *BasicValue_NullString = "";

BasicValue::BasicValue(bool val) : type(Type::Boolean), value{.bool_val = val} { }
BasicValue::BasicValue(uint8_t val) : type(Type::Byte), value{.byt = val} { }
BasicValue::BasicValue(int16_t val) : type(Type::Int16), value{.i16 = val} { }
BasicValue::BasicValue(uint16_t val) : type(Type::Uint16), value{.u16 = val} { }
BasicValue::BasicValue(int32_t val) : type(Type::Int32), value{.i32 = val} { }
BasicValue::BasicValue(uint32_t val) : type(Type::Uint32), value{.u32 = val} { }
BasicValue::BasicValue(int64_t val) : type(Type::Int64), value{.i64 = val} { }
BasicValue::BasicValue(uint64_t val) : type(Type::Uint64), value{.u64 = val} { }
BasicValue::BasicValue(float val) : type(Type::Double), value{.dbl = val} { }
BasicValue::BasicValue(double val) : type(Type::Double), value{.dbl = val} { }
BasicValue::BasicValue(StringView val)
: type(Type::String)
, value{.str = val.empty() ? (char *)BasicValue_NullString
						   : const_cast<char *>(val.pdup().data())} { }

const char *BasicValue::getSig() const {
	switch (type) {
	case Type::Byte: return "y"; break;
	case Type::Boolean: return "b"; break;
	case Type::Int16: return "n"; break;
	case Type::Uint16: return "q"; break;
	case Type::Int32: return "i"; break;
	case Type::Uint32: return "u"; break;
	case Type::Int64: return "x"; break;
	case Type::Uint64: return "t"; break;
	case Type::Double: return "d"; break;
	case Type::String: return "s"; break;
	case Type::Path: return "o"; break;
	case Type::Signature: return "g"; break;
	case Type::Fd: return "h"; break;
	default: return nullptr; break;
	}
	return nullptr;
}

BusFilter::~BusFilter() {
	if (added) {
		if (connection->lib->dbus_error_is_set(&error)) {
			connection->lib->dbus_error_free(&error);
		}
		connection->lib->dbus_bus_remove_match(connection->connection, filter.data(), &error);
		if (connection->lib->dbus_error_is_set(&error)) {
			log::source().error("DBus", "Fail to remove filter: ", error.name, ": ", error.message);
		}
	}
	if (connection->lib->dbus_error_is_set(&error)) {
		connection->lib->dbus_error_free(&error);
	}
	connection->removeMatchFilter(this);
	connection = nullptr;
}

BusFilter::BusFilter(NotNull<Connection> c, StringView filter)
: connection(c), filter(filter.str<Interface>()) {
	connection->lib->dbus_error_init(&error);
	connection->lib->dbus_bus_add_match(connection->connection, filter.data(), &error);
	if (connection->lib->dbus_error_is_set(&error)) {
		log::source().error("DBus", "Fail to add filter: ", error.name, ": ", error.message);
		connection->lib->dbus_bus_remove_match(connection->connection, filter.data(), nullptr);
	} else {
		added = true;
	}
}

BusFilter::BusFilter(NotNull<Connection> c, StringView filter, StringView interface,
		StringView signal, Function<uint32_t(NotNull<const BusFilter>, NotNull<DBusMessage>)> &&cb)
: BusFilter(c, filter) {
	if (added) {
		this->interface = interface.str<Interface>();
		this->signal = signal.str<Interface>();
		handler = sp::move(cb);
		connection->addMatchFilter(this);
	}
}

Connection::~Connection() {
	close();
	if (lib->dbus_error_is_set(&error)) {
		lib->dbus_error_free(&error);
	}
}

Connection::Connection(Library *lib, EventCallback &&cb, DBusBusType type)
: lib(lib), callback(sp::move(cb)), type(type) {
	lib->dbus_error_init(&error);
	connection = lib->dbus_bus_get_private(type, &error);

	if (lib->dbus_error_is_set(&error)) {
		log::source().error("DBus", "Fail to connect: ", error.name, ": ", error.message);
	}

	if (connection) {
		// DBus is large enough to call _exit for the whole app, damn it...
		lib->dbus_connection_set_exit_on_disconnect(connection, false);

		lib->dbus_connection_set_watch_functions(connection, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			return conn->callback(conn, Event{Event::AddWatch, {watch}});
		}, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::RemoveWatch, {watch}});
		}, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::ToggleWatch, {watch}});
		}, this, nullptr);

		lib->dbus_connection_set_timeout_functions(connection,
				[](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			return conn->callback(conn, Event{Event::AddTimeout, {.timeout = timeout}});
		}, [](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::RemoveTimeout, {.timeout = timeout}});
		}, [](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::ToggleTimeout, {.timeout = timeout}});
		}, this, nullptr);

		lib->dbus_connection_set_wakeup_main_function(connection, [](void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::Wakeup, {nullptr}});
		}, this, nullptr);

		lib->dbus_connection_set_dispatch_status_function(connection,
				[](DBusConnection *connection, DBusDispatchStatus new_status, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
				conn->callback(conn, Event{Event::Dispatch, {nullptr}});
			}
		}, this, nullptr);

		lib->dbus_connection_add_filter(connection,
				[](DBusConnection *, DBusMessage *msg, void *data) -> DBusHandlerResult {
			auto conn = reinterpret_cast<Connection *>(data);
			for (auto &it : conn->matchFilters) {
				if (it->handler) {
					// check if interface message
					if (conn->lib->dbus_message_is_signal(msg, it->interface.data(),
								it->signal.data())) {
						auto res = it->handler(it, msg);
						if (res == DBUS_HANDLER_RESULT_HANDLED) {
							return DBUS_HANDLER_RESULT_HANDLED;
						}

						// or PropertiesChanged - check first arg
					} else if (conn->lib->dbus_message_is_signal(msg,
									   "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
						ReadIterator iter(it->connection->lib, msg);
						auto name = iter.getString();
						if (name == it->interface) {
							auto res = it->handler(it, msg);
							if (res == DBUS_HANDLER_RESULT_HANDLED) {
								return DBUS_HANDLER_RESULT_HANDLED;
							}
						}
					}
				}
			}

			return DBusHandlerResult(conn->callback(conn, Event{Event::Message, {.message = msg}}));
		}, this, nullptr);
	}
}

static void parseServiceList(Library *lib, Set<String> &services, DBusMessage *reply) {
	Type current_type = Type::Invalid;
	DBusMessageIter iter;

	lib->dbus_message_iter_init(reply, &iter);
	while ((current_type = Type(lib->dbus_message_iter_get_arg_type(&iter))) != Type::Invalid) {
		if (current_type == Type::Array) {
			Type sub_type = Type::Invalid;
			DBusMessageIter sub;
			lib->dbus_message_iter_recurse(&iter, &sub);
			while ((sub_type = Type(lib->dbus_message_iter_get_arg_type(&sub))) != Type::Invalid) {
				if (sub_type == Type::String) {
					char *str = nullptr;
					lib->dbus_message_iter_get_basic(&sub, &str);
					if (str && *str != ':') {
						services.emplace(str);
					}
				}
				lib->dbus_message_iter_next(&sub);
			}
		}
		lib->dbus_message_iter_next(&iter);
	}
}

void Connection::setup() {
	if (connection) {
		callMethod("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
				"ListNames", nullptr, [](NotNull<Connection> c, DBusMessage *reply) {
			parseServiceList(c->lib, c->services, reply);
			c->connected = true;
			c->callback(c, Event{Event::Connected});
		}, this);
	} else {
		failed = true;
		callback(this, Event{Event::Failed});
	}
}

DBusPendingCall *Connection::callMethod(StringView bus, StringView path, StringView iface,
		StringView method, const Callback<void(WriteIterator &)> &argsCallback,
		Function<void(NotNull<Connection>, DBusMessage *)> &&resultCallback, Ref *ref) {

	struct MessageData {
		Rc<Library> interface;
		Rc<Connection> connection;
		Function<void(NotNull<Connection>, DBusMessage *)> callback;
		Rc<Ref> ref;

		static void parseReply(DBusPendingCall *pending, void *user_data) {
			auto data = reinterpret_cast<MessageData *>(user_data);

			if (data->interface->dbus_pending_call_get_completed(pending)) {
				auto reply = data->interface->dbus_pending_call_steal_reply(pending);
				if (reply) {
					if (data->callback) {
						data->callback(data->connection, reply);
					}
					data->interface->dbus_message_unref(reply);
				}
			}

			data->interface->dbus_pending_call_unref(pending);
		}

		static void freeMessage(void *user_data) {
			auto data = reinterpret_cast<MessageData *>(user_data);
			delete data;
		}
	};

	DBusPendingCall *ret = nullptr;

	auto message =
			lib->dbus_message_new_method_call(bus.data(), path.data(), iface.data(), method.data());

	if (argsCallback) {
		perform_temporary([&] {
			WriteIterator iter(lib, message);
			argsCallback(iter);

			// describe(lib, message, makeCallback(std::cout));
		});
	}

	auto success = lib->dbus_connection_send_with_reply(connection, message, &ret,
			DBUS_TIMEOUT_USE_DEFAULT);

	lib->dbus_message_unref(message);

	if (success && ret) {
		auto data = new MessageData;
		data->interface = lib;
		data->connection = this;
		data->callback = sp::move(resultCallback);
		data->ref = ref;

		lib->dbus_pending_call_set_notify(ret, MessageData::parseReply, data,
				MessageData::freeMessage);
		flush();
	}
	return ret;
}

DBusPendingCall *Connection::callMethod(StringView bus, StringView path, StringView iface,
		StringView method, Function<void(NotNull<Connection>, DBusMessage *)> &&cb, Ref *ref) {
	return callMethod(bus, path, iface, method, nullptr, sp::move(cb), ref);
}

bool Connection::handle(event::Handle *handle, const Event &ev, event::PollFlags flags) {
	switch (ev.type) {
	case Event::TriggerWatch: return lib->dbus_watch_handle(ev.watch, getWatchFlags(flags)); break;
	case Event::TriggerTimeout: return lib->dbus_timeout_handle(ev.timeout); break;
	default: break;
	}
	return false;
}

void Connection::flush() {
	if (!connection) {
		return;
	}
	lib->dbus_connection_flush(connection);
}

DBusDispatchStatus Connection::dispatch() {
	if (!connection) {
		return DBUS_DISPATCH_COMPLETE;
	}
	return lib->dbus_connection_dispatch(connection);
}

void Connection::dispatchAll() {
	if (!connection || busy) {
		return;
	}

	busy = true;
	while (lib->dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {
		// empty
	}
	busy = false;
}

void Connection::close() {
	if (lib && connection) {
		dispatchAll();

		lib->dbus_connection_close(connection);
		lib->dbus_connection_unref(connection);
		connection = nullptr;
	}
}

void Connection::addMatchFilter(BusFilter *f) { matchFilters.emplace(f); }

void Connection::removeMatchFilter(BusFilter *f) { matchFilters.erase(f); }

bool Library::init() {
	_handle = Dso("libdbus-1.so");
	if (!_handle) {
		log::source().error("DBusLibrary", "Fail to open libdbus-1.so: ", _handle.getError());
		return false;
	}

	if (open(_handle)) {
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

bool Library::open(Dso &handle) {
	XL_LOAD_PROTO(handle, dbus_error_init)
	XL_LOAD_PROTO(handle, dbus_error_free)
	XL_LOAD_PROTO(handle, dbus_message_new_method_call)
	XL_LOAD_PROTO(handle, dbus_message_append_args)
	XL_LOAD_PROTO(handle, dbus_message_is_signal)
	XL_LOAD_PROTO(handle, dbus_message_is_error)
	XL_LOAD_PROTO(handle, dbus_message_unref)
	XL_LOAD_PROTO(handle, dbus_message_iter_init)
	XL_LOAD_PROTO(handle, dbus_message_iter_recurse)
	XL_LOAD_PROTO(handle, dbus_message_iter_next)
	XL_LOAD_PROTO(handle, dbus_message_iter_has_next)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_arg_type)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_element_type)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_element_count)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_fixed_array)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_basic)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_signature)
	XL_LOAD_PROTO(handle, dbus_message_iter_init_append)
	XL_LOAD_PROTO(handle, dbus_message_iter_append_basic)
	XL_LOAD_PROTO(handle, dbus_message_iter_append_fixed_array)
	XL_LOAD_PROTO(handle, dbus_message_iter_open_container)
	XL_LOAD_PROTO(handle, dbus_message_iter_close_container)
	XL_LOAD_PROTO(handle, dbus_message_iter_abandon_container)
	XL_LOAD_PROTO(handle, dbus_message_iter_abandon_container_if_open)
	XL_LOAD_PROTO(handle, dbus_message_get_type)
	XL_LOAD_PROTO(handle, dbus_message_get_path)
	XL_LOAD_PROTO(handle, dbus_message_get_interface)
	XL_LOAD_PROTO(handle, dbus_message_get_member)
	XL_LOAD_PROTO(handle, dbus_message_get_error_name)
	XL_LOAD_PROTO(handle, dbus_message_get_destination)
	XL_LOAD_PROTO(handle, dbus_message_get_sender)
	XL_LOAD_PROTO(handle, dbus_message_get_signature)
	XL_LOAD_PROTO(handle, dbus_connection_set_exit_on_disconnect)
	XL_LOAD_PROTO(handle, dbus_connection_send_with_reply_and_block)
	XL_LOAD_PROTO(handle, dbus_connection_send_with_reply)
	XL_LOAD_PROTO(handle, dbus_connection_set_watch_functions)
	XL_LOAD_PROTO(handle, dbus_connection_set_timeout_functions)
	XL_LOAD_PROTO(handle, dbus_connection_set_wakeup_main_function)
	XL_LOAD_PROTO(handle, dbus_connection_set_dispatch_status_function)
	XL_LOAD_PROTO(handle, dbus_connection_add_filter)
	XL_LOAD_PROTO(handle, dbus_connection_close)
	XL_LOAD_PROTO(handle, dbus_connection_unref)
	XL_LOAD_PROTO(handle, dbus_connection_flush)
	XL_LOAD_PROTO(handle, dbus_connection_dispatch)
	XL_LOAD_PROTO(handle, dbus_error_is_set)
	XL_LOAD_PROTO(handle, dbus_bus_get)
	XL_LOAD_PROTO(handle, dbus_bus_get_private)
	XL_LOAD_PROTO(handle, dbus_bus_add_match)
	XL_LOAD_PROTO(handle, dbus_bus_remove_match)
	XL_LOAD_PROTO(handle, dbus_pending_call_ref)
	XL_LOAD_PROTO(handle, dbus_pending_call_unref)
	XL_LOAD_PROTO(handle, dbus_pending_call_set_notify)
	XL_LOAD_PROTO(handle, dbus_pending_call_get_completed)
	XL_LOAD_PROTO(handle, dbus_pending_call_steal_reply)
	XL_LOAD_PROTO(handle, dbus_pending_call_block)
	XL_LOAD_PROTO(handle, dbus_watch_get_unix_fd)
	XL_LOAD_PROTO(handle, dbus_watch_get_flags)
	XL_LOAD_PROTO(handle, dbus_watch_get_data)
	XL_LOAD_PROTO(handle, dbus_watch_set_data)
	XL_LOAD_PROTO(handle, dbus_watch_handle)
	XL_LOAD_PROTO(handle, dbus_watch_get_enabled)
	XL_LOAD_PROTO(handle, dbus_timeout_get_interval)
	XL_LOAD_PROTO(handle, dbus_timeout_get_data)
	XL_LOAD_PROTO(handle, dbus_timeout_set_data)
	XL_LOAD_PROTO(handle, dbus_timeout_handle)
	XL_LOAD_PROTO(handle, dbus_timeout_get_enabled)

	if (!validateFunctionList(&_dbus_first_fn, &_dbus_last_fn)) {
		log::source().error("XcbLibrary", "Fail to load libxcb");
		return false;
	}

	return true;
}

void Library::close() { _handle.close(); }

unsigned int getWatchFlags(event::PollFlags events) {
	short flags = 0;
	if (hasFlag(events, event::PollFlags::In)) {
		flags |= DBUS_WATCH_READABLE;
	}
	if (hasFlag(events, event::PollFlags::Out)) {
		flags |= DBUS_WATCH_WRITABLE;
	}
	if (hasFlag(events, event::PollFlags::HungUp)) {
		flags |= DBUS_WATCH_HANGUP;
	}
	if (hasFlag(events, event::PollFlags::Err)) {
		flags |= DBUS_WATCH_ERROR;
	}
	return flags;
}

event::PollFlags getPollFlags(unsigned int flags) {
	event::PollFlags ret = event::PollFlags::None;
	if (flags & DBUS_WATCH_READABLE) {
		ret |= event::PollFlags::In;
	}
	if (flags & DBUS_WATCH_WRITABLE) {
		ret |= event::PollFlags::Out;
	}
	if (flags & DBUS_WATCH_HANGUP) {
		ret |= event::PollFlags::HungUp;
	}
	if (flags & DBUS_WATCH_ERROR) {
		ret |= event::PollFlags::Err;
	}
	return ret;
}


struct MessageDescriptionParser {
	bool onBasicValue(const BasicValue &val);
	bool onArrayBegin(Type type);
	bool onArrayEnd();
	bool onStructBegin(StringView sig);
	bool onStructEnd();
	bool onVariantBegin(StringView sig);
	bool onVariantEnd();
	bool onDictEntryBegin();
	bool onDictEntryEnd();

	const CallbackStream *out = nullptr;
	uint32_t indentLevel = 0;
};

void describe(Library *lib, NotNull<DBusMessage> message, const CallbackStream &out) {
	out << "Header:\n";

	auto iface = lib->dbus_message_get_interface(message);
	if (iface) {
		out << "\tInterface: " << iface << "\n";
	}
	auto path = lib->dbus_message_get_path(message);
	if (path) {
		out << "\tPath: " << path << "\n";
	}
	auto member = lib->dbus_message_get_member(message);
	if (member) {
		out << "\tMember: " << member << "\n";
	}
	auto dest = lib->dbus_message_get_destination(message);
	if (dest) {
		out << "\tDestination: " << dest << "\n";
	}
	auto sender = lib->dbus_message_get_sender(message);
	if (sender) {
		out << "\tSender: " << sender << "\n";
	}

	out << "Data:\n";
	MessageDescriptionParser parser;
	parser.indentLevel = 1;
	parser.out = &out;

	lib->parseMessage(message, parser);
}

void describe(Library *lib, NotNull<DBusMessageIter> message, const CallbackStream &out) {
	MessageDescriptionParser parser;
	parser.out = &out;

	lib->parseMessage(message, parser);
}

SP_PUBLIC bool isFixedType(Type t) {
	switch (t) {
	case Type::Byte:
	case Type::Boolean:
	case Type::Int16:
	case Type::Uint16:
	case Type::Int32:
	case Type::Uint32:
	case Type::Int64:
	case Type::Uint64:
	case Type::Double:
	case Type::Fd: return true; break;
	default: break;
	}
	return false;
}

SP_PUBLIC bool isBasicType(Type t) {
	switch (t) {
	case Type::Byte:
	case Type::Boolean:
	case Type::Int16:
	case Type::Uint16:
	case Type::Int32:
	case Type::Uint32:
	case Type::Int64:
	case Type::Uint64:
	case Type::Double:
	case Type::String:
	case Type::Path:
	case Type::Signature:
	case Type::Fd: return true; break;
	default: break;
	}
	return false;
}

SP_PUBLIC bool isContainerType(Type t) {
	switch (t) {
	case Type::Array:
	case Type::Variant:
	case Type::Struct:
	case Type::DictEntry: return true; break;
	default: break;
	}
	return false;
}

ReadIterator::ReadIterator(NotNull<Library> dbus, NotNull<DBusMessage> msg) {
	lib = dbus;
	lib->dbus_message_iter_init(msg, &iter);
	type = Type(lib->dbus_message_iter_get_arg_type(&iter));
}

Type ReadIterator::getElementType() const {
	if (type != Type::Array) {
		return Type::Invalid;
	}

	return Type(lib->dbus_message_iter_get_element_type(const_cast<DBusMessageIter *>(&iter)));
}

BasicValue ReadIterator::getValue() const {
	BasicValue ret;
	ret.type = Type::Invalid;
	if (isBasicType(type)) {
		ret.type = type;
		lib->dbus_message_iter_get_basic(const_cast<DBusMessageIter *>(&iter), &ret.value);
	}
	return ret;
}

bool ReadIterator::getBool() const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return getValue().value.i16; break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return getValue().value.i32; break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return getValue().value.i64; break;
	case Type::Uint64: return getValue().value.u64; break;
	case Type::Double: return getValue().value.dbl; break;
	case Type::Variant: return recurse().getBool(); break;
	default: break;
	}
	return false;
}

uint32_t ReadIterator::getU32(uint32_t def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return static_cast<uint32_t>(getValue().value.i16); break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return static_cast<uint32_t>(getValue().value.i32); break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return static_cast<uint32_t>(getValue().value.i64); break;
	case Type::Uint64: return static_cast<uint32_t>(getValue().value.u64); break;
	case Type::Double: return static_cast<uint32_t>(getValue().value.dbl); break;
	case Type::Variant: return recurse().getU32(def); break;
	default: break;
	}
	return def;
}

uint64_t ReadIterator::getU64(uint64_t def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return static_cast<uint64_t>(getValue().value.i16); break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return static_cast<uint64_t>(getValue().value.i32); break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return static_cast<uint64_t>(getValue().value.i64); break;
	case Type::Uint64: return getValue().value.u64; break;
	case Type::Double: return static_cast<uint32_t>(getValue().value.dbl); break;
	case Type::Variant: return recurse().getU64(def); break;
	default: break;
	}
	return def;
}

int32_t ReadIterator::getI32(int32_t def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return getValue().value.i16; break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return getValue().value.i32; break;
	case Type::Uint32: return static_cast<int32_t>(getValue().value.u32); break;
	case Type::Int64: return static_cast<int32_t>(getValue().value.i64); break;
	case Type::Uint64: return static_cast<int32_t>(getValue().value.u64); break;
	case Type::Double: return static_cast<int32_t>(getValue().value.dbl); break;
	case Type::Variant: return recurse().getI32(def); break;
	default: break;
	}
	return def;
}

int64_t ReadIterator::getI64(int64_t def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return getValue().value.i16; break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return getValue().value.i32; break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return getValue().value.i64; break;
	case Type::Uint64: return static_cast<int64_t>(getValue().value.u64); break;
	case Type::Double: return static_cast<int64_t>(getValue().value.dbl); break;
	case Type::Variant: return recurse().getI64(def); break;
	default: break;
	}
	return def;
}

float ReadIterator::getFloat(float def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return getValue().value.i16; break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return getValue().value.i32; break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return static_cast<float>(getValue().value.i64); break;
	case Type::Uint64: return static_cast<float>(getValue().value.u64); break;
	case Type::Double: return static_cast<float>(getValue().value.dbl); break;
	case Type::Variant: return recurse().getFloat(def); break;
	default: break;
	}
	return def;
}

double ReadIterator::getDouble(double def) const {
	switch (getType()) {
	case Type::Byte: return getValue().value.byt; break;
	case Type::Boolean: return getValue().value.bool_val; break;
	case Type::Int16: return getValue().value.i16; break;
	case Type::Uint16: return getValue().value.u16; break;
	case Type::Int32: return getValue().value.i32; break;
	case Type::Uint32: return getValue().value.u32; break;
	case Type::Int64: return static_cast<double>(getValue().value.i64); break;
	case Type::Uint64: return static_cast<double>(getValue().value.u64); break;
	case Type::Double: return getValue().value.dbl; break;
	case Type::Variant: return recurse().getDouble(def); break;
	default: break;
	}
	return def;
}
StringView ReadIterator::getString() const {
	switch (getType()) {
	case Type::String: return StringView(getValue().value.str); break;
	case Type::Path: return StringView(getValue().value.str); break;
	case Type::Signature: return StringView(getValue().value.str); break;
	case Type::Variant: return recurse().getString(); break;
	default: break;
	}
	return StringView();
}

BytesView ReadIterator::getBytes() const {
	if (getType() != Type::Array) {
		return BytesView();
	}

	auto t = Type(lib->dbus_message_iter_get_element_type(const_cast<DBusMessageIter *>(&iter)));
	if (t != Type::Byte) {
		return BytesView();
	}

	DBusMessageIter sub;
	lib->dbus_message_iter_recurse(const_cast<DBusMessageIter *>(&iter), &sub);

	const uint8_t *bytes = nullptr;
	int size = 0;

	lib->dbus_message_iter_get_fixed_array(&sub, &bytes, &size);

	return BytesView(bytes, size_t(size));
}

ReadIterator ReadIterator::recurse() const {
	ReadIterator val;
	val.lib = lib;
	lib->dbus_message_iter_recurse(const_cast<DBusMessageIter *>(&iter), &val.iter);
	val.type = Type(lib->dbus_message_iter_get_arg_type(&val.iter));
	return val;
}

bool ReadIterator::foreach (const Callback<void(const ReadIterator &)> &cb) const {
	if (isBasicType(type)) {
		return false;
	}

	ReadIterator val = recurse();
	if (type == Type::Variant) {
		if (val.type == Type::Array || val.type == Type::Struct) {
			auto ret = val.foreach (cb);
			val.next();
			return ret;
		}
	}

	while (val) {
		cb(val);
		val.next();
	}
	return true;
}

bool ReadIterator::foreachDictEntry(
		const Callback<void(StringView, const ReadIterator &)> &cb) const {
	if (type == Type::Variant) {
		ReadIterator val = recurse();
		if (val.type == Type::Array) {
			auto ret = val.foreachDictEntry(cb);
			if (ret) {
				val.next();
			}
			return ret;
		}
		return false;
	}

	if (type != Type::Array) {
		return false;
	}

	auto elemType =
			Type(lib->dbus_message_iter_get_element_type(const_cast<DBusMessageIter *>(&iter)));
	if (elemType != Type::DictEntry) {
		return false;
	}

	ReadIterator val = recurse();
	while (val) {
		auto sub = val.recurse();
		auto v = sub.getValue();
		StringView key = StringView(v.value.str);
		sub.next();

		cb(key, sub);
		val.next();
	}

	return true;
}

bool ReadIterator::next() {
	if (type != Type::Invalid) {
		++index;
		lib->dbus_message_iter_next(&iter);
		type = Type(lib->dbus_message_iter_get_arg_type(&iter));
		return true;
	}
	return false;
}


WriteIterator::WriteIterator(NotNull<Library> dbus, NotNull<DBusMessage> msg) {
	lib = dbus;
	lib->dbus_message_iter_init_append(msg, &iter);
	valid = true;
}

bool WriteIterator::add(SpanView<bool> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;

	if (addArray("b", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}
bool WriteIterator::add(SpanView<uint8_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "y", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Byte), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<int16_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "n", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Int16), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<uint16_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "q", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Uint16), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<int32_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "i", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Int32), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<uint32_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "u", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Uint32), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<int64_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "x", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Int64), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<uint64_t> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "t", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Uint64), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<double> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), "d", &sub);
	lib->dbus_message_iter_append_fixed_array(&sub, toInt(Type::Double), val.data(), val.size());
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(SpanView<StringView> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	if (addArray("s", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}

bool WriteIterator::add(SpanView<String> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	if (addArray("s", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}

bool WriteIterator::addPath(SpanView<StringView> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	if (addArray("o", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue::makePath(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}

bool WriteIterator::addSignature(SpanView<StringView> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;
	if (addArray("g", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue::makeSignature(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}

bool WriteIterator::addFd(SpanView<int> val) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;

	if (addArray("h", [&](WriteIterator &arrIt) {
		for (auto &it : val) { arrIt.add(BasicValue::makeFd(it)); }
	})) {
		++index;
		return true;
	}
	return false;
}

bool WriteIterator::add(BasicValue val) {
	if (!canAddType(val.type)) {
		return false;
	}

	subtype = val.type;
	lib->dbus_message_iter_append_basic(&iter, toInt(val.type), &val.value);
	++index;
	return true;
}

bool WriteIterator::add(StringView key, BasicValue val) {
	if (!canAddType(Type::DictEntry)) {
		return false;
	}

	subtype = Type::DictEntry;

	String d;
	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::DictEntry), nullptr, &sub);

	if (key.terminated()) {
		const char *ptr = key.data();
		lib->dbus_message_iter_append_basic(&iter, toInt(Type::String), &ptr);
	} else {
		d = key.str<Interface>();
		const char *ptr = d.data();
		lib->dbus_message_iter_append_basic(&iter, toInt(Type::String), &ptr);
	}

	lib->dbus_message_iter_append_basic(&iter, toInt(val.type), &val.value);
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::add(StringView key, const Callback<void(WriteIterator &)> &cb) {
	if (!canAddType(Type::DictEntry)) {
		return false;
	}

	subtype = Type::DictEntry;

	WriteIterator next(lib, Type::DictEntry);

	String d;
	auto ret = lib->dbus_message_iter_open_container(&iter, toInt(Type::DictEntry), nullptr,
			&next.iter);
	if (!ret) {
		valid = false;
		return false;
	}

	if (key.terminated()) {
		const char *ptr = key.data();
		lib->dbus_message_iter_append_basic(&next.iter, toInt(Type::String), &ptr);
	} else {
		d = key.str<Interface>();
		const char *ptr = d.data();
		lib->dbus_message_iter_append_basic(&next.iter, toInt(Type::String), &ptr);
	}

	cb(next);

	if (next.index == 0 || !next.valid) {
		lib->dbus_message_iter_abandon_container_if_open(&iter, &next.iter);
		valid = false;
		return false;
	}

	lib->dbus_message_iter_close_container(&iter, &next.iter);
	++index;
	return true;
}

bool WriteIterator::addVariant(BasicValue val) {
	if (!canAddType(Type::Variant)) {
		return false;
	}

	subtype = Type::Variant;

	DBusMessageIter sub;
	lib->dbus_message_iter_open_container(&iter, toInt(Type::Variant), val.getSig(), &sub);
	lib->dbus_message_iter_append_basic(&sub, toInt(val.type), &val.value);
	lib->dbus_message_iter_close_container(&iter, &sub);
	++index;
	return true;
}

bool WriteIterator::addVariant(StringView key, BasicValue val) {
	return add(key, [&](WriteIterator &iter) { iter.addVariant(val); });
}

bool WriteIterator::addVariant(StringView key, const char *sig,
		const Callback<void(WriteIterator &)> &cb) {
	return add(key, [&](WriteIterator &iter) { iter.addVariant(sig, cb); });
}

bool WriteIterator::addVariantMap(const Callback<void(WriteIterator &)> &cb) {
	return addVariant("a{sv}", [&](WriteIterator &iter) {
		iter.addArray("{sv}", [&](WriteIterator &iter) {
			iter.subtype = Type::DictEntry;
			cb(iter);
		});
	});
}

bool WriteIterator::addVariantMap(StringView key, const Callback<void(WriteIterator &)> &cb) {
	return addVariant(key, "a{sv}", [&](WriteIterator &iter) {
		iter.addArray("{sv}", [&](WriteIterator &iter) {
			iter.subtype = Type::DictEntry;
			cb(iter);
		});
	});
}

bool WriteIterator::addVariantArray(const Callback<void(WriteIterator &)> &cb) {
	return addVariant("av", [&](WriteIterator &iter) {
		iter.addArray("v", [&](WriteIterator &iter) {
			iter.subtype = Type::Variant;
			cb(iter);
		});
	});
}

bool WriteIterator::addVariantArray(StringView key, const Callback<void(WriteIterator &)> &cb) {
	return add(key, [&](WriteIterator &iter) { iter.addVariantArray(cb); });
}

bool WriteIterator::addMap(const Callback<void(WriteIterator &)> &cb) {
	return addArray("{sv}", cb);
}

bool WriteIterator::addArray(const char *sig, const Callback<void(WriteIterator &)> &cb) {
	if (!canAddType(Type::Array)) {
		return false;
	}

	subtype = Type::Array;

	WriteIterator next(lib, Type::Array);

	lib->dbus_message_iter_open_container(&iter, toInt(Type::Array), sig, &next.iter);

	if (strlen(sig) == 1) {
		next.subtype = Type(sig[0]);
	} else if (memcmp(sig, "{sv}", "{sv}"_len) == 0) {
		next.subtype = Type::DictEntry;
	}

	cb(next);

	if (!next.valid) {
		lib->dbus_message_iter_abandon_container_if_open(&iter, &next.iter);
		valid = false;
		return false;
	}

	lib->dbus_message_iter_close_container(&iter, &next.iter);
	++index;
	return true;
}

bool WriteIterator::addStruct(const Callback<void(WriteIterator &)> &cb) {
	if (!canAddType(Type::Struct)) {
		return false;
	}

	subtype = Type::Struct;

	WriteIterator next(lib, Type::Struct);

	lib->dbus_message_iter_open_container(&iter, toInt(Type::Struct), nullptr, &next.iter);

	cb(next);

	if (!next.valid) {
		lib->dbus_message_iter_abandon_container_if_open(&iter, &next.iter);
		valid = false;
		return false;
	}

	lib->dbus_message_iter_close_container(&iter, &next.iter);
	++index;
	return true;
}

bool WriteIterator::addVariant(const char *sig, const Callback<void(WriteIterator &)> &cb) {
	if (!canAddType(Type::Variant)) {
		return false;
	}

	subtype = Type::Variant;

	WriteIterator next(lib, Type::Variant);

	lib->dbus_message_iter_open_container(&iter, toInt(Type::Variant), sig, &next.iter);

	cb(next);

	if (next.index != 1 && !next.valid) {
		lib->dbus_message_iter_abandon_container_if_open(&iter, &next.iter);
		valid = false;
		return false;
	}

	lib->dbus_message_iter_close_container(&iter, &next.iter);
	++index;
	return true;
}

bool WriteIterator::canAddType(Type newType) const {
	if (!valid) {
		return false;
	}
	if (type == Type::Array) {
		if (subtype == Type::Invalid) {
			return true;
		}
		return subtype == newType;
	} else if (type == Type::Variant || type == Type::DictEntry) {
		return index < 1;
	} else if (newType == Type::DictEntry) {
		return type == Type::Array;
	}
	return true;
}

WriteIterator::WriteIterator(NotNull<Library> lib, Type type)
: lib(lib), type(type), valid(true) { }

bool MessageDescriptionParser::onBasicValue(const BasicValue &val) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << val.type << "(";
	switch (val.type) {
	case Type::Byte: (*out) << int(val.value.byt); break;
	case Type::Boolean: (*out) << (val.value.bool_val ? "true" : "false"); break;
	case Type::Int16: (*out) << val.value.i16; break;
	case Type::Uint16: (*out) << val.value.u16; break;
	case Type::Int32: (*out) << val.value.i32; break;
	case Type::Uint32: (*out) << val.value.u32; break;
	case Type::Int64: (*out) << val.value.i64; break;
	case Type::Uint64: (*out) << val.value.u64; break;
	case Type::Double: (*out) << val.value.dbl; break;
	case Type::String: (*out) << val.value.str; break;
	case Type::Path: (*out) << val.value.str; break;
	case Type::Signature: (*out) << val.value.str; break;
	case Type::Fd: (*out) << int(val.value.fd); break;
	default: break;
	}
	(*out) << ")\n";
	return true;
}
bool MessageDescriptionParser::onArrayBegin(Type type) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Array(" << type << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onArrayEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onStructBegin(StringView sig) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Struct(" << sig << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onStructEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onVariantBegin(StringView sig) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Variant(" << sig << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onVariantEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onDictEntryBegin() {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "DictEntry\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onDictEntryEnd() {
	--indentLevel;
	return true;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry,
		BasicValue &target) {
	MessagePropertyParser parser;
	parser.lib = lib;
	parser.target = &target;
	return lib->parseMessage(entry, parser) && parser.found;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry,
		Vector<uint32_t> &target) {
	MessagePropertyParser parser;
	parser.lib = lib;
	parser.u32ArrayTarget = &target;
	return lib->parseMessage(entry, parser) && parser.found;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry, bool &val) {
	BasicValue ret;
	if (parse(lib, entry, ret)) {
		switch (ret.type) {
		case Type::Boolean: val = ret.value.bool_val; break;
		default:
			log::source().error("DBus", "Fail to read int32_t property: invalid type");
			return false;
			break;
		}
		return true;
	}
	return false;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry, int32_t &val) {
	BasicValue ret;
	if (parse(lib, entry, ret)) {
		switch (ret.type) {
		case Type::Byte: val = ret.value.byt; break;
		case Type::Boolean: val = ret.value.bool_val; break;
		case Type::Int16: val = ret.value.i16; break;
		case Type::Int32: val = ret.value.i32; break;
		default:
			log::source().error("DBus", "Fail to read int32_t property: invalid type");
			return false;
			break;
		}
		return true;
	}
	return false;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry, uint32_t &val) {
	BasicValue ret;
	if (parse(lib, entry, ret)) {
		switch (ret.type) {
		case Type::Byte: val = ret.value.byt; break;
		case Type::Boolean: val = ret.value.bool_val; break;
		case Type::Uint16: val = ret.value.u16; break;
		case Type::Uint32: val = ret.value.u32; break;
		default:
			log::source().error("DBus", "Fail to read uint32_t property: invalid type");
			return false;
			break;
		}
		return true;
	}
	return false;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry, float &val) {
	BasicValue ret;
	if (parse(lib, entry, ret)) {
		switch (ret.type) {
		case Type::Byte: val = ret.value.byt; break;
		case Type::Boolean: val = ret.value.bool_val; break;
		case Type::Uint16: val = ret.value.u16; break;
		case Type::Uint32: val = ret.value.u32; break;
		case Type::Double: val = static_cast<float>(ret.value.dbl); break;
		default:
			log::source().error("DBus", "Fail to read float property: invalid type");
			return false;
			break;
		}
		return true;
	}
	return false;
}

bool MessagePropertyParser::parse(Library *lib, NotNull<DBusMessageIter> entry, const char *&val) {
	BasicValue ret;
	if (parse(lib, entry, ret)) {
		switch (ret.type) {
		case Type::String: val = ret.value.str; break;
		case Type::Path: val = ret.value.str; break;
		case Type::Signature: val = ret.value.str; break;
		default:
			log::source().error("DBus", "Fail to read string property: invalid type");
			return false;
			break;
		}
		return true;
	}
	return false;
}

bool MessagePropertyParser::onArray(size_t size, Type type, NotNull<DBusMessageIter> entry) {
	if (!found && u32ArrayTarget && type == Type::Uint32) {
		if (size > 0 && size != maxOf<size_t>()) {
			uint32_t *ptr = nullptr;
			int size = 0;
			lib->dbus_message_iter_get_fixed_array(entry, &ptr, &size);

			u32ArrayTarget->resize(size);
			memcpy(u32ArrayTarget->data(), ptr, sizeof(uint32_t) * size);
			found = true;
			return true;
		}
	}
	return false;
}

bool MessagePropertyParser::onBasicValue(const BasicValue &val) {
	if (target) {
		if (!found) {
			*target = val;
			found = true;
			return true;
		}
	}
	return false;
}

} // namespace stappler::xenolith::platform::dbus
