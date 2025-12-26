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

#ifndef STAPPLER_DB_SQLITE_SPSQLITEHANDLE_H_
#define STAPPLER_DB_SQLITE_SPSQLITEHANDLE_H_

#include "SPSqlHandle.h"
#include "SPSqliteDriver.h"
#include "SPDbScheme.h"
#include "SPSql.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

enum class TransactionLevel {
	Deferred,
	Immediate,
	Exclusive,
};

class SP_PUBLIC SqliteQueryInterface : public db::QueryInterface {
public:
	using Binder = db::Binder;

	struct BindingData {
		uint32_t idx = 0;
		Bytes data;
		Type type;
	};

	SqliteQueryInterface(const sql::Driver *d, const sql::QueryStorageHandle *, Driver::Handle h);

	size_t push(String &&val);
	size_t push(const StringView &val);
	size_t push(Bytes &&val);
	size_t push(StringStream &query, const Value &val, bool force, bool compress = false);

	virtual void bindInt(db::Binder &, StringStream &query, int64_t val) override;
	virtual void bindUInt(db::Binder &, StringStream &query, uint64_t val) override;
	virtual void bindDouble(db::Binder &, StringStream &query, double val) override;
	virtual void bindString(db::Binder &, StringStream &query, const String &val) override;
	virtual void bindMoveString(db::Binder &, StringStream &query, String &&val) override;
	virtual void bindStringView(db::Binder &, StringStream &query, const StringView &val) override;
	virtual void bindBytes(db::Binder &, StringStream &query, const Bytes &val) override;
	virtual void bindMoveBytes(db::Binder &, StringStream &query, Bytes &&val) override;
	virtual void bindCoderSource(db::Binder &, StringStream &query, const stappler::CoderSource &val) override;
	virtual void bindValue(db::Binder &, StringStream &query, const Value &val) override;
	virtual void bindDataField(db::Binder &, StringStream &query, const db::Binder::DataField &f) override;
	virtual void bindTypeString(db::Binder &, StringStream &query, const db::Binder::TypeString &type) override;
	virtual void bindFullText(db::Binder &, StringStream &query, const db::Binder::FullTextField &d) override;
	virtual void bindFullTextFrom(db::Binder &, StringStream &query, const db::Binder::FullTextFrom &d) override;
	virtual void bindFullTextRank(db::Binder &, StringStream &query, const db::Binder::FullTextRank &d) override;
	virtual void bindFullTextQuery(db::Binder &, StringStream &query, const db::Binder::FullTextQueryRef &d) override;
	virtual void bindIntVector(Binder &, StringStream &query, const Vector<int64_t> &vec) override;
	virtual void bindDoubleVector(Binder &b, StringStream &query, const Vector<double> &vec) override;
	virtual void bindStringVector(Binder &b, StringStream &query, const Vector<StringView> &vec) override;
	virtual void clear() override;

public:
	const sql::Driver *driver = nullptr;
	const sql::QueryStorageHandle *storage = nullptr;
	Driver::Handle handle;
	Vector<BindingData> params;
};

class SP_PUBLIC Handle : public db::sql::SqlHandle {
public:
	Handle(const Driver *, Driver::Handle);

	Handle(const Handle &) = delete;
	Handle &operator=(const Handle &) = delete;

	Handle(Handle &&) = delete;
	Handle &operator=(Handle &&) = delete;

	explicit operator bool () const;

	const Driver *getDriver() const { return driver; }
	Driver::Handle getHandle() const;
	Driver::Connection getConnection() const;

	virtual void makeQuery(const Callback<void(sql::SqlQuery &)> &cb, const sql::QueryStorageHandle *) override;

	virtual bool selectQuery(const db::sql::SqlQuery &, const Callback<bool(sql::Result &)> &cb,
			const Callback<void(const Value &)> &err = nullptr) override;
	virtual bool performSimpleQuery(const StringView &,
			const Callback<void(const Value &)> &err = nullptr) override;
	virtual bool performSimpleSelect(const StringView &, const Callback<void(sql::Result &)> &cb,
			const Callback<void(const Value &)> &err = nullptr) override;

	virtual bool isSuccess() const override;

	void close();

public: // adapter interface
	virtual bool init(const BackendInterface::Config &cfg, const Map<StringView, const Scheme *> &) override;

protected:
	virtual bool beginTransaction() override;
	virtual bool endTransaction() override;

	using ViewIdVec = Vector<Pair<const Scheme::ViewScheme *, int64_t>>;

	const Driver *driver = nullptr;
	Driver::Handle handle = Driver::Handle(nullptr);
	Driver::Connection conn = Driver::Connection(nullptr);
	int lastError = 0;
	TransactionLevel level = TransactionLevel::Deferred;
	stappler::sql::Profile _profile = stappler::sql::Profile::Postgres;
};

}

#endif /* STAPPLER_DB_SQLITE_SPSQLITEHANDLE_H_ */
