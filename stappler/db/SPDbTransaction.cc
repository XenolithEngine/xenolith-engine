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

#include "SPDbTransaction.h"

#include "SPDbAdapter.h"
#include "SPDbScheme.h"
#include "SPDbWorker.h"

#include <sprt/runtime/mem/userdata.h>

namespace STAPPLER_VERSIONIZED stappler::db {

Transaction::Op Transaction::getTransactionOp(Action a) {
	switch (a) {
	case Action::Get: return FieldGet; break;
	case Action::Set: return FieldSet; break;
	case Action::Append: return FieldAppend; break;
	case Action::Remove: return FieldClear; break;
	case Action::Count: return FieldCount; break;
	}
	return None;
}

Transaction Transaction::acquire(const Adapter &adapter) {
	auto pool = pool::acquire();

	if (auto d = pool::get<Transaction::Data>(pool, adapter.getTransactionKey())) {
		auto ret = Transaction(d);
		ret.retain();
		return ret;
	} else {
		d = new (pool) Transaction::Data{adapter, pool};
		d->role = AccessRoleId::System;
		pool::store(pool, d, adapter.getTransactionKey());
		auto ret = Transaction(d);
		ret.retain();

		if (adapter.getApplicationInterface()) {
			adapter.getApplicationInterface()->initTransaction(ret);
		}

		return ret;
	}

	return Transaction(nullptr);
}

Transaction Transaction::acquireIfExists() { return acquireIfExists(pool::acquire()); }

Transaction Transaction::acquireIfExists(stappler::memory::pool_t *pool) {
	auto stack = pool::get<Stack>(pool, config::STORAGE_TRANSACTION_STACK_KEY);
	if (!stack) {
		return Transaction(nullptr);
	}

	return stack->stack.empty() ? Transaction(nullptr) : Transaction(stack->stack.back());
}

void Transaction::retain() const {
	auto p = pool::acquire();
	auto stack = pool::get<Stack>(p, config::STORAGE_TRANSACTION_STACK_KEY);
	if (!stack) {
		stack = new (p) Stack;
		pool::store(p, stack, config::STORAGE_TRANSACTION_STACK_KEY);
	}

	stack->stack.emplace_back(_data);
}

void Transaction::release() const {
	auto p = pool::acquire();
	auto stack = pool::get<Stack>(p, config::STORAGE_TRANSACTION_STACK_KEY);
	if (stack) {
		auto it = std::find(stack->stack.rbegin(), stack->stack.rend(), _data);
		if (it != stack->stack.rend()) {
			stack->stack.erase(std::next(it).base());
		}
	}
}

Transaction::Transaction(nullptr_t) : Transaction((Data *)nullptr) { }

Transaction::Transaction(Data *d) : _data(d) { }

void Transaction::setRole(AccessRoleId id) const { _data->role = id; }
AccessRoleId Transaction::getRole() const { return _data->role; }

const Value &Transaction::setValue(const StringView &key, Value &&val) {
	return _data->data.emplace(key.str<Interface>(), sp::move(val)).first->second;
}

const Value &Transaction::getValue(const StringView &key) const {
	auto it = _data->data.find(key);
	if (it != _data->data.end()) {
		return it->second;
	}
	return Value::Null;
}

Value Transaction::setObject(int64_t id, Value &&val) const {
	Value ret;
	memory::perform_conditional([&] {
		ret = _data->objects.emplace(id, sp::move(val)).first->second;
	}, _data->objects.get_allocator());
	return ret;
}

Value Transaction::getObject(int64_t id) const {
	auto it = _data->objects.find(id);
	if (it != _data->objects.end()) {
		return it->second;
	}
	return Value::Null;
}

void Transaction::setStatus(int value) { _data->status = value; }
int Transaction::getStatus() const { return _data->status; }

void Transaction::setAdapter(const Adapter &a) { _data->adapter = a; }
const Adapter &Transaction::getAdapter() const { return _data->adapter; }

bool Transaction::isInTransaction() const { return _data->adapter.isInTransaction(); }

TransactionStatus Transaction::getTransactionStatus() const {
	return _data->adapter.getTransactionStatus();
}

struct DataHolder {
	DataHolder(Transaction::Data *data, Worker &w) : _data(data) {
		_tmpRole = data->role;
		if (w.isSystem()) {
			data->role = AccessRoleId::System;
		}
	}

	~DataHolder() { _data->role = _tmpRole; }

	Transaction::Data *_data = nullptr;
	AccessRoleId _tmpRole = AccessRoleId::Nobody;
};

bool Transaction::foreach (Worker &w, const Query &query, const Callback<bool(Value &)> &cb) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.foreach (w, query, cb);
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Select)) {
		return false;
	}

	auto r = w.scheme().getAccessRole(_data->role);
	auto d = w.scheme().getAccessRole(AccessRoleId::Default);

	if ((d && d->onSelect && !d->onSelect(w, query))
			|| (r && r->onSelect && !r->onSelect(w, query))) {
		return false;
	}

	return _data->adapter.foreach (w, query, [&, this](Value &val) -> bool {
		if (processReturnObject(w.scheme(), val)) {
			return cb(val);
		}
		return true;
	});
}

Value Transaction::select(Worker &w, const Query &query) const {
	if (!w.scheme().hasAccessControl()) {
		auto val = _data->adapter.select(w, query);
		if (val.empty()) {
			return Value();
		}
		return val;
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Select)) {
		return Value();
	}

	auto r = w.scheme().getAccessRole(_data->role);
	auto d = w.scheme().getAccessRole(AccessRoleId::Default);

	if ((d && d->onSelect && !d->onSelect(w, query))
			|| (r && r->onSelect && !r->onSelect(w, query))) {
		return Value();
	}

	auto val = _data->adapter.select(w, query);

	auto &arr = val.asArray();
	auto it = arr.begin();
	while (it != arr.end()) {
		if (processReturnObject(w.scheme(), *it)) {
			++it;
		} else {
			it = arr.erase(it);
		}
	}

	if (val.empty()) {
		return Value();
	}
	return val;
}

size_t Transaction::count(Worker &w, const Query &q) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.count(w, q);
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Count)) {
		return 0;
	}

	auto r = w.scheme().getAccessRole(_data->role);
	auto d = w.scheme().getAccessRole(AccessRoleId::Default);

	if ((d && d->onCount && !d->onCount(w, q)) || (r && r->onCount && !r->onCount(w, q))) {
		return 0;
	}

	return _data->adapter.count(w, q);
}

bool Transaction::remove(Worker &w, uint64_t oid) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.remove(w, oid);
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Remove)) {
		return false;
	}

	auto r = w.scheme().getAccessRole(_data->role);
	auto d = w.scheme().getAccessRole(AccessRoleId::Default);

	bool hasR = (r && r->onRemove);
	bool hasD = (d && d->onRemove);

	if (hasR || hasD) {
		if (auto obj = acquireObject(w.scheme(), oid)) {
			if ((!hasD || d->onRemove(w, obj)) && (!hasR || r->onRemove(w, obj))) {
				return _data->adapter.remove(w, oid);
			}
		}
		return false;
	}

	return _data->adapter.remove(w, oid);
}

Value Transaction::create(Worker &w, Value &data) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.create(w, data);
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Create)) {
		return Value();
	}

	Value ret;
	if (perform([&, this] {
		auto r = w.scheme().getAccessRole(_data->role);
		auto d = w.scheme().getAccessRole(AccessRoleId::Default);

		if (data.isArray()) {
			auto &arr = data.asArray();
			auto it = arr.begin();
			while (it != arr.end()) {
				if ((d && d->onCreate && !d->onCreate(w, *it))
						|| (r && r->onCreate && !r->onCreate(w, *it))) {
					it = arr.erase(it);
				} else {
					++it;
				}
			}

			if (auto val = _data->adapter.create(w, data)) {
				auto &arr = val.asArray();
				auto it = arr.begin();

				while (it != arr.end()) {
					if ((d && d->onCreate && !d->onCreate(w, *it))
							|| (r && r->onCreate && !r->onCreate(w, *it))) {
						it = arr.erase(it);
					} else {
						++it;
					}
				}

				ret = !arr.empty() ? sp::move(val) : Value(true);
				return true; // if user can not see result - return success but with no object
			}
		} else {
			if ((d && d->onCreate && !d->onCreate(w, data))
					|| (r && r->onCreate && !r->onCreate(w, data))) {
				return false;
			}

			if (auto val = _data->adapter.create(w, data)) {
				ret = processReturnObject(w.scheme(), val) ? sp::move(val) : Value(true);
				return true; // if user can not see result - return success but with no object
			}
		}
		return false;
	})) {
		return ret;
	}
	return Value();
}

Value Transaction::save(Worker &w, uint64_t oid, Value &obj, Value &patch,
		Set<const Field *> &fields) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.save(w, oid, obj, patch, fields);
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Save)) {
		return Value();
	}

	Value ret;
	if (perform([&, this] {
		auto r = w.scheme().getAccessRole(_data->role);
		auto d = w.scheme().getAccessRole(AccessRoleId::Default);

		bool hasR = (r && r->onSave);
		bool hasD = (d && d->onSave);

		if (hasR || hasD) {
			if ((hasD && !d->onSave(w, obj, patch, fields))
					|| (hasR && !r->onSave(w, obj, patch, fields))) {
				return false;
			}

			if (auto val = _data->adapter.save(w, oid, obj, patch, fields)) {
				ret = processReturnObject(w.scheme(), val) ? sp::move(val) : Value(true);
				return true;
			}
			return false;
		}

		if (auto val = _data->adapter.save(w, oid, obj, patch, fields)) {
			ret = processReturnObject(w.scheme(), val) ? sp::move(val) : Value(true);
			return true;
		}
		return false;
	})) {
		return ret;
	}
	return Value();
}

Value Transaction::patch(Worker &w, uint64_t oid, Value &data) const {
	Value tmp;
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.save(w, oid, tmp, data, Set<const Field *>());
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), Patch)) {
		return Value();
	}

	Value ret;
	if (perform([&, this] {
		auto r = w.scheme().getAccessRole(_data->role);
		auto d = w.scheme().getAccessRole(AccessRoleId::Default);
		if ((d && d->onPatch && !d->onPatch(w, oid, data))
				|| (r && r->onPatch && !r->onPatch(w, oid, data))) {
			return false;
		}

		if (auto val = _data->adapter.save(w, oid, tmp, data, Set<const Field *>())) {
			ret = processReturnObject(w.scheme(), val) ? sp::move(val) : Value(true);
			return true;
		}
		return false;
	})) {
		return ret;
	}
	return Value();
}

Value Transaction::field(Action a, Worker &w, uint64_t oid, const Field &f, Value &&patch) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.field(a, w, oid, f, sp::move(patch));
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), getTransactionOp(a), &f)) {
		return Value();
	}

	auto r = w.scheme().getAccessRole(_data->role);
	auto d = w.scheme().getAccessRole(AccessRoleId::Default);

	if ((r && r->onField) || (d && d->onField) || f.getSlot()->readFilterFn) {
		if (auto obj = acquireObject(w.scheme(), oid)) {
			return field(a, w, obj, f, sp::move(patch));
		}
		return Value();
	}

	Value ret;
	if (perform([&, this] {
		ret = _data->adapter.field(a, w, oid, f, sp::move(patch));
		return true;
	})) {
		if (a != Action::Remove) {
			if (processReturnField(w.scheme(), Value(oid), f, ret)) {
				return ret;
			}
		} else {
			return ret;
		}
	}
	return Value();
}
Value Transaction::field(Action a, Worker &w, const Value &obj, const Field &f,
		Value &&patch) const {
	if (!w.scheme().hasAccessControl()) {
		return _data->adapter.field(a, w, obj, f, sp::move(patch));
	}

	DataHolder h(_data, w);

	if (!isOpAllowed(w.scheme(), getTransactionOp(a), &f)) {
		return Value();
	}

	Value ret;
	if (perform([&, this] {
		auto r = w.scheme().getAccessRole(_data->role);
		auto d = w.scheme().getAccessRole(AccessRoleId::Default);

		if ((d && d->onField && !d->onField(a, w, obj, f, patch))
				|| (r && r->onField && !r->onField(a, w, obj, f, patch))) {
			return false;
		}

		ret = _data->adapter.field(a, w, obj, f, sp::move(patch));
		return true;
	})) {
		if (a != Action::Remove) {
			if (processReturnField(w.scheme(), obj, f, ret)) {
				return ret;
			}
		} else {
			return ret;
		}
	}
	return Value();
}

bool Transaction::removeFromView(const Scheme &scheme, const FieldView &field, uint64_t oid,
		const Value &obj) const {
	if (!isOpAllowed(scheme, RemoveFromView)) {
		return false;
	}

	return _data->adapter.removeFromView(field, &scheme, oid);
}

bool Transaction::addToView(const Scheme &scheme, const FieldView &field, uint64_t oid,
		const Value &obj, const Value &viewObj) const {
	if (!isOpAllowed(scheme, AddToView)) {
		return false;
	}

	return _data->adapter.addToView(field, &scheme, oid, viewObj);
}

int64_t Transaction::getDeltaValue(const Scheme &scheme) {
	if (!isOpAllowed(scheme, Delta)) {
		return false;
	}

	return _data->adapter.getDeltaValue(scheme);
}

int64_t Transaction::getDeltaValue(const Scheme &scheme, const FieldView &f, uint64_t id) {
	if (!isOpAllowed(scheme, DeltaView)) {
		return false;
	}

	return _data->adapter.getDeltaValue(scheme, f, id);
}

Vector<int64_t> Transaction::performQueryListForIds(const QueryList &list, size_t count) const {
	for (auto &it : list.getItems()) {
		if (!isOpAllowed(*it.scheme, Id)) {
			return Vector<int64_t>();
		}
	}

	return _data->adapter.performQueryListForIds(list, count);
}

Value Transaction::performQueryList(const QueryList &list, size_t count, bool forUpdate) const {
	count = (count == stappler::maxOf<size_t>()) ? list.size() : count;
	for (auto &it : list.getItems()) {
		if (!isOpAllowed(*it.scheme, Id)) {
			return Value();
		}
	}

	if (!isOpAllowed(*list.getScheme(), Select)) {
		return Value();
	}

	Value vals;
	auto &t = list.getContinueToken();
	if (t && count == list.size()) {
		if (count > 1) {
			auto &item = list.getItems().at(list.size() - 2);
			return performQueryListField(list, *item.field);
		} else {
			auto q = list.getItems().back().query;
			vals = t.perform(*list.getItems().back().scheme, *this, q,
					t.hasFlag(ContinueToken::Inverted) ? Ordering::Descending
													   : Ordering::Ascending);
		}
	} else {
		vals = _data->adapter.performQueryList(list, count, forUpdate);
	}

	if (vals) {
		auto &arr = vals.asArray();
		auto it = arr.begin();
		while (it != arr.end()) {
			if (processReturnObject(*list.getScheme(), *it)) {
				++it;
			} else {
				it = arr.erase(it);
			}
		}
	}
	return vals;
}

Value Transaction::performQueryListField(const QueryList &list, const Field &f) const {
	auto count = list.size();
	if (f.getType() == Type::View || f.getType() == Type::Set) {
		count -= 1;
	}

	for (auto &it : list.getItems()) {
		if (!isOpAllowed(*it.scheme, Id)) {
			return Value();
		}
	}

	if (!isOpAllowed(*list.getScheme(), FieldGet, &f)) {
		return Value();
	}

	auto ids = performQueryListForIds(list, count);
	if (ids.size() == 1) {
		auto id = ids.front();
		auto scheme = list.getItems().at(count - 1).scheme;
		if (f.getType() == Type::View || f.getType() == Type::Set) {
			db::Query q = db::Query::field(id, f.getName(), list.getItems().back().query);
			Worker w(*scheme, *this);

			Value obj(id);
			auto r = scheme->getAccessRole(_data->role);
			auto d = scheme->getAccessRole(AccessRoleId::Default);

			if ((r && r->onField) || (d && d->onField) || f.getSlot()->readFilterFn) {
				if ((obj = acquireObject(w.scheme(), id))) {
					Value tmp;
					if (!d->onField(Action::Get, w, obj, f, tmp)
							|| !r->onField(Action::Get, w, obj, f, tmp)) {
						return Value();
					}
				}
			}

			Value val;
			if (auto &t = list.getContinueToken()) {
				val = t.perform(*scheme, *this, q,
						t.hasFlag(ContinueToken::Inverted) ? Ordering::Descending
														   : Ordering::Ascending);
			} else {
				val = scheme->select(*this, q);
			}
			if (val) {
				if (!processReturnField(*scheme, obj, f, val)) {
					return Value();
				}
			}
			return val;
		} else {
			if (auto obj = acquireObject(*scheme, id)) {
				return scheme->getProperty(*this, obj, f,
						list.getItems().at(list.size() - 1).getQueryFields());
			}
		}
	}

	return Value();
}

void Transaction::scheduleAutoField(const Scheme &scheme, const Field &field, uint64_t id) const {
	mem_pool::perform([&] {
		if (!_data->delayedTasks) {
			_data->delayedTasks = new (_data->pool) Vector<TaskData *>;
		}

		TaskData *data = nullptr;

		for (auto &it : *_data->delayedTasks) {
			if (it->scheme == &scheme && it->field == &field) {
				data = it;
			}
		}

		if (!data) {
			auto d = new (_data->pool) TaskData;
			d->field = &field;
			d->scheme = &scheme;

			_data->delayedTasks->emplace_back(d);

			data = d;
		}

		data->objects.emplace(id);
	}, _data->pool);
}

bool Transaction::beginTransaction() const { return _data->adapter.beginTransaction(); }

static void Transaction_runAutoFields(const Transaction &t, const Vector<uint64_t> &vec,
		const Scheme &scheme, const Field &field) {
	auto &defs = field.getSlot()->autoField;
	if (defs.defaultFn) {
		auto includeSelf =
				(std::find(defs.requireFields.begin(), defs.requireFields.end(), field.getName())
						== defs.requireFields.end());
		for (auto &id : vec) {
			Query q;
			q.select(id);
			for (auto &req : defs.requireFields) { q.include(req); }
			if (includeSelf) {
				q.include(field.getName());
			}

			auto objs = scheme.select(t, q);
			if (auto obj = objs.getValue(0)) {
				auto newValue = defs.defaultFn(obj);
				if (newValue != obj.getValue(field.getName())) {
					Value patch;
					patch.setValue(sp::move(newValue), field.getName().str<Interface>());
					scheme.update(t, obj, patch, UpdateFlags::Protected | UpdateFlags::NoReturn);
				}
			}
		}
	}
}


bool Transaction::endTransaction() const {
	if (_data->adapter.endTransaction()) {
		if (!_data->adapter.isInTransaction()) {
			if (_data->delayedTasks) {
				for (auto &it : *_data->delayedTasks) {
					_data->adapter.getApplicationInterface()->scheduleAyncDbTask(
							[d = it](stappler::memory::pool_t *p)
									-> Function<void(const Transaction &t)> {
						auto vec = new (p) Vector<uint64_t>(p);
						for (auto &it : d->objects) { vec->push_back(it); }

						return [vec, scheme = d->scheme, field = d->field](const Transaction &t) {
							Transaction_runAutoFields(t, *vec, *scheme, *field);
						};
					});
				}
				_data->delayedTasks = nullptr;
			}

			clearObjectStorage();
		}
		return true;
	}
	return false;
}

void Transaction::cancelTransaction() const { _data->adapter.cancelTransaction(); }

void Transaction::clearObjectStorage() const { _data->objects.clear(); }

static bool Transaction_processFields(const Scheme &scheme, const Value &val, Value &obj,
		const Map<String, Field> &vec) {
	if (obj.isDictionary()) {
		auto &dict = obj.asDict();
		auto it = dict.begin();
		while (it != dict.end()) {
			auto f_it = vec.find(it->first);
			if (f_it != vec.end()) {
				auto slot = f_it->second.getSlot();
				if (slot->readFilterFn) {
					if (!slot->readFilterFn(scheme, val, it->second)) {
						it = dict.erase(it);
						continue;
					}
				}

				if (slot->type == Type::Extra) {
					auto extraSlot = static_cast<const FieldExtra *>(slot);
					if (!Transaction_processFields(scheme, val, it->second, extraSlot->fields)) {
						it = dict.erase(it);
						continue;
					}
				}
			}
			++it;
		}
	}

	return true;
}

bool Transaction::processReturnObject(const Scheme &scheme, Value &val) const {
	if (!scheme.hasAccessControl()) {
		return true;
	}

	auto r = scheme.getAccessRole(_data->role);
	auto d = scheme.getAccessRole(AccessRoleId::Default);

	if ((d && d->onReturn && !d->onReturn(scheme, val))
			|| (r && r->onReturn && !r->onReturn(scheme, val))) {
		return false;
	}

	return Transaction_processFields(scheme, val, val, scheme.getFields());
}

bool Transaction::processReturnField(const Scheme &scheme, const Value &obj, const Field &field,
		Value &val) const {
	if (!scheme.hasAccessControl()) {
		return true;
	}

	auto slot = field.getSlot();
	if (slot->readFilterFn) {
		if (obj.isInteger()) {
			if (auto tmpObj = acquireObject(scheme, obj.getInteger())) {
				if (!slot->readFilterFn(scheme, tmpObj, val)) {
					return false;
				}
			} else {
				return false;
			}
		} else {
			if (!slot->readFilterFn(scheme, obj, val)) {
				return false;
			}
		}
	}

	auto r = scheme.getAccessRole(_data->role);
	auto d = scheme.getAccessRole(AccessRoleId::Default);

	if ((d && d->onReturnField && !d->onReturnField(scheme, field, val))
			|| (r && r->onReturnField && !r->onReturnField(scheme, field, val))) {
		return false;
	}

	if (field.getType() == Type::Object || field.getType() == Type::Set
			|| field.getType() == Type::View) {
		if (auto nextScheme = field.getForeignScheme()) {
			if (val.isDictionary()) {
				if (!processReturnObject(*nextScheme, val)) {
					return false;
				}
			} else if (val.isArray()) {
				auto &arr = val.asArray();
				auto it = arr.begin();
				while (it != arr.end()) {
					if (processReturnObject(*nextScheme, *it)) {
						++it;
					} else {
						it = arr.erase(it);
					}
				}
			}
		}
	}
	return true;
}

bool Transaction::isOpAllowed(const Scheme &scheme, Op op, const Field *f) const {
	if (!scheme.hasAccessControl()) {
		return true;
	}

	if (auto r = scheme.getAccessRole(_data->role)) {
		return r->operations.test(stappler::toInt(op));
	}
	switch (op) {
	case Op::None:
	case Op::Max: return false; break;
	case Op::Id:
	case Op::Select:
	case Op::Count:
	case Op::Delta:
	case Op::DeltaView:
	case Op::FieldGet: return true; break;
	case Op::Remove:
	case Op::Create:
	case Op::Save:
	case Op::Patch:
	case Op::FieldSet:
	case Op::FieldAppend:
	case Op::FieldClear:
	case Op::FieldCount:
	case Op::RemoveFromView:
	case Op::AddToView:
		return _data->role == AccessRoleId::Admin || _data->role == AccessRoleId::System;
		break;
	}
	return false;
}

Transaction::Data::Data(const Adapter &adapter, memory::pool_t *p) : adapter(adapter), pool(p) { }


Value Transaction::acquireObject(const Scheme &scheme, uint64_t oid) const {
	Value ret;
	memory::perform_conditional([&] {
		auto it = _data->objects.find(oid);
		if (it == _data->objects.end()) {
			if (auto obj = Worker(scheme, *this).asSystem().get(oid)) {
				ret = _data->objects.emplace(oid, sp::move(obj)).first->second;
			}
		} else {
			ret = it->second;
		}
	}, _data->objects.get_allocator());
	return ret;
}

AccessRole &AccessRole::define(Transaction::Op op) {
	operations.set(op);
	return *this;
}
AccessRole &AccessRole::define(AccessRoleId id) {
	users.set(stappler::toInt(id));
	return *this;
}
AccessRole &AccessRole::define() { return *this; }
AccessRole &AccessRole::define(OnSelect &&val) {
	if (val.get()) {
		operations.set(Transaction::Select);
	}
	onSelect = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnCount &&val) {
	if (val.get()) {
		operations.set(Transaction::Count);
	}
	onCount = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnCreate &&val) {
	if (val.get()) {
		operations.set(Transaction::Create);
	}
	onCreate = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnPatch &&val) {
	if (val.get()) {
		operations.set(Transaction::Patch);
	}
	onPatch = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnSave &&val) {
	if (val.get()) {
		operations.set(Transaction::Save);
	}
	onSave = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnRemove &&val) {
	if (val.get()) {
		operations.set(Transaction::Remove);
	}
	onRemove = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnField &&val) {
	onField = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnReturn &&val) {
	onReturn = sp::move(val.get());
	return *this;
}
AccessRole &AccessRole::define(OnReturnField &&val) {
	onReturnField = sp::move(val.get());
	return *this;
}

} // namespace stappler::db
