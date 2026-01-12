/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPPqDriver.h"
#include "SPPqHandle.h"
#include "SPSqlHandle.h"
#include "SPDso.h"
#include "SPDbFieldExtensions.h"

#include <sprt/runtime/mem/userdata.h>

namespace STAPPLER_VERSIONIZED stappler::db::pq {

constexpr static auto LIST_DB_TYPES = "SELECT oid, typname, typcategory FROM pg_type WHERE typcategory = 'B'"
		" OR typcategory = 'D' OR typcategory = 'I' OR typcategory = 'N' OR typcategory = 'S' OR typcategory = 'U';";

enum ConnStatusType {
	CONNECTION_OK,
	CONNECTION_BAD,
};

enum ExecStatusType {
	PGRES_EMPTY_QUERY = 0,
	PGRES_COMMAND_OK,
	PGRES_TUPLES_OK,
	PGRES_COPY_OUT,
	PGRES_COPY_IN,
	PGRES_BAD_RESPONSE,
	PGRES_NONFATAL_ERROR,
	PGRES_FATAL_ERROR,
	PGRES_COPY_BOTH,
	PGRES_SINGLE_TUPLE
};

enum PGTransactionStatusType {
	PQTRANS_IDLE,
	PQTRANS_ACTIVE,
	PQTRANS_INTRANS,
	PQTRANS_INERROR,
	PQTRANS_UNKNOWN
};

struct pgNotify {
	char *relname; /* notification condition name */
	int be_pid; /* process ID of notifying server process */
	char *extra; /* notification parameter */
	/* Fields below here are private to libpq; apps should not use 'em */
	pgNotify *next; /* list link */
};

struct DriverSym : AllocBase {
	using PQnoticeProcessor = void (*)(void *arg, const char *message);
	using PQresultStatusType = ExecStatusType (*)(const void *res);
	using PQconnectdbParamsType = void *(*)(const char *const *keywords, const char *const *values,
			int expand_dbname);
	using PQfinishType = void (*)(void *conn);
	using PQfformatType = int (*)(const void *res, int field_num);
	using PQgetisnullType = int (*)(const void *res, int tup_num, int field_num);
	using PQgetvalueType = char *(*)(const void *res, int tup_num, int field_num);
	using PQgetlengthType = int (*)(const void *res, int tup_num, int field_num);
	using PQfnameType = char *(*)(const void *res, int field_num);
	using PQftypeType = unsigned int (*)(const void *res, int field_num);
	using PQntuplesType = int (*)(const void *res);
	using PQnfieldsType = int (*)(const void *res);
	using PQcmdTuplesType = char *(*)(void *res);
	using PQresStatusType = char *(*)(ExecStatusType status);
	using PQresultErrorMessageType = char *(*)(const void *res);
	using PQclearType = void (*)(void *res);
	using PQexecType = void *(*)(void *conn, const char *query);
	using PQexecParamsType = void *(*)(void *conn, const char *command, int nParams,
			const void *paramTypes, const char *const *paramValues, const int *paramLengths,
			const int *paramFormats, int resultFormat);
	using PQsendQueryType = int (*)(void *conn, const char *query);
	using PQstatusType = ConnStatusType (*)(void *conn);
	using PQerrorMessageType = char *(*)(const void *conn);
	using PQresetType = void (*)(void *conn);
	using PQtransactionStatusType = PGTransactionStatusType (*)(void *conn);
	using PQsetnonblockingType = int (*)(void *conn, int arg);
	using PQsocketType = int (*)(const void *conn);
	using PQconsumeInputType = int (*)(void *conn);
	using PQnotifiesType = pgNotify *(*)(void *conn);
	using PQfreememType = void (*)(void *ptr);
	using PQisBusyType = int (*)(void *conn);
	using PQgetResultType = void *(*)(void *conn);
	using PQsetNoticeProcessorType = void (*)(void *conn, PQnoticeProcessor, void *);

	DriverSym(StringView n, Dso &&d) : name(n), ptr(move(d)) {
		this->PQresultStatus = ptr.sym<DriverSym::PQresultStatusType>("PQresultStatus");
		this->PQconnectdbParams = ptr.sym<DriverSym::PQconnectdbParamsType>("PQconnectdbParams");
		this->PQfinish = ptr.sym<DriverSym::PQfinishType>("PQfinish");
		this->PQfformat = ptr.sym<DriverSym::PQfformatType>("PQfformat");
		this->PQgetisnull = ptr.sym<DriverSym::PQgetisnullType>("PQgetisnull");
		this->PQgetvalue = ptr.sym<DriverSym::PQgetvalueType>("PQgetvalue");
		this->PQgetlength = ptr.sym<DriverSym::PQgetlengthType>("PQgetlength");
		this->PQfname = ptr.sym<DriverSym::PQfnameType>("PQfname");
		this->PQftype = ptr.sym<DriverSym::PQftypeType>("PQftype");
		this->PQntuples = ptr.sym<DriverSym::PQntuplesType>("PQntuples");
		this->PQnfields = ptr.sym<DriverSym::PQnfieldsType>("PQnfields");
		this->PQcmdTuples = ptr.sym<DriverSym::PQcmdTuplesType>("PQcmdTuples");
		this->PQresStatus = ptr.sym<DriverSym::PQresStatusType>("PQresStatus");
		this->PQresultErrorMessage =
				ptr.sym<DriverSym::PQresultErrorMessageType>("PQresultErrorMessage");
		this->PQclear = ptr.sym<DriverSym::PQclearType>("PQclear");
		this->PQexec = ptr.sym<DriverSym::PQexecType>("PQexec");
		this->PQexecParams = ptr.sym<DriverSym::PQexecParamsType>("PQexecParams");
		this->PQsendQuery = ptr.sym<DriverSym::PQsendQueryType>("PQsendQuery");
		this->PQstatus = ptr.sym<DriverSym::PQstatusType>("PQstatus");
		this->PQerrorMessage = ptr.sym<DriverSym::PQerrorMessageType>("PQerrorMessage");
		this->PQreset = ptr.sym<DriverSym::PQresetType>("PQreset");
		this->PQtransactionStatus =
				ptr.sym<DriverSym::PQtransactionStatusType>("PQtransactionStatus");
		this->PQsetnonblocking = ptr.sym<DriverSym::PQsetnonblockingType>("PQsetnonblocking");
		this->PQsocket = ptr.sym<DriverSym::PQsocketType>("PQsocket");
		this->PQconsumeInput = ptr.sym<DriverSym::PQconsumeInputType>("PQconsumeInput");
		this->PQnotifies = ptr.sym<DriverSym::PQnotifiesType>("PQnotifies");
		this->PQfreemem = ptr.sym<DriverSym::PQfreememType>("PQfreemem");
		this->PQisBusy = ptr.sym<DriverSym::PQisBusyType>("PQisBusy");
		this->PQgetResult = ptr.sym<DriverSym::PQgetResultType>("PQgetResult");
		this->PQsetNoticeProcessor =
				ptr.sym<DriverSym::PQsetNoticeProcessorType>("PQsetNoticeProcessor");
	}

	~DriverSym() { }

	explicit operator bool() const {
		void **begin = (void **)&this->PQconnectdbParams;
		void **end = (void **)&this->PQsetNoticeProcessor + 1;
		while (begin != end) {
			if (*begin == nullptr) {
				return false;
			}
			++begin;
		}
		return true;
	}

	DriverSym(DriverSym &&) = default;
	DriverSym &operator=(DriverSym &&) = default;

	StringView name;
	Dso ptr;
	PQconnectdbParamsType PQconnectdbParams = nullptr;
	PQfinishType PQfinish = nullptr;
	PQresultStatusType PQresultStatus = nullptr;
	PQfformatType PQfformat = nullptr;
	PQgetisnullType PQgetisnull = nullptr;
	PQgetvalueType PQgetvalue = nullptr;
	PQgetlengthType PQgetlength = nullptr;
	PQfnameType PQfname = nullptr;
	PQftypeType PQftype = nullptr;
	PQntuplesType PQntuples = nullptr;
	PQnfieldsType PQnfields = nullptr;
	PQcmdTuplesType PQcmdTuples = nullptr;
	PQresStatusType PQresStatus = nullptr;
	PQresultErrorMessageType PQresultErrorMessage = nullptr;
	PQclearType PQclear = nullptr;
	PQexecType PQexec = nullptr;
	PQexecParamsType PQexecParams = nullptr;
	PQsendQueryType PQsendQuery = nullptr;
	PQstatusType PQstatus = nullptr;
	PQerrorMessageType PQerrorMessage = nullptr;
	PQresetType PQreset = nullptr;
	PQtransactionStatusType PQtransactionStatus = nullptr;
	PQsetnonblockingType PQsetnonblocking = nullptr;
	PQsocketType PQsocket = nullptr;
	PQconsumeInputType PQconsumeInput = nullptr;
	PQnotifiesType PQnotifies = nullptr;
	PQfreememType PQfreemem = nullptr;
	PQisBusyType PQisBusy = nullptr;
	PQgetResultType PQgetResult = nullptr;
	PQsetNoticeProcessorType PQsetNoticeProcessor = nullptr;
	uint32_t refCount = 1;
};

struct DriverHandle {
	void *conn;
	const Driver *driver;
	Time ctime;
	pool_t *pool;
};

struct DriverLibStorage {
	std::mutex s_driverMutex;
	std::map<std::string, DriverSym, std::less<void>> s_driverLibs;

	static DriverLibStorage *getInstance();

	DriverSym *openLib(StringView lib) {
		std::unique_lock<std::mutex> lock(s_driverMutex);

		auto target = lib.str<stappler::memory::StandartInterface>();
		auto it = s_driverLibs.find(target);
		if (it != s_driverLibs.end()) {
			++it->second.refCount;
			return &it->second;
		}

		if (auto d = Dso(target)) {
			DriverSym syms(target, move(d));
			if (syms) {
				auto ret = s_driverLibs.emplace(target, move(syms)).first;
				ret->second.name = ret->first;
				return &ret->second;
			}
		}

		return nullptr;
	}

	void closeLib(DriverSym *sym) {
		std::unique_lock<std::mutex> lock(s_driverMutex);
		if (sym->refCount == 1) {
			s_driverLibs.erase(sym->name.str<stappler::memory::StandartInterface>());
		} else {
			--sym->refCount;
		}
	}
};

static DriverLibStorage *s_libStorage;
SPUNUSED static String pg_numeric_to_string(BytesViewNetwork r);

DriverLibStorage *DriverLibStorage::getInstance() {
	if (!s_libStorage) {
		s_libStorage = new DriverLibStorage;
	}
	return s_libStorage;
}

void Driver_noticeMessage(void *arg, const char *message) {
	// std::cout << "Notice: " << message << "\n";
	// Silence libpq notices
}

static void Driver_insert_sorted(Vector<sprt::pair<uint32_t, BackendInterface::StorageType>> &vec,
		uint32_t oid, BackendInterface::StorageType type) {
	auto it = std::upper_bound(vec.begin(), vec.end(), oid,
			[](uint32_t l, const sprt::pair<uint32_t, BackendInterface::StorageType> &r) -> bool {
		return l < r.first;
	});
	vec.emplace(it, oid, type);
}

static void Driver_insert_sorted(Vector<sprt::pair<uint32_t, String>> &vec, uint32_t oid,
		StringView type) {
	auto it = std::upper_bound(vec.begin(), vec.end(), oid,
			[](uint32_t l, const sprt::pair<uint32_t, String> &r) -> bool { return l < r.first; });
	vec.emplace(it, oid, type.str<Interface>());
}

bool Driver::init(Handle handle, const Vector<StringView> &dbs) {
	if (_init) {
		return true;
	}

	auto conn = getConnection(handle);
	Vector<StringView> toCreate(dbs);
	if (!dbs.empty()) {
		auto res = exec(conn, "SELECT datname FROM pg_database;");

		for (size_t i = 0; i < getNTuples(res); ++i) {
			auto name = StringView(getValue(res, i, 0), getLength(res, i, 0));
			auto it = std::find(toCreate.begin(), toCreate.end(), name);
			if (it != toCreate.end()) {
				toCreate.erase(it);
			}
		}

		clearResult(res);

		if (!toCreate.empty()) {
			for (auto &it : toCreate) {
				StringStream query;
				query << "CREATE DATABASE " << it << ";";
				auto q = query.data();
				auto res = exec(conn, q);
				clearResult(res);
			}
		}
	}

	ResultCursor result(this, exec(conn, LIST_DB_TYPES));

	db::sql::Result res(&result);

	memory::context<memory::pool_t *> ctx(_storageTypes.get_allocator(),
			memory::context<memory::pool_t *>::conditional);

	for (auto it : res) {
		auto tid = it.toInteger(0);
		auto tname = it.at(1);
		if (tname == "bool") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Bool);
		} else if (tname == "bytea") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::Bytes);
		} else if (tname == "char") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Char);
		} else if (tname == "int8") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Int8);
		} else if (tname == "int4") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Int4);
		} else if (tname == "int2") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Int2);
		} else if (tname == "float4") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::Float4);
		} else if (tname == "float8") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::Float8);
		} else if (tname == "varchar") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::VarChar);
		} else if (tname == "text") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Text);
		} else if (tname == "name") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid), BackendInterface::StorageType::Text);
		} else if (tname == "numeric") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::Numeric);
		} else if (tname == "tsvector") {
			Driver_insert_sorted(_storageTypes, uint32_t(tid),
					BackendInterface::StorageType::TsVector);
		} else {
			Driver_insert_sorted(_customTypes, uint32_t(tid), tname);
		}
	}

	_init = true;
	return true;
}

void Driver::performWithStorage(Handle handle,
		const Callback<void(const db::Adapter &)> &cb) const {
	auto targetPool = pool::acquire();

	db::pq::Handle h(this, handle);
	db::Adapter storage(&h, _application);
	pool::userdata_set((void *)&h, config::STORAGE_INTERFACE_KEY.data(), nullptr, targetPool);

	cb(storage);

	auto stack = sprt::memory::pool::get<db::Transaction::Stack>(targetPool,
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
	memory::perform_conditional([&] { ret = new (pool) db::pq::Handle(this, handle); }, pool);
	return ret;
}

Driver::Handle Driver::connect(const Map<StringView, StringView> &params) const {
	auto p = pool::create(pool::acquire());
	Driver::Handle rec;
	memory::perform_conditional([&] {
		Vector<const char *> keywords;
		keywords.reserve(params.size());
		Vector<const char *> values;
		values.reserve(params.size());

		for (auto &it : params) {
			if (it.first == "host" || it.first == "hostaddr" || it.first == "port"
					|| it.first == "dbname" || it.first == "user" || it.first == "password"
					|| it.first == "passfile" || it.first == "channel_binding"
					|| it.first == "connect_timeout" || it.first == "client_encoding"
					|| it.first == "options" || it.first == "application_name"
					|| it.first == "fallback_application_name" || it.first == "keepalives"
					|| it.first == "keepalives_idle" || it.first == "keepalives_interval"
					|| it.first == "keepalives_count" || it.first == "tcp_user_timeout"
					|| it.first == "replication" || it.first == "gssencmode"
					|| it.first == "sslmode" || it.first == "requiressl"
					|| it.first == "sslcompression" || it.first == "sslcert" || it.first == "sslkey"
					|| it.first == "sslpassword" || it.first == "sslrootcert"
					|| it.first == "sslcrl" || it.first == "requirepeer"
					|| it.first == "ssl_min_protocol_version"
					|| it.first == "ssl_max_protocol_version" || it.first == "krbsrvname"
					|| it.first == "gsslib" || it.first == "service"
					|| it.first == "target_session_attrs") {
				keywords.emplace_back(it.first.data());
				values.emplace_back(it.second.data());
			} else if (it.first != "driver" && it.first == "nmin" && it.first == "nkeep"
					&& it.first == "nmax" && it.first == "exptime" && it.first == "persistent") {
				log::source().error("pq::Driver", "unknown connection parameter: ", it.first, "=",
						it.second);
			}
		}

		keywords.emplace_back(nullptr);
		values.emplace_back(nullptr);

		rec = doConnect(keywords.data(), values.data(), 0);
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

bool Driver::isValid(Handle handle) const {
	if (!handle.get()) {
		return false;
	}

	auto conn = getConnection(handle);
	if (conn.get()) {
		return isValid(conn);
	}
	return false;
}

bool Driver::isValid(Connection conn) const {
	if (_handle->PQstatus(conn.get()) != CONNECTION_OK) {
		_handle->PQreset(conn.get());
		if (_handle->PQstatus(conn.get()) != CONNECTION_OK) {
			return false;
		}
	}
	return true;
}

bool Driver::isIdle(Connection conn) const {
	return getTransactionStatus(conn) == TransactionStatus::Idle;
}

Time Driver::getConnectionTime(Handle handle) const {
	auto db = (DriverHandle *)handle.get();
	return db->ctime;
}

int Driver::listenForNotifications(Handle handle) const {
	auto conn = getConnection(handle).get();

	auto query = toString("LISTEN ", config::BROADCAST_CHANNEL_NAME, ";");
	int querySent = _handle->PQsendQuery(conn, query.data());
	if (querySent == 0) {
		log::source().error("Postgres", _handle->PQerrorMessage(conn));
		return -1;
	}

	if (_handle->PQsetnonblocking(conn, 1) == -1) {
		log::source().error("Postgres", _handle->PQerrorMessage(conn));
		return -1;
	} else {
		return _handle->PQsocket(conn);
	}
}

bool Driver::consumeNotifications(Handle handle, const Callback<void(StringView)> &cb) const {
	auto conn = getConnection(handle).get();

	auto connStatusType = _handle->PQstatus(conn);
	if (connStatusType == CONNECTION_BAD) {
		return false;
	}

	int rc = _handle->PQconsumeInput(conn);
	if (rc == 0) {
		log::source().error("Postgres", _handle->PQerrorMessage(conn));
		return false;
	}
	pgNotify *notify;
	while ((notify = _handle->PQnotifies(conn)) != NULL) {
		cb(notify->relname);
		_handle->PQfreemem(notify);
	}
	if (_handle->PQisBusy(conn) == 0) {
		void *result;
		while ((result = _handle->PQgetResult(conn)) != NULL) { _handle->PQclear(result); }
	}
	return true;
}

Driver::TransactionStatus Driver::getTransactionStatus(Connection conn) const {
	auto ret = _handle->PQtransactionStatus(conn.get());
	switch (ret) {
	case PQTRANS_IDLE: return TransactionStatus::Idle; break;
	case PQTRANS_ACTIVE: return TransactionStatus::Active; break;
	case PQTRANS_INTRANS: return TransactionStatus::InTrans; break;
	case PQTRANS_INERROR: return TransactionStatus::InError; break;
	case PQTRANS_UNKNOWN: return TransactionStatus::Unknown; break;
	}
	return TransactionStatus::Unknown;
}

Driver::Status Driver::getStatus(Result res) const {
	auto err = _handle->PQresultStatus(res.get());
	switch (err) {
	case PGRES_EMPTY_QUERY: return Driver::Status::Empty; break;
	case PGRES_COMMAND_OK: return Driver::Status::CommandOk; break;
	case PGRES_TUPLES_OK: return Driver::Status::TuplesOk; break;
	case PGRES_COPY_OUT: return Driver::Status::CopyOut; break;
	case PGRES_COPY_IN: return Driver::Status::CopyIn; break;
	case PGRES_BAD_RESPONSE: return Driver::Status::BadResponse; break;
	case PGRES_NONFATAL_ERROR: return Driver::Status::NonfatalError; break;
	case PGRES_FATAL_ERROR: return Driver::Status::FatalError; break;
	case PGRES_COPY_BOTH: return Driver::Status::CopyBoth; break;
	case PGRES_SINGLE_TUPLE: return Driver::Status::SingleTuple; break;
	default: break;
	}
	return Driver::Status::Empty;
}

bool Driver::isBinaryFormat(Result res, size_t field) const {
	return _handle->PQfformat(res.get(), int(field)) != 0;
}

bool Driver::isNull(Result res, size_t row, size_t field) const {
	return _handle->PQgetisnull(res.get(), int(row), int(field));
}

char *Driver::getValue(Result res, size_t row, size_t field) const {
	return _handle->PQgetvalue(res.get(), int(row), int(field));
}

size_t Driver::getLength(Result res, size_t row, size_t field) const {
	return size_t(_handle->PQgetlength(res.get(), int(row), int(field)));
}

char *Driver::getName(Result res, size_t field) const {
	return _handle->PQfname(res.get(), int(field));
}

unsigned int Driver::getType(Result res, size_t field) const {
	return _handle->PQftype(res.get(), int(field));
}

size_t Driver::getNTuples(Result res) const { return size_t(_handle->PQntuples(res.get())); }

size_t Driver::getNFields(Result res) const { return size_t(_handle->PQnfields(res.get())); }

size_t Driver::getCmdTuples(Result res) const {
	return stappler::StringToNumber<size_t>(_handle->PQcmdTuples(res.get()));
}

char *Driver::getStatusMessage(Status st) const {
	switch (st) {
	case Status::Empty: return _handle->PQresStatus(PGRES_EMPTY_QUERY); break;
	case Status::CommandOk: return _handle->PQresStatus(PGRES_COMMAND_OK); break;
	case Status::TuplesOk: return _handle->PQresStatus(PGRES_TUPLES_OK); break;
	case Status::CopyOut: return _handle->PQresStatus(PGRES_COPY_OUT); break;
	case Status::CopyIn: return _handle->PQresStatus(PGRES_COPY_IN); break;
	case Status::BadResponse: return _handle->PQresStatus(PGRES_BAD_RESPONSE); break;
	case Status::NonfatalError: return _handle->PQresStatus(PGRES_NONFATAL_ERROR); break;
	case Status::FatalError: return _handle->PQresStatus(PGRES_FATAL_ERROR); break;
	case Status::CopyBoth: return _handle->PQresStatus(PGRES_COPY_BOTH); break;
	case Status::SingleTuple: return _handle->PQresStatus(PGRES_SINGLE_TUPLE); break;
	}
	return nullptr;
}

char *Driver::getResultErrorMessage(Result res) const {
	return _handle->PQresultErrorMessage(res.get());
}

void Driver::clearResult(Result res) const {
	if (_dbCtrl) {
		_dbCtrl(true);
	}
	_handle->PQclear(res.get());
}

Driver::Result Driver::exec(Connection conn, const char *query) const {
	if (_dbCtrl) {
		_dbCtrl(false);
	}
	return Driver::Result(_handle->PQexec(conn.get(), query));
}

Driver::Result Driver::exec(Connection conn, const char *command, int nParams,
		const char *const *paramValues, const int *paramLengths, const int *paramFormats,
		int resultFormat) const {
	if (_dbCtrl) {
		_dbCtrl(false);
	}
	return Driver::Result(_handle->PQexecParams(conn.get(), command, nParams, nullptr, paramValues,
			paramLengths, paramFormats, resultFormat));
}

BackendInterface::StorageType Driver::getTypeById(uint32_t oid) const {
	auto it = std::lower_bound(_storageTypes.begin(), _storageTypes.end(), oid,
			[](const sprt::pair<uint32_t, BackendInterface::StorageType> &l, uint32_t r) -> bool {
		return l.first < r;
	});
	if (it != _storageTypes.end() && it->first == oid) {
		return it->second;
	}
	return BackendInterface::StorageType::Unknown;
}

StringView Driver::getTypeNameById(uint32_t oid) const {
	auto it = std::lower_bound(_customTypes.begin(), _customTypes.end(), oid,
			[](const sprt::pair<uint32_t, String> &l, uint32_t r) -> bool { return l.first < r; });
	if (it != _customTypes.end() && it->first == oid) {
		return it->second;
	}
	return StringView();
}

Driver::~Driver() { }

Driver *Driver::open(pool_t *pool, ApplicationInterface *app, StringView path,
		const void *external) {
	auto ret = new (pool::acquire()) Driver(pool, app, path, external);
	if (ret->_handle) {
		return ret;
	}
	return nullptr;
}

Driver::Driver(pool_t *pool, ApplicationInterface *app, StringView path, const void *external)
: sql::Driver(pool, app), _external(external) {
	DriverSym *l = nullptr;
	if (!path.empty() && path != "pgsql") {
		l = DriverLibStorage::getInstance()->openLib(path);
	} else {
		StringView name = path;
		if (path.empty() || path == "pgsql") {
#if WIN32
			name = StringView("libpq.dll");
#else
			name = StringView("libpq.so");
#endif
		}

		l = DriverLibStorage::getInstance()->openLib(name);

		if (!l) {
#if WIN32
			name = StringView("libpq.5.dll");
#else
			name = StringView("libpq.so.5");
#endif
			l = DriverLibStorage::getInstance()->openLib(name);
		}
	}

	if (l) {
		_handle = l;

		pool::cleanup_register(pool, [this] {
			DriverLibStorage::getInstance()->closeLib(_handle);
			_handle = nullptr;
		});

		auto it = _customFields.emplace(FieldIntArray::FIELD_NAME);
		if (!FieldIntArray::registerForPostgres(it.first->second)) {
			_customFields.erase(it.first);
		}

		it = _customFields.emplace(FieldBigIntArray::FIELD_NAME);
		if (!FieldBigIntArray::registerForPostgres(it.first->second)) {
			_customFields.erase(it.first);
		}

		it = _customFields.emplace(FieldPoint::FIELD_NAME);
		if (!FieldPoint::registerForPostgres(it.first->second)) {
			_customFields.erase(it.first);
		}

		it = _customFields.emplace(FieldTextArray::FIELD_NAME);
		if (!FieldTextArray::registerForPostgres(it.first->second)) {
			_customFields.erase(it.first);
		}
	}
}

ResultCursor::ResultCursor(const Driver *d, Driver::Result res) : driver(d), result(res) {
	err = result.get() ? driver->getStatus(result) : Driver::Status::FatalError;
	nrows = driver->getNTuples(result);
}

ResultCursor::~ResultCursor() { clear(); }

bool ResultCursor::isBinaryFormat(size_t field) const {
	return driver->isBinaryFormat(result, field) != 0;
}

bool ResultCursor::isNull(size_t field) const { return driver->isNull(result, currentRow, field); }

StringView ResultCursor::toString(size_t field) const {
	if (isBinaryFormat(field)) {
		auto t = driver->getType(result, field);
		auto s = driver->getTypeById(t);
		switch (s) {
		case BackendInterface::StorageType::Unknown:
			driver->getApplicationInterface()->error("DB", "Unknown type conversion",
					Value(driver->getTypeNameById(t)));
			return StringView();
			break;
		case BackendInterface::StorageType::TsVector: return StringView(); break;
		case BackendInterface::StorageType::Bool:
			return StringView(toString(toBool(field))).pdup();
			break;
		case BackendInterface::StorageType::Char: break;
		case BackendInterface::StorageType::Float4:
		case BackendInterface::StorageType::Float8:
			return StringView(toString(toDouble(field))).pdup();
			break;
		case BackendInterface::StorageType::Int2:
		case BackendInterface::StorageType::Int4:
		case BackendInterface::StorageType::Int8:
			return StringView(toString(toInteger(field))).pdup();
			break;
		case BackendInterface::StorageType::Text:
		case BackendInterface::StorageType::VarChar:
			return StringView(driver->getValue(result, currentRow, field),
					driver->getLength(result, currentRow, field));
			break;
		case BackendInterface::StorageType::Numeric: {
			stappler::BytesViewNetwork r(
					(const uint8_t *)driver->getValue(result, currentRow, field),
					driver->getLength(result, currentRow, field));
			auto str = pg_numeric_to_string(r);
			return StringView(str).pdup();
			break;
		}
		case BackendInterface::StorageType::Bytes:
			return StringView(base16::encode<Interface>(toBytes(field))).pdup();
			break;
		}
		return StringView();
	} else {
		return StringView(driver->getValue(result, currentRow, field),
				driver->getLength(result, currentRow, field));
	}
}

BytesView ResultCursor::toBytes(size_t field) const {
	if (isBinaryFormat(field)) {
		return BytesView((uint8_t *)driver->getValue(result, currentRow, field),
				driver->getLength(result, currentRow, field));
	} else {
		auto val = driver->getValue(result, currentRow, field);
		auto len = driver->getLength(result, currentRow, field);
		if (len > 2 && memcmp(val, "\\x", 2) == 0) {
			auto d = new (std::nothrow) Bytes(
					stappler::base16::decode<Interface>(stappler::CoderSource(val + 2, len - 2)));
			return BytesView(*d);
		}
		return BytesView((uint8_t *)val, len);
	}
}
int64_t ResultCursor::toInteger(size_t field) const {
	if (isBinaryFormat(field)) {
		stappler::BytesViewNetwork r((const uint8_t *)driver->getValue(result, currentRow, field),
				driver->getLength(result, currentRow, field));
		switch (r.size()) {
		case 1: return r.readUnsigned(); break;
		case 2: return r.readUnsigned16(); break;
		case 4: return r.readUnsigned32(); break;
		case 8: return r.readUnsigned64(); break;
		default: break;
		}
		return 0;
	} else {
		auto val = driver->getValue(result, currentRow, field);
		return stappler::StringToNumber<int64_t>(val, nullptr, 0);
	}
}
double ResultCursor::toDouble(size_t field) const {
	if (isBinaryFormat(field)) {
		stappler::BytesViewNetwork r((const uint8_t *)driver->getValue(result, currentRow, field),
				driver->getLength(result, currentRow, field));
		switch (r.size()) {
		case 2: return r.readFloat16(); break;
		case 4: return r.readFloat32(); break;
		case 8: return r.readFloat64(); break;
		default: break;
		}
		return 0;
	} else {
		auto val = driver->getValue(result, currentRow, field);
		return stappler::StringToNumber<double>(val, nullptr, 0);
	}
}
bool ResultCursor::toBool(size_t field) const {
	auto val = driver->getValue(result, currentRow, field);
	if (!isBinaryFormat(field)) {
		if (val) {
			if (*val == 'T' || *val == 't' || *val == 'y') {
				return true;
			}
		}
		return false;
	} else {
		return val && *val != 0;
	}
}
Value ResultCursor::toTypedData(size_t field) const {
	auto t = driver->getType(result, field);
	auto s = driver->getTypeById(t);
	switch (s) {
	case BackendInterface::StorageType::Unknown:
		driver->getApplicationInterface()->error("DB", "Unknown type conversion",
				Value(driver->getTypeNameById(t)));
		return Value();
		break;
	case BackendInterface::StorageType::TsVector: return Value(); break;
	case BackendInterface::StorageType::Bool: return Value(toBool(field)); break;
	case BackendInterface::StorageType::Char: break;
	case BackendInterface::StorageType::Float4:
	case BackendInterface::StorageType::Float8: return Value(toDouble(field)); break;
	case BackendInterface::StorageType::Int2:
	case BackendInterface::StorageType::Int4:
	case BackendInterface::StorageType::Int8: return Value(toInteger(field)); break;
	case BackendInterface::StorageType::Text:
	case BackendInterface::StorageType::VarChar: return Value(toString(field)); break;
	case BackendInterface::StorageType::Numeric: {
		stappler::BytesViewNetwork r((const uint8_t *)driver->getValue(result, currentRow, field),
				driver->getLength(result, currentRow, field));
		auto str = pg_numeric_to_string(r);

		auto v = StringView(str).readDouble();
		if (v.valid()) {
			return Value(v.get());
		} else {
			return Value(str);
		}
		break;
	}
	case BackendInterface::StorageType::Bytes:
		return Value(toBytes(field).bytes<Interface>());
		break;
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

int64_t ResultCursor::toId() const {
	if (isBinaryFormat(0)) {
		stappler::BytesViewNetwork r((const uint8_t *)driver->getValue(result, 0, 0),
				driver->getLength(result, 0, 0));
		switch (r.size()) {
		case 1: return int64_t(r.readUnsigned()); break;
		case 2: return int64_t(r.readUnsigned16()); break;
		case 4: return int64_t(r.readUnsigned32()); break;
		case 8: return int64_t(r.readUnsigned64()); break;
		default: break;
		}
		return 0;
	} else {
		auto val = driver->getValue(result, 0, 0);
		return stappler::StringToNumber<int64_t>(val, nullptr, 0);
	}
}
StringView ResultCursor::getFieldName(size_t field) const {
	auto ptr = driver->getName(result, field);
	if (ptr) {
		return StringView(ptr);
	}
	return StringView();
}
bool ResultCursor::isSuccess() const { return result.get() && pgsql_is_success(err); }
bool ResultCursor::isEmpty() const { return nrows - currentRow <= 0; }
bool ResultCursor::isEnded() const { return currentRow >= nrows; }
size_t ResultCursor::getFieldsCount() const { return driver->getNFields(result); }
size_t ResultCursor::getAffectedRows() const { return driver->getCmdTuples(result); }
size_t ResultCursor::getRowsHint() const { return nrows; }
bool ResultCursor::next() {
	if (!isEmpty()) {
		++currentRow;
		return !isEmpty();
	}
	return false;
}
void ResultCursor::reset() { currentRow = 0; }
Value ResultCursor::getInfo() const {
	return Value({
		stappler::pair("error", Value(stappler::toInt(err))),
		stappler::pair("status", Value(driver->getStatusMessage(err))),
		stappler::pair("desc",
				Value(result.get() ? driver->getResultErrorMessage(result)
								   : "Fatal database error")),
	});
}
void ResultCursor::clear() {
	if (result.get()) {
		driver->clearResult(result);
		result = Driver::Result(nullptr);
	}
}

Driver::Status ResultCursor::getError() const { return err; }

} // namespace stappler::db::pq

namespace STAPPLER_VERSIONIZED stappler::db::pq {

/* HTTPD ap_dbd_t mimic */
struct DriverConnectionHandle {
	void *connection;
};

struct DriverExternalHandle {
	DriverConnectionHandle *handle;
	void *driver;
};

Driver::Handle Driver::doConnect(const char *const *keywords, const char *const *values,
		int expand_dbname) const {
	if (_external) {
		log::source().error("pq::Driver",
				"Driver in external mode can not do connection by itself");
		return Driver::Handle(nullptr);
	}

	auto p = pool::acquire();
	auto h = (DriverHandle *)pool::palloc(p, sizeof(DriverHandle));
	h->pool = p;
	h->driver = this;
	h->conn = _handle->PQconnectdbParams(keywords, values, expand_dbname);

	if (h->conn) {
		auto status = _handle->PQstatus(h->conn);
		if (status != CONNECTION_OK) {
			log::source().error("db::pq::Driver",
					"Fail to connect: ", _handle->PQerrorMessage(h->conn));
			_handle->PQfinish(h->conn);
			return Driver::Handle(nullptr);
		}
		_handle->PQsetNoticeProcessor(h->conn, Driver_noticeMessage, (void *)this);

		pool::cleanup_register(p, [h = (DriverSym *)_handle, ret = h] {
			if (ret->conn) {
				h->PQfinish(ret->conn);
				ret->conn = nullptr;
			}
		});

		return Driver::Handle(h);
	}
	return Driver::Handle(nullptr);
}

Driver::Connection Driver::getConnection(Handle _h) const {
	if (_external) {
		auto h = (DriverExternalHandle *)_h.get();
		if (h->driver == _external) {
			return Driver::Connection(h->handle->connection);
		}
	} else {
		auto h = (DriverHandle *)_h.get();
		return Driver::Connection(h->conn);
	}
	return Driver::Connection(nullptr);
}

} // namespace stappler::db::pq
