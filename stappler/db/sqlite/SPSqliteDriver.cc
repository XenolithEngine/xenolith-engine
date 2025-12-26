/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPSqliteDriver.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPSqliteDriverHandle.cc"
#include "SPSqliteHandle.h"
#include "SPDbFieldExtensions.h"
#include "sqlite3.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

static void sp_ts_update_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args);
static void sp_ts_rank_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args);
static void sp_ts_query_valid_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args);

constexpr static auto DATABASE_DEFAULTS = StringView(R"Sql(
CREATE TABLE IF NOT EXISTS "__objects" (
	"control" INT NOT NULL PRIMARY KEY DEFAULT 0,
	"__oid" BIGINT NOT NULL DEFAULT 0
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS "__removed" (
	__oid BIGINT NOT NULL PRIMARY KEY
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS "__versions" (
	name TEXT NOT NULL PRIMARY KEY,
	version INT NOT NULL
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS "__sessions" (
	name BLOB NOT NULL PRIMARY KEY,
	mtime BIGINT NOT NULL,
	maxage BIGINT NOT NULL,
	data BLOB
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS "__broadcasts" (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	date BIGINT NOT NULL,
	msg BLOB
);

CREATE TABLE IF NOT EXISTS "__login" (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	"user" BIGINT NOT NULL,
	name TEXT NOT NULL,
	password BLOB NOT NULL,
	date BIGINT NOT NULL,
	success BOOLEAN NOT NULL,
	addr TEXT,
	host TEXT,
	path TEXT
);

CREATE TABLE IF NOT EXISTS "__words" (
	id BIGINT NOT NULL,
	word TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS "__broadcasts_idx_date" ON "__broadcasts" ("date");
CREATE INDEX IF NOT EXISTS "__login_idx_user" ON "__login" ("user");
CREATE INDEX IF NOT EXISTS "__login_idx_date" ON "__login" ("date");
CREATE UNIQUE INDEX IF NOT EXISTS "__words_idx_id" ON "__words" ("id");
)Sql");

Driver *Driver::open(pool_t *pool, ApplicationInterface *app, StringView path) {
	DriverSym *l = nullptr;
	if (!path.empty() && path != "sqlite" && path != "sqlite3") {
		l = DriverLibStorage::getInstance()->openLib(path);
	} else {
		StringView name = path;
		if (path.empty() || path == "sqlite" || path == "sqlite3") {
			l = DriverLibStorage::getInstance()->openSelf();
			if (!l) {
#if WIN32
				name = StringView("sqlite3.dll");
#else
				name = StringView("libsqlite3.so");
#endif
			}
		}

		if (!l) {
			l = DriverLibStorage::getInstance()->openLib(name);
		}

		if (!l) {
#if WIN32
			name = StringView("sqlite3.0.dll");
#else
			name = StringView("libsqlite3.so.0");
#endif
			l = DriverLibStorage::getInstance()->openLib(name);
		}
	}

	if (l) {
		if (l->_initialize() == SQLITE_OK) {
			return new (std::nothrow) Driver(pool, app, path, l);
		} else {
			log::source().error("sqlite::Driver", "sqlite3_initialize failed");
			DriverLibStorage::getInstance()->closeLib(l);
		}
	}

	return nullptr;
}

Driver::~Driver() { _handle->_shutdown(); }

bool Driver::init(Handle handle, const Vector<StringView> &) { return true; }

void Driver::performWithStorage(Handle handle,
		const Callback<void(const db::Adapter &)> &cb) const {
	auto targetPool = pool::acquire();

	db::sqlite::Handle h(this, handle);
	db::Adapter storage(&h, _application);
	pool::userdata_set((void *)&h, config::STORAGE_INTERFACE_KEY.data(), nullptr, targetPool);

	cb(storage);

	auto stack = stappler::memory::pool::get<db::Transaction::Stack>(targetPool,
			config::STORAGE_TRANSACTION_STACK_KEY);
	if (stack) {
		for (auto &it : stack->stack) {
			if (it->adapter == storage) {
				it->adapter = db::Adapter(nullptr, _application);
				_application->error("Root", "Incomplete transaction found");
			}
		}
	}
	pool::userdata_set((void *)nullptr, storage.getTransactionKey().data(), nullptr, targetPool);
	pool::userdata_set((void *)nullptr, config::STORAGE_INTERFACE_KEY.data(), nullptr, targetPool);
}

BackendInterface *Driver::acquireInterface(Handle handle, pool_t *pool) const {
	BackendInterface *ret = nullptr;
	memory::perform_conditional([&] { ret = new (pool) db::sqlite::Handle(this, handle); }, pool);
	return ret;
}

static void sp_sqlite_next_oid_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();

	sqlite3_int64 ret = 0;
	DriverHandle *data = (DriverHandle *)sym->_user_data(ctx);
	std::unique_lock lock(data->mutex);
	auto err = sym->step(data->oidQuery);
	if (err == SQLITE_ROW) {
		ret = sym->_column_int64(data->oidQuery, 0);
	}
	if (!ret) {
		ret = Time::now().toMicros();
	}
	sym->reset(data->oidQuery);
	sym->_result_int64(ctx, ret);
}

static void sp_sqlite_now_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();

	sym->_result_int64(ctx, Time::now().toMicros());
}

static void sp_sqlite_user_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();

	DriverHandle *data = (DriverHandle *)sym->_user_data(ctx);
	sym->_result_int64(ctx, data->userId);
}

static Driver::Handle Driver_setupDriver(const Driver *d, DriverSym *_handle, pool_t *p,
		StringView dbname, StringView journal, int flags) {
	sqlite3 *db = nullptr;
	if (!dbname.is('/') && !dbname.is(':')) {
		if (auto app = d->getApplicationInterface()) {
			dbname = StringView(filepath::merge<Interface>(app->getDocumentRoot(), dbname)).pdup();
		} else {
			filesystem::enumerateWritablePaths(FileInfo{dbname, stappler::FileCategory::AppData},
					[&](StringView path, FileFlags) {
				dbname = path.pdup();
				return false;
			});
		}
	}
	filesystem::mkdir_recursive(FileInfo(filepath::root(FileInfo{dbname})));
#if WIN32
	dbname = StringView(filesystem::native::posixToNative<Interface>(dbname)).pdup();
#endif
	if (_handle->open(dbname.data(), &db, flags, nullptr) == SQLITE_OK) {
		_handle->_db_config(db, SQLITE_DBCONFIG_DQS_DDL, 0, nullptr);
		_handle->_db_config(db, SQLITE_DBCONFIG_DQS_DML, 0, nullptr);
		_handle->_db_config(db, SQLITE_DBCONFIG_ENABLE_FKEY, 1, nullptr);

		if (!journal.empty()) {
			auto m = stappler::string::toupper<Interface>(journal);
			auto mode = stappler::string::toupper<Interface>(
					Driver_exec(_handle, p, db, "PRAGMA journal_mode;"));
			if (mode.empty()) {
				_handle->close(db);
				return Driver::Handle(nullptr);
			}

			if (mode != m) {
				auto query = toString("PRAGMA journal_mode = ", m);
				auto cmode =
						stappler::string::toupper<Interface>(Driver_exec(_handle, p, db, query));
				if (mode.empty() || cmode != m) {
					log::source().error("sqlite::Driver", "fail to enable journal_mode '", m, "'");
					_handle->close(db);
					return Driver::Handle(nullptr);
				}
			}
		}

		auto queryData = DATABASE_DEFAULTS;
		auto outPtr = queryData.data();

		bool success = true;
		while (outPtr && *outPtr != 0 && success) {
			auto size = queryData.size() - (outPtr - queryData.data());
			StringView nextQuery(outPtr, size);
			nextQuery.skipChars<StringView::WhiteSpace>();
			if (nextQuery.empty()) {
				break;
			}

			sqlite3_stmt *stmt = nullptr;
			auto err = _handle->prepare(db, nextQuery.data(), int(nextQuery.size()), 0, &stmt,
					&outPtr);
			if (err != SQLITE_OK) {
				auto len = outPtr - nextQuery.data();
				StringView performedQuery(nextQuery.data(), len);
				auto info = d->getInfo(Driver::Connection(db), err);
				info.setString(performedQuery, "query");
#if DEBUG
				log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
				break;
			}

			err = _handle->step(stmt);

			if (err != SQLITE_OK && err != SQLITE_DONE && err != SQLITE_ROW) {
				auto info = d->getInfo(Driver::Connection(db), err);
				info.setString(nextQuery, "query");
#if DEBUG
				log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
				_handle->finalize(stmt);
				break;
			}

			success = ResultCursor::statusIsSuccess(err);
			_handle->finalize(stmt);
		}

		auto mem = pool::palloc(p, sizeof(DriverHandle));
		auto h = new (mem) DriverHandle;
		h->pool = p;
		h->driver = d;
		h->sym = _handle;
		;
		h->conn = db;
		h->name = dbname.pdup(p);
		h->ctime = Time::now();
		h->mutex.lock();

		do {
			StringView getStmt("SELECT \"__oid\" FROM \"__objects\" WHERE \"control\" = 0;");
			sqlite3_stmt *gstmt = nullptr;
			auto err =
					_handle->prepare(db, getStmt.data(), int(getStmt.size()), 0, &gstmt, nullptr);
			err = _handle->step(gstmt);
			if (err == SQLITE_DONE) {
				StringView createStmt(
						"INSERT OR IGNORE INTO \"__objects\" (\"__oid\") VALUES (0);");
				sqlite3_stmt *cstmt = nullptr;
				err = _handle->prepare(db, createStmt.data(), int(createStmt.size()), 0, &cstmt,
						nullptr);
				err = _handle->step(cstmt);
				_handle->finalize(cstmt);
			}
			_handle->finalize(gstmt);

			StringView oidStmt(
					"UPDATE OR IGNORE \"__objects\" SET \"__oid\" = \"__oid\" + 1 WHERE "
					"\"control\" = 0 RETURNING \"__oid\";");

			sqlite3_stmt *stmt = nullptr;
			err = _handle->prepare(db, oidStmt.data(), int(oidStmt.size()),
					SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
			if (err == SQLITE_OK) {
				h->oidQuery = stmt;
			}
		} while (0);

		do {
			StringView
					str("INSERT INTO \"__words\"(\"id\",\"word\") VALUES(?1, ?2) ON CONFLICT(id) "
						"DO " "UPDATE SET word=word RETURNING \"id\", \"word\";");

			sqlite3_stmt *stmt = nullptr;
			auto err = _handle->prepare(db, str.data(), int(str.size()), SQLITE_PREPARE_PERSISTENT,
					&stmt, nullptr);
			if (err == SQLITE_OK) {
				h->wordsQuery = stmt;
			}
		} while (0);

		_handle->_create_function_v2(db, "sp_sqlite_next_oid", 0, SQLITE_UTF8, (void *)h,
				sp_sqlite_next_oid_xFunc, nullptr, nullptr, nullptr);
		_handle->_create_function_v2(db, "sp_sqlite_now", 0, SQLITE_UTF8, (void *)h,
				sp_sqlite_now_xFunc, nullptr, nullptr, nullptr);
		_handle->_create_function_v2(db, "sp_sqlite_user", 0, SQLITE_UTF8, (void *)h,
				sp_sqlite_user_xFunc, nullptr, nullptr, nullptr);

		_handle->_create_function_v2(db, "sp_ts_update", 6, SQLITE_UTF8, (void *)h,
				sp_ts_update_xFunc, nullptr, nullptr, nullptr);
		_handle->_create_function_v2(db, "sp_ts_rank", 3, SQLITE_UTF8, (void *)h, sp_ts_rank_xFunc,
				nullptr, nullptr, nullptr);
		_handle->_create_function_v2(db, "sp_ts_query_valid", 2, SQLITE_UTF8, (void *)h,
				sp_ts_query_valid_xFunc, nullptr, nullptr, nullptr);

		_handle->_create_module(db, "sp_unwrap", &s_UnwrapModule, (void *)d);

		h->mutex.unlock();

		pool::pre_cleanup_register(p, [h] {
			if (h->oidQuery) {
				h->sym->finalize(h->oidQuery);
			}
			if (h->wordsQuery) {
				h->sym->finalize(h->wordsQuery);
			}
			h->sym->close(h->conn);
		});

		return Driver::Handle(h);
	}

	return Driver::Handle(nullptr);
}

Driver::Handle Driver::connect(const Map<StringView, StringView> &params) const {
	auto p = pool::create(pool::acquire());
	Driver::Handle rec = Driver::Handle(nullptr);

	memory::perform([&] {
		int flags = 0;
		StringView mode;
		StringView dbname("db.sqlite");
		StringView journal;

		for (auto &it : params) {
			if (it.first == "dbname") {
				dbname = it.second;
			} else if (it.first == "mode") {
				mode = it.second;
			} else if (it.first == "cache") {
				if (it.second == "shared") {
					flags |= SQLITE_OPEN_SHAREDCACHE;
				} else if (it.second == "private") {
					flags |= SQLITE_OPEN_PRIVATECACHE;
				}
			} else if (it.first == "threading") {
				if (it.second == "serialized") {
					flags |= SQLITE_OPEN_FULLMUTEX;
				} else if (it.second == "multi" || it.second == "multithread"
						|| it.second == "multithreaded") {
					flags |= SQLITE_OPEN_NOMUTEX;
				}
			} else if (it.first == "journal") {
				if (it.second == "delete" || it.second == "truncate" || it.second == "persist"
						|| it.second == "memory" || it.second == "wal" || it.second == "off") {
					journal = it.second;
				}
			} else if (it.first != "driver" && it.first == "nmin" && it.first == "nkeep"
					&& it.first == "nmax" && it.first == "exptime" && it.first == "persistent") {
				log::source().error("sqlite::Driver", "unknown connection parameter: ", it.first,
						"=", it.second);
			}
		}

		if (!mode.empty()) {
			if (mode == "ro") {
				flags |= SQLITE_OPEN_READONLY;
			} else if (mode == "rw") {
				flags |= SQLITE_OPEN_READWRITE;
			} else if (mode == "rwc") {
				flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			} else if (mode == "memory") {
				flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY;
			} else {
				log::source().error("sqlite::Driver", "unknown mode parameter: ", mode);
			}
		} else {
			flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
		}

		rec = Driver_setupDriver(this, _handle, p, dbname, journal, flags);
	}, p);

	if (!rec.get()) {
		pool::destroy(p);
	}
	return rec;
}

void Driver::finish(Handle h) const {
	auto db = (DriverHandle *)h.get();
	if (db && db->pool) {
		pool::destroy(db->pool);
	}
}

Driver::Connection Driver::getConnection(Handle h) const {
	auto db = (DriverHandle *)h.get();
	return Driver::Connection(db->conn);
}

bool Driver::isValid(Handle) const { return true; }

bool Driver::isValid(Connection) const { return true; }

bool Driver::isIdle(Connection) const { return true; }

Time Driver::getConnectionTime(Handle handle) const {
	auto db = (DriverHandle *)handle.get();
	return db->ctime;
}

StringView Driver::getDbName(Handle h) const {
	auto db = (DriverHandle *)h.get();
	return db->name;
}

Value Driver::getInfo(Connection conn, int err) const {
	return Value({
		stappler::pair("error", Value(err)),
		stappler::pair("status", Value(_handle->_errstr(int(err)))),
		stappler::pair("desc", Value(_handle->_errmsg((sqlite3 *)conn.get()))),
	});
}

void Driver::setUserId(Handle h, int64_t userId) const {
	auto db = (DriverHandle *)h.get();
	db->userId = userId;
}

uint64_t Driver::insertWord(Handle h, StringView word) const {
	auto data = (DriverHandle *)h.get();

	uint64_t hash = hash::hash32(word.data(), uint32_t(word.size()), 0) << 16;

	std::unique_lock lock(data->mutex);
	bool success = false;
	while (!success) {
		_handle->_bind_int64(data->wordsQuery, 1, hash);
		_handle->_bind_text(data->wordsQuery, 2, word.data(), int(word.size()), nullptr);

		auto err = _handle->step(data->wordsQuery);
		if (err == SQLITE_ROW) {
			auto w = StringView((const char *)_handle->_column_text(data->wordsQuery, 1),
					_handle->_column_bytes(data->wordsQuery, 1));
			if (w == word) {
				success = true;
				_handle->reset(data->wordsQuery);
				break;
			} else {
				log::source().debug("sqlite::Driver", "Hash collision: ", w, " ", word, " ", hash);
			}
		}
		_handle->reset(data->wordsQuery);
		++hash;
	}

	return hash;
}

Driver::Driver(pool_t *pool, ApplicationInterface *app, StringView mem, DriverSym *sym)
: sql::Driver(pool, app) {
	_handle = sym;
	_driverPath = mem.pdup();

	pool::cleanup_register(pool, [this] {
		DriverLibStorage::getInstance()->closeLib(_handle);
		_handle = nullptr;
	});

	auto it = _customFields.emplace(FieldIntArray::FIELD_NAME);
	if (!FieldIntArray::registerForSqlite(it.first->second)) {
		_customFields.erase(it.first);
	}

	it = _customFields.emplace(FieldBigIntArray::FIELD_NAME);
	if (!FieldBigIntArray::registerForSqlite(it.first->second)) {
		_customFields.erase(it.first);
	}

	it = _customFields.emplace(FieldPoint::FIELD_NAME);
	if (!FieldPoint::registerForSqlite(it.first->second)) {
		_customFields.erase(it.first);
	}

	it = _customFields.emplace(FieldTextArray::FIELD_NAME);
	if (!FieldTextArray::registerForSqlite(it.first->second)) {
		_customFields.erase(it.first);
	}
}

bool ResultCursor::statusIsSuccess(int x) {
	return (x == SQLITE_DONE) || (x == SQLITE_ROW) || (x == SQLITE_OK);
}

ResultCursor::ResultCursor(const Driver *d, Driver::Connection conn, Driver::Result res, int status)
: driver(d), conn(conn), result(res), err(status) { }

ResultCursor::~ResultCursor() { clear(); }

bool ResultCursor::isBinaryFormat(size_t field) const { return true; }

BackendInterface::StorageType ResultCursor::getType(size_t field) const {
	auto t = driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field));
	switch (t) {
	case SQLITE_INTEGER: return BackendInterface::StorageType::Int8; break;
	case SQLITE_FLOAT: return BackendInterface::StorageType::Float8; break;
	case SQLITE_TEXT: return BackendInterface::StorageType::Text; break;
	case SQLITE_BLOB: return BackendInterface::StorageType::Bytes; break;
	case SQLITE_NULL: return BackendInterface::StorageType::Unknown; break;
	default: return BackendInterface::StorageType::Unknown; break;
	}
	return BackendInterface::StorageType::Unknown;
}

bool ResultCursor::isNull(size_t field) const {
	return driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))
			== SQLITE_NULL;
}

StringView ResultCursor::toString(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER:
		return StringView(toString(driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(),
								  int(field))))
				.pdup();
		break;
	case SQLITE_FLOAT:
		return StringView(toString(driver->getHandle()->_column_double((sqlite3_stmt *)result.get(),
								  int(field))))
				.pdup();
		break;
	case SQLITE_TEXT:
		return StringView((const char *)driver->getHandle()->_column_text(
								  (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_BLOB:
		return StringView((const char *)driver->getHandle()->_column_blob(
								  (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_NULL: return StringView("(null)"); break;
	}

	return StringView();
}

BytesView ResultCursor::toBytes(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER: {
		int64_t value =
				driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(), int(field));
		return BytesView((const uint8_t *)&value, sizeof(int64_t)).pdup();
		break;
	}
	case SQLITE_FLOAT: {
		double value =
				driver->getHandle()->_column_double((sqlite3_stmt *)result.get(), int(field));
		return BytesView((const uint8_t *)&value, sizeof(int64_t)).pdup();
		break;
	}
	case SQLITE_TEXT:
		return BytesView((const uint8_t *)driver->getHandle()->_column_text(
								 (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_BLOB:
		return BytesView((const uint8_t *)driver->getHandle()->_column_blob(
								 (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_NULL: return BytesView(); break;
	}

	return BytesView();
}

int64_t ResultCursor::toInteger(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER:
		return driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(), int(field));
		break;
	case SQLITE_FLOAT:
		return int64_t(
				driver->getHandle()->_column_double((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_TEXT:
		return StringView((const char *)driver->getHandle()->_column_text(
								  (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)))
				.readInteger(10)
				.get(0);
		break;
	case SQLITE_BLOB:
		return int64_t(BytesView((const uint8_t *)driver->getHandle()->_column_blob(
										 (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)))
						.readUnsigned64());
		break;
	case SQLITE_NULL: break;
	}

	return 0;
}

double ResultCursor::toDouble(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER:
		return double(driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_FLOAT:
		return driver->getHandle()->_column_double((sqlite3_stmt *)result.get(), int(field));
		break;
	case SQLITE_TEXT:
		return StringView((const char *)driver->getHandle()->_column_text(
								  (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)))
				.readDouble()
				.get(0);
		break;
	case SQLITE_BLOB:
		return BytesView((const uint8_t *)driver->getHandle()->_column_blob(
								 (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)))
				.readFloat64();
		break;
	case SQLITE_NULL: break;
	}

	return 0.0;
}
bool ResultCursor::toBool(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER:
		return driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(), int(field)) != 0;
		break;
	case SQLITE_FLOAT:
		return driver->getHandle()->_column_double((sqlite3_stmt *)result.get(), int(field)) != 0.0;
		break;
	case SQLITE_TEXT: {
		StringView data((const char *)driver->getHandle()->_column_text(
								(sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		if (data == "1" || data == "true" || data == "TRUE") {
			return true;
		}
		break;
	}
	case SQLITE_BLOB: {
		BytesView data((const uint8_t *)driver->getHandle()->_column_blob(
							   (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field)));
		if (data.empty()) {
			return false;
		} else {
			return true;
		}
		break;
	}
	case SQLITE_NULL: break;
	}

	return false;
}
Value ResultCursor::toTypedData(size_t field) const {
	switch (driver->getHandle()->_column_type((sqlite3_stmt *)result.get(), int(field))) {
	case SQLITE_INTEGER:
		return Value(int64_t(
				driver->getHandle()->_column_int64((sqlite3_stmt *)result.get(), int(field))));
		break;
	case SQLITE_FLOAT:
		return Value(driver->getHandle()->_column_double((sqlite3_stmt *)result.get(), int(field)));
		break;
	case SQLITE_TEXT:
		return Value(StringView((const char *)driver->getHandle()->_column_text(
										(sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field))));
		break;
	case SQLITE_BLOB:
		return Value(BytesView((const uint8_t *)driver->getHandle()->_column_blob(
									   (sqlite3_stmt *)result.get(), int(field)),
				driver->getHandle()->_column_bytes((sqlite3_stmt *)result.get(), int(field))));
		break;
	case SQLITE_NULL: break;
	}

	return Value();
}

Value ResultCursor::toCustomData(size_t field, const FieldCustom *f) const {
	auto info = driver->getCustomFieldInfo(f->getDriverTypeName());
	if (!info) {
		return Value();
	}
	return info->readFromStorage(*f, *this, field);
}

int64_t ResultCursor::toId() const { return toInteger(0); }
StringView ResultCursor::getFieldName(size_t field) const {
	if (auto ptr = driver->getHandle()->_column_name((sqlite3_stmt *)result.get(), int(field))) {
		return StringView(ptr);
	}
	return StringView();
}
bool ResultCursor::isSuccess() const { return result.get() && statusIsSuccess(err); }
bool ResultCursor::isEmpty() const { return err != SQLITE_ROW; }
bool ResultCursor::isEnded() const { return err == SQLITE_DONE; }
size_t ResultCursor::getFieldsCount() const {
	return driver->getHandle()->_column_count((sqlite3_stmt *)result.get());
}
size_t ResultCursor::getAffectedRows() const {
	return driver->getHandle()->_changes((sqlite3 *)conn.get());
}
size_t ResultCursor::getRowsHint() const { return 0; }
Value ResultCursor::getInfo() const {
	return Value({
		stappler::pair("error", Value(err)),
		stappler::pair("status", Value(driver->getHandle()->_errstr(int(err)))),
		stappler::pair("desc", Value(driver->getHandle()->_errmsg((sqlite3 *)conn.get()))),
	});
}

bool ResultCursor::next() {
	if (err == SQLITE_ROW) {
		err = driver->getHandle()->step((sqlite3_stmt *)result.get());
		return err == SQLITE_ROW;
	}
	return false;
}

void ResultCursor::reset() {
	if (result.get()) {
		driver->getHandle()->reset((sqlite3_stmt *)result.get());
		err = driver->getHandle()->step((sqlite3_stmt *)result.get());
		result = Driver::Result(nullptr);
	}
}

void ResultCursor::clear() {
	if (result.get()) {
		driver->getHandle()->finalize((sqlite3_stmt *)result.get());
		result = Driver::Result(nullptr);
	}
}

int ResultCursor::getError() const { return err; }

} // namespace stappler::db::sqlite
