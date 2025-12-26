/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPSqlDriver.h"
#include "SPPqDriver.h"
#include "SPSqliteDriver.h"
#include "SPDbAdapter.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

thread_local std::map<StringView, Map<StringView, const void *>> tl_DriverQueryStorage;

QueryStorageHandle::QueryStorageHandle(const Driver *d, StringView n,
		Map<StringView, const void *> *dt)
: driver(d), name(n), data(dt) { }

QueryStorageHandle::~QueryStorageHandle() {
	if (driver) {
		driver->unregisterQueryStorage(name);
	}
}

QueryStorageHandle::QueryStorageHandle(QueryStorageHandle &&other)
: driver(other.driver), name(other.name), data(other.data) {
	other.driver = nullptr;
}

QueryStorageHandle &QueryStorageHandle::operator=(QueryStorageHandle &&other) {
	driver = other.driver;
	name = other.name;
	data = other.data;
	other.driver = nullptr;
	return *this;
}

Driver *Driver::open(pool_t *pool, ApplicationInterface *app, StringView path,
		const void *external) {
	memory::context ctx(pool, memory::context<pool_t *>::conditional);

	Driver *ret = nullptr;

	if (path == "pgsql") {
		ret = pq::Driver::open(pool, app, StringView(), external);
	} else if (path.starts_with("pgsql:")) {
		path += "pgsql:"_len;
		ret = pq::Driver::open(pool, app, path, external);
	} else if (path == "sqlite" || path == "sqlite3") {
		ret = sqlite::Driver::open(pool, app, path);
	}

	registerCleanupDestructor(ret, pool);
	return ret;
}

Driver::~Driver() { }

void Driver::setDbCtrl(Function<void(bool)> &&fn) { _dbCtrl = sp::move(fn); }

const CustomFieldInfo *Driver::getCustomFieldInfo(StringView key) const {
	auto it = _customFields.find(key);
	if (it != _customFields.end()) {
		return &it->second;
	}
	return nullptr;
}

QueryStorageHandle Driver::makeQueryStorage(StringView name) const {
	auto d = registerQueryStorage(name);
	if (d) {
		return QueryStorageHandle(this, name, d);
	}
	return QueryStorageHandle(nullptr, name, nullptr);
}

Map<StringView, const void *> *Driver::getQueryStorage(StringView name) const {
	auto it = tl_DriverQueryStorage.find(name);
	if (it != tl_DriverQueryStorage.end()) {
		return &it->second;
	}
	return nullptr;
}

Map<StringView, const void *> *Driver::getCurrentQueryStorage() const {
	if (tl_DriverQueryStorage.size() > 0) {
		return &tl_DriverQueryStorage.begin()->second;
	}
	return nullptr;
}

Map<StringView, const void *> *Driver::registerQueryStorage(StringView name) const {
	if (tl_DriverQueryStorage.find(name) != tl_DriverQueryStorage.end()) {
		return nullptr;
	}

	auto ret = &tl_DriverQueryStorage.emplace(name, Map<StringView, const void *>()).first->second;

	return ret;
}

void Driver::unregisterQueryStorage(StringView name) const { tl_DriverQueryStorage.erase(name); }

Driver::Driver(pool_t *p, ApplicationInterface *app) : _pool(p), _application(app) {
	if (!app) {
		auto mem = pool::palloc(_pool, sizeof(ApplicationInterface));

		_application = new (mem) ApplicationInterface;
	}
}

} // namespace stappler::db::sql
