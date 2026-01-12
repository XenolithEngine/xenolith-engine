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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSLIBRARY_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSLIBRARY_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/stream.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/window/types.h>

#include "private/SPRTDso.h"

#include <dbus/dbus.h>

namespace sprt::window::dbus {

class Library;
struct Connection;
class WriteIterator;

struct Event {
	enum Type {
		None,
		AddWatch,
		ToggleWatch,
		RemoveWatch,
		TriggerWatch, // emitted by polling
		AddTimeout,
		ToggleTimeout,
		RemoveTimeout,
		TriggerTimeout, // emitted by polling
		Dispatch,
		Wakeup,

		Connected,
		Message,
		Failed,
	};

	Type type = None;
	union {
		DBusWatch *watch;
		DBusTimeout *timeout;
		DBusMessage *message;
	};
};

enum class Type : int {
	Invalid = int('\0'),
	Byte = int('y'),
	Boolean = int('b'),
	Int16 = int('n'),
	Uint16 = int('q'),
	Int32 = int('i'),
	Uint32 = int('u'),
	Int64 = int('x'),
	Uint64 = int('t'),
	Double = int('d'),
	String = int('s'),
	Path = int('o'),
	Signature = int('g'),
	Fd = int('h'),

	/* Compound types */
	Array = int('a'),
	Variant = int('v'),
	Struct = int('r'),
	DictEntry = int('e'),
};

enum class MessageType {
	Invalid = int(0),
	MethodCall = int(1),
	MethodReturn = int(2),
	Error = int(3),
	Signal = int(4),
};

struct BasicValue {
	static BasicValue makeBool(bool);
	static BasicValue makeByte(uint8_t);
	static BasicValue makeInteger(int16_t);
	static BasicValue makeInteger(uint16_t);
	static BasicValue makeInteger(int32_t);
	static BasicValue makeInteger(uint32_t);
	static BasicValue makeInteger(int64_t);
	static BasicValue makeInteger(uint64_t);
	static BasicValue makeDouble(double);
	static BasicValue makeString(StringView);
	static BasicValue makePath(StringView);
	static BasicValue makeSignature(StringView);
	static BasicValue makeFd(int);

	BasicValue() = default;
	BasicValue(bool);
	BasicValue(uint8_t);
	BasicValue(int16_t);
	BasicValue(uint16_t);
	BasicValue(int32_t);
	BasicValue(uint32_t);
	BasicValue(int64_t);
	BasicValue(uint64_t);
	BasicValue(float);
	BasicValue(double);
	BasicValue(StringView);

	const char *getSig() const;

	Type type = Type::Invalid;
	DBusBasicValue value;
};

struct SPRT_API Error {
	Library *iface = nullptr;

	Error(Library *);
	~Error();

	bool isSet() const;
	void reset();
};

struct SPRT_API BusFilter : public Ref {
	DBusError error;
	Rc<Connection> connection;
	memory::dynstring filter;
	memory::dynstring interface;
	memory::dynstring signal;
	memory::dynfunction<uint32_t(NotNull<const BusFilter>, NotNull<DBusMessage>)> handler;
	bool added = false;

	virtual ~BusFilter();
	BusFilter(NotNull<Connection>, StringView filter);

	BusFilter(NotNull<Connection>, StringView filter, StringView interface, StringView signal,
			memory::dynfunction<uint32_t(NotNull<const BusFilter>, NotNull<DBusMessage>)> &&);
};

struct SPRT_API Connection : public Ref {
	using EventCallback = memory::dynfunction<uint32_t(Connection *, const Event &)>;

	Rc<Library> lib;
	EventCallback callback;
	DBusConnection *connection = nullptr;
	DBusBusType type;
	DBusError error;

	bool connected = false;
	bool failed = false;
	bool busy = false;
	memory::dynstring name;
	memory::dynset<memory::dynstring> services;
	memory::dynset<BusFilter *> matchFilters;

	virtual ~Connection();

	Connection(Library *, EventCallback &&, DBusBusType);

	void setup();

	explicit operator bool() const { return connection != nullptr; }

	DBusPendingCall *callMethod(StringView bus, StringView path, StringView iface,
			StringView method, const sprt::callback<void(WriteIterator &)> &,
			memory::dynfunction<void(NotNull<Connection>, DBusMessage *)> &&, Ref * = nullptr);

	DBusPendingCall *callMethod(StringView bus, StringView path, StringView iface,
			StringView method, memory::dynfunction<void(NotNull<Connection>, DBusMessage *)> &&,
			Ref * = nullptr);

	bool handle(const Event &, filesystem::PollFlags);

	void flush();
	DBusDispatchStatus dispatch();
	void dispatchAll();

	void close();

	void addMatchFilter(BusFilter *);
	void removeMatchFilter(BusFilter *);
};

class SPRT_API Library : public Ref {
public:
	virtual ~Library() = default;

	Library() { }

	bool init();

	bool open(Dso &handle);
	void close();

	template <typename Parser>
	bool parseMessage(NotNull<DBusMessage>, Parser &);

	template <typename Parser>
	bool parseMessage(NotNull<DBusMessageIter>, Parser &);

	decltype(&_sprt_null_fn) _dbus_first_fn = &_sprt_null_fn;
	SPRT_DEFINE_PROTO(dbus_error_init)
	SPRT_DEFINE_PROTO(dbus_error_free)
	SPRT_DEFINE_PROTO(dbus_message_new_method_call)
	SPRT_DEFINE_PROTO(dbus_message_append_args)
	SPRT_DEFINE_PROTO(dbus_message_is_signal)
	SPRT_DEFINE_PROTO(dbus_message_is_error)
	SPRT_DEFINE_PROTO(dbus_message_unref)
	SPRT_DEFINE_PROTO(dbus_message_iter_init)
	SPRT_DEFINE_PROTO(dbus_message_iter_recurse)
	SPRT_DEFINE_PROTO(dbus_message_iter_next)
	SPRT_DEFINE_PROTO(dbus_message_iter_has_next)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_arg_type)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_element_type)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_element_count)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_fixed_array)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_basic)
	SPRT_DEFINE_PROTO(dbus_message_iter_get_signature)
	SPRT_DEFINE_PROTO(dbus_message_iter_init_append)
	SPRT_DEFINE_PROTO(dbus_message_iter_append_basic)
	SPRT_DEFINE_PROTO(dbus_message_iter_append_fixed_array)
	SPRT_DEFINE_PROTO(dbus_message_iter_open_container)
	SPRT_DEFINE_PROTO(dbus_message_iter_close_container)
	SPRT_DEFINE_PROTO(dbus_message_iter_abandon_container)
	SPRT_DEFINE_PROTO(dbus_message_iter_abandon_container_if_open)
	SPRT_DEFINE_PROTO(dbus_message_get_type)
	SPRT_DEFINE_PROTO(dbus_message_get_path)
	SPRT_DEFINE_PROTO(dbus_message_get_interface)
	SPRT_DEFINE_PROTO(dbus_message_get_member)
	SPRT_DEFINE_PROTO(dbus_message_get_error_name)
	SPRT_DEFINE_PROTO(dbus_message_get_destination)
	SPRT_DEFINE_PROTO(dbus_message_get_sender)
	SPRT_DEFINE_PROTO(dbus_message_get_signature)
	SPRT_DEFINE_PROTO(dbus_connection_set_exit_on_disconnect)
	SPRT_DEFINE_PROTO(dbus_connection_send_with_reply_and_block)
	SPRT_DEFINE_PROTO(dbus_connection_send_with_reply)
	SPRT_DEFINE_PROTO(dbus_connection_set_watch_functions)
	SPRT_DEFINE_PROTO(dbus_connection_set_timeout_functions)
	SPRT_DEFINE_PROTO(dbus_connection_set_wakeup_main_function)
	SPRT_DEFINE_PROTO(dbus_connection_set_dispatch_status_function)
	SPRT_DEFINE_PROTO(dbus_connection_add_filter)
	SPRT_DEFINE_PROTO(dbus_connection_close)
	SPRT_DEFINE_PROTO(dbus_connection_unref)
	SPRT_DEFINE_PROTO(dbus_connection_flush)
	SPRT_DEFINE_PROTO(dbus_connection_dispatch)
	SPRT_DEFINE_PROTO(dbus_error_is_set)
	SPRT_DEFINE_PROTO(dbus_bus_get)
	SPRT_DEFINE_PROTO(dbus_bus_get_private)
	SPRT_DEFINE_PROTO(dbus_bus_add_match)
	SPRT_DEFINE_PROTO(dbus_bus_remove_match)
	SPRT_DEFINE_PROTO(dbus_pending_call_ref)
	SPRT_DEFINE_PROTO(dbus_pending_call_unref)
	SPRT_DEFINE_PROTO(dbus_pending_call_set_notify)
	SPRT_DEFINE_PROTO(dbus_pending_call_get_completed)
	SPRT_DEFINE_PROTO(dbus_pending_call_steal_reply)
	SPRT_DEFINE_PROTO(dbus_pending_call_block)
	SPRT_DEFINE_PROTO(dbus_watch_get_unix_fd)
	SPRT_DEFINE_PROTO(dbus_watch_get_flags)
	SPRT_DEFINE_PROTO(dbus_watch_get_data)
	SPRT_DEFINE_PROTO(dbus_watch_set_data)
	SPRT_DEFINE_PROTO(dbus_watch_handle)
	SPRT_DEFINE_PROTO(dbus_watch_get_enabled)
	SPRT_DEFINE_PROTO(dbus_timeout_get_interval)
	SPRT_DEFINE_PROTO(dbus_timeout_get_data)
	SPRT_DEFINE_PROTO(dbus_timeout_set_data)
	SPRT_DEFINE_PROTO(dbus_timeout_handle)
	SPRT_DEFINE_PROTO(dbus_timeout_get_enabled)
	decltype(&_sprt_null_fn) _dbus_last_fn = &_sprt_null_fn;

protected:
	Dso _handle;
};

SPRT_API unsigned int getWatchFlags(filesystem::PollFlags events);
SPRT_API filesystem::PollFlags getPollFlags(unsigned int);
SPRT_API void describe(Library *lib, NotNull<DBusMessage> message,
		const callback<void(StringView)> &);
SPRT_API void describe(Library *lib, NotNull<DBusMessageIter> message,
		const callback<void(StringView)> &);

SPRT_API bool isFixedType(Type);
SPRT_API bool isBasicType(Type);
SPRT_API bool isContainerType(Type);

template <typename Parser>
struct MessageParserData {
	Library *lib = nullptr;
	Parser *parser = nullptr;
	BasicValue value;
};

struct ReadIterator {
	ReadIterator() = default;
	ReadIterator(NotNull<Library>, NotNull<DBusMessage>);

	bool is(Type t) const { return type == t; }
	Type getType() const { return type; }

	uint32_t getIndex() const { return index; }

	Type getElementType() const;

	BasicValue getValue() const;

	bool getBool() const;
	uint32_t getU32(uint32_t def = 0) const;
	uint64_t getU64(uint64_t def = 0) const;
	int32_t getI32(int32_t def = 0) const;
	int64_t getI64(int64_t def = 0) const;
	float getFloat(float def = 0) const;
	double getDouble(double def = 0) const;
	StringView getString() const;
	BytesView getBytes() const;

	ReadIterator recurse() const;

	bool foreach (const callback<void(const ReadIterator &)> &) const;
	bool foreachDictEntry(const callback<void(StringView, const ReadIterator &)> &) const;

	bool next();

	explicit operator bool() const { return type != Type::Invalid; }

	Library *lib = nullptr;
	DBusMessageIter iter;
	Type type = Type::Invalid;
	uint32_t index = 0;
};

class WriteIterator {
public:
	WriteIterator() = default;
	WriteIterator(NotNull<Library>, NotNull<DBusMessage>);

	bool add(SpanView<bool>);
	bool add(SpanView<uint8_t>);
	bool add(SpanView<int16_t>);
	bool add(SpanView<uint16_t>);
	bool add(SpanView<int32_t>);
	bool add(SpanView<uint32_t>);
	bool add(SpanView<int64_t>);
	bool add(SpanView<uint64_t>);
	bool add(SpanView<double>);
	bool add(SpanView<StringView>);
	bool addPath(SpanView<StringView>);
	bool addSignature(SpanView<StringView>);
	bool addFd(SpanView<int>);
	bool add(BasicValue);

	bool add(StringView, BasicValue);
	bool add(StringView, const callback<void(WriteIterator &)> &);

	bool addVariant(BasicValue);
	bool addVariant(StringView, BasicValue);
	bool addVariant(StringView, const char *sig, const callback<void(WriteIterator &)> &);

	// Adds a{sv} map
	bool addVariantMap(const callback<void(WriteIterator &)> &);
	bool addVariantMap(StringView, const callback<void(WriteIterator &)> &);

	// Adds av array
	bool addVariantArray(const callback<void(WriteIterator &)> &);
	bool addVariantArray(StringView, const callback<void(WriteIterator &)> &);

	bool addMap(const callback<void(WriteIterator &)> &);
	bool addArray(const char *sig, const callback<void(WriteIterator &)> &);
	bool addVariant(const char *sig, const callback<void(WriteIterator &)> &);
	bool addStruct(const callback<void(WriteIterator &)> &);

	Type getType() const { return type; }
	Type getSubType() const { return subtype; }
	explicit operator bool() const { return valid; }

protected:
	bool canAddType(Type) const;

	WriteIterator(NotNull<Library>, Type);

	Library *lib = nullptr;
	Type type = Type::Invalid;
	Type subtype = Type::Invalid;
	uint32_t index = 0;
	bool valid = false;
	DBusMessageIter iter;
};

template <typename T>
struct MessageParserTraits {
	static constexpr auto onBasicValue =
			requires(T *t, const BasicValue &val) { t->onBasicValue(val); };

	static constexpr auto onArrayBegin = requires(T *t, Type type) { t->onArrayBegin(type); };
	static constexpr auto onArrayEnd = requires(T *t) { t->onArrayEnd(); };

	static constexpr auto onArray = requires(T *t, size_t size, Type type,
			NotNull<DBusMessageIter> iter) { t->onArray(size, type, iter); };

	static constexpr auto onStructBegin = requires(T *t, StringView str) { t->onStructBegin(str); };
	static constexpr auto onStructEnd = requires(T *t) { t->onStructEnd(); };

	static constexpr auto onStruct = requires(T *t, StringView str, NotNull<DBusMessageIter> iter) {
		t->onStruct(str, iter);
	};

	static constexpr auto onVariantBegin =
			requires(T *t, StringView str) { t->onVariantBegin(str); };
	static constexpr auto onVariantEnd = requires(T *t) { t->onVariantEnd(); };

	static constexpr auto onVariant = requires(T *t, StringView str,
			NotNull<DBusMessageIter> iter) { t->onVariant(str, iter); };

	static constexpr auto onDictEntryBegin = requires(T *t) { t->onVariantBegin(); };
	static constexpr auto onDictEntryEnd = requires(T *t) { t->onVariantEnd(); };

	static constexpr auto onDictEntry = requires(T *t, const BasicValue &val,
			NotNull<DBusMessageIter> iter) { t->onDictEntry(val, iter); };
};

template <typename Parser>
static inline bool _parseMessage(MessageParserData<Parser> &data, DBusMessageIter *iter,
		Type rootType = Type::Invalid) {
	using Traits = MessageParserTraits<Parser>;

	Type currentType = Type::Invalid;

	while ((currentType = Type(data.lib->dbus_message_iter_get_arg_type(iter))) != Type::Invalid) {
		currentType = Type(data.lib->dbus_message_iter_get_arg_type(iter));
		switch (currentType) {
		case Type::Invalid: break;
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
		case Type::Fd:
			data.value.type = currentType;
			data.lib->dbus_message_iter_get_basic(iter, &data.value.value);
			if constexpr (Traits::onBasicValue) {
				if (!data.parser->onBasicValue(data.value)) {
					return false;
				}
			} else {
				return false;
			}
			break;

		case Type::Array:
			if constexpr (Traits::onArray) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				auto elemType = Type(data.lib->dbus_message_iter_get_element_type(iter));
				size_t size = Max<size_t>;
				if (isFixedType(elemType)) {
					size = data.lib->dbus_message_iter_get_element_count(iter);
				}
				if (!data.parser->onArray(size, elemType, &sub)) {
					return false;
				}
			} else if constexpr (Traits::onArrayBegin && Traits::onArrayEnd) {
				if (!data.parser->onArrayBegin(
							Type(data.lib->dbus_message_iter_get_element_type(iter)))) {
					return false;
				}

				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onArrayEnd() || !ret) {
					return false;
				}
			} else {
				return false;
			}
			break;
		case Type::Struct:
			if constexpr (Traits::onStruct) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onStruct(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)), &sub)) {
					return false;
				}
			} else if constexpr (Traits::onStructBegin && Traits::onStructEnd) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onStructBegin(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)))) {
					return false;
				}

				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onStructEnd() || !ret) {
					return false;
				}
			} else {
				return false;
			}
			break;
		case Type::Variant:
			if constexpr (Traits::onVariant) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onVariant(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)), &sub)) {
					return false;
				}
			} else if constexpr (Traits::onVariantBegin && Traits::onVariantEnd) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onVariantBegin(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)))) {
					return false;
				}

				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onVariantEnd() || !ret) {
					return false;
				}
			} else {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				if (!_parseMessage(data, &sub, currentType)) {
					return false;
				}
			}
			break;
		case Type::DictEntry:
			if (rootType == Type::Array) {
				if constexpr (Traits::onDictEntry) {
					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					auto type = Type(data.lib->dbus_message_iter_get_arg_type(&sub));
					switch (type) {
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
					case Type::Fd:
						data.value.type = type;
						data.lib->dbus_message_iter_get_basic(&sub, &data.value.value);
						data.lib->dbus_message_iter_next(&sub);
						if (!data.parser->onDictEntry(data.value, &sub)) {
							return false;
						}
						break;
					default:
						log::vperror(__SPRT_LOCATION, "DBus", "invalid DictEntry key");
						return false;
						break;
					}

				} else if constexpr (Traits::onDictEntryBegin && Traits::onDictEntryEnd) {
					if (!data.parser->onDictEntryBegin()) {
						return false;
					}

					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					auto ret = _parseMessage(data, &sub, currentType);

					if (!data.parser->onDictEntryEnd() || !ret) {
						return false;
					}
				} else {
					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					if (!_parseMessage(data, &sub, currentType)) {
						return false;
					}
				}
			} else {
				log::vperror(__SPRT_LOCATION, "DBus", "DictEntry should be within Array");
				return false;
			}
			break;
		}
		data.lib->dbus_message_iter_next(iter);
	}
	return true;
}

struct MessagePropertyParser {
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, BasicValue &target);
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry,
			memory::dynvector<uint32_t> &target);
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, bool &val);

	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, int32_t &val);
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, uint32_t &val);
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, float &val);
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, const char *&val);
	bool onArray(size_t size, Type type, NotNull<DBusMessageIter> entry);
	bool onBasicValue(const BasicValue &val);

	Library *lib = nullptr;
	bool found = false;
	BasicValue *target = nullptr;
	memory::dynvector<uint32_t> *u32ArrayTarget = nullptr;
};

template <typename Parser>
inline bool Library::parseMessage(NotNull<DBusMessage> msg, Parser &parser) {
	MessageParserData<Parser> data{this, &parser};

	DBusMessageIter iter;
	dbus_message_iter_init(msg, &iter);

	return _parseMessage(data, &iter);
}

template <typename Parser>
inline bool Library::parseMessage(NotNull<DBusMessageIter> iter, Parser &parser) {
	MessageParserData<Parser> data{this, &parser};
	return _parseMessage(data, iter);
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &stream,
		Type t) {
	switch (t) {
	case Type::Invalid: stream << "Invalid"; break;
	case Type::Byte: stream << "Byte"; break;
	case Type::Boolean: stream << "Boolean"; break;
	case Type::Int16: stream << "Int16"; break;
	case Type::Uint16: stream << "Uint16"; break;
	case Type::Int32: stream << "Int32"; break;
	case Type::Uint32: stream << "Uint32"; break;
	case Type::Int64: stream << "Int64"; break;
	case Type::Uint64: stream << "Uint64"; break;
	case Type::Double: stream << "Double"; break;
	case Type::String: stream << "String"; break;
	case Type::Path: stream << "Path"; break;
	case Type::Signature: stream << "Signature"; break;
	case Type::Fd: stream << "Fd"; break;

	/* Compound types */
	case Type::Array: stream << "Array"; break;
	case Type::Variant: stream << "Variant"; break;
	case Type::Struct: stream << "Struct"; break;
	case Type::DictEntry: stream << "DictEntry"; break;
	}
	return stream;
}

} // namespace sprt::window::dbus

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSLIBRARY_H_ */
