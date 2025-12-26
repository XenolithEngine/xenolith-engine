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

#ifndef STAPPLER_DB_SPDBTRANSACTION_H_
#define STAPPLER_DB_SPDBTRANSACTION_H_

#include "SPDbAdapter.h"
#include "SPDbField.h"
#include "SPDbQueryList.h"

namespace STAPPLER_VERSIONIZED stappler::db {

enum class AccessRoleId {
	Nobody = 0,
	Authorized = 1,
	UserDefined1,
	UserDefined2,
	UserDefined3,
	UserDefined4,
	UserDefined5,
	UserDefined6,
	UserDefined7,
	UserDefined8,
	UserDefined9,
	UserDefined10,
	UserDefined11,
	Admin,
	System,
	Default,
	Max = 16,
};

class SP_PUBLIC Transaction : public AllocBase {
public:
	enum Op {
		None = 0,
		Id,
		Select,
		Count,
		Remove,
		Create,
		Save,
		Patch,
		FieldGet,
		FieldSet,
		FieldAppend,
		FieldClear,
		FieldCount,
		Delta,
		DeltaView,
		RemoveFromView,
		AddToView,
		Max,
	};

	struct TaskData : AllocPool {
		const Scheme *scheme = nullptr;
		const Field *field = nullptr;
		Set<uint64_t> objects;
	};

	struct Data : AllocPool {
		Adapter adapter;
		pool_t * pool;
		Map<String, Value> data;
		int status = 0;

		Vector<TaskData *> *delayedTasks = nullptr;

		mutable Map<int64_t, Value> objects;
		mutable AccessRoleId role = AccessRoleId::Nobody;

		Data(const Adapter &, memory::pool_t * = nullptr);
	};

	struct Stack : AllocPool {
		Vector<Data *> stack;
	};

	static Op getTransactionOp(Action);

	static Transaction acquire(const Adapter &);

	static Transaction acquireIfExists();
	static Transaction acquireIfExists(memory::pool_t *);

	Transaction(nullptr_t);

	void setRole(AccessRoleId) const;
	AccessRoleId getRole() const;

	void setStatus(int);
	int getStatus() const;

	const Value &setValue(const StringView &, Value &&);
	const Value &getValue(const StringView &) const;

	Value setObject(int64_t, Value &&) const;
	Value getObject(int64_t) const;

	void setAdapter(const Adapter &);
	const Adapter &getAdapter() const;

	explicit operator bool () const { return _data != nullptr && _data->adapter; }

	Value acquireObject(const Scheme &, uint64_t oid) const;

	bool perform(const Callback<bool()> & cb) const;
	bool performAsSystem(const Callback<bool()> & cb) const;

	bool isInTransaction() const;
	TransactionStatus getTransactionStatus() const;

	bool foreach(Worker &, const Query &, const Callback<bool(Value &)> &) const;

	// returns Array with zero or more Dictionaries with object data or Null value
	Value select(Worker &, const Query &) const;

	size_t count(Worker &, const Query &) const;

	bool remove(Worker &t, uint64_t oid) const;

	Value create(Worker &, Value &data) const;
	Value save(Worker &, uint64_t oid, Value &obj, Value &patch, Set<const Field *> &fields) const;
	Value patch(Worker &, uint64_t oid, Value &data) const;

	Value field(Action, Worker &, uint64_t oid, const Field &, Value && = Value()) const;
	Value field(Action, Worker &, const Value &, const Field &, Value && = Value()) const;

	bool removeFromView(const Scheme &, const FieldView &, uint64_t oid, const Value &obj) const;
	bool addToView(const Scheme &, const FieldView &, uint64_t oid, const Value &obj, const Value &viewObj) const;

	int64_t getDeltaValue(const Scheme &); // scheme-based delta
	int64_t getDeltaValue(const Scheme &, const FieldView &, uint64_t); // view-based delta

	Vector<int64_t> performQueryListForIds(const QueryList &, size_t count = stappler::maxOf<size_t>()) const;
	Value performQueryList(const QueryList &, size_t count = stappler::maxOf<size_t>(), bool forUpdate = false) const;
	Value performQueryListField(const QueryList &, const Field &) const;

	void scheduleAutoField(const Scheme &, const Field &, uint64_t id) const;

	void retain() const;
	void release() const;

protected:
	struct TransactionGuard {
		TransactionGuard(const Transaction &t) : _t(&t) { _t->retain(); }
		~TransactionGuard() { _t->release(); }

		const Transaction *_t;
	};

	friend struct TransactionGuard;
	friend class Worker; // for transaction start|stop and role redefinition

	bool beginTransaction() const;
	bool endTransaction() const;
	void cancelTransaction() const;

	void clearObjectStorage() const;

	bool processReturnObject(const Scheme &, Value &) const;
	bool processReturnField(const Scheme &, const Value &obj, const Field &, Value &) const;

	bool isOpAllowed(const Scheme &, Op, const Field * = nullptr) const;

	Transaction(Data *);

	Data *_data = nullptr;
};

struct SP_PUBLIC AccessRole : public AllocBase {
	using OnSelect = stappler::ValueWrapper<Function<bool(Worker &, const Query &)>, class OnSelectTag>;
	using OnCount = stappler::ValueWrapper<Function<bool(Worker &, const Query &)>, class OnCountTag>;
	using OnCreate = stappler::ValueWrapper<Function<bool(Worker &, Value &obj)>, class OnCreateTag>;
	using OnPatch = stappler::ValueWrapper<Function<bool(Worker &, int64_t id, Value &obj)>, class OnPatchTag>;
	using OnSave = stappler::ValueWrapper<Function<bool(Worker &, const Value &obj, Value &patch, Set<const Field *> &fields)>, class OnSaveTag>;
	using OnRemove = stappler::ValueWrapper<Function<bool(Worker &, const Value &)>, class OnRemoveTag>;
	using OnField = stappler::ValueWrapper<Function<bool(Action, Worker &, const Value &, const Field &, Value &)>, class OnFieldTag>;
	using OnReturn = stappler::ValueWrapper<Function<bool(const Scheme &, Value &)>, class OnReturnTag>;
	using OnReturnField = stappler::ValueWrapper<Function<bool(const Scheme &, const Field &, Value &)>, class OnReturnFieldTag>;

	template <typename ... Args>
	static AccessRole Empty(Args && ... args);

	template <typename ... Args>
	static AccessRole Default(Args && ... args);

	template <typename ... Args>
	static AccessRole Admin(Args && ... args);

	template <typename T, typename ... Args>
	AccessRole &define(T &&, Args && ... args);

	AccessRole &define();
	AccessRole &define(AccessRoleId);
	AccessRole &define(Transaction::Op);
	AccessRole &define(OnSelect &&);
	AccessRole &define(OnCount &&);
	AccessRole &define(OnCreate &&);
	AccessRole &define(OnPatch &&);
	AccessRole &define(OnSave &&);
	AccessRole &define(OnRemove &&);
	AccessRole &define(OnField &&);
	AccessRole &define(OnReturn &&);
	AccessRole &define(OnReturnField &&);

	std::bitset<stappler::toInt(AccessRoleId::Max)> users;
	std::bitset<stappler::toInt(Transaction::Op::Max)> operations;

	Function<bool(Worker &, const Query &)> onSelect;
	Function<bool(Worker &, const Query &)> onCount;

	Function<bool(Worker &, Value &obj)> onCreate;
	Function<bool(Worker &, int64_t id, Value &obj)> onPatch;
	Function<bool(Worker &, const Value &obj, Value &patch, Set<const Field *> &fields)> onSave;
	Function<bool(Worker &, const Value &)> onRemove;

	Function<bool(Action, Worker &, const Value &, const Field &, Value &)> onField;

	Function<bool(const Scheme &, Value &)> onReturn;
	Function<bool(const Scheme &, const Field &, Value &)> onReturnField;
};

inline bool Transaction::perform(const Callback<bool()> &cb) const {
	TransactionGuard g(*this);

	if (isInTransaction()) {
		if (!cb()) {
			cancelTransaction();
		} else {
			return true;
		}
	} else {
		if (beginTransaction()) {
			if (!cb()) {
				cancelTransaction();
			}
			return endTransaction();
		}
	}
	return false;
}

inline bool Transaction::performAsSystem(const Callback<bool()> &cb) const {
	auto tmpRole = getRole();
	setRole(AccessRoleId::System);
	auto ret = perform(cb);
	setRole(tmpRole);
	return ret;
}

template <typename T, typename ... Args>
inline AccessRole &AccessRole::define(T &&v, Args && ... args) {
	define(std::forward<T>(v));
	define(std::forward<Args>(args)...);
	return *this;
}

template <typename ... Args>
AccessRole AccessRole::Empty(Args && ... args) {
	AccessRole ret;
	ret.define(std::forward<Args>(args)...);
	return ret;
}

template <typename ... Args>
AccessRole AccessRole::Default(Args && ... args) {
	AccessRole ret;

	ret.operations.set(Transaction::Op::Id);
	ret.operations.set(Transaction::Op::Select);
	ret.operations.set(Transaction::Op::Count);
	ret.operations.set(Transaction::Op::Delta);
	ret.operations.set(Transaction::Op::DeltaView);
	ret.operations.set(Transaction::Op::FieldGet);
	ret.operations.set(Transaction::Op::FieldCount);

	ret.define(std::forward<Args>(args)...);

	return ret;
}

template <typename ... Args>
AccessRole AccessRole::Admin(Args && ... args) {
	AccessRole ret;

	ret.operations.set(Transaction::Op::Id);
	ret.operations.set(Transaction::Op::Select);
	ret.operations.set(Transaction::Op::Count);
	ret.operations.set(Transaction::Op::Delta);
	ret.operations.set(Transaction::Op::DeltaView);
	ret.operations.set(Transaction::Op::FieldGet);

	ret.operations.set(Transaction::Op::Remove);
	ret.operations.set(Transaction::Op::Create);
	ret.operations.set(Transaction::Op::Save);
	ret.operations.set(Transaction::Op::Patch);
	ret.operations.set(Transaction::Op::FieldSet);
	ret.operations.set(Transaction::Op::FieldAppend);
	ret.operations.set(Transaction::Op::FieldClear);
	ret.operations.set(Transaction::Op::FieldCount);
	ret.operations.set(Transaction::Op::RemoveFromView);
	ret.operations.set(Transaction::Op::AddToView);

	ret.define(std::forward<Args>(args)...);

	return ret;
}

}

#endif /* STAPPLER_DB_SPDBTRANSACTION_H_ */
