/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLStorageServer.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "XLStorageComponent.h"
#include "SPValid.h"
#include "SPThread.h"
#include "SPSqlDriver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

struct ServerComponentData : public db::AllocBase {
	db::pool_t *pool;
	ComponentContainer *container;

	db::Map<StringView, Component *> components;
	db::Map<std::type_index, Component *> typedComponents;
	db::Map<StringView, const db::Scheme *> schemes;
};

class ServerComponentLoader : public ComponentLoader {
public:
	virtual ~ServerComponentLoader();

	ServerComponentLoader(Server::ServerData *, const db::Transaction &);

	virtual db::pool_t *getPool() const override { return _pool; }
	virtual const Server &getServer() const override { return *_server; }
	virtual const db::Transaction &getTransaction() const override { return *_transaction; }

	virtual void exportComponent(Component *) override;

	virtual const db::Scheme *exportScheme(const db::Scheme &) override;

	bool run(ComponentContainer *);

protected:
	Server::ServerData *_data = nullptr;
	db::pool_t *_pool = nullptr;
	const Server *_server = nullptr;
	const db::Transaction *_transaction = nullptr;
	ServerComponentData *_components = nullptr;
};

struct ServerDataTaskCallback {
	Function<bool(const Server &, const db::Transaction &)> callback;
	Rc<Ref> ref;

	ServerDataTaskCallback() = default;
	ServerDataTaskCallback(Function<bool(const Server &, const db::Transaction &)> &&cb)
	: callback(sp::move(cb)) { }
	ServerDataTaskCallback(Function<bool(const Server &, const db::Transaction &)> &&cb, Ref *ref)
	: callback(sp::move(cb)), ref(ref) { }
};

struct ServerDataStorage : memory::AllocPool {
	db::Map<StringView, StringView> params;
	db::Map<StringView, const db::Scheme *> predefinedSchemes;
	db::Map<ComponentContainer *, ServerComponentData *> components;
	db::String documentRoot;
	memory::PriorityQueue<ServerDataTaskCallback> queue;
	StringView serverName;
};

struct Server::ServerData : public thread::Thread, public db::ApplicationInterface {
	memory::allocator_t *_serverAlloc = nullptr;
	memory::pool_t *serverPool = nullptr;
	memory::pool_t *threadPool = nullptr;
	memory::pool_t *asyncPool = nullptr;
	AppThread *application = nullptr;
	ServerDataStorage *storage = nullptr;

	std::condition_variable condition;
	Mutex mutexQueue;
	Mutex mutexFree;
	db::sql::Driver *driver = nullptr;
	db::sql::Driver::Handle handle;
	Server *server = nullptr;
	uint64_t now = 0;

	mutable db::Vector<db::Function<void(const db::Transaction &)>> *asyncTasks = nullptr;

	db::BackendInterface::Config interfaceConfig;

	// accessed from main thread only, std memory model
	mem_std::Map<StringView, Rc<ComponentContainer>> appComponents;

	const db::Transaction *currentTransaction = nullptr;

	ServerData();
	virtual ~ServerData();

	bool execute(const ServerDataTaskCallback &);
	void runAsync();

	virtual void threadInit() override;
	virtual bool worker() override;
	virtual void threadDispose() override;

	void handleHeartbeat();

	void addAsyncTask(
			const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb)
			const;

	bool addComponent(ComponentContainer *, const db::Transaction &);
	void removeComponent(ComponentContainer *, const db::Transaction &t);

	virtual void scheduleAyncDbTask(
			const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb)
			const override;

	virtual db::StringView getDocumentRoot() const override;

	virtual const db::Scheme *getFileScheme() const override;
	virtual const db::Scheme *getUserScheme() const override;

	virtual void pushErrorMessage(db::Value &&) const override;
	virtual void pushDebugMessage(db::Value &&) const override;

	virtual void initTransaction(db::Transaction &) const override;
};


XL_DECLARE_EVENT_CLASS(Server, onBroadcast)

Rc<ApplicationExtension> Server::createServer(AppThread *app, const Value &params) {
	return Rc<storage::Server>::create(app, params);
}

Server::~Server() { }

bool Server::init(AppThread *app, const Value &params) {
	auto alloc = memory::allocator::create();
	auto pool = memory::pool::create(alloc);

	memory::context ctx(pool);

	_data = new (std::nothrow) ServerData;
	_data->_serverAlloc = alloc;
	_data->serverPool = pool;
	_data->application = app;

	StringView driver;

	for (auto &it : params.asDict()) {
		if (it.first == "driver") {
			driver = StringView(it.second.getString());
		} else if (it.first == "serverName") {
			_data->storage->serverName = StringView(it.second.getString()).pdup(pool);
		} else {
			_data->storage->params.emplace(StringView(it.first).pdup(pool),
					StringView(it.second.getString()).pdup(pool));
		}
	}

	if (driver.empty()) {
		driver = StringView("sqlite");
	}

	_data->driver = db::sql::Driver::open(pool, _data, driver);
	if (!_data->driver) {
		log::source().error("storage::Server", "Fail to open DB driver: ", driver);
		return false;
	}

	_data->server = this;
	return _data->run();
}

void Server::initialize(AppThread *) { }

void Server::invalidate(AppThread *) {
	if (_data) {
		for (auto &it : _data->appComponents) { it.second->handleComponentsUnloaded(*this); }

		auto alloc = _data->_serverAlloc;
		auto serverPool = _data->serverPool;
		_data->stop();
		_data->condition.notify_all();
		_data->waitStopped();
		memory::pool::destroy(serverPool);
		memory::allocator::destroy(alloc);
		_data->storage = nullptr;
		_data->release(0);
		_data = nullptr;
	}
}

void Server::update(AppThread *, const UpdateTime &t, bool) { }

Rc<ComponentContainer> Server::getComponentContainer(StringView key) const {
	auto it = _data->appComponents.find(key);
	if (it != _data->appComponents.end()) {
		return it->second;
	}
	return nullptr;
}

bool Server::addComponentContainer(const Rc<ComponentContainer> &comp) {
	if (getComponentContainer(comp->getName()) != nullptr) {
		log::source().error("storage::Server", "Component with name ", comp->getName(),
				" already loaded");
		return false;
	}

	perform([this, comp](const Server &serv, const db::Transaction &t) -> bool {
		if (_data->addComponent(comp, t)) {
			_data->application->performOnAppThread(
					[this, comp] { comp->handleComponentsLoaded(*this); }, this);
		}
		return true;
	});
	_data->appComponents.emplace(comp->getName(), comp);
	return true;
}

bool Server::removeComponentContainer(const Rc<ComponentContainer> &comp) {
	if (!_data) {
		return false;
	}

	auto it = _data->appComponents.find(comp->getName());
	if (it == _data->appComponents.end()) {
		log::source().error("storage::Server", "Component with name ", comp->getName(),
				" is not loaded");
		return false;
	}

	if (it->second != comp) {
		log::source().error("storage::Server",
				"Component you try to remove is not the same that was loaded");
		return false;
	}

	auto refId = _data->application->retain();
	auto selfRefId = retain();
	perform([this, comp, refId, selfRefId](const Server &serv, const db::Transaction &t) -> bool {
		_data->removeComponent(comp, t);
		_data->application->release(refId);
		release(selfRefId);
		return true;
	}, comp);
	_data->appComponents.erase(it);
	comp->handleComponentsUnloaded(*this);
	return true;
}

bool Server::get(CoderSource key, DataCallback &&cb) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform([this, p, key = key.view().bytes<Interface>()](const Server &serv,
						   const db::Transaction &t) {
		auto d = t.getAdapter().get(key);
		_data->application->performOnAppThread([p, ret = xenolith::Value(d)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::set(CoderSource key, Value &&data, DataCallback &&cb) const {
	if (cb) {
		auto p = new DataCallback(sp::move(cb));
		return perform([this, p, key = key.view().bytes<Interface>(), data = sp::move(data)](
							   const Server &serv, const db::Transaction &t) {
			auto d = t.getAdapter().get(key);
			t.getAdapter().set(key, data);
			_data->application->performOnAppThread([p, ret = xenolith::Value(d)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([key = key.view().bytes<Interface>(), data = move(data)](const Server &serv,
							   const db::Transaction &t) {
			t.getAdapter().set(key, data);
			return true;
		});
	}
}

bool Server::clear(CoderSource key, DataCallback &&cb) const {
	if (cb) {
		auto p = new DataCallback(sp::move(cb));
		return perform([this, p, key = key.view().bytes<Interface>()](const Server &serv,
							   const db::Transaction &t) {
			auto d = t.getAdapter().get(key);
			t.getAdapter().clear(key);
			_data->application->performOnAppThread([p, ret = xenolith::Value(d)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([key = key.view().bytes<Interface>()](const Server &serv,
							   const db::Transaction &t) {
			t.getAdapter().clear(key);
			return true;
		});
	}
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform(
			[this, scheme = &scheme, oid, flags, p](const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform([this, scheme = &scheme, alias = alias.str<Interface>(), flags,
						   p](const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id,
		db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, sp::move(cb), oid, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, sp::move(cb), oid, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, sp::move(cb), str, flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, StringView field,
		db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform([this, scheme = &scheme, oid, field = field.str<Interface>(), flags,
						   p](const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, field, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, StringView field,
		db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform(
			[this, scheme = &scheme, alias = alias.str<Interface>(), field = field.str<Interface>(),
					flags, p](const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, field, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, StringView field,
		db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, sp::move(cb), oid, field, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, sp::move(cb), oid, field, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, sp::move(cb), str, field, flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		InitList<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, sp::move(cb), oid, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		InitList<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, sp::move(cb), alias, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id,
		InitList<StringView> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, sp::move(cb), str, sp::move(fields), flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		InitList<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, sp::move(cb), oid, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		InitList<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, sp::move(cb), alias, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id,
		InitList<const char *> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, sp::move(cb), str, sp::move(fields), flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) { mem_std::emplace_ordered(fieldsVec, it); }
	return get(scheme, sp::move(cb), oid, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) { mem_std::emplace_ordered(fieldsVec, it); }
	return get(scheme, sp::move(cb), alias, sp::move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id,
		InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, sp::move(cb), oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, sp::move(cb), str, sp::move(fields), flags);
		}
	}
	return false;
}

// returns Array with zero or more Dictionaries with object data or Null value
bool Server::select(const Scheme &scheme, DataCallback &&cb, QueryCallback &&qcb,
		db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	if (qcb) {
		auto p = new DataCallback(sp::move(cb));
		auto q = new QueryCallback(sp::move(qcb));
		return perform([this, scheme = &scheme, p, q, flags](const Server &serv,
							   const db::Transaction &t) {
			db::Query query;
			(*q)(query);
			delete q;
			auto ret = scheme->select(t, query, flags);
			_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		auto p = new DataCallback(sp::move(cb));
		return perform(
				[this, scheme = &scheme, p, flags](const Server &serv, const db::Transaction &t) {
			auto ret = scheme->select(t, db::Query(), flags);
			_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	}
}

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb,
		db::UpdateFlags flags) const {
	return create(scheme, sp::move(data), sp::move(cb), flags, db::Conflict::None);
}

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb,
		db::Conflict::Flags conflict) const {
	return create(scheme, sp::move(data), sp::move(cb), db::UpdateFlags::None, conflict);
}

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb, db::UpdateFlags flags,
		db::Conflict::Flags conflict) const {
	if (cb) {
		auto p = new DataCallback(sp::move(cb));
		return perform([this, scheme = &scheme, data = move(data), flags, conflict,
							   p](const Server &serv, const db::Transaction &t) {
			auto ret = scheme->create(t, data, flags | db::UpdateFlags::NoReturn, conflict);
			_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([scheme = &scheme, data = sp::move(data), flags,
							   conflict](const Server &serv, const db::Transaction &t) {
			scheme->create(t, data, flags | db::UpdateFlags::NoReturn, conflict);
			return true;
		});
	}
}

bool Server::update(const Scheme &scheme, uint64_t oid, Value &&data, DataCallback &&cb,
		db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(sp::move(cb));
		return perform([this, scheme = &scheme, oid, data = sp::move(data), flags,
							   p](const Server &serv, const db::Transaction &t) {
			db::Value patch(data);
			auto ret = scheme->update(t, oid, patch, flags);
			_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([scheme = &scheme, oid, data = sp::move(data), flags](const Server &serv,
							   const db::Transaction &t) {
			db::Value patch(data);
			scheme->update(t, oid, patch, flags | db::UpdateFlags::NoReturn);
			return true;
		});
	}
}

bool Server::update(const Scheme &scheme, const Value &obj, Value &&data, DataCallback &&cb,
		db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(sp::move(cb));
		return perform([this, scheme = &scheme, obj, data = sp::move(data), flags,
							   p](const Server &serv, const db::Transaction &t) {
			db::Value value(obj);
			db::Value patch(data);
			auto ret = scheme->update(t, value, patch, flags);
			_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([scheme = &scheme, obj, data = sp::move(data), flags](const Server &serv,
							   const db::Transaction &t) {
			db::Value value(obj);
			db::Value patch(data);
			scheme->update(t, value, patch, flags | db::UpdateFlags::NoReturn);
			return true;
		});
	}
}

bool Server::remove(const Scheme &scheme, uint64_t oid, Function<void(bool)> &&cb) const {
	if (cb) {
		auto p = new Function<void(bool)>(sp::move(cb));
		return perform(
				[this, scheme = &scheme, oid, p](const Server &serv, const db::Transaction &t) {
			auto ret = scheme->remove(t, oid);
			_data->application->performOnAppThread([p, ret] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([scheme = &scheme, oid](const Server &serv, const db::Transaction &t) {
			scheme->remove(t, oid);
			return true;
		});
	}
}

bool Server::remove(const Scheme &scheme, const Value &obj, Function<void(bool)> &&cb) const {
	return remove(scheme, obj.getInteger("__oid"), sp::move(cb));
}

bool Server::count(const Scheme &scheme, Function<void(size_t)> &&cb) const {
	if (cb) {
		auto p = new Function<void(size_t)>(sp::move(cb));
		return perform([this, scheme = &scheme, p](const Server &serv, const db::Transaction &t) {
			auto c = scheme->count(t);
			_data->application->performOnAppThread([p, c] {
				(*p)(c);
				delete p;
			});
			return true;
		});
	}
	return false;
}

bool Server::count(const Scheme &scheme, Function<void(size_t)> &&cb, QueryCallback &&qcb) const {
	if (qcb) {
		if (cb) {
			auto p = new Function<void(size_t)>(sp::move(cb));
			auto q = new QueryCallback(sp::move(qcb));
			return perform(
					[this, scheme = &scheme, p, q](const Server &serv, const db::Transaction &t) {
				db::Query query;
				(*q)(query);
				delete q;
				auto c = scheme->count(t, query);
				_data->application->performOnAppThread([p, c] {
					(*p)(c);
					delete p;
				});
				return true;
			});
		}
	} else {
		return count(scheme, sp::move(cb));
	}
	return false;
}

bool Server::touch(const Scheme &scheme, uint64_t id) const {
	return perform([scheme = &scheme, id](const Server &serv, const db::Transaction &t) {
		scheme->touch(t, id);
		return true;
	});
}

bool Server::touch(const Scheme &scheme, const Value &obj) const {
	return perform([scheme = &scheme, obj](const Server &serv, const db::Transaction &t) {
		db::Value value(obj);
		scheme->touch(t, value);
		return true;
	});
}

bool Server::perform(Function<bool(const Server &, const db::Transaction &)> &&cb, Ref *ref) const {
	if (!_data) {
		return false;
	}

	if (thread::Thread::getCurrentThreadId() == _data->getThreadId()) {
		_data->execute(ServerDataTaskCallback(sp::move(cb), ref));
	} else {
		_data->storage->queue.push(0, false, ServerDataTaskCallback(sp::move(cb), ref));
		_data->condition.notify_one();
	}
	return true;
}

AppThread *Server::getApplication() const { return _data->application; }

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		Vector<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform([this, scheme = &scheme, oid, flags, p, fields = sp::move(fields)](
						   const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, fields, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		Vector<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(sp::move(cb));
	return perform(
			[this, scheme = &scheme, alias = alias.str<Interface>(), flags, p,
					fields = sp::move(fields)](const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, fields, flags);
		_data->application->performOnAppThread([p, ret = xenolith::Value(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

Server::ServerData::ServerData() {
	storage = new (memory::pool::acquire()) ServerDataStorage;
	storage->queue.setQueueLocking(mutexQueue);
	storage->queue.setFreeLocking(mutexFree);

	filesystem::enumeratePaths(FileCategory::AppData, [&](StringView str, FileFlags) {
		storage->documentRoot = str.str<db::Interface>();
		return false;
	});
}

Server::ServerData::~ServerData() { }

bool Server::ServerData::execute(const ServerDataTaskCallback &task) {
	if (currentTransaction) {
		if (!task.callback) {
			return false;
		}
		return task.callback(*server, *currentTransaction);
	}

	bool ret = false;

	memory::perform_clear([&] {
		driver->performWithStorage(handle, [&, this](const db::Adapter &adapter) {
			adapter.performWithTransaction([&, this](const db::Transaction &t) {
				currentTransaction = &t;
				auto ret = task.callback(*server, t);
				currentTransaction = nullptr;
				return ret;
			});
		});
	}, threadPool);

	runAsync();

	return ret;
}

void Server::ServerData::runAsync() {
	memory::perform_clear([&] {
		while (asyncTasks && driver->isValid(handle)) {
			auto tmp = asyncTasks;
			asyncTasks = nullptr;

			driver->performWithStorage(handle, [&, this](const db::Adapter &adapter) {
				adapter.performWithTransaction([&, this](const db::Transaction &t) {
					auto &vec = *tmp;
					currentTransaction = &t;
					for (auto &it : vec) { it(t); }
					currentTransaction = nullptr;
					return true;
				});
			});
		}
	}, asyncPool);
}

void Server::ServerData::threadInit() {
	memory::perform([&] {
		handle = driver->connect(storage->params);
		if (!handle.get()) {
			StringStream out;
			for (auto &it : storage->params) { out << "\n\t" << it.first << ": " << it.second; }
			log::source().error("StorageServer", "Fail to initialize DB with params: ", out.str());
		}
	}, serverPool);

	if (!handle.get()) {
		return;
	}

	asyncPool = memory::pool::create();
	threadPool = memory::pool::create();

	memory::perform_clear([&] {
		driver->init(handle, db::Vector<db::StringView>());

		driver->performWithStorage(handle, [&, this](const db::Adapter &adapter) {
			db::Scheme::initSchemes(storage->predefinedSchemes);
			interfaceConfig.name = adapter.getDatabaseName();
			adapter.init(interfaceConfig, storage->predefinedSchemes);
		});
	}, threadPool);

	runAsync();

	if (!storage->serverName.empty()) {
		thread::ThreadInfo::setThreadInfo(storage->serverName);
	}

	now = sp::platform::clock(ClockType::Monotonic);

	Thread::threadInit();
}

bool Server::ServerData::worker() {
	if (!_continueExecution.test_and_set()) {
		return false;
	}

	auto t = sp::platform::clock(ClockType::Monotonic);
	if (t - now > TimeInterval::seconds(1).toMicros()) {
		now = t;
		handleHeartbeat();
	}

	ServerDataTaskCallback task;
	do {
		storage->queue.pop_direct([&](memory::PriorityQueue<ServerDataTaskCallback>::PriorityType,
										  ServerDataTaskCallback &&cb) { task = move(cb); });
	} while (0);

	if (!task.callback) {
		std::unique_lock<std::mutex> lock(mutexQueue);
		if (!storage->queue.empty(lock)) {
			return true;
		}
		condition.wait_for(lock, std::chrono::seconds(1));
		return true;
	}

	if (!driver->isValid(handle)) {
		return false;
	}

	slog().debug("Server::ServerData", "execute");
	execute(task);
	return true;
}

void Server::ServerData::threadDispose() {
	while (!storage->queue.empty()) {
		ServerDataTaskCallback task;
		do {
			storage->queue.pop_direct(
					[&](memory::PriorityQueue<ServerDataTaskCallback>::PriorityType,
							ServerDataTaskCallback &&cb) SP_COVERAGE_TRIVIAL { task = move(cb); });
		} while (0);

		if (task.callback) {
			execute(task);
		}
	}

	memory::perform([&] {
		if (driver->isValid(handle)) {
			driver->performWithStorage(handle, [&, this](const db::Adapter &adapter) {
				auto it = storage->components.begin();
				while (it != storage->components.end()) {
					adapter.performWithTransaction([&, this](const db::Transaction &t) {
						do {
							memory::context ctx(it->second->pool);
							for (auto &iit : it->second->components) {
								iit.second->handleChildRelease(*server, t);
								iit.second->~Component();
							}

							it->second->container->handleStorageDisposed(t);
						} while (0);
						return true;
					});
					memory::pool::destroy(it->second->pool);
					it = storage->components.erase(it);
				}
				storage->components.clear();
			});
		}
	}, threadPool);

	memory::pool::destroy(threadPool);
	memory::pool::destroy(asyncPool);

	Thread::threadDispose();
}

void Server::ServerData::handleHeartbeat() {
	for (auto &it : storage->components) {
		for (auto &iit : it.second->components) { iit.second->handleHeartbeat(*server); }
	}
}

void Server::ServerData::addAsyncTask(
		const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb)
		const {
	memory::perform([&] {
		if (!asyncTasks) {
			asyncTasks = new (asyncPool) db::Vector<db::Function<void(const db::Transaction &)>>;
		}
		asyncTasks->emplace_back(setupCb(asyncPool));
	}, asyncPool);
}

bool Server::ServerData::addComponent(ComponentContainer *comp, const db::Transaction &t) {
	ServerComponentLoader loader(this, t);

	memory::perform([&] { comp->handleStorageInit(loader); }, loader.getPool());

	return loader.run(comp);
}

void Server::ServerData::removeComponent(ComponentContainer *comp, const db::Transaction &t) {
	auto cmpIt = storage->components.find(comp);
	if (cmpIt == storage->components.end()) {
		return;
	}

	do {
		memory::context ctx(cmpIt->second->pool);
		for (auto &it : cmpIt->second->components) {
			it.second->handleChildRelease(*server, t);
			it.second->~Component();
		}

		cmpIt->second->container->handleStorageDisposed(t);
	} while (0);

	memory::pool::destroy(cmpIt->second->pool);
	storage->components.erase(cmpIt);
}

void Server::ServerData::scheduleAyncDbTask(
		const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb)
		const {
	addAsyncTask(setupCb);
}

db::StringView Server::ServerData::getDocumentRoot() const { return storage->documentRoot; }

const db::Scheme *Server::ServerData::getFileScheme() const { return nullptr; }

const db::Scheme *Server::ServerData::getUserScheme() const { return nullptr; }

void Server::ServerData::pushErrorMessage(db::Value &&val) const {
	log::source().error("xenolith::Server", data::EncodeFormat::Pretty, val);
}

void Server::ServerData::pushDebugMessage(db::Value &&val) const {
	log::source().debug("xenolith::Server", data::EncodeFormat::Pretty, val);
}

void Server::ServerData::initTransaction(db::Transaction &t) const {
	for (auto &it : storage->components) {
		for (auto &iit : it.second->components) { iit.second->handleStorageTransaction(t); }
	}
}

ServerComponentLoader::~ServerComponentLoader() {
	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

ServerComponentLoader::ServerComponentLoader(Server::ServerData *data, const db::Transaction &t)
: _data(data)
, _pool(memory::pool::create(data->serverPool))
, _server(data->server)
, _transaction(&t) {
	memory::context ctx(_pool);

	_components = new (std::nothrow) ServerComponentData;
	_components->pool = _pool;
}

void ServerComponentLoader::exportComponent(Component *comp) {
	memory::context ctx(_pool);

	_components->components.emplace(comp->getName(), comp);
}

const db::Scheme *ServerComponentLoader::exportScheme(const db::Scheme &scheme) {
	return _components->schemes.emplace(scheme.getName(), &scheme).first->second;
}

bool ServerComponentLoader::run(ComponentContainer *comp) {
	memory::context ctx(_pool);

	_components->container = comp;
	_data->storage->components.emplace(comp, _components);

	db::Scheme::initSchemes(_components->schemes);
	_transaction->getAdapter().init(_data->interfaceConfig, _components->schemes);

	for (auto &it : _components->components) {
		it.second->handleChildInit(*_server, *_transaction);
	}

	_pool = nullptr;
	_components = nullptr;
	return true;
}
} // namespace stappler::xenolith::storage
