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

#include "SPDbWorker.h"
#include "SPDbFile.h"
#include "SPDbScheme.h"
#include "SPValid.h"

namespace STAPPLER_VERSIONIZED stappler::db {

Conflict Conflict::update(StringView f) {
	return Conflict(f, Query::Select(), Conflict::Flags::WithoutCondition);
}

Conflict::Conflict(Conflict::Flags f): flags(f) { }

Conflict::Conflict(StringView field, Query::Select &&cond, Flags f)
: field(field.str<Interface>()), condition(sp::move(cond)), flags(f) { }

Conflict::Conflict(StringView field, Query::Select &&cond, Vector<String> &&mask)
: field(field.str<Interface>()), condition(sp::move(cond)), mask(sp::move(mask)) { }

Conflict &Conflict::setFlags(Flags f) {
	flags = f;
	return *this;
}

static void prepareGetQuery(Query &query, uint64_t oid, bool forUpdate) {
	query.select(oid);
	if (forUpdate) {
		query.forUpdate();
	}
}

static void prepareGetQuery(Query &query, const StringView &alias, bool forUpdate) {
	query.select(alias);
	if (forUpdate) {
		query.forUpdate();
	}
}

void Worker::RequiredFields::clear() {
	includeFields.clear();
	excludeFields.clear();
	includeNone = false;
}
void Worker::RequiredFields::reset(const Scheme &s) {
	clear();
	scheme = &s;
}

void Worker::RequiredFields::include(std::initializer_list<StringView> il) {
	for (auto &it : il) {
		include(it);
	}
}
void Worker::RequiredFields::include(const Set<const Field *> &f) {
	for (auto &it : f) {
		include(it);
	}
}
void Worker::RequiredFields::include(const StringView &name) {
	if (auto f = scheme->getField(name)) {
		include(f);
	}
}
void Worker::RequiredFields::include(const Field *f) {
	auto it = std::lower_bound(includeFields.begin(), includeFields.end(), f);
	if (it == includeFields.end()) {
		includeFields.emplace_back(f);
	} else if (*it != f) {
		includeFields.emplace(it, f);
	}
	includeNone = false;
}

void Worker::RequiredFields::exclude(std::initializer_list<StringView> il) {
	for (auto &it : il) {
		exclude(it);
	}
}
void Worker::RequiredFields::exclude(const Set<const Field *> &f) {
	for (auto &it : f) {
		exclude(it);
	}
}
void Worker::RequiredFields::exclude(const StringView &name) {
	if (auto f = scheme->getField(name)) {
		exclude(f);
	}
}
void Worker::RequiredFields::exclude(const Field *f) {
	auto it = std::lower_bound(excludeFields.begin(), excludeFields.end(), f);
	if (it == excludeFields.end()) {
		excludeFields.emplace_back(f);
	} else if (*it != f) {
		excludeFields.emplace(it, f);
	}
	includeNone = false;
}


Worker::ConditionData::ConditionData(const Query::Select &sel, const Field *f) {
	compare = sel.compare;
	value1 = sel.value1;
	value2 = sel.value2;
	field = f;
}

Worker::ConditionData::ConditionData(Query::Select &&sel, const Field *f) {
	compare = sel.compare;
	value1 = sp::move(sel.value1);
	value2 = sp::move(sel.value2);
	field = f;
}

void Worker::ConditionData::set(Query::Select &&sel, const Field *f) {
	compare = sel.compare;
	value1 = sp::move(sel.value1);
	value2 = sp::move(sel.value2);
	field = f;
}

void Worker::ConditionData::set(const Query::Select &sel, const Field *f) {
	compare = sel.compare;
	value1 = sel.value1;
	value2 = sel.value2;
	field = f;
}

Worker::Worker(const Scheme &s, const Adapter &a) : _scheme(&s), _transaction(Transaction::acquire(a)) {
	_required.scheme = _scheme;
	// _transaction.retain(); //  acquire = retain
}
Worker::Worker(const Scheme &s, const Transaction &t) : _scheme(&s), _transaction(t) {
	_required.scheme = _scheme;
	_transaction.retain();
}
Worker::Worker(const Worker &w) : _scheme(w._scheme), _transaction(w._transaction) {
	_required.scheme = _scheme;
	_transaction.retain();
}
Worker::~Worker() {
	if (_transaction) {
		_transaction.release();
	}
}

const Transaction &Worker::transaction() const {
	return _transaction;
}
const Scheme &Worker::scheme() const {
	return *_scheme;
}

const ApplicationInterface *Worker::getApplicationInterface() const {
	return _transaction.getAdapter().getApplicationInterface();
}

void Worker::includeNone() {
	_required.clear();
	_required.includeNone = true;
}
void Worker::clearRequiredFields() {
	_required.clear();
}

bool Worker::shouldIncludeNone() const {
	return _required.includeNone;
}

bool Worker::shouldIncludeAll() const {
	return _required.includeAll;
}

Worker &Worker::asSystem() {
	_isSystem = true;
	return *this;
}

bool Worker::isSystem() const {
	return _isSystem;
}

const Worker::RequiredFields &Worker::getRequiredFields() const {
	return _required;
}

const Map<const Field *, Worker::ConflictData> &Worker::getConflicts() const {
	return _conflict;
}

const Vector<Worker::ConditionData> &Worker::getConditions() const {
	return _conditions;
}

Value Worker::get(uint64_t oid, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}

Value Worker::get(const StringView &alias, UpdateFlags flags) {
	if (!_scheme->hasAliases()) {
		return Value();
	}
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}

Value Worker::get(const Value &id, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, flags);
		}
	}
	return Value();
}

Value Worker::get(uint64_t oid, std::initializer_list<StringView> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		if (auto f = _scheme->getField(it)) {
			query.include(f->getName().str<Interface>());
		}
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const StringView &alias, std::initializer_list<StringView> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		if (auto f = _scheme->getField(it)) {
			query.include(f->getName().str<Interface>());
		}
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const Value &id, std::initializer_list<StringView> &&fields, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, sp::move(fields), flags);
		}
	}
	return Value();
}

Value Worker::get(uint64_t oid, std::initializer_list<const char *> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		if (auto f = _scheme->getField(it)) {
			query.include(f->getName().str<Interface>());
		}
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const StringView &alias, std::initializer_list<const char *> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		if (auto f = _scheme->getField(it)) {
			query.include(f->getName().str<Interface>());
		}
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const Value &id, std::initializer_list<const char *> &&fields, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, sp::move(fields), flags);
		}
	}
	return Value();
}

Value Worker::get(uint64_t oid, std::initializer_list<const Field *> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		query.include(it->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const StringView &alias, std::initializer_list<const Field *> &&fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		query.include(it->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const Value &id, std::initializer_list<const Field *> &&fields, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, sp::move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, sp::move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, sp::move(fields), flags);
		}
	}
	return Value();
}

Value Worker::get(uint64_t oid, SpanView<const Field *> fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		query.include(it->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const StringView &alias, SpanView<const Field *> fields, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	for (auto &it : fields) {
		query.include(it->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const Value &id, SpanView<const Field *> fields, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, fields, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, fields, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, fields, flags);
		}
	}
	return Value();
}

Value Worker::get(uint64_t oid, StringView it, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, oid, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	if (auto f = _scheme->getField(it)) {
		query.include(f->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const StringView &alias, StringView it, UpdateFlags flags) {
	Query query;
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) { _required.includeAll = true; }
	prepareGetQuery(query, alias, (flags & UpdateFlags::GetForUpdate) != UpdateFlags::None);
	if (auto f = _scheme->getField(it)) {
		query.include(f->getName().str<Interface>());
	}
	return reduceGetQuery(query, (flags & UpdateFlags::Cached) != UpdateFlags::None);
}
Value Worker::get(const Value &id, StringView it, UpdateFlags flags) {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(oid, it, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(oid, it, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(str, it, flags);
		}
	}
	return Value();
}

bool Worker::foreach(const Query &query, const Callback<bool(Value &)> &cb, UpdateFlags flags) {
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) {
		_required.includeAll = true;
	}
	return _scheme->foreachWithWorker(*this, query, cb);
}

Value Worker::select(const Query &q, UpdateFlags flags) {
	if ((flags & UpdateFlags::GetAll) != UpdateFlags::None) {
		_required.includeAll = true;
	}
	return _scheme->selectWithWorker(*this, q);
}

// returns Dictionary with single object data or Null value
Value Worker::create(const Value &data, bool isProtected) {
	return _scheme->createWithWorker(*this, data, isProtected);
}

Value Worker::create(const Value &data, UpdateFlags flags) {
	if ((flags & UpdateFlags::NoReturn) != UpdateFlags::None) {
		includeNone();
	}
	return _scheme->createWithWorker(*this, data, (flags & UpdateFlags::Protected) != UpdateFlags::None);
}

Value Worker::create(const Value &data, UpdateFlags flags, const Conflict &c) {
	if ((flags & UpdateFlags::NoReturn) != UpdateFlags::None) {
		includeNone();
	}
	if (!addConflict(c)) {
		return Value();
	}
	return _scheme->createWithWorker(*this, data, (flags & UpdateFlags::Protected) != UpdateFlags::None);
}
Value Worker::create(const Value &data, UpdateFlags flags, const Vector<Conflict> &c) {
	if ((flags & UpdateFlags::NoReturn) != UpdateFlags::None) {
		includeNone();
	}
	if (!addConflict(c)) {
		return Value();
	}
	return _scheme->createWithWorker(*this, data, (flags & UpdateFlags::Protected) != UpdateFlags::None);
}
Value Worker::create(const Value &data, Conflict::Flags flags) {
	return create(data, Conflict(flags));
}
Value Worker::create(const Value &data, const Conflict &c) {
	if (!addConflict(c)) {
		return Value();
	}
	return _scheme->createWithWorker(*this, data, false);
}
Value Worker::create(const Value &data, const Vector<Conflict> &c) {
	if (!addConflict(c)) {
		return Value();
	}
	return _scheme->createWithWorker(*this, data, false);
}

Value Worker::update(uint64_t oid, const Value &data, bool isProtected) {
	return _scheme->updateWithWorker(*this, oid, data, isProtected);
}

Value Worker::update(const Value & obj, const Value &data, bool isProtected) {
	return _scheme->updateWithWorker(*this, obj, data, isProtected);
}

Value Worker::update(uint64_t oid, const Value &data, UpdateFlags flags) {
	if ((flags & UpdateFlags::NoReturn) != UpdateFlags::None) {
		includeNone();
	}
	return _scheme->updateWithWorker(*this, oid, data, (flags & UpdateFlags::Protected) != UpdateFlags::None);
}

Value Worker::update(const Value & obj, const Value &data, UpdateFlags flags) {
	if ((flags & UpdateFlags::NoReturn) != UpdateFlags::None) {
		includeNone();
	}
	return _scheme->updateWithWorker(*this, obj, data, (flags & UpdateFlags::Protected) != UpdateFlags::None);
}

Value Worker::update(uint64_t oid, const Value &data, UpdateFlags flags, const Query::Select &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(oid, data, flags);
}
Value Worker::update(const Value & obj, const Value &data, UpdateFlags flags, const Query::Select &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(obj, data, flags);
}
Value Worker::update(uint64_t oid, const Value &data, UpdateFlags flags, const Vector<Query::Select> &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(oid, data, flags);
}
Value Worker::update(const Value & obj, const Value &data, UpdateFlags flags, const Vector<Query::Select> &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(obj, data, flags);
}

Value Worker::update(uint64_t oid, const Value &data, const Query::Select &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(oid, data);
}
Value Worker::update(const Value & obj, const Value &data, const Query::Select &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(obj, data);
}
Value Worker::update(uint64_t oid, const Value &data, const Vector<Query::Select> &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(oid, data);
}
Value Worker::update(const Value & obj, const Value &data, const Vector<Query::Select> &sel) {
	if (!addCondition(sel)) {
		return Value();
	}
	return update(obj, data);
}

bool Worker::remove(uint64_t oid) {
	return _scheme->removeWithWorker(*this, oid);
}

bool Worker::remove(const Value &data) {
	return _scheme->removeWithWorker(*this, data.getInteger("__oid"));
}

size_t Worker::count() {
	return _scheme->countWithWorker(*this, Query());
}
size_t Worker::count(const Query &q) {
	return _scheme->countWithWorker(*this, q);
}

void Worker::touch(uint64_t oid) {
	_scheme->touchWithWorker(*this, oid);
}
void Worker::touch(const Value & obj) {
	_scheme->touchWithWorker(*this, obj);
}

Value Worker::getField(uint64_t oid, const StringView &s, std::initializer_list<StringView> fields) {
	if (auto f = _scheme->getField(s)) {
		return getField(oid, *f, getFieldSet(*f, fields));
	}
	return Value();
}
Value Worker::getField(const Value &obj, const StringView &s, std::initializer_list<StringView> fields) {
	if (auto f = _scheme->getField(s)) {
		return getField(obj, *f, getFieldSet(*f, fields));
	}
	return Value();
}

Value Worker::getField(uint64_t oid, const StringView &s, const Set<const Field *> &fields) {
	if (auto f = _scheme->getField(s)) {
		return getField(oid, *f, fields);
	}
	return Value();
}
Value Worker::getField(const Value &obj, const StringView &s, const Set<const Field *> &fields) {
	if (auto f = _scheme->getField(s)) {
		return getField(obj, *f, fields);
	}
	return Value();
}

Value Worker::setField(uint64_t oid, const StringView &s, Value &&v) {
	if (auto f = _scheme->getField(s)) {
		return setField(oid, *f, sp::move(v));
	}
	return Value();
}
Value Worker::setField(const Value &obj, const StringView &s, Value &&v) {
	if (auto f = _scheme->getField(s)) {
		return setField(obj, *f, sp::move(v));
	}
	return Value();
}
Value Worker::setField(uint64_t oid, const StringView &s, InputFile &file) {
	if (auto f = _scheme->getField(s)) {
		return setField(oid, *f, file);
	}
	return Value();
}
Value Worker::setField(const Value &obj, const StringView &s, InputFile &file) {
	return setField(obj.getInteger(s), s, file);
}

bool Worker::clearField(uint64_t oid, const StringView &s, Value && objs) {
	if (auto f = _scheme->getField(s)) {
		return clearField(oid, *f, sp::move(objs));
	}
	return false;
}
bool Worker::clearField(const Value &obj, const StringView &s, Value && objs) {
	if (auto f = _scheme->getField(s)) {
		return clearField(obj, *f, sp::move(objs));
	}
	return false;
}

Value Worker::appendField(uint64_t oid, const StringView &s, Value &&v) {
	auto f = _scheme->getField(s);
	if (f) {
		return appendField(oid, *f, sp::move(v));
	}
	return Value();
}
Value Worker::appendField(const Value &obj, const StringView &s, Value &&v) {
	auto f = _scheme->getField(s);
	if (f) {
		return appendField(obj, *f, sp::move(v));
	}
	return Value();
}

size_t Worker::countField(uint64_t oid, const StringView &s) {
	auto f = _scheme->getField(s);
	if (f) {
		return countField(oid, *f);
	}
	return 0;
}

size_t Worker::countField(const Value &obj, const StringView &s) {
	auto f = _scheme->getField(s);
	if (f) {
		return countField(obj, *f);
	}
	return 0;
}

Value Worker::getField(uint64_t oid, const Field &f, std::initializer_list<StringView> fields) {
	if (auto s = f.getForeignScheme()) {
		_required.reset(*s);
		include(fields);
	} else {
		_required.clear();
	}
	return _scheme->fieldWithWorker(Action::Get, *this, oid, f);
}
Value Worker::getField(const Value &obj, const Field &f, std::initializer_list<StringView> fields) {
	if (auto s = f.getForeignScheme()) {
		_required.reset(*s);
		include(fields);
	} else {
		_required.clear();
	}
	return _scheme->fieldWithWorker(Action::Get, *this, obj, f);
}

Value Worker::getField(uint64_t oid, const Field &f, const Set<const Field *> &fields) {
	if (auto s = f.getForeignScheme()) {
		_required.reset(*s);
		include(fields);
	} else {
		_required.clear();
	}
	return _scheme->fieldWithWorker(Action::Get, *this, oid, f);
}
Value Worker::getField(const Value &obj, const Field &f, const Set<const Field *> &fields) {
	if (f.isSimpleLayout() && obj.hasValue(f.getName())) {
		return obj.getValue(f.getName());
	} else if (f.isFile() && fields.empty()) {
		return File::getData(_transaction, obj.isInteger() ? obj.asInteger() : obj.getInteger(f.getName()));
	}

	if (auto s = f.getForeignScheme()) {
		_required.reset(*s);
		include(fields);
	} else {
		_required.clear();
	}

	return _scheme->fieldWithWorker(Action::Get, *this, obj, f);
}

Value Worker::setField(uint64_t oid, const Field &f, Value &&v) {
	if (v.isNull()) {
		clearField(oid, f);
		return Value();
	}
	return _scheme->fieldWithWorker(Action::Set, *this, oid, f, sp::move(v));
}
Value Worker::setField(const Value &obj, const Field &f, Value &&v) {
	if (v.isNull()) {
		clearField(obj, f);
		return Value();
	}
	return _scheme->fieldWithWorker(Action::Set, *this, obj, f, sp::move(v));
}
Value Worker::setField(uint64_t oid, const Field &f, InputFile &file) {
	if (f.isFile()) {
		return _scheme->setFileWithWorker(*this, oid, f, file);
	}
	return Value();
}
Value Worker::setField(const Value &obj, const Field &f, InputFile &file) {
	return setField(obj.getInteger("__oid"), f, file);
}

bool Worker::clearField(uint64_t oid, const Field &f, Value &&objs) {
	if (!f.hasFlag(Flags::Required)) {
		return _scheme->fieldWithWorker(Action::Remove, *this, oid, f, sp::move(objs)).asBool();
	}
	return false;
}
bool Worker::clearField(const Value &obj, const Field &f, Value &&objs) {
	if (!f.hasFlag(Flags::Required)) {
		return _scheme->fieldWithWorker(Action::Remove, *this, obj, f, sp::move(objs)).asBool();
	}
	return false;
}

Value Worker::appendField(uint64_t oid, const Field &f, Value &&v) {
	if (f.getType() == Type::Array || (f.getType() == Type::Set && f.isReference())) {
		return _scheme->fieldWithWorker(Action::Append, *this, oid, f, sp::move(v));
	}
	return Value();
}
Value Worker::appendField(const Value &obj, const Field &f, Value &&v) {
	if (f.getType() == Type::Array || (f.getType() == Type::Set && f.isReference())) {
		return _scheme->fieldWithWorker(Action::Append, *this, obj, f, sp::move(v));
	}
	return Value();
}

size_t Worker::countField(uint64_t oid, const Field &f) {
	auto d = _scheme->fieldWithWorker(Action::Count, *this, oid, f);
	if (d.isInteger()) {
		return size_t(d.asInteger());
	}
	return 0;
}

size_t Worker::countField(const Value &obj, const Field &f) {
	auto d = _scheme->fieldWithWorker(Action::Count, *this, obj, f);
	if (d.isInteger()) {
		return size_t(d.asInteger());
	}
	return 0;
}

Set<const Field *> Worker::getFieldSet(const Field &f, std::initializer_list<StringView> il) const {
	Set<const Field *> ret;
	auto target = f.getForeignScheme();
	for (auto &it : il) {
		ret.emplace(target->getField(it));
	}
	return ret;
}

bool Worker::addConflict(const Conflict &c) {
	if (c.field.empty()) {
		// add for all unique fields
		auto tmpC = c;
		for (auto &it : scheme().getFields()) {
			if (it.second.isIndexed() && it.second.hasFlag(Flags::Unique)) {
				tmpC.field = it.first;
				addConflict(tmpC);
			}
		}
		return true;
	} else {
		auto f = scheme().getField(c.field);
		if (!f || !f->hasFlag(Flags::Unique)) {
			_transaction.getAdapter().getApplicationInterface()->error("db::Worker", "Invalid ON CONFLICT field - no unique constraint");
			return false;
		}

		const Field *selField = nullptr;

		ConflictData d;
		if (c.condition.field.empty()) {
			d.flags = Conflict::WithoutCondition;
		} else {
			selField = scheme().getField(c.condition.field);
			if (!selField || !selField->isIndexed()
					|| !checkIfComparationIsValid(selField->getType(), c.condition.compare, selField->getFlags()) || !c.condition.textQuery.empty()) {
				_transaction.getAdapter().getApplicationInterface()->error("db::Worker", "Invalid ON CONFLICT condition - not applicable");
				return false;
			}
		}

		d.field = f;
		if (selField) {
			d.condition.set(sp::move(c.condition), selField);
		}

		for (auto &it : c.mask) {
			if (auto field = scheme().getField(it)) {
				d.mask.emplace_back(field);
			}
		}

		d.flags |= c.flags;

		_conflict.emplace(f, sp::move(d));
		return true;
	}
}

bool Worker::addConflict(const Vector<Conflict> &c) {
	for (auto &it : c) {
		if (!addConflict(it)) {
			return false;
		}
	}
	return true;
}

bool Worker::addCondition(const Query::Select &sel) {
	auto selField = scheme().getField(sel.field);
	if (!selField || !checkIfComparationIsValid(selField->getType(), sel.compare, selField->getFlags()) || !sel.textQuery.empty()) {
		_transaction.getAdapter().getApplicationInterface()->error("db::Worker", "Invalid ON CONFLICT condition - not applicable");
		return false;
	}

	_conditions.emplace_back(sel, selField);
	return true;
}

bool Worker::addCondition(const Vector<Query::Select> &sel) {
	for (auto &it : sel) {
		if (!addCondition(it)) {
			return false;
		}
	}
	return true;
}

Value Worker::reduceGetQuery(const Query &query, bool cached) {
	if (auto id = query.getSingleSelectId()) {
		if (cached && !_scheme->isDetouched()) {
			if (auto v = _transaction.getObject(id)) {
				return v;
			}
		}
		auto ret = _scheme->selectWithWorker(*this, query);
		if (ret.isArray() && ret.size() >= 1) {
			if (cached && !_scheme->isDetouched()) {
				_transaction.setObject(id, Value(ret.getValue(0)));
			}
			return ret.getValue(0);
		}
	} else {
		auto ret = _scheme->selectWithWorker(*this, query);
		if (ret.isArray() && ret.size() >= 1) {
			return ret.getValue(0);
		}
	}

	return Value();
}

FieldResolver::FieldResolver(const Scheme &scheme, const Worker &w, const Query &q)
: scheme(&scheme), required(&w.getRequiredFields()), query(&q) { }

FieldResolver::FieldResolver(const Scheme &scheme, const Worker &w)
: scheme(&scheme), required(&w.getRequiredFields()) { }

FieldResolver::FieldResolver(const Scheme &scheme, const Query &q)
: scheme(&scheme), query(&q) { }

FieldResolver::FieldResolver(const Scheme &scheme, const Query &q, const Set<const Field *> &set)
: scheme(&scheme), query(&q) {
	for (auto &it : set) {
		emplace_ordered(requiredFields, it);
	}
}

FieldResolver::FieldResolver(const Scheme &scheme) : scheme(&scheme) { }

FieldResolver::FieldResolver(const Scheme &scheme, const Set<const Field *> &set) : scheme(&scheme) {
	for (auto &it : set) {
		emplace_ordered(requiredFields, it);
	}
}

bool FieldResolver::shouldResolveFields() const {
	if (!required) {
		return true;
	} else if (required->includeNone || (required->scheme != nullptr && required->scheme != scheme)) {
		return false;
	}
	return true;
}

bool FieldResolver::hasIncludesOrExcludes() const {
	bool hasFields = false;
	if (required) {
		hasFields = !required->excludeFields.empty() || !required->includeFields.empty();
	}
	if (!hasFields && query) {
		hasFields = !query->getIncludeFields().empty() || !query->getExcludeFields().empty();
	}
	return hasFields;
}

bool FieldResolver::shouldIncludeAll() const {
	return required && required->includeAll;
}

bool FieldResolver::shouldIncludeField(const Field &f) const {
	if (query) {
		for (auto &it : query->getIncludeFields()) {
			if (it.name == f.getName()) {
				return true;
			}
		}
	}
	if (required) {
		auto it = std::lower_bound(required->includeFields.begin(), required->includeFields.end(), &f);
		if (it != required->includeFields.end() && *it == &f) {
			return true;
		}
	}
	if (query && required) {
		return query->getIncludeFields().empty() && required->includeFields.empty();
	} else if (query) {
		return query->getIncludeFields().empty();
	} else if (required) {
		return required->includeFields.empty();
	}
	return false;
}

bool FieldResolver::shouldExcludeField(const Field &f) const {
	if (query) {
		for (auto &it : query->getExcludeFields()) {
			if (it.name == f.getName()) {
				return true;
			}
		}
	}
	if (required) {
		auto it = std::lower_bound(required->excludeFields.begin(), required->excludeFields.end(), &f);
		if (it != required->excludeFields.end() && *it == &f) {
			return true;
		}
	}
	return false;
}

bool FieldResolver::isFieldRequired(const Field &f) const {
	auto it = std::lower_bound(requiredFields.begin(), requiredFields.end(), &f);
	if (it == requiredFields.end() || *it != &f) {
		return false;
	}
	return true;
}

Vector<const Field *> FieldResolver::getVirtuals() const {
	Vector<const Field *> virtuals;
	if (!hasIncludesOrExcludes()) {
		for (auto &it : scheme->getFields()) {
			auto type = it.second.getType();
			if (type == Type::Virtual && (!it.second.hasFlag(Flags::ForceExclude) || shouldIncludeAll())) {
				emplace_ordered(virtuals, &it.second);
			}
		}
	} else {
		auto &forceInclude = scheme->getForceInclude();
		for (auto &it : scheme->getFields()) {
			auto type = it.second.getType();
			if (type == Type::Virtual) {
				if (it.second.hasFlag(Flags::ForceInclude) || forceInclude.find(&it.second) != forceInclude.end()) {
					emplace_ordered(virtuals, &it.second);
				} else if (shouldIncludeField(it.second)) {
					if (!shouldExcludeField(it.second)) {
						emplace_ordered(virtuals, &it.second);
					}
				}
			}
		}
	}

	return virtuals;
}

bool FieldResolver::readFields(const Worker::FieldCallback &cb, bool isSimpleGet) {
	if (!shouldResolveFields()) {
		return false;
	} else if (!hasIncludesOrExcludes()) {
		// no includes/excludes
		if (!scheme->hasForceExclude() || shouldIncludeAll()) {
			// no force-excludes or all fields are required, so, return *
			cb("*", nullptr);
		} else {
			// has force-excludes, iterate through fields

			cb("__oid", nullptr);
			for (auto &it : scheme->getFields()) {
				if (it.second.hasFlag(Flags::ForceExclude)) {
					continue;
				}

				auto type = it.second.getType();
				if (type == Type::Set || type == Type::Array || type == Type::View
						|| type == Type::FullTextView || type == Type::Virtual) {
					continue;
				}

				cb(it.second.getName(), &it.second);
			}
		}
	} else {
		// has excludes or includes
		cb("__oid", nullptr);

		auto &forceInclude = scheme->getForceInclude();

		Vector<const Field *> virtuals = getVirtuals();
		for (auto &it : virtuals) {
			auto slot = it->getSlot<FieldVirtual>();
			for (auto &iit : slot->requireFields) {
				if (auto f = scheme->getField(iit)) {
					emplace_ordered(requiredFields, f);
				}
			}
		}

		for (auto &it : scheme->getFields()) {
			auto type = it.second.getType();
			if (type == Type::Set || type == Type::Array || type == Type::View || type == Type::FullTextView || type == Type::Virtual) {
				continue;
			}

			if (it.second.hasFlag(Flags::ForceInclude) || isFieldRequired(it.second) || (!isSimpleGet && forceInclude.find(&it.second) != forceInclude.end())) {
				cb(it.second.getName(), &it.second);
			} else if (!isSimpleGet && shouldIncludeField(it.second)) {
				if (!shouldExcludeField(it.second)) {
					cb(it.second.getName(), &it.second);
				}
			}
		}
	}
	return true;
}

void FieldResolver::include(StringView mem) {
	if (auto f = scheme->getField(mem)) {
		emplace_ordered(requiredFields, f);
	}
}

}
