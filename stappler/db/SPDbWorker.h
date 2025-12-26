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

#ifndef STAPPLER_DB_SPDBWORKER_H_
#define STAPPLER_DB_SPDBWORKER_H_

#include "SPDbField.h"
#include "SPDbQueryList.h"
#include "SPDbTransaction.h"

namespace STAPPLER_VERSIONIZED stappler::db {

enum class UpdateFlags : uint32_t {
	None = 0,
	Protected = 1 << 0,
	NoReturn = 1 << 1,
	GetAll = 1 << 2,
	GetForUpdate = 1 << 3,
	Cached = 1 << 4, // cache 'get' result within transaction
};

SP_DEFINE_ENUM_AS_MASK(UpdateFlags)

struct SP_PUBLIC Conflict {
	enum Flags : uint32_t {
		None,
		DoNothing = 1 << 0,
		WithoutCondition = 1 << 2,
	};

	static Conflict update(StringView);

	String field;
	Query::Select condition;
	Vector<String> mask;
	Flags flags = DoNothing;

	Conflict(Conflict::Flags);
	Conflict(StringView field, Query::Select &&, Flags = None);
	Conflict(StringView field, Query::Select &&, Vector<String> &&);

	Conflict &setFlags(Flags);
};

SP_DEFINE_ENUM_AS_MASK(Conflict::Flags)

class SP_PUBLIC Worker : public AllocBase {
public:
	using FieldCallback = stappler::Callback<void(const StringView &name, const Field *f)>;

	struct RequiredFields {
		const Scheme *scheme = nullptr;
		Vector<const Field *> includeFields;
		Vector<const Field *> excludeFields;
		bool includeNone = false;
		bool includeAll = false;

		void clear();
		void reset(const Scheme &);

		void include(std::initializer_list<StringView>);
		void include(const Set<const Field *> &);
		void include(const StringView &);
		void include(const Field *);

		void exclude(std::initializer_list<StringView>);
		void exclude(const Set<const Field *> &);
		void exclude(const StringView &);
		void exclude(const Field *);
	};

	struct ConditionData {
		Comparation compare = Comparation::Equal;
		Value value1;
		Value value2;
		const Field *field = nullptr;

		ConditionData() { }
		ConditionData(const Query::Select &, const Field *);
		ConditionData(Query::Select &&, const Field *);

		void set(Query::Select &&, const Field *);
		void set(const Query::Select &, const Field *);
	};

	struct ConflictData {
		const Field *field;
		ConditionData condition;
		Vector<const Field *> mask;
		Conflict::Flags flags = Conflict::None;

		bool isDoNothing() const { return (flags & Conflict::DoNothing) != Conflict::None; }
		bool hasCondition() const { return (flags & Conflict::WithoutCondition) == Conflict::None; }
	};

	Worker(const Scheme &, const Adapter &);
	Worker(const Scheme &, const Transaction &);
	explicit Worker(const Worker &);

	Worker(Worker &&) = delete;
	Worker &operator=(Worker &&) = delete;
	Worker &operator=(const Worker &) = delete;

	~Worker();

	template <typename Callback>
	bool perform(const Callback &) const;

	const Transaction &transaction() const;
	const Scheme &scheme() const;

	const ApplicationInterface *getApplicationInterface() const;

	void includeNone();

	template <typename T>
	Worker &include(T &&t) {
		_required.include(std::forward<T>(t));
		return *this;
	}

	template <typename T>
	Worker &exclude(T &&t) {
		_required.exclude(std::forward<T>(t));
		return *this;
	}

	void clearRequiredFields();

	bool shouldIncludeNone() const;
	bool shouldIncludeAll() const;

	Worker &asSystem();
	bool isSystem() const;

	const RequiredFields &getRequiredFields() const;
	const Map<const Field *, ConflictData> &getConflicts() const;
	const Vector<ConditionData> &getConditions() const;

public:
	Value get(uint64_t oid, UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, UpdateFlags = UpdateFlags::None);

	Value get(uint64_t oid, StringView, UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, StringView, UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, StringView, UpdateFlags = UpdateFlags::None);

	Value get(uint64_t oid, std::initializer_list<StringView> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, std::initializer_list<StringView> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, std::initializer_list<StringView> &&fields,
			UpdateFlags = UpdateFlags::None);

	Value get(uint64_t oid, std::initializer_list<const char *> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, std::initializer_list<const char *> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, std::initializer_list<const char *> &&fields,
			UpdateFlags = UpdateFlags::None);

	Value get(uint64_t oid, std::initializer_list<const Field *> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, std::initializer_list<const Field *> &&fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, std::initializer_list<const Field *> &&fields,
			UpdateFlags = UpdateFlags::None);

	Value get(uint64_t oid, SpanView<const Field *> fields, UpdateFlags = UpdateFlags::None);
	Value get(const StringView &alias, SpanView<const Field *> fields,
			UpdateFlags = UpdateFlags::None);
	Value get(const Value &id, SpanView<const Field *> fields, UpdateFlags = UpdateFlags::None);

	bool foreach (const Query &, const Callback<bool(Value &)> &, UpdateFlags = UpdateFlags::None);

	// returns Array with zero or more Dictionaries with object data or Null value
	Value select(const Query &, UpdateFlags = UpdateFlags::None);

	// returns Dictionary with single object data or Null value
	Value create(const Value &data, bool isProtected = false);
	Value create(const Value &data, UpdateFlags);
	Value create(const Value &data, UpdateFlags, const Conflict &);
	Value create(const Value &data, UpdateFlags, const Vector<Conflict> &);
	Value create(const Value &data, Conflict::Flags);
	Value create(const Value &data, const Conflict &);
	Value create(const Value &data, const Vector<Conflict> &);

	Value update(uint64_t oid, const Value &data, bool isProtected = false);
	Value update(const Value &obj, const Value &data, bool isProtected = false);

	Value update(uint64_t oid, const Value &data, UpdateFlags);
	Value update(const Value &obj, const Value &data, UpdateFlags);

	Value update(uint64_t oid, const Value &data, UpdateFlags, const Query::Select &);
	Value update(const Value &obj, const Value &data, UpdateFlags, const Query::Select &);
	Value update(uint64_t oid, const Value &data, UpdateFlags, const Vector<Query::Select> &);
	Value update(const Value &obj, const Value &data, UpdateFlags, const Vector<Query::Select> &);

	Value update(uint64_t oid, const Value &data, const Query::Select &);
	Value update(const Value &obj, const Value &data, const Query::Select &);
	Value update(uint64_t oid, const Value &data, const Vector<Query::Select> &);
	Value update(const Value &obj, const Value &data, const Vector<Query::Select> &);

	bool remove(uint64_t oid);
	bool remove(const Value &);

	size_t count();
	size_t count(const Query &);

	void touch(uint64_t id);
	void touch(const Value &obj);

public:
	Value getField(uint64_t oid, const StringView &, std::initializer_list<StringView> fields);
	Value getField(const Value &, const StringView &, std::initializer_list<StringView> fields);
	Value getField(uint64_t oid, const StringView &,
			const Set<const Field *> & = Set<const Field *>());
	Value getField(const Value &, const StringView &,
			const Set<const Field *> & = Set<const Field *>());

	Value setField(uint64_t oid, const StringView &, Value &&);
	Value setField(const Value &, const StringView &, Value &&);
	Value setField(uint64_t oid, const StringView &, InputFile &);
	Value setField(const Value &, const StringView &, InputFile &);

	bool clearField(uint64_t oid, const StringView &, Value && = Value());
	bool clearField(const Value &, const StringView &, Value && = Value());

	Value appendField(uint64_t oid, const StringView &, Value &&);
	Value appendField(const Value &, const StringView &, Value &&);

	size_t countField(uint64_t oid, const StringView &);
	size_t countField(const Value &, const StringView &);

public:
	Value getField(uint64_t oid, const Field &, std::initializer_list<StringView> fields);
	Value getField(const Value &, const Field &, std::initializer_list<StringView> fields);
	Value getField(uint64_t oid, const Field &, const Set<const Field *> & = Set<const Field *>());
	Value getField(const Value &, const Field &, const Set<const Field *> & = Set<const Field *>());

	Value setField(uint64_t oid, const Field &, Value &&);
	Value setField(const Value &, const Field &, Value &&);
	Value setField(uint64_t oid, const Field &, InputFile &);
	Value setField(const Value &, const Field &, InputFile &);

	bool clearField(uint64_t oid, const Field &, Value && = Value());
	bool clearField(const Value &, const Field &, Value && = Value());

	Value appendField(uint64_t oid, const Field &, Value &&);
	Value appendField(const Value &, const Field &, Value &&);

	size_t countField(uint64_t oid, const Field &);
	size_t countField(const Value &, const Field &);

protected:
	friend class Scheme;

	Set<const Field *> getFieldSet(const Field &f, std::initializer_list<StringView> il) const;

	bool addConflict(const Conflict &);
	bool addConflict(const Vector<Conflict> &);

	bool addCondition(const Query::Select &);
	bool addCondition(const Vector<Query::Select> &);

	Value reduceGetQuery(const Query &query, bool cached);

	Map<const Field *, ConflictData> _conflict;
	Vector<ConditionData> _conditions;
	RequiredFields _required;
	const Scheme *_scheme = nullptr;
	Transaction _transaction;
	bool _isSystem = false;
};

struct FieldResolver {
	FieldResolver(const Scheme &scheme, const Worker &w, const Query &q);
	FieldResolver(const Scheme &scheme, const Worker &w);
	FieldResolver(const Scheme &scheme, const Query &q);
	FieldResolver(const Scheme &scheme, const Query &q, const Set<const Field *> &);
	FieldResolver(const Scheme &scheme);
	FieldResolver(const Scheme &scheme, const Set<const Field *> &);

	bool shouldResolveFields() const;
	bool hasIncludesOrExcludes() const;
	bool shouldIncludeAll() const;
	bool shouldIncludeField(const Field &f) const;
	bool shouldExcludeField(const Field &f) const;
	bool isFieldRequired(const Field &f) const;
	Vector<const Field *> getVirtuals() const;
	bool readFields(const Worker::FieldCallback &cb, bool isSimpleGet = false);

	void include(StringView);

	const Scheme *scheme = nullptr;
	const Worker::RequiredFields *required = nullptr;
	const Query *query = nullptr;
	Vector<const Field *> requiredFields;
};

template <typename Callback>
inline bool Worker::perform(const Callback &cb) const {
	static_assert(std::is_invocable_v<Callback, const Transaction &>, "Invalid callback type");
	static_assert(std::is_same_v<bool, std::invoke_result_t<Callback, const Transaction &>>,
			"Invalid callback return type");
	return _transaction.perform([&, this]() -> bool { return cb(_transaction); });
}

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDBWORKER_H_ */
