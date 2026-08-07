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

#include "SPRTWinLinuxDBusPortal.h"

#if SPRT_LINUX

#include <sprt/runtime/window/controller.h>

#include <fcntl.h>
#include <unistd.h>

namespace sprt::window::dbus {

static constexpr auto PORTAL_SERVICE_NAME = "org.freedesktop.portal.Desktop";
static constexpr auto PORTAL_SERVICE_PATH = "/org/freedesktop/portal/desktop";
static constexpr auto PORTAL_FILE_CHOOSER_INTERFACE = "org.freedesktop.portal.FileChooser";
static constexpr auto PORTAL_OPEN_URI_INTERFACE = "org.freedesktop.portal.OpenURI";
static constexpr auto PORTAL_TRASH_INTERFACE = "org.freedesktop.portal.Trash";
static constexpr auto PORTAL_REQUEST_INTERFACE = "org.freedesktop.portal.Request";

// Every Request object the portal makes for us lives under this prefix, so one namespace rule
// catches them all and no handle_token has to be minted to predict a path. Correlation is by the
// object path in the signal against the one the method call returned; D-Bus preserves per-sender
// ordering, so that path is always known by the time a signal can arrive.
static constexpr auto PORTAL_REQUEST_FILTER = "type='signal',"
											  "interface='org.freedesktop.portal.Request',"
											  "member='Response',"
											  "path_namespace='/org/freedesktop/portal/desktop/"
											  "request'";

// Response codes of org.freedesktop.portal.Request::Response.
static constexpr uint32_t PORTAL_RESPONSE_SUCCESS = 0;
static constexpr uint32_t PORTAL_RESPONSE_CANCELLED = 1;

namespace {

// "file:///home/user/my%20file.txt" -> "/home/user/my file.txt". Anything that is not a local file
// URI (a trash:// or smb:// entry the picker may return) yields an empty string and is dropped.
String decodeFileUri(StringView uri) {
	if (!uri.starts_with("file://")) {
		return String();
	}
	uri += "file://"_len;

	auto hex = [](char c) -> int {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		return -1;
	};

	String out;
	out.reserve(uri.size());
	for (size_t i = 0; i < uri.size(); ++i) {
		if (uri[i] == '%' && i + 2 < uri.size()) {
			auto hi = hex(uri[i + 1]);
			auto lo = hex(uri[i + 2]);
			if (hi >= 0 && lo >= 0) {
				out.push_back(char(hi * 16 + lo));
				i += 2;
				continue;
			}
		}
		out.push_back(uri[i]);
	}
	return out;
}

// Add one a{sv} entry.
//
// This exists only to disambiguate: with a string literal for the key, addVariant(StringView,
// BasicValue) and addVariant(const char *, const callback &) each need exactly one user conversion,
// so overload resolution cannot choose between them.
void addOption(WriteIterator &options, StringView key, BasicValue value) {
	options.addVariant(key, value);
}

// The portal takes paths as NUL-terminated byte arrays rather than strings, because a filesystem
// path is not required to be UTF-8.
void addPathOption(WriteIterator &options, StringView key, StringView path) {
	options.addVariant(key, "ay", [&](WriteIterator &v) {
		Vector<uint8_t> bytes;
		bytes.reserve(path.size() + 1);
		for (auto c : path) { bytes.emplace_back(uint8_t(c)); }
		bytes.emplace_back(uint8_t(0));
		v.add(SpanView<uint8_t>(bytes));
	});
}

// a(sa(us)): a list of (name, list of (kind, spec)) where kind 0 is a glob and 1 a MIME type.
void addFilterList(WriteIterator &arr, SpanView<FileFilter> filters) {
	for (auto &filter : filters) {
		arr.addStruct([&](WriteIterator &entry) {
			entry.add(BasicValue(StringView(filter.name)));
			entry.addArray("(us)", [&](WriteIterator &specs) {
				for (auto &pattern : filter.patterns) {
					specs.addStruct([&](WriteIterator &spec) {
						spec.add(BasicValue(uint32_t(0)));
						spec.add(BasicValue(StringView(pattern)));
					});
				}
				for (auto &mime : filter.mimeTypes) {
					specs.addStruct([&](WriteIterator &spec) {
						spec.add(BasicValue(uint32_t(1)));
						spec.add(BasicValue(StringView(mime)));
					});
				}
			});
		});
	}
}

// Open a path for the portal to act on. The portal identifies files by descriptor, not by name, so
// that a sandboxed caller cannot name a path it has no access to.
int openForPortal(StringView path) {
	auto str = path.terminated() ? path : StringView(path.str<String>());
	return ::open(str.data(), O_RDONLY | O_CLOEXEC);
}

// Run `cb` over every directory D-Bus looks in for session service files, most specific first, the
// same set and order dbus-daemon itself uses.
void foreachServiceDir(const callback<void(StringView)> &cb) {
	String home;
	if (auto dataHome = ::getenv("XDG_DATA_HOME")) {
		home = StringView(dataHome).str<String>();
	} else if (auto userHome = ::getenv("HOME")) {
		home = toString(userHome, "/.local/share");
	}
	if (!home.empty()) {
		cb(home);
	}

	auto dirs = ::getenv("XDG_DATA_DIRS");
	StringView reader(dirs ? StringView(dirs) : StringView("/usr/local/share:/usr/share"));
	while (!reader.empty()) {
		auto dir = reader.readUntil<StringView::Chars<':'>>();
		reader.skipChars<StringView::Chars<':'>>();
		if (!dir.empty()) {
			cb(dir);
		}
	}
}

} // namespace

bool isPortalDialogType(DialogType type) {
	switch (type) {
	case DialogType::OpenFile:
	case DialogType::OpenDirectory:
	case DialogType::SaveFile:
	case DialogType::RevealInFileManager:
	case DialogType::MoveToTrash: return true;
	case DialogType::Color:
	case DialogType::Font:
		// No portal interface exists for either.
		break;
	}
	return false;
}

bool detectDesktopPortal() {
	bool found = false;
	foreachServiceDir([&](StringView dir) {
		if (found) {
			return;
		}
		auto path = toString(dir, "/dbus-1/services/", PORTAL_SERVICE_NAME, ".service");
		if (::access(path.data(), R_OK) == 0) {
			found = true;
		}
	});
	return found;
}

WindowCapabilities getPortalDialogCapabilities(bool hasPortal) {
	if (hasPortal) {
		return WindowCapabilities::FileDialogs | WindowCapabilities::SystemFileActions;
	}
	return WindowCapabilities::None;
}

bool PortalDialogHandle::init(NotNull<ContextController> controller, NotNull<Controller> dbus,
		NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req, NativeWindow *parent,
		StringView parentHandle, Function<void(Rc<DialogRequest> &&)> &&fallback) {
	if (!DialogHandle::init(controller, target, sprt::move(req), parent)) {
		return false;
	}

	auto connection = dbus->getSessionBus();
	if (!connection || !connection->connected) {
		return false;
	}

	_dbus = dbus;
	_fallback = sprt::move(fallback);

	// Subscribe before calling. dbus_bus_add_match with an error slot is a blocking round trip, so
	// the rule is live on the bus before the portal can possibly emit — no lost-signal race.
	if (_request->type != DialogType::MoveToTrash) {
		_filter = Rc<BusFilter>::alloc(connection, PORTAL_REQUEST_FILTER, PORTAL_REQUEST_INTERFACE,
				"Response", [this](NotNull<const BusFilter>, NotNull<DBusMessage> msg) -> uint32_t {
			// Every live portal dialog sees every Response; only the one it belongs to claims it.
			auto path = StringView(_dbus->getLibrary()->dbus_message_get_path(msg));
			if (_requestPath.empty() || path != _requestPath) {
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			}
			handleResponse(msg);
			return DBUS_HANDLER_RESULT_HANDLED;
		});
		if (!_filter || !_filter->added) {
			return false;
		}
	}

	return sendRequest(parentHandle);
}

bool PortalDialogHandle::sendRequest(StringView parentHandle) {
	auto connection = _dbus->getSessionBus();
	const auto &req = *_request;

	auto onReply = [this](NotNull<Connection>, DBusMessage *reply) { handleRequestReply(reply); };

	switch (req.type) {
	case DialogType::OpenFile:
	case DialogType::OpenDirectory:
	case DialogType::SaveFile: {
		const bool save = req.type == DialogType::SaveFile;
		auto call = connection->callMethod(PORTAL_SERVICE_NAME, PORTAL_SERVICE_PATH,
				PORTAL_FILE_CHOOSER_INTERFACE, save ? "SaveFile" : "OpenFile",
				[&](WriteIterator &iter) {
			iter.add(BasicValue(parentHandle));
			iter.add(BasicValue(req.title.empty() ? StringView("Choose") : StringView(req.title)));
			iter.addMap([&](WriteIterator &options) {
				addOption(options, "modal", BasicValue(hasFlag(req.flags, DialogFlags::Modal)));
				if (!req.acceptLabel.empty()) {
					addOption(options, "accept_label", BasicValue(StringView(req.acceptLabel)));
				}
				if (!save) {
					addOption(options, "directory",
							BasicValue(req.type == DialogType::OpenDirectory));
					addOption(options, "multiple",
							BasicValue(hasFlag(req.flags, DialogFlags::Multiple)));
				} else if (!req.filename.empty()) {
					addOption(options, "current_name", BasicValue(StringView(req.filename)));
				}
				if (!req.path.empty()) {
					addPathOption(options, "current_folder", req.path);
				}
				if (!req.filters.empty()) {
					options.addVariant("filters", "a(sa(us))", [&](WriteIterator &v) {
						v.addArray("(sa(us))",
								[&](WriteIterator &arr) { addFilterList(arr, req.filters); });
					});
					if (req.filter < req.filters.size()) {
						options.addVariant("current_filter", "(sa(us))", [&](WriteIterator &v) {
							addFilterList(v, makeSpanView(&req.filters[req.filter], 1));
						});
					}
				}
			});
		}, onReply, this);
		return call != nullptr;
	}
	case DialogType::RevealInFileManager: {
		if (req.paths.empty()) {
			return false;
		}
		auto fd = openForPortal(req.paths.front());
		if (fd < 0) {
			return false;
		}
		// libdbus dups the descriptor as it is appended, so ours is done once callMethod returns.
		auto call = connection->callMethod(PORTAL_SERVICE_NAME, PORTAL_SERVICE_PATH,
				PORTAL_OPEN_URI_INTERFACE, "OpenDirectory", [&](WriteIterator &iter) {
			iter.add(BasicValue(parentHandle));
			iter.add(BasicValue::makeFd(fd));
			iter.addMap([](WriteIterator &) { });
		}, onReply, this);
		::close(fd);
		return call != nullptr;
	}
	case DialogType::MoveToTrash: {
		if (req.paths.empty()) {
			return false;
		}
		// Open every descriptor before sending anything: failing here has to leave the request
		// completely untouched, so that the caller can still route it to another backend.
		Vector<int> fds;
		for (auto &path : req.paths) {
			auto fd = openForPortal(path);
			if (fd < 0) {
				for (auto it : fds) { ::close(it); }
				return false;
			}
			fds.emplace_back(fd);
		}

		// TrashFile is the one portal call with no Request object: it answers directly, one file
		// per call. Fire them all and let the last reply settle the result — reporting on the first
		// failure instead would leave the remaining files half-processed and unaccounted for. The
		// counter is armed up front because a reply cannot arrive until we are back on the looper.
		_pendingReplies = uint32_t(fds.size());
		bool sent = false;
		for (auto fd : fds) {
			auto call = connection->callMethod(PORTAL_SERVICE_NAME, PORTAL_SERVICE_PATH,
					PORTAL_TRASH_INTERFACE, "TrashFile", [&](WriteIterator &iter) {
				iter.add(BasicValue::makeFd(fd));
			}, [this](NotNull<Connection>, DBusMessage *reply) {
				if (!isActive()) {
					return;
				}
				auto lib = _dbus->getLibrary();
				// The reply is `u`: 1 for success, 0 for failure.
				if (MessageType(lib->dbus_message_get_type(reply)) == MessageType::Error
						|| ReadIterator(lib, reply).getU32() != 1) {
					_actionFailed = true;
				}
				if (--_pendingReplies == 0) {
					finalize(_actionFailed ? Status::ErrorUnknown : Status::Ok);
				}
			}, this);
			::close(fd);
			if (call) {
				sent = true;
			} else {
				// Nothing will ever answer for this one, so it must not be waited on.
				--_pendingReplies;
				_actionFailed = true;
			}
		}
		// With nothing in flight there is no completion to come, and the request is still ours to
		// hand back.
		return sent;
	}
	case DialogType::Color:
	case DialogType::Font:
		// Filtered out by isPortalDialogType before we get here.
		break;
	}
	return false;
}

void PortalDialogHandle::handleRequestReply(DBusMessage *reply) {
	if (!isActive()) {
		return;
	}

	auto lib = _dbus->getLibrary();
	if (MessageType(lib->dbus_message_get_type(reply)) == MessageType::Error) {
		// The portal refused the call itself, so nothing has been shown to the user and handing the
		// request to another backend is invisible to them. This is the common shape of "xdg-desktop-
		// portal is installed but its FileChooser backend is not", and of a portal too old for the
		// method we asked for.
		auto errorName = StringView(lib->dbus_message_get_error_name(reply));
		oslog::vpdebug(__SPRT_LOCATION, "dbus::Portal", "Portal rejected the request: ", errorName);

		releaseFilter();
		if (_fallback) {
			auto fn = sprt::move(_fallback);
			_fallback = nullptr;
			if (auto req = abandon()) {
				fn(sprt::move(req));
				return;
			}
			return;
		}
		finalize(Status::ErrorNotSupported);
		return;
	}

	// The reply is the `o` handle of the Request object the answer will arrive on.
	_requestPath = ReadIterator(lib, reply).getString().str<String>();
	if (_requestPath.empty()) {
		releaseFilter();
		finalize(Status::ErrorUnknown);
		return;
	}

	if (_closePending) {
		// cancel() ran while the call was in flight; the path only just became known.
		closeRequest();
	}
}

void PortalDialogHandle::handleResponse(NotNull<DBusMessage> msg) {
	if (!isActive()) {
		return;
	}

	ReadIterator iter(_dbus->getLibrary(), msg);
	auto response = iter.getU32(PORTAL_RESPONSE_CANCELLED);

	// The dialog is gone either way, so the subscription goes first — and it must go before
	// finalize(), which can drop the last reference to us.
	releaseFilter();

	if (response != PORTAL_RESPONSE_SUCCESS) {
		// 1 is "the user dismissed it", 2 is "it ended some other way"; neither is an error we can
		// act on, and both mean the same thing to the caller.
		finalize(Status::Declined);
		return;
	}

	DialogResult result;
	result.status = Status::Ok;
	readResults(msg, result);

	// Reveal has no payload — the portal doing the deed is the whole answer.
	if (_request->type != DialogType::RevealInFileManager && result.paths.empty()) {
		finalize(Status::Declined);
		return;
	}

	finalize(sprt::move(result));
}

void PortalDialogHandle::readResults(NotNull<DBusMessage> msg, DialogResult &result) {
	auto lib = _dbus->getLibrary();

	ReadIterator iter(lib, msg);
	if (!iter.next()) {
		return;
	}

	iter.foreachDictEntry([&](StringView key, const ReadIterator &value) {
		if (key == "uris") {
			value.foreach ([&](const ReadIterator &uri) {
				auto path = decodeFileUri(uri.getString());
				if (!path.empty()) {
					result.paths.emplace_back(sprt::move(path));
				}
			});
		} else if (key == "current_filter") {
			// (sa(us)) — the portal returns the filter itself, not its index, so map it back by
			// name. Two recursions: out of the variant, then into the struct's first member.
			// Duplicate names are the caller's problem; the first match wins.
			auto name = value.recurse().recurse().getString();
			for (uint32_t i = 0; i < _request->filters.size(); ++i) {
				if (StringView(_request->filters[i].name) == name) {
					result.filter = i;
					break;
				}
			}
		}
	});
}

Status PortalDialogHandle::cancel(Status st) {
	if (!isActive()) {
		return Status::ErrorAlreadyPerformed;
	}

	closeRequest();
	releaseFilter();
	return DialogHandle::cancel(st);
}

void PortalDialogHandle::handleBackendLost() {
	if (!isActive()) {
		return;
	}

	// No Close() attempt: the connection that would carry it is the one that just died. Drop the
	// request path first so closeRequest() cannot be talked into arming _closePending for a reply
	// that will never come.
	_requestPath.clear();
	_closePending = false;

	releaseFilter();
	finalize(Status::ErrorCancelled);
}

void PortalDialogHandle::closeRequest() {
	if (_requestPath.empty()) {
		// The method call has not answered yet, so there is no object to close. Remember it: the
		// reply handler will do it as soon as the path exists, otherwise the portal would leave its
		// dialog on screen after we have stopped listening.
		_closePending = true;
		return;
	}
	if (auto connection = _dbus->getSessionBus()) {
		connection->callMethod(PORTAL_SERVICE_NAME, _requestPath, PORTAL_REQUEST_INTERFACE, "Close",
				nullptr, this);
	}
	_requestPath.clear();
	_closePending = false;
}

void PortalDialogHandle::releaseFilter() {
	if (!_filter) {
		return;
	}

	// ~BusFilter erases itself from the set the D-Bus dispatcher is iterating right now, and calls
	// RemoveMatch, which blocks on a bus round trip. Neither may happen here — nor on the
	// application thread, where finalize()'s completion task may otherwise drop the last reference
	// to this handle. Handing it to the context looper puts the destruction on the right thread, on
	// a later turn.
	_controller->getLooper()->performOnThread([filter = sprt::move(_filter)]() mutable {
		filter = nullptr;
	}, this, false, "PortalDialogHandle::releaseFilter");
	_filter = nullptr;
}

} // namespace sprt::window::dbus

#endif // SPRT_LINUX
