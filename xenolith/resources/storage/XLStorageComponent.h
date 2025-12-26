/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RESOURCES_STORAGE_XLSTORAGECOMPONENT_H_
#define XENOLITH_RESOURCES_STORAGE_XLSTORAGECOMPONENT_H_

#include "XLCommon.h"
#include "SPDbScheme.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

class ComponentLoader;
class Server;

class SP_PUBLIC Component : public db::AllocBase {
public:
	virtual ~Component();

	Component(ComponentLoader &, StringView name);

	virtual void handleChildInit(const Server &, const db::Transaction &);
	virtual void handleChildRelease(const Server &, const db::Transaction &);

	virtual void handleStorageTransaction(db::Transaction &);
	virtual void handleHeartbeat(const Server &);

	StringView getName() const { return _name; }

protected:
	db::String _name;
};

class SP_PUBLIC ComponentLoader {
public:
	virtual ~ComponentLoader();

	virtual db::pool_t *getPool() const = 0;
	virtual const Server &getServer() const = 0;
	virtual const db::Transaction &getTransaction() const = 0;

	virtual void exportComponent(Component *) = 0;

	virtual const db::Scheme * exportScheme(const db::Scheme &) = 0;

	template <typename ... Args>
	void define(db::Scheme &scheme, Args && ... args) {
		exportScheme(scheme);
		scheme.define(std::forward<Args>(args)...);
	}
};

class SP_PUBLIC ComponentContainer : public Ref {
public:
	using TaskCallback = Function<bool(const Server &, const db::Transaction &)>;

	virtual ~ComponentContainer();

	virtual bool init(StringView);

	virtual void handleStorageInit(ComponentLoader &);
	virtual void handleStorageDisposed(const db::Transaction &);

	virtual void handleComponentsLoaded(const Server &serv);
	virtual void handleComponentsUnloaded(const Server &serv);

	StringView getName() const { return _name; }

	void setServer(const Server *serv) { _server = serv; }
	const Server *getServer() const { return _server; }

	bool isLoaded() const { return _loaded; }

	bool perform(TaskCallback &&, Ref * = nullptr) const;

protected:
	bool _loaded = false;
	String _name;
	const Server *_server = nullptr;
	mutable Vector<Pair<TaskCallback, Rc<Ref>>> _pendingTasks;
};

}

#endif /* XENOLITH_RESOURCES_STORAGE_XLSTORAGECOMPONENT_H_ */
