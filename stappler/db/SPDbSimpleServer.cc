/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPDbSimpleServer.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::db {

SimpleServer::~SimpleServer() {
	auto pool = _data->staticPool;
	_data = nullptr;
	if (pool) {
		memory::pool::destroy(pool);
	}
	sprt::memory::pool::terminate();
}

SimpleServer::SimpleServer() {
	sprt::memory::pool::initialize();

	auto pool = memory::pool::create();

	mem_pool::perform([&, this] {
		_data = new (pool) SimpleServerData;
		_data->staticPool = pool;
		_data->contextPool = memory::pool::create(_data->staticPool);
		_data->updatePool = memory::pool::create(_data->staticPool);

		defineUserScheme(_data->users);
		defineFileScheme(_data->files);
		defineErrorScheme(_data->errors);
	}, pool);
}

bool SimpleServer::init(const Value &params, StringView root, AccessRoleId role,
		SpanView<const db::Scheme *> schemes) {
	db::Map<StringView, StringView> initParams;

	mem_pool::perform([&, this] {
		StringView driver;

		for (auto &it : params.asDict()) {
			if (it.first == "driver") {
				driver = StringView(it.second.getString());
			} else {
				initParams.emplace(it.first, it.second.getString());
			}
		}

		if (driver.empty()) {
			driver = StringView("sqlite");
		}

		_data->driver = db::sql::Driver::open(_data->staticPool, this, driver);
	}, _data->staticPool);

	if (!_data || !_data->driver) {
		log::source().error("db::SimpleServer", "Fail to load db driver");
		return false;
	}

	mem_pool::perform([&, this] {
		if (root.empty()) {
			_data->documentRoot = filesystem::findPath<db::Interface>(FileCategory::AppData);
		} else {
			_data->documentRoot = root.str<Interface>();
		}

		_data->handle = _data->driver->connect(initParams);

		if (!_data->handle.get()) {
			db::StringStream out;
			for (auto &it : initParams) { out << "\n\t" << it.first << ": " << it.second; }
			log::source().error("db::SimpleServer",
					"Fail to initialize DB with params: ", out.str());
		}
	}, _data->staticPool);

	if (!_data->handle.get()) {
		return false;
	}

	mem_pool::perform([&, this] {
		_data->driver->init(_data->handle, db::Vector<db::StringView>());

		_data->driver->performWithStorage(_data->handle, [&, this](const db::Adapter &adapter) {
			db::Map<StringView, const db::Scheme *> predefinedSchemes;

			predefinedSchemes.emplace(_data->users.getName(), &_data->users);
			predefinedSchemes.emplace(_data->files.getName(), &_data->files);
			predefinedSchemes.emplace(_data->errors.getName(), &_data->errors);

			for (auto &it : schemes) { predefinedSchemes.emplace(it->getName(), it); }

			db::Scheme::initSchemes(predefinedSchemes);

			_data->interfaceConfig.name = adapter.getDatabaseName().pdup(_data->staticPool);
			_data->interfaceConfig.fileScheme = getFileScheme();
			adapter.init(_data->interfaceConfig, predefinedSchemes);
		});
	}, _data->contextPool);
	memory::pool::clear(_data->contextPool);

	return true;
}

void SimpleServer::scheduleAyncDbTask(
		const Callback<Function<void(const Transaction &)>(pool_t *)> &setupCb) const {
	memory::context<pool_t *> ctx(_data->updatePool, memory::context<pool_t *>::conditional);

	if (!_data->asyncTasks) {
		_data->asyncTasks = new (_data->updatePool) Vector<Function<void(const Transaction &)>>;
	}
	_data->asyncTasks->emplace_back(setupCb(_data->updatePool));
}

StringView SimpleServer::getDocumentRoot() const { return _data->documentRoot; }

const Scheme *SimpleServer::getFileScheme() const { return &_data->files; }

const Scheme *SimpleServer::getUserScheme() const { return &_data->users; }

void SimpleServer::pushErrorMessage(Value &&value) const {
	log::source().error("db::SimpleServer", data::EncodeFormat::Pretty, value);
}

void SimpleServer::pushDebugMessage(Value &&value) const {
	log::source().debug("db::SimpleServer", data::EncodeFormat::Pretty, value);
}

StringView SimpleServer::getDatabaseName() const { return _data->interfaceConfig.name; }

void SimpleServer::initTransaction(Transaction &t) const { t.setRole(_data->defaultRole); }

void SimpleServer::update() {
	mem_pool::perform([&, this] {
		while (_data->asyncTasks && _data->driver->isValid(_data->handle)) {
			auto tmp = _data->asyncTasks;
			_data->asyncTasks = nullptr;

			_data->driver->performWithStorage(_data->handle, [&](const db::Adapter &adapter) {
				adapter.performWithTransaction([&](const db::Transaction &t) {
					for (auto &it : *tmp) { it(t); }
					return true;
				});
			});
		}
	}, _data->updatePool);
	memory::pool::clear(_data->updatePool);
}

void SimpleServer::perform(const Callback<bool(const Transaction &)> &cb) {
	mem_pool::perform([&, this] {
		_data->driver->performWithStorage(_data->handle, [&](const db::Adapter &adapter) {
			adapter.performWithTransaction([&](const db::Transaction &t) { return cb(t); });
		});
	}, _data->contextPool);
	memory::pool::clear(_data->contextPool);
}

} // namespace stappler::db
