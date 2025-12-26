/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_DB_SQLITE_SPSQLITEDRIVERHANDLE_H_
#define CORE_DB_SQLITE_SPSQLITEDRIVERHANDLE_H_

#include "SPSqliteDriver.h"
#include "SPDso.h"
#include "sqlite3.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

struct DriverSym : AllocBase {
	static const DriverSym *getCurrent();

	DriverSym(StringView n, Dso &&d);
	DriverSym(StringView n);

	~DriverSym();

	explicit operator bool() const;

	DriverSym(DriverSym &&) = default;
	DriverSym &operator=(DriverSym &&) = default;

	int open(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) const;
	int close(sqlite3 *) const;
	int prepare(sqlite3 *db, const char *zSql, int nByte, unsigned int prepFlags,
			sqlite3_stmt **ppStmt, const char **pzTail) const;
	int step(sqlite3_stmt *pStmt) const;
	int reset(sqlite3_stmt *pStmt) const;
	int finalize(sqlite3_stmt *pStmt) const;

	decltype(&sqlite3_initialize) _initialize;

	decltype(&sqlite3_malloc) _malloc;
	decltype(&sqlite3_free) _free;

	decltype(&sqlite3_open_v2) _open_v2;
	decltype(&sqlite3_close) _close;
	decltype(&sqlite3_db_config) _db_config;
	decltype(&sqlite3_prepare_v3) _prepare_v3;
	decltype(&sqlite3_step) _step;
	decltype(&sqlite3_reset) _reset;
	decltype(&sqlite3_finalize) _finalize;

	decltype(&sqlite3_create_function_v2) _create_function_v2;
	decltype(&sqlite3_create_module) _create_module;
	decltype(&sqlite3_declare_vtab) _declare_vtab;

	decltype(&sqlite3_result_double) _result_double;
	decltype(&sqlite3_result_int) _result_int;
	decltype(&sqlite3_result_int64) _result_int64;
	decltype(&sqlite3_result_blob64) _result_blob64;
	decltype(&sqlite3_result_text64) _result_text64;
	decltype(&sqlite3_result_null) _result_null;

	decltype(&sqlite3_errstr) _errstr;
	decltype(&sqlite3_errmsg) _errmsg;

	decltype(&sqlite3_bind_blob) _bind_blob;
	decltype(&sqlite3_bind_text) _bind_text;
	decltype(&sqlite3_bind_int64) _bind_int64;

	decltype(&sqlite3_column_blob) _column_blob;
	decltype(&sqlite3_column_double) _column_double;
	decltype(&sqlite3_column_int) _column_int;
	decltype(&sqlite3_column_int64) _column_int64;
	decltype(&sqlite3_column_text) _column_text;
	decltype(&sqlite3_column_text16) _column_text16;
	decltype(&sqlite3_column_value) _column_value;
	decltype(&sqlite3_column_bytes) _column_bytes;
	decltype(&sqlite3_column_bytes16) _column_bytes16;
	decltype(&sqlite3_column_type) _column_type;
	decltype(&sqlite3_column_name) _column_name;
	decltype(&sqlite3_column_count) _column_count;
	decltype(&sqlite3_changes) _changes;

	decltype(&sqlite3_value_blob) _value_blob;
	decltype(&sqlite3_value_bytes) _value_bytes;
	decltype(&sqlite3_value_text) _value_text;
	decltype(&sqlite3_value_int) _value_int;
	decltype(&sqlite3_value_int64) _value_int64;

	decltype(&sqlite3_user_data) _user_data;
	decltype(&sqlite3_shutdown) _shutdown;

	StringView name;
	Dso ptr;
	uint32_t refCount = 1;
};

struct DriverHandle {
	sqlite3 *conn;
	const Driver *driver;
	DriverSym *sym;
	void *padding;
	pool_t *pool;
	StringView name;
	sqlite3_stmt *oidQuery = nullptr;
	sqlite3_stmt *wordsQuery = nullptr;
	int64_t userId = 0;
	Time ctime;
	std::mutex mutex;
};

struct TextQueryData : AllocBase {
	const FullTextQuery *query;
	Vector<uint64_t> pos;
	Vector<uint64_t> neg;
};

struct DriverLibStorage {
	std::mutex _driverMutex;
	std::map<std::string, DriverSym, std::less<void>> _driverLibs;

	static DriverLibStorage *getInstance();

	DriverSym *openSelf();

	DriverSym *openLib(StringView lib);
	void closeLib(DriverSym *sym);
};

static DriverLibStorage *s_libStorage = nullptr;
static thread_local const DriverSym *tl_currentSym = nullptr;

const DriverSym *DriverSym::getCurrent() { return tl_currentSym; }

DriverSym::DriverSym(StringView n, Dso &&d) : name(n) {
	_initialize = d.sym<decltype(_initialize)>("sqlite3_initialize");
	_malloc = d.sym<decltype(_malloc)>("sqlite3_malloc");
	_free = d.sym<decltype(_free)>("sqlite3_free");
	_open_v2 = d.sym<decltype(_open_v2)>("sqlite3_open_v2");
	_close = d.sym<decltype(_close)>("sqlite3_close");
	_db_config = d.sym<decltype(_db_config)>("sqlite3_db_config");
	_prepare_v3 = d.sym<decltype(_prepare_v3)>("sqlite3_prepare_v3");
	_step = d.sym<decltype(_step)>("sqlite3_step");
	_reset = d.sym<decltype(_reset)>("sqlite3_reset");
	_finalize = d.sym<decltype(_finalize)>("sqlite3_finalize");
	_create_function_v2 = d.sym<decltype(_create_function_v2)>("sqlite3_create_function_v2");
	_create_module = d.sym<decltype(_create_module)>("sqlite3_create_module");
	_declare_vtab = d.sym<decltype(_declare_vtab)>("sqlite3_declare_vtab");
	_result_double = d.sym<decltype(_result_double)>("sqlite3_result_double");
	_result_int = d.sym<decltype(_result_int)>("sqlite3_result_int");
	_result_int64 = d.sym<decltype(_result_int64)>("sqlite3_result_int64");
	_result_blob64 = d.sym<decltype(_result_blob64)>("sqlite3_result_blob64");
	_result_text64 = d.sym<decltype(_result_text64)>("sqlite3_result_text64");
	_result_null = d.sym<decltype(_result_null)>("sqlite3_result_null");
	_errstr = d.sym<decltype(_errstr)>("sqlite3_errstr");
	_errmsg = d.sym<decltype(_errmsg)>("sqlite3_errmsg");
	_bind_blob = d.sym<decltype(_bind_blob)>("sqlite3_bind_blob");
	_bind_text = d.sym<decltype(_bind_text)>("sqlite3_bind_text");
	_bind_int64 = d.sym<decltype(_bind_int64)>("sqlite3_bind_int64");
	_column_blob = d.sym<decltype(_column_blob)>("sqlite3_column_blob");
	_column_double = d.sym<decltype(_column_double)>("sqlite3_column_double");
	_column_int = d.sym<decltype(_column_int)>("sqlite3_column_int");
	_column_int64 = d.sym<decltype(_column_int64)>("sqlite3_column_int64");
	_column_text = d.sym<decltype(_column_text)>("sqlite3_column_text");
	_column_text16 = d.sym<decltype(_column_text16)>("sqlite3_column_text16");
	_column_value = d.sym<decltype(_column_value)>("sqlite3_column_value");
	_column_bytes = d.sym<decltype(_column_bytes)>("sqlite3_column_bytes");
	_column_bytes16 = d.sym<decltype(_column_bytes16)>("sqlite3_column_bytes16");
	_column_type = d.sym<decltype(_column_type)>("sqlite3_column_type");
	_column_name = d.sym<decltype(_column_name)>("sqlite3_column_name");
	_column_count = d.sym<decltype(_column_count)>("sqlite3_column_count");
	_value_blob = d.sym<decltype(_value_blob)>("sqlite3_value_blob");
	_value_bytes = d.sym<decltype(_value_bytes)>("sqlite3_value_bytes");
	_value_text = d.sym<decltype(_value_text)>("sqlite3_value_text");
	_value_int = d.sym<decltype(_value_int)>("sqlite3_value_int");
	_value_int64 = d.sym<decltype(_value_int64)>("sqlite3_value_int64");
	_changes = d.sym<decltype(_changes)>("sqlite3_changes");
	_user_data = d.sym<decltype(_user_data)>("sqlite3_user_data");
	_shutdown = d.sym<decltype(_shutdown)>("sqlite3_shutdown");

	ptr = move(d);
}

DriverSym::DriverSym(StringView n) : name(n) {
#if !defined(STAPPLER_SHARED) || defined(STAPPLER_SQLITE_LINKED)
	_initialize = &sqlite3_initialize;
	_malloc = &sqlite3_malloc;
	_free = &sqlite3_free;
	_open_v2 = &sqlite3_open_v2;
	_close = &sqlite3_close;
	_db_config = &sqlite3_db_config;
	_prepare_v3 = &sqlite3_prepare_v3;
	_step = &sqlite3_step;
	_reset = &sqlite3_reset;
	_finalize = &sqlite3_finalize;
	_create_function_v2 = &sqlite3_create_function_v2;
	_create_module = &sqlite3_create_module;
	_declare_vtab = &sqlite3_declare_vtab;
	_result_double = &sqlite3_result_double;
	_result_int = &sqlite3_result_int;
	_result_int64 = &sqlite3_result_int64;
	_result_blob64 = &sqlite3_result_blob64;
	_result_text64 = &sqlite3_result_text64;
	_result_null = &sqlite3_result_null;
	_errstr = &sqlite3_errstr;
	_errmsg = &sqlite3_errmsg;
	_bind_blob = &sqlite3_bind_blob;
	_bind_text = &sqlite3_bind_text;
	_bind_int64 = &sqlite3_bind_int64;
	_column_blob = &sqlite3_column_blob;
	_column_double = &sqlite3_column_double;
	_column_int = &sqlite3_column_int;
	_column_int64 = &sqlite3_column_int64;
	_column_text = &sqlite3_column_text;
	_column_text16 = &sqlite3_column_text16;
	_column_value = &sqlite3_column_value;
	_column_bytes = &sqlite3_column_bytes;
	_column_bytes16 = &sqlite3_column_bytes16;
	_column_type = &sqlite3_column_type;
	_column_name = &sqlite3_column_name;
	_column_count = &sqlite3_column_count;
	_value_blob = &sqlite3_value_blob;
	_value_bytes = &sqlite3_value_bytes;
	_value_text = &sqlite3_value_text;
	_value_int = &sqlite3_value_int;
	_value_int64 = &sqlite3_value_int64;
	_changes = &sqlite3_changes;
	_user_data = &sqlite3_user_data;
	_shutdown = &sqlite3_shutdown;
#endif
}

DriverSym::~DriverSym() { }

DriverSym::operator bool() const {
	void **begin = (void **)&this->_initialize;
	void **end = (void **)&this->_shutdown + 1;
	while (begin != end) {
		if (*begin == nullptr) {
			return false;
		}
		++begin;
	}
	return true;
}

int DriverSym::open(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _open_v2(filename, ppDb, flags, zVfs);
	tl_currentSym = prev;
	return ret;
}

int DriverSym::close(sqlite3 *db) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _close(db);
	tl_currentSym = prev;
	return ret;
}

int DriverSym::prepare(sqlite3 *db, const char *zSql, int nByte, unsigned int prepFlags,
		sqlite3_stmt **ppStmt, const char **pzTail) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _prepare_v3(db, zSql, nByte, prepFlags, ppStmt, pzTail);
	tl_currentSym = prev;
	return ret;
}

int DriverSym::step(sqlite3_stmt *pStmt) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _step(pStmt);
	tl_currentSym = prev;
	return ret;
}

int DriverSym::reset(sqlite3_stmt *pStmt) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _reset(pStmt);
	tl_currentSym = prev;
	return ret;
}

int DriverSym::finalize(sqlite3_stmt *pStmt) const {
	auto prev = tl_currentSym;
	tl_currentSym = this;
	auto ret = _finalize(pStmt);
	tl_currentSym = prev;
	return ret;
}

DriverLibStorage *DriverLibStorage::getInstance() {
	if (!s_libStorage) {
		s_libStorage = new DriverLibStorage;
	}
	return s_libStorage;
}

DriverSym *DriverLibStorage::openSelf() {
#if !defined(STAPPLER_SHARED) || defined(STAPPLER_SQLITE_LINKED)
	return openLib(StringView());
#else
	std::unique_lock<std::mutex> lock(_driverMutex);

	std::string target;
	auto it = _driverLibs.find(target);
	if (it != _driverLibs.end()) {
		++it->second.refCount;
		return &it->second;
	}

	if (auto d = Dso(StringView(), DsoFlags::Self)) {
		if (d.sym("sqlite3_initialize")) {
			DriverSym syms(target, move(d));
			if (syms) {
				auto ret = _driverLibs.emplace(target, move(syms)).first;
				ret->second.name = ret->first;
				return &ret->second;
			}
		}
	}

	return nullptr;
#endif
}

DriverSym *DriverLibStorage::openLib(StringView lib) {
	std::unique_lock<std::mutex> lock(_driverMutex);

#if STAPPLER_SHARED && !defined(STAPPLER_SQLITE_LINKED)
	auto target = lib.str<stappler::memory::StandartInterface>();
	auto it = _driverLibs.find(target);
	if (it != _driverLibs.end()) {
		++it->second.refCount;
		return &it->second;
	}

	if (auto d = Dso(target)) {
		DriverSym syms(target, move(d));
		if (syms) {
			auto ret = _driverLibs.emplace(target, move(syms)).first;
			ret->second.name = ret->first;
			return &ret->second;
		}
	}
#else
	auto it = _driverLibs.find(StringView());
	if (it != _driverLibs.end()) {
		++it->second.refCount;
		return &it->second;
	} else {
		DriverSym syms(lib);
		if (syms) {
			auto ret = _driverLibs.emplace(std::string(), move(syms)).first;
			ret->second.name = ret->first;
			return &ret->second;
		}
	}
#endif
	return nullptr;
}

void DriverLibStorage::closeLib(DriverSym *sym) {
	std::unique_lock<std::mutex> lock(_driverMutex);
	if (sym->refCount == 1) {
		_driverLibs.erase(sym->name.str<stappler::memory::StandartInterface>());
	} else {
		--sym->refCount;
	}
}

static StringView Driver_exec(const DriverSym *sym, pool_t *p, sqlite3 *db, StringView query) {
	sqlite3_stmt *stmt = nullptr;
	auto err = sym->prepare(db, query.data(), int(query.size()), 0, &stmt, nullptr);
	if (err != SQLITE_OK) {
		log::source().error("sqlite::Driver", err, ": ", sym->_errstr(int(err)), ": ",
				sym->_errmsg(db), ":\n", query);
		return StringView();
	}

	err = sym->step(stmt);
	if (err != SQLITE_ROW) {
		if (err < 100) {
			log::source().error("sqlite::Driver", err, ": ", sym->_errstr(int(err)), ": ",
					sym->_errmsg(db), ":\n", query);
		}
		sym->finalize(stmt);
		return StringView();
	}

	if (p) {
		StringView result((const char *)sym->_column_text(stmt, 0),
				size_t(sym->_column_bytes(stmt, 0)));
		result = result.pdup(p);
		sym->finalize(stmt);
		return result;
	}

	sym->finalize(stmt);
	return StringView();
}

} // namespace stappler::db::sqlite

#endif /* CORE_DB_SQLITE_SPSQLITEDRIVERHANDLE_H_ */
