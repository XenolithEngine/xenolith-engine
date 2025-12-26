/**
Copyright (c) 2018-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_DB_SQL_SPSQLHANDLE_H_
#define STAPPLER_DB_SQL_SPSQLHANDLE_H_

#include "SPSqlQuery.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

class Driver;
struct QueryStorageHandle;

using Result = db::Result;

class SP_PUBLIC SqlHandle : public db::BackendInterface {
public:
	using Scheme = db::Scheme;
	using Worker = db::Worker;
	using Field = db::Field;
	using Operator = stappler::sql::Operator;
	using Comparation = stappler::sql::Comparation;
	using QueryList = db::QueryList;

	static StringView getKeyValueSchemeName();
	static String getNameForDelta(const Scheme &scheme);

	SqlHandle(const Driver *);

	const Driver *getDriver() const { return _driver; }

	virtual bool set(const CoderSource &, const Value &, stappler::TimeInterval) override;
	virtual Value get(const CoderSource &) override;
	virtual bool clear(const CoderSource &) override;

	virtual db::User * authorizeUser(const db::Auth &auth, const StringView &iname, const StringView &password) override;

	virtual bool isNotificationsSupported() const { return false; }

	virtual void makeSessionsCleanup() override;
	void finalizeBroadcast();
	virtual int64_t processBroadcasts(const Callback<void(stappler::BytesView)> &, int64_t value) override;
	virtual void broadcast(const Bytes &) override;

	virtual int64_t getDeltaValue(const Scheme &scheme) override;
	virtual int64_t getDeltaValue(const Scheme &scheme, const db::FieldView &view, uint64_t tag) override;

	// get change history for scheme that supports delta ops
	Value getHistory(const Scheme &, const stappler::Time &, bool resolveUsers = false);

	// get change history for view field in object
	Value getHistory(const db::FieldView &, const Scheme *, uint64_t tag, const stappler::Time &, bool resolveUsers = false);

	// get changed objects in scheme from timestamp
	Value getDeltaData(const Scheme &, const stappler::Time &);

	// get changed objects from view field in object from timestamp
	Value getDeltaData(const Scheme &, const db::FieldView &, const stappler::Time &, uint64_t);

	virtual void makeQuery(const stappler::Callback<void(SqlQuery &)> &cb, const QueryStorageHandle *) = 0;

	virtual bool selectQuery(const SqlQuery &, const Callback<bool(Result &)> &cb,
			const Callback<void(const Value &)> &err = nullptr) = 0;
	virtual bool performSimpleQuery(const StringView &,
			const Callback<void(const Value &)> &err = nullptr) = 0;
	virtual bool performSimpleSelect(const StringView &, const Callback<void(Result &)> &cb,
			const Callback<void(const Value &)> &err = nullptr) = 0;

	virtual bool isSuccess() const = 0;

	virtual bool foreach(Worker &, const Query &, const Callback<bool(Value &)> &) override;

	virtual Value select(Worker &, const db::Query &) override;

	virtual Value create(Worker &, const Vector<InputField> &, Vector<InputRow> &, bool multiCreate) override;
	virtual Value save(Worker &, uint64_t oid, const Value &obj, const Vector<InputField> &, InputRow &) override;

	virtual bool remove(Worker &, uint64_t oid) override;

	virtual size_t count(Worker &, const db::Query &) override;

	virtual Value field(db::Action, Worker &, uint64_t oid, const Field &, Value &&) override;
	virtual Value field(db::Action, Worker &, const Value &, const Field &, Value &&) override;

protected: // prop interface
	virtual Vector<int64_t> performQueryListForIds(const QueryList &, size_t count = stappler::maxOf<size_t>()) override;
	virtual Value performQueryList(const QueryList &, size_t count = stappler::maxOf<size_t>(), bool forUpdate = false) override;

	virtual bool removeFromView(const db::FieldView &, const Scheme *, uint64_t oid) override;
	virtual bool addToView(const db::FieldView &, const Scheme *, uint64_t oid, const Value &) override;

	virtual Vector<int64_t> getReferenceParents(const Scheme &, uint64_t oid, const Scheme *, const Field *) override;

	int64_t selectQueryId(const SqlQuery &);
	size_t performQuery(const SqlQuery &);

	Value selectValueQuery(const Scheme &, const SqlQuery &, const Vector<const Field *> &virtuals);
	Value selectValueQuery(const Field &, const SqlQuery &, const Vector<const Field *> &virtuals);
	void selectValueQuery(Value &, const FieldView &, const SqlQuery &);

	Value getFileField(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f);
	size_t getFileCount(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f);

	Value getArrayField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f);
	size_t getArrayCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f);

	Value getObjectField(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f);
	size_t getObjectCount(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f);

	Value getSetField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &);
	size_t getSetCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &);

	Value getViewField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &);
	size_t getViewCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &);

	Value getSimpleField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f);
	size_t getSimpleCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f);

	bool insertIntoSet(SqlQuery &, const Scheme &s, int64_t id, const db::FieldObject &field, const Field &f, const Value &d);
	bool insertIntoArray(SqlQuery &, const Scheme &s, int64_t id, const Field &field, Value &d);
	bool insertIntoRefSet(SqlQuery &, const Scheme &s, int64_t id, const Field &field, const Vector<int64_t> &d);
	bool cleanupRefSet(SqlQuery &query, const Scheme &, uint64_t oid, const Field &, const Vector<int64_t> &objsToRemove);

	void performPostUpdate(const db::Transaction &, SqlQuery &query, const Scheme &s, Value &data, int64_t id, const Value &upd, bool clear);

	const Driver *_driver = nullptr;
	Vector<stappler::Pair<stappler::Time, Bytes>> _bcasts;
};

}

#endif /* STAPPLER_DB_SQL_STSQLHANDLE_H_ */
