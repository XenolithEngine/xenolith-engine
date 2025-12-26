/**
Copyright (c) 2018-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_DB_SPDBINTERFACE_H_
#define STAPPLER_DB_SPDBINTERFACE_H_

#include "SPDbAuth.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class Result;
class ApplicationInterface;

enum class DeltaAction {
	Create = 1,
	Update,
	Delete,
	Append,
	Erase
};

/* Common storage/database interface, used for schemes and some other operations,
 * that requires persistent storage
 */
class SP_PUBLIC BackendInterface : public AllocBase {
public:
	enum class StorageType {
		Unknown,
		Bool,
		Char,
		Float4,
		Float8,
		Int2,
		Int4,
		Int8,
		Text,
		VarChar,
		Numeric,
		Bytes,
		TsVector,
	};

	struct Config {
		StringView name;
		const Scheme *fileScheme = nullptr;
	};

	virtual ~BackendInterface() { }

public: // key-value storage
	// set or replace value for specific key for specified time interval (no values stored forever)
	// if value is replaced, it's expiration time also updated
	virtual bool set(const stappler::CoderSource &, const Value &, stappler::TimeInterval) = 0;

	// get value for specific key
	virtual Value get(const stappler::CoderSource &) = 0;

	// remove value for specific key
	virtual bool clear(const stappler::CoderSource &) = 0;

	// get list of ids ('__oid's) for hierarchical query list
	// performs only first `count` queries in list
	virtual Vector<int64_t> performQueryListForIds(const QueryList &, size_t count) = 0;

	// get objects for specific hierarchical query list
	// performs only first `count` queries in list
	// optionally, mark objects as selected for update (lock them in DB)
	virtual Value performQueryList(const QueryList &, size_t count, bool forUpdate) = 0;

public:
	// initialize schemes in database
	// all fields, indexes, constraints and triggers updated to match schemes definition
	virtual bool init(const Config &serv, const Map<StringView, const Scheme *> &) = 0;

	// force temporary data cleanup
	virtual void makeSessionsCleanup() { }

	// force broadcast data processing
	virtual int64_t processBroadcasts(const Callback<void(BytesView)> &, int64_t value) { return 0; }

	// perform select operation with result cursor callback
	// fields will not be resolved in this case, you should call `decode` or `toData` from result manually
	virtual bool foreach(Worker &, const Query &, const Callback<bool(Value &)> &) = 0;

	// perform select operation, returns resolved data
	virtual Value select(Worker &, const Query &) = 0;

	// create new object or objects, returns new values
	virtual Value create(Worker &, const Vector<InputField> &inputField, Vector<InputRow> &inputRows, bool multiCreate) = 0;
	// virtual Value create(Worker &, Map<StringView, InputValue> &) = 0;

	// perform update operation (read-modify-write), update only specified fields in new object
	virtual Value save(Worker &, uint64_t oid, const Value &obj, const Vector<InputField> &, InputRow &) = 0;

	// delete object by id
	virtual bool remove(Worker &, uint64_t oid) = 0;

	// count objects for specified query
	virtual size_t count(Worker &, const Query &) = 0;

public:
	// perform generic operation on object's field (Array or Set)
	virtual Value field(Action, Worker &, uint64_t oid, const Field &, Value &&) = 0;

	// perform generic operation on object's field (Array or Set)
	virtual Value field(Action, Worker &, const Value &, const Field &, Value &&) = 0;

	// add object (last parameter) info View field of scheme
	virtual bool addToView(const FieldView &, const Scheme *, uint64_t oid, const Value &) = 0;

	// remove object with specific id from View field
	virtual bool removeFromView(const FieldView &, const Scheme *, uint64_t oid) = 0;

	// find in which sets object with id can be found
	virtual Vector<int64_t> getReferenceParents(const Scheme &, uint64_t oid, const Scheme *, const Field *) = 0;

public: // others
	virtual bool beginTransaction() = 0;
	virtual bool endTransaction() = 0;

	// try to authorize user with name and password, using fields and scheme from Auth object
	// authorization is protected with internal '__login" scheme to prevent bruteforce attacks
	virtual User * authorizeUser(const Auth &, const StringView &name, const StringView &password) = 0;

	// send broadcast with data
	virtual void broadcast(const Bytes &) = 0;

	// get scheme delta value (like, last modification time) for scheme
	// Scheme should have Delta flag
	virtual int64_t getDeltaValue(const Scheme &) = 0;

	// get delta value for View field in specific object
	// View should have Delta flag
	virtual int64_t getDeltaValue(const Scheme &, const FieldView &, uint64_t) = 0;

public:
	// prevent transaction from successfully competition
	void cancelTransaction() { transactionStatus = TransactionStatus::Rollback; }

	// check if there is active transaction
	bool isInTransaction() const { return transactionStatus != TransactionStatus::None; }

	// get active transaction status
	TransactionStatus getTransactionStatus() const { return transactionStatus; }

	// get current database name (driver-specific)
	StringView getDatabaseName() const { return dbName; }

	virtual String getTransactionKey() const { return String(); }

protected:
	StringView dbName;
    TransactionStatus transactionStatus = TransactionStatus::None;
};

class SP_PUBLIC Binder {
public:
	struct DataField {
		const Field *field;
		const Value &data;
		bool force = false;
		bool compress = false;
	};

	struct FullTextField {
		const Field *field;
		const FullTextVector &data;
	};

	struct FullTextFrom {
		StringView scheme;
		const Field *field;
		StringView query;
	};

	struct FullTextRank {
		StringView scheme;
		const Field *field;
		StringView query;
	};

	struct FullTextQueryRef {
		StringView scheme;
		const Field *field;
		const FullTextQuery &query;
	};

	struct TypeString {
		StringView str;
		StringView type;

		template <typename Str, typename Type>
		TypeString(Str && str, Type && type)
		: str(str), type(type) { }
	};

	void setInterface(QueryInterface *);
	QueryInterface * getInterface() const;

	void writeBind(StringStream &, int64_t);
	void writeBind(StringStream &, uint64_t);
	void writeBind(StringStream &, double);
	void writeBind(StringStream &query, Time val);
	void writeBind(StringStream &query, TimeInterval val);
	void writeBind(StringStream &, const String &);
	void writeBind(StringStream &, String &&);
	void writeBind(StringStream &, const StringView &);
	void writeBind(StringStream &, const Bytes &);
	void writeBind(StringStream &, Bytes &&);
	void writeBind(StringStream &, const CoderSource &);
	void writeBind(StringStream &, const Value &);
	void writeBind(StringStream &, const DataField &);
	void writeBind(StringStream &, const TypeString &);
	void writeBind(StringStream &, const FullTextField &);
	void writeBind(StringStream &, const FullTextFrom &);
	void writeBind(StringStream &, const FullTextRank &);
	void writeBind(StringStream &, const FullTextQueryRef &);
	void writeBind(StringStream &, const stappler::sql::PatternComparator<const Value &> &);
	void writeBind(StringStream &, const stappler::sql::PatternComparator<const StringView &> &);
	void writeBind(StringStream &, const Vector<int64_t> &);
	void writeBind(StringStream &, const Vector<double> &);
	void writeBind(StringStream &, const Vector<StringView> &);

	void writeBindArray(StringStream &, const Vector<int64_t> &);
	void writeBindArray(StringStream &, const Vector<double> &);
	void writeBindArray(StringStream &, const Vector<StringView> &);
	void writeBindArray(StringStream &, const Value &);

	void clear();

protected:
	QueryInterface *_iface = nullptr;
};

class SP_PUBLIC QueryInterface {
public:
	virtual ~QueryInterface() = default;

	virtual void bindInt(Binder &, StringStream &, int64_t) = 0;
	virtual void bindUInt(Binder &, StringStream &, uint64_t) = 0;
	virtual void bindDouble(Binder &, StringStream &, double) = 0;
	virtual void bindString(Binder &, StringStream &, const String &) = 0;
	virtual void bindMoveString(Binder &, StringStream &, String &&) = 0;
	virtual void bindStringView(Binder &, StringStream &, const StringView &) = 0;
	virtual void bindBytes(Binder &, StringStream &, const Bytes &) = 0;
	virtual void bindMoveBytes(Binder &, StringStream &, Bytes &&) = 0;
	virtual void bindCoderSource(Binder &, StringStream &, const stappler::CoderSource &) = 0;
	virtual void bindValue(Binder &, StringStream &, const Value &) = 0;
	virtual void bindDataField(Binder &, StringStream &, const Binder::DataField &) = 0;
	virtual void bindTypeString(Binder &, StringStream &, const Binder::TypeString &) = 0;
	virtual void bindFullText(Binder &, StringStream &, const Binder::FullTextField &) = 0;
	virtual void bindFullTextFrom(Binder &, StringStream &, const Binder::FullTextFrom &) = 0;
	virtual void bindFullTextRank(Binder &, StringStream &, const Binder::FullTextRank &) = 0;
	virtual void bindFullTextQuery(Binder &, StringStream &, const Binder::FullTextQueryRef &d) = 0;
	virtual void bindIntVector(Binder &, StringStream &, const Vector<int64_t> &) = 0;
	virtual void bindDoubleVector(Binder &, StringStream &, const Vector<double> &) = 0;
	virtual void bindStringVector(Binder &, StringStream &, const Vector<StringView> &) = 0;

	virtual void clear() = 0;
};

class SP_PUBLIC ResultCursor {
public:
	virtual ~ResultCursor() = default;

	virtual bool isBinaryFormat(size_t field) const = 0;

	virtual bool isNull(size_t field) const = 0;

	virtual StringView toString(size_t field) const = 0;
	virtual stappler::BytesView toBytes(size_t field) const = 0;

	virtual int64_t toInteger(size_t field) const = 0;
	virtual double toDouble(size_t field) const = 0;
	virtual bool toBool(size_t field) const = 0;

	virtual Value toTypedData(size_t field) const = 0;

	virtual Value toCustomData(size_t field, const FieldCustom *) const = 0;

	virtual int64_t toId() const = 0;

	virtual StringView getFieldName(size_t field) const = 0;

	virtual bool isSuccess() const = 0;
	virtual bool isEmpty() const = 0;
	virtual bool isEnded() const = 0;
	virtual size_t getFieldsCount() const = 0;
	virtual size_t getAffectedRows() const = 0;
	virtual size_t getRowsHint() const = 0;

	virtual Value getInfo() const = 0;
	virtual bool next() = 0;
	virtual void reset() = 0;
	virtual void clear() = 0;
};

struct SP_PUBLIC ResultRow {
	ResultRow(const db::ResultCursor *, size_t);
	ResultRow(const ResultRow & other) noexcept;
	ResultRow & operator=(const ResultRow &other) noexcept;

	size_t size() const;
	Value toData(const db::Scheme &, const Map<String, db::Field> & = Map<String, db::Field>(),
			const Vector<const Field *> &virtuals = Vector<const Field *>());

	Value encode() const;

	StringView front() const;
	StringView back() const;

	bool isNull(size_t) const;
	StringView at(size_t) const;

	StringView toString(size_t) const;
	BytesView toBytes(size_t) const;
	int64_t toInteger(size_t) const;
	double toDouble(size_t) const;
	bool toBool(size_t) const;

	Value toTypedData(size_t n) const;

	Value toData(size_t n, const db::Field &);

	const db::ResultCursor *result = nullptr;
	size_t row = 0;
};

class SP_PUBLIC Result {
public:
	struct Iter {
		Iter() noexcept {}
		Iter(Result *res, size_t n) noexcept : result(res), row(n) { }

		Iter& operator=(const Iter &other) { result = other.result; row = other.row; return *this; }
		bool operator==(const Iter &other) const { return row == other.row; }
		bool operator!=(const Iter &other) const { return row != other.row; }
		bool operator<(const Iter &other) const { return row < other.row; }
		bool operator>(const Iter &other) const { return row > other.row; }
		bool operator<=(const Iter &other) const { return row <= other.row; }
		bool operator>=(const Iter &other) const { return row >= other.row; }

		Iter& operator++() { if (!result->next()) { row = stappler::maxOf<size_t>(); } return *this; }

		ResultRow operator*() const { return ResultRow(result->_cursor, row); }

		Result *result = nullptr;
		size_t row = 0;
	};

	Result() = default;
	Result(db::ResultCursor *);
	~Result();

	Result(const Result &) = delete;
	Result & operator=(const Result &) = delete;

	Result(Result &&);
	Result & operator=(Result &&);

	explicit operator bool () const;
	bool success() const;

	Value info() const;

	bool empty() const;
	size_t nrows() const { return getRowsHint(); }
	size_t nfields() const { return _nfields; }
	size_t getRowsHint() const;
	size_t getAffectedRows() const;

	int64_t readId();

	void clear();

	Iter begin();
	Iter end();

	ResultRow current() const;
	bool next();

	StringView name(size_t) const;

	Value decode(const db::Scheme &, const Vector<const Field *> &virtuals);
	Value decode(const db::Field &, const Vector<const Field *> &virtuals);
	Value decode(const db::FieldView &);

protected:
	friend struct ResultRow;

	db::ResultCursor *_cursor = nullptr;
	size_t _row = 0;

	bool _success = false;

	size_t _nfields = 0;
};

}

#endif /* STAPPLER_DB_SPDBINTERFACE_H_ */
