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

#include "SPSqliteHandle.h"
#include "SPSqliteDriver.h"
#include "SPSqliteDriverHandle.cc"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

class SqliteQuery : public db::sql::SqlQuery {
public:
	virtual ~SqliteQuery() = default;

	SqliteQuery(db::QueryInterface *q, const sql::Driver *d) : SqlQuery(q, d) { }

	virtual void writeFullTextWhere(WhereContinue &whi, db::Operator op, const db::Scheme &scheme,
			const db::Query::Select &sel, StringView ftsQuery) override {
		auto functionCall = toString("sp_ts_query_valid(\"", scheme.getName(), "\".\"", sel.field,
				"\", '", ftsQuery, "')");
		whi.where(op, SqlQuery::Field(StringView(functionCall), SqlQuery::Field::PlainText),
				Comparation::Equal, RawStringView{StringView("1")});
	}
};

SqliteQueryInterface::SqliteQueryInterface(const sql::Driver *d, const sql::QueryStorageHandle *s,
		Driver::Handle h)
: driver(d), storage(s), handle(h) { }

size_t SqliteQueryInterface::push(String &&val) {
	auto &it = params.emplace_back(BindingData());
	it.data.assign_strong((uint8_t *)val.data(), val.size() + 1);
	it.idx = uint32_t(params.size());
	it.type = Type::Text;
	return it.idx;
}

size_t SqliteQueryInterface::push(const StringView &val) {
	auto &it = params.emplace_back(BindingData());
	it.data.assign_strong((uint8_t *)val.data(), val.size() + 1);
	it.data.data()[val.size()] = 0;
	it.idx = uint32_t(params.size());
	it.type = Type::Text;
	return it.idx;
}

size_t SqliteQueryInterface::push(Bytes &&val) {
	auto &it = params.emplace_back(BindingData());
	it.data = sp::move(val);
	it.idx = uint32_t(params.size());
	it.type = Type::Bytes;
	return it.idx;
}

size_t SqliteQueryInterface::push(StringStream &query, const Value &val, bool force,
		bool compress) {
	if (!force || val.getType() == Value::Type::EMPTY) {
		switch (val.getType()) {
		case Value::Type::EMPTY: query << "NULL"; break;
		case Value::Type::BOOLEAN: query << (val.asBool() ? "TRUE" : "FALSE"); break;
		case Value::Type::INTEGER: query << val.asInteger(); break;
		case Value::Type::DOUBLE:
			if (std::isnan(val.asDouble())) {
				query << "'NaN'";
			} else if (val.asDouble() == std::numeric_limits<double>::infinity()) {
				query << "'-Infinity'";
			} else if (-val.asDouble() == std::numeric_limits<double>::infinity()) {
				query << "'Infinity'";
			} else {
				query << std::setprecision(std::numeric_limits<double>::max_digits10 + 1)
					  << val.asDouble();
			}
			break;
		case Value::Type::CHARSTRING: query << "?" << push(val.getString()); break;
		case Value::Type::BYTESTRING: query << "?" << push(val.asBytes()); break;
		case Value::Type::ARRAY:
		case Value::Type::DICTIONARY:
			query << "?"
				  << push(data::write<Interface>(val,
							 EncodeFormat(EncodeFormat::Cbor,
									 compress ? EncodeFormat::LZ4HCCompression
											  : EncodeFormat::DefaultCompress)));
			break;
		default: break;
		}
	} else {
		query << "?"
			  << push(data::write<Interface>(val,
						 EncodeFormat(EncodeFormat::Cbor,
								 compress ? EncodeFormat::LZ4HCCompression
										  : EncodeFormat::DefaultCompress)));
	}
	return params.size();
}

void SqliteQueryInterface::bindInt(db::Binder &, StringStream &query, int64_t val) { query << val; }
void SqliteQueryInterface::bindUInt(db::Binder &, StringStream &query, uint64_t val) {
	query << val;
}
void SqliteQueryInterface::bindDouble(db::Binder &, StringStream &query, double val) {
	query << std::setprecision(std::numeric_limits<double>::max_digits10 + 1) << val;
}
void SqliteQueryInterface::bindString(db::Binder &, StringStream &query, const String &val) {
	if (auto num = push(String(val))) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindMoveString(db::Binder &, StringStream &query, String &&val) {
	if (auto num = push(sp::move(val))) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindStringView(db::Binder &, StringStream &query,
		const StringView &val) {
	if (auto num = push(val)) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindBytes(db::Binder &, StringStream &query, const Bytes &val) {
	if (auto num = push(Bytes(val))) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindMoveBytes(db::Binder &, StringStream &query, Bytes &&val) {
	if (auto num = push(sp::move(val))) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindCoderSource(db::Binder &, StringStream &query,
		const stappler::CoderSource &val) {
	if (auto num = push(Bytes(val.data(), val.data() + val.size()))) {
		query << "?" << num;
	}
}
void SqliteQueryInterface::bindValue(db::Binder &, StringStream &query, const Value &val) {
	push(query, val, false);
}
void SqliteQueryInterface::bindDataField(db::Binder &, StringStream &query,
		const db::Binder::DataField &f) {
	if (f.field && f.field->getType() == db::Type::Custom) {
		auto c = f.field->getSlot<db::FieldCustom>();
		if (auto info = driver->getCustomFieldInfo(c->getDriverTypeName())) {
			if (!info->writeToStorage(*c, *this, query, f.data)) {
				query << "NULL";
			}
		} else {
			query << "NULL";
		}
	} else {
		push(query, f.data, f.force, f.compress);
	}
}
void SqliteQueryInterface::bindTypeString(db::Binder &, StringStream &query,
		const db::Binder::TypeString &type) {
	if (auto num = push(type.str)) {
		query << "?" << num;
	}
}

void SqliteQueryInterface::bindFullText(db::Binder &, StringStream &query,
		const db::Binder::FullTextField &d) {
	auto slot = d.field->getSlot<FieldFullTextView>();
	auto result = slot->searchConfiguration->encodeSearchVectorData(d.data);
	if (auto num = push(move(result))) {
		query << "?" << num;
	}

	storage->data->emplace(d.field->getName(), &d.data);
}

void SqliteQueryInterface::bindFullTextFrom(db::Binder &, StringStream &query,
		const db::Binder::FullTextFrom &d) {
	auto tableName = toString(d.scheme, "_f_", d.field->getName());
	auto fieldId = toString(d.scheme, "_id");

	auto &storageData = *storage->data;

	auto it = storageData.find(d.query);
	if (it != storageData.end()) {
		auto q = (TextQueryData *)it->second;
		query << " INNER JOIN (SELECT DISTINCT \"" << fieldId << "\" as id FROM \"" << tableName
			  << "\" WHERE word IN (";

		bool first = true;
		for (auto &w : q->pos) {
			if (first) {
				first = false;
			} else {
				query << ",";
			}
			query << w;
		}

		query << ")) AS \"__" << d.scheme << "_" << d.field->getName() << "\"" " ON (\"" << d.scheme
			  << "\".__oid=\"__" << d.scheme << "_" << d.field->getName() << "\".id)";
	}
}

void SqliteQueryInterface::bindFullTextRank(db::Binder &, StringStream &query,
		const db::Binder::FullTextRank &d) {
	auto slot = d.field->getSlot<FieldFullTextView>();
	query << " sp_ts_rank(" << d.scheme << ".\"" << d.field->getName() << "\", '" << d.query
		  << "', " << toInt(slot->normalization) << ")";
}

void SqliteQueryInterface::bindFullTextQuery(db::Binder &, StringStream &query,
		const db::Binder::FullTextQueryRef &d) {
	query << d.scheme << "." << d.field->getName();

	auto it = storage->data->find(query.weak());
	while (it != storage->data->end()) {
		query << "_";
		it = storage->data->find(query.weak());
	}

	auto q = new (std::nothrow) TextQueryData;
	q->query = &d.query;
	q->query->decompose([&, this](StringView pos) {
		emplace_ordered(q->pos, ((const Driver *)driver)->insertWord(handle, pos));
	}, [&, this](StringView neg) {
		emplace_ordered(q->neg, ((const Driver *)driver)->insertWord(handle, neg));
	});

	auto str = StringView(query.weak());
	storage->data->emplace(str.pdup(), q);
}

void SqliteQueryInterface::bindIntVector(Binder &, StringStream &query,
		const Vector<int64_t> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		query << it;
	}
	query << ")";
}

void SqliteQueryInterface::bindDoubleVector(Binder &b, StringStream &query,
		const Vector<double> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		bindDouble(b, query, it);
	}
	query << ")";
}

void SqliteQueryInterface::bindStringVector(Binder &b, StringStream &query,
		const Vector<StringView> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		bindStringView(b, query, it);
	}
	query << ")";
}

void SqliteQueryInterface::clear() { params.clear(); }

Handle::Handle(const Driver *d, Driver::Handle h) : SqlHandle(d), driver(d), handle(h) {
	_profile = stappler::sql::Profile::Sqlite;
	if (h.get()) {
		auto c = d->getConnection(h);
		if (c.get()) {
			conn = c;
			dbName = driver->getDbName(h);
		}
	}
}

Handle::operator bool() const { return conn.get() != nullptr; }

Driver::Handle Handle::getHandle() const { return handle; }

Driver::Connection Handle::getConnection() const { return conn; }

void Handle::close() { conn = Driver::Connection(nullptr); }

void Handle::makeQuery(const stappler::Callback<void(sql::SqlQuery &)> &cb,
		const sql::QueryStorageHandle *storage) {
	SqliteQueryInterface interface(_driver, storage, handle);
	SqliteQuery query(&interface, _driver);
	query.setProfile(_profile);
	cb(query);
}

bool Handle::selectQuery(const sql::SqlQuery &query,
		const stappler::Callback<bool(sql::Result &)> &cb,
		const Callback<void(const Value &)> &errCb) {
	if (!conn.get() || getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	auto queryInterface = static_cast<SqliteQueryInterface *>(query.getInterface());

	auto queryString = query.getQuery().weak();

	sqlite3_stmt *stmt = nullptr;
	auto err = driver->getHandle()->prepare((sqlite3 *)conn.get(), queryString.data(),
			int(queryString.size()), 0, &stmt, nullptr);
	if (err != SQLITE_OK) {
		auto info = driver->getInfo(conn, err);
		info.setString(query.getQuery().str(), "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		cancelTransaction();
		return false;
	}

	// bind
	for (auto &it : queryInterface->params) {
		switch (it.type) {
		case Type::Text:
			driver->getHandle()->_bind_text(stmt, it.idx, (const char *)it.data.data(),
					int(it.data.size() - 1), SQLITE_STATIC);
			break;
		case Type::Bytes:
			driver->getHandle()->_bind_blob(stmt, it.idx, it.data.data(), int(it.data.size()),
					SQLITE_STATIC);
			break;
		default: break;
		}
	}

	err = driver->getHandle()->step(stmt);
	if (err != SQLITE_OK && err != SQLITE_DONE && err != SQLITE_ROW) {
		auto info = driver->getInfo(conn, err);
		info.setString(query.getQuery().str(), "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		driver->getHandle()->finalize(stmt);
		cancelTransaction();
		return false;
	}

	ResultCursor cursor(driver, conn, Driver::Result(stmt), err);
	db::sql::Result ret(&cursor);
	cb(ret);
	return true;
}

bool Handle::performSimpleQuery(const StringView &query,
		const Callback<void(const Value &)> &errCb) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	StringView queryData = query;

	const char *outPtr = query.data();
	bool success = true;
	while (outPtr && *outPtr != 0 && success) {
		auto size = queryData.size() - (outPtr - queryData.data());
		StringView nextQuery(outPtr, size);
		nextQuery.skipChars<StringView::WhiteSpace>();
		if (nextQuery.empty()) {
			break;
		}

		sqlite3_stmt *stmt = nullptr;
		auto err = driver->getHandle()->prepare((sqlite3 *)conn.get(), nextQuery.data(),
				int(nextQuery.size()), 0, &stmt, &outPtr);
		if (err != SQLITE_OK) {
			auto len = outPtr - nextQuery.data();
			StringView performedQuery(nextQuery.data(), len);
			auto info = driver->getInfo(conn, err);
			info.setString(performedQuery, "query");
#if DEBUG
			log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
			if (errCb) {
				errCb(info);
			}
			driver->getApplicationInterface()->debug("Database", "Fail to perform query",
					sp::move(info));
			driver->getApplicationInterface()->error("Database", "Fail to perform query");
			cancelTransaction();
			return false;
		}

		err = driver->getHandle()->step(stmt);

		if (err != SQLITE_OK && err != SQLITE_DONE && err != SQLITE_ROW) {
			auto info = driver->getInfo(conn, err);
			info.setString(nextQuery, "query");
#if DEBUG
			log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
			if (errCb) {
				errCb(info);
			}
			driver->getApplicationInterface()->debug("Database", "Fail to perform query",
					sp::move(info));
			driver->getApplicationInterface()->error("Database", "Fail to perform query");
			driver->getHandle()->finalize(stmt);
			cancelTransaction();
			return false;
		}

		success = ResultCursor::statusIsSuccess(err);
		driver->getHandle()->finalize(stmt);
	}
	return success;
}

bool Handle::performSimpleSelect(const StringView &query,
		const stappler::Callback<void(sql::Result &)> &cb,
		const Callback<void(const Value &)> &errCb) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	auto err = driver->getHandle()->prepare((sqlite3 *)conn.get(), query.data(), int(query.size()),
			0, &stmt, nullptr);
	if (err != SQLITE_OK) {
		auto info = driver->getInfo(conn, err);
		info.setString(query, "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		cancelTransaction();
		return false;
	}

	err = driver->getHandle()->step(stmt);

	ResultCursor cursor(driver, conn, Driver::Result(stmt), err);
	db::sql::Result ret(&cursor);
	cb(ret);
	return true;
}

bool Handle::isSuccess() const { return ResultCursor::statusIsSuccess(lastError); }

bool Handle::beginTransaction() {
	if (transactionStatus != db::TransactionStatus::None) {
		return false;
	}

	if (_driver->getApplicationInterface()) {
		driver->setUserId(handle, _driver->getApplicationInterface()->getUserIdFromContext());
	}

	switch (level) {
	case TransactionLevel::Deferred:
		if (performSimpleQuery("BEGIN DEFERRED"_weak)) {
			level = TransactionLevel::Deferred;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	case TransactionLevel::Immediate:
		if (performSimpleQuery("BEGIN IMMEDIATE"_weak)) {
			level = TransactionLevel::Immediate;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	case TransactionLevel::Exclusive:
		if (performSimpleQuery("BEGIN EXCLUSIVE"_weak)) {
			level = TransactionLevel::Exclusive;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	default: break;
	}
	return false;
}

bool Handle::endTransaction() {
	switch (transactionStatus) {
	case db::TransactionStatus::Commit:
		transactionStatus = db::TransactionStatus::None;
		if (performSimpleQuery("COMMIT"_weak)) {
			finalizeBroadcast();
			return true;
		}
		break;
	case db::TransactionStatus::Rollback:
		transactionStatus = db::TransactionStatus::None;
		if (performSimpleQuery("ROLLBACK"_weak)) {
			finalizeBroadcast();
			return false;
		}
		break;
	default: break;
	}
	return false;
}

} // namespace stappler::db::sqlite
