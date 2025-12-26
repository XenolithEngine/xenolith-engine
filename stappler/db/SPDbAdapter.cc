/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPDbAdapter.h"
#include "SPDbScheme.h"
#include "SPDbFieldExtensions.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::db {

void ApplicationInterface::defineUserScheme(Scheme &scheme) {
	// clang-format off
	scheme.define({
		db::Field::Text("name", db::Transform::Alias, db::Flags::Required),
		db::Field::Bytes("pubkey", db::Transform::PublicKey, db::Flags::Indexed),
		db::Field::Password("password", db::PasswordSalt(config::DEFAULT_PASSWORD_SALT),
				db::Flags::Required | db::Flags::Protected),
		db::Field::Boolean("isAdmin", Value(false)),
		db::Field::Extra("data",
				Vector<db::Field>({
					db::Field::Text("email", db::Transform::Email),
					db::Field::Text("public"),
					db::Field::Text("desc"),
				})),
		db::Field::Text("email", db::Transform::Email, db::Flags::Unique),
	});
	// clang-format on
}

void ApplicationInterface::defineFileScheme(Scheme &scheme) {
	// clang-format off
	scheme.define({
		db::Field::Text("location", db::Transform::Url),
		db::Field::Text("type", db::Flags::ReadOnly),
		db::Field::Integer("size", db::Flags::ReadOnly),
		db::Field::Integer("mtime", db::Flags::AutoMTime | db::Flags::ReadOnly),
		db::Field::Extra("image",
				Vector<db::Field>{
					db::Field::Integer("width"),
					db::Field::Integer("height"),
				})});
	// clang-format on
}

void ApplicationInterface::defineErrorScheme(Scheme &scheme) {
	// clang-format off
	scheme.define({
		db::Field::Boolean("hidden", Value(false)),
		db::Field::Boolean("delivered", Value(false)), db::Field::Text("name"),
		db::Field::Text("documentRoot"), db::Field::Text("url"), db::Field::Text("request"),
		db::Field::Text("ip"), db::Field::Data("headers"), db::Field::Data("data"),
		db::Field::Integer("time"),
		db::Field::Custom(new (std::nothrow) db::FieldTextArray("tags", db::Flags::Indexed,
				db::DefaultFn([&](const Value &data) -> Value {
			Vector<String> tags;
			for (auto &it : data.getArray("data")) {
				auto text = it.getString("source");
				if (!text.empty()) {
					emplace_ordered(tags, text);
				}
			}
	
			Value ret;
			for (auto &it : tags) { ret.addString(it); }
			return ret;
		})))
	});
	// clang-format on
}

ApplicationInterface::~ApplicationInterface() { }

db::Adapter ApplicationInterface::getAdapterFromContext() const {
	if (auto p = pool::acquire()) {
		db::BackendInterface *h = nullptr;
		pool::userdata_get((void **)&h, db::config::STORAGE_INTERFACE_KEY.data(), p);
		if (h) {
			return db::Adapter(h, this);
		}
	}
	return db::Adapter(nullptr, nullptr);
}

void ApplicationInterface::scheduleAyncDbTask(
		const Callback<Function<void(const Transaction &)>(pool_t *)> &setupCb) const {
	log::source().error("ApplicationInterface", "scheduleAyncDbTask is not define");
	::abort();
}

StringView ApplicationInterface::getDocumentRoot() const {
	StringView ret;
	filesystem::enumeratePaths(FileCategory::AppData, [&](StringView path, FileFlags) {
		ret = path;
		return false;
	});
	return ret;
}

void ApplicationInterface::pushErrorMessage(Value &&val) const {
	log::source().error("ApplicationInterface", data::EncodeFormat::Pretty, val);
}

void ApplicationInterface::pushDebugMessage(Value &&val) const {
	log::source().debug("ApplicationInterface", data::EncodeFormat::Pretty, val);
}

void ApplicationInterface::reportDbUpdate(StringView data, bool successful) {
	auto dir = filepath::merge<Interface>(getDocumentRoot(), ".reports");
	filesystem::mkdir(FileInfo{dir});
	auto path = toString(dir, "/update.", stappler::Time::now().toMilliseconds(), ".sql");
	stappler::filesystem::write(FileInfo{path}, (const uint8_t *)data.data(), data.size());
}

Adapter Adapter::FromContext(const ApplicationInterface *app) {
	return app->getAdapterFromContext();
}

Adapter::Adapter(BackendInterface *iface, const ApplicationInterface *app)
: _application(app), _interface(iface) { }

Adapter::Adapter(const Adapter &other) {
	_application = other._application;
	_interface = other._interface;
}

Adapter &Adapter::operator=(const Adapter &other) {
	_application = other._application;
	_interface = other._interface;
	return *this;
}

String Adapter::getTransactionKey() const {
	if (!_interface) {
		return String();
	}

	auto ret = _interface->getTransactionKey();
	if (!ret.empty()) {
		return ret;
	}

	char buf[32] = {0};
	auto prefix = StringView(config::STORAGE_TRANSACTION_PREFIX);
	memcpy(buf, prefix.data(), prefix.size());
	stappler::base16::encode(buf + prefix.size(), 32 - prefix.size(),
			stappler::CoderSource((const uint8_t *)(_interface), sizeof(void *)));
	return String(buf, prefix.size() + sizeof(void *) * 2);
}

bool Adapter::set(const stappler::CoderSource &key, const Value &val,
		stappler::TimeInterval maxAge) const {
	return _interface->set(key, val, maxAge);
}

Value Adapter::get(const stappler::CoderSource &key) const { return _interface->get(key); }

bool Adapter::clear(const stappler::CoderSource &key) const { return _interface->clear(key); }

Vector<int64_t> Adapter::performQueryListForIds(const QueryList &ql, size_t count) const {
	return _interface->performQueryListForIds(ql, count);
}
Value Adapter::performQueryList(const QueryList &ql, size_t count, bool forUpdate) const {
	auto targetScheme = ql.getScheme();
	if (targetScheme) {
		// virtual fields should be resolved within interface
		return _interface->performQueryList(ql, count, forUpdate);
	}
	return Value();
}

bool Adapter::init(const BackendInterface::Config &cfg,
		const Map<StringView, const Scheme *> &schemes) const {
	Scheme::initSchemes(schemes);
	return _interface->init(cfg, schemes);
}

void Adapter::makeSessionsCleanup() const { _interface->makeSessionsCleanup(); }

User *Adapter::authorizeUser(const Auth &auth, const StringView &name,
		const StringView &password) const {
	if (_interface) {
		return _interface->authorizeUser(auth, name, password);
	}
	return nullptr;
}

void Adapter::broadcast(const Bytes &data) const { _interface->broadcast(data); }

void Adapter::broadcast(const Value &val) const {
	broadcast(data::write<Interface>(val, EncodeFormat::Cbor));
}

void Adapter::broadcast(StringView url, Value &&val, bool exclusive) const {
	broadcast(Value({
		stappler::pair("url", Value(url)),
		stappler::pair("exclusive", Value(exclusive)),
		stappler::pair("data", sp::move(val)),
	}));
}

bool Adapter::performWithTransaction(const Callback<bool(const db::Transaction &)> &cb) const {
	if (auto t = db::Transaction::acquire(*this)) {
		bool success = true;
		if (isInTransaction()) {
			if (!cb(t)) {
				cancelTransaction();
				success = false;
			}
		} else {
			if (beginTransaction()) {
				if (!cb(t)) {
					cancelTransaction();
					success = false;
					endTransaction();
				} else {
					success = endTransaction();
				}
			} else {
				success = false;
			}
		}
		t.release();
		return success;
	}
	return false;
}

int64_t Adapter::getDeltaValue(const Scheme &s) { return _interface->getDeltaValue(s); }

int64_t Adapter::getDeltaValue(const Scheme &s, const FieldView &v, uint64_t id) {
	return _interface->getDeltaValue(s, v, id);
}

bool Adapter::foreach (Worker &w, const Query &q, const Callback<bool(Value &)> &cb) const {
	return _interface->foreach (w, q, cb);
}

Value Adapter::select(Worker &w, const Query &q) const {
	auto targetScheme = &w.scheme();
	auto ordField = q.getQueryField();
	if (!ordField.empty()) {
		if (auto f = targetScheme->getField(ordField)) {
			targetScheme = f->getForeignScheme();
		} else {
			return Value();
		}
	}

	// virtual fields should be resolved within interface
	return _interface->select(w, q);
}

Value Adapter::create(Worker &w, Value &changeSet) const {
	auto &scheme = w.scheme();
	auto &fullTextFields = scheme.getFullTextFields();

	Vector<InputField> inputFields;
	Vector< InputRow > inputRows;

	bool stop = false;
	if (changeSet.isDictionary()) {
		auto &targetRow = inputRows.emplace_back();
		for (auto &it : scheme.getFields()) {
			auto &val = changeSet.getValue(it.first);
			if (val) {
				if (fullTextFields.find(&it.second) == fullTextFields.end()) {
					targetRow.values.emplace_back(move(val));
				} else {
					targetRow.values.emplace_back(Value(val));
				}
				inputFields.emplace_back(InputField{&it.second});
			} else {
				if (it.second.hasFlag(Flags::Required)) {
					w.getApplicationInterface()->error("Storage", "No value for required field",
							Value({std::make_pair("field", Value(it.first))}));
					stop = true;
				}
			}
		}
	} else if (changeSet.isArray()) {
		for (auto &rowValues : changeSet.asArray()) {
			for (auto &it : scheme.getFields()) {
				auto &val = rowValues.getValue(it.first);
				if (val) {
					emplace_ordered(inputFields, InputField{&it.second});
				} else {
					if (it.second.hasFlag(Flags::Required)) {
						w.getApplicationInterface()->error("Storage", "No value for required field",
								Value({std::make_pair("field", Value(it.first))}));
						stop = true;
					}
				}
			}
		}
	} else {
		stop = true;
	}

	if (stop) {
		return Value();
	}

	if (changeSet.isArray()) {
		for (auto &rowValues : changeSet.asArray()) {
			auto &targetRow = inputRows.emplace_back();
			for (auto &it : inputFields) {
				auto &v = rowValues.getValue(it.field->getName());
				if (v) {
					if (fullTextFields.find(it.field) == fullTextFields.end()) {
						targetRow.values.emplace_back(move(v));
					} else {
						targetRow.values.emplace_back(Value(v));
					}
				} else {
					targetRow.values.emplace_back(Value(v));
				}
			}
		}
	}

	processFullTextFields(scheme, changeSet, inputFields, inputRows);

	auto ret = _interface->create(w, inputFields, inputRows, changeSet.isArray());
	auto updateData = [&](Value &value) -> bool {
		for (auto &it : value.asDict()) {
			auto f = w.scheme().getField(it.first);
			if (f && f->getType() == Type::Virtual) {
				auto slot = f->getSlot<FieldVirtual>();
				if (slot->writeFn) {
					if (!slot->writeFn(w.scheme(), ret, it.second)) {
						return false;
					}
				} else {
					return false;
				}
			}
		}
		return true;
	};

	if (ret.isArray()) {
		for (auto &it : ret.asArray()) {
			if (!updateData(it)) {
				_interface->cancelTransaction();
				return Value();
			}
		}
	} else if (ret.isDictionary()) {
		if (!updateData(ret)) {
			_interface->cancelTransaction();
			return Value();
		}
	}
	return ret;
}

static void Adapter_mergeValues(const Scheme &scheme, const Field &f, const Value &obj,
		Value &original, Value &newVal) {
	if (f.getType() == Type::Extra) {
		if (newVal.isDictionary()) {
			auto &extraFields = static_cast<const FieldExtra *>(f.getSlot())->fields;
			for (auto &it : newVal.asDict()) {
				auto f_it = extraFields.find(it.first);
				if (f_it != extraFields.end()) {
					auto slot = f_it->second.getSlot();
					auto &val = original.getValue(it.first);
					if (!slot->replaceFilterFn
							|| slot->replaceFilterFn(scheme, obj, val, it.second)) {
						if (!it.second.isNull()) {
							if (val) {
								Adapter_mergeValues(scheme, f_it->second, obj, val, it.second);
							} else {
								original.setValue(sp::move(it.second), it.first);
							}
						} else {
							original.erase(it.first);
						}
					}
				}
			}
		} else if (newVal.isArray() && f.getTransform() == Transform::Array) {
			original.setValue(sp::move(newVal));
		}
	} else {
		original.setValue(sp::move(newVal));
	}
}

Value Adapter::save(Worker &w, uint64_t oid, Value &obj, Value &patch,
		const Set<const Field *> &fields) const {
	bool hasNonVirtualUpdates = false;
	Map<const FieldVirtual *, Value> virtualWrites;

	Vector<InputField> inputFields;
	Vector<InputRow> inputRows;
	auto &inputRow = inputRows.emplace_back();
	if (!fields.empty()) {
		for (auto &it : fields) {
			auto &patchValue = patch.getValue(it->getName());
			auto &val = obj.getValue(it->getName());

			if (!patchValue.isNull()) {
				if (val) {
					inputRow.values.emplace_back(Value(val));
					Adapter_mergeValues(w.scheme(), *it, obj, inputRow.values.back().value,
							patchValue);
				} else {
					inputRow.values.emplace_back(Value(sp::move(patchValue)));
				}
				obj.setValue(inputRow.values.back().value, it->getName());
			} else {
				inputRow.values.emplace_back(Value());
				obj.erase(it->getName());
			}

			inputFields.emplace_back(InputField{it});
		}

		processFullTextFields(w.scheme(), obj, inputFields, inputRows);
	} else {
		for (auto &it : patch.asDict()) {
			auto f = w.scheme().getField(it.first);
			if (f) {
				inputFields.emplace_back(InputField{f});
				inputRow.values.emplace_back(Value(sp::move(it.second)));
			}
		}
		processFullTextFields(w.scheme(), patch, inputFields, inputRows);
	}


	size_t i = 0;
	for (auto &it : inputFields) {
		if (it.field->getType() != Type::Virtual) {
			hasNonVirtualUpdates = true;
		} else {
			if (inputRow.values[i].hasValue()) {
				virtualWrites.emplace(it.field->getSlot<FieldVirtual>(),
						move(inputRow.values[i].value));
			}
		}
		++i;
	}

	Value ret;
	if (hasNonVirtualUpdates) {
		ret = _interface->save(w, oid, obj, inputFields, inputRow);
	} else {
		ret = obj;
	}
	if (ret) {
		for (auto &it : virtualWrites) {
			if (it.first->writeFn) {
				if (it.first->writeFn(w.scheme(), obj, it.second)) {
					ret.setValue(sp::move(it.second), it.first->getName());
				} else {
					_interface->cancelTransaction();
					return Value();
				}
			} else {
				_interface->cancelTransaction();
				return Value();
			}
		}
	}
	return ret;
}

bool Adapter::remove(Worker &w, uint64_t oid) const { return _interface->remove(w, oid); }

size_t Adapter::count(Worker &w, const Query &q) const { return _interface->count(w, q); }

Value Adapter::field(Action a, Worker &w, uint64_t oid, const Field &f, Value &&data) const {
	return _interface->field(a, w, oid, f, sp::move(data));
}

Value Adapter::field(Action a, Worker &w, const Value &obj, const Field &f, Value &&data) const {
	return _interface->field(a, w, obj, f, sp::move(data));
}

bool Adapter::addToView(const FieldView &v, const Scheme *s, uint64_t oid,
		const Value &data) const {
	return _interface->addToView(v, s, oid, data);
}
bool Adapter::removeFromView(const FieldView &v, const Scheme *s, uint64_t oid) const {
	return _interface->removeFromView(v, s, oid);
}

Vector<int64_t> Adapter::getReferenceParents(const Scheme &s, uint64_t oid, const Scheme *fs,
		const Field *f) const {
	return _interface->getReferenceParents(s, oid, fs, f);
}

bool Adapter::beginTransaction() const { return _interface->beginTransaction(); }

bool Adapter::endTransaction() const { return _interface->endTransaction(); }

void Adapter::cancelTransaction() const { _interface->cancelTransaction(); }

bool Adapter::isInTransaction() const { return _interface->isInTransaction(); }

TransactionStatus Adapter::getTransactionStatus() const {
	return _interface->getTransactionStatus();
}

void Adapter::processFullTextFields(const Scheme &scheme, Value &patch, Vector<InputField> &ifields,
		Vector<InputRow> &ivalues) const {
	auto addFullTextView = [&](const Field *f, const FieldFullTextView *slot) {
		if (slot->viewFn) {
			size_t target = 0;
			auto iit = std::find(ifields.begin(), ifields.end(), InputField{f});
			if (iit == ifields.end()) {
				ifields.emplace_back(InputField{f});
				for (auto &row : ivalues) { row.values.emplace_back(Value()); }
				target = ifields.size() - 1;
			} else {
				target = iit - ifields.begin();
			}

			size_t i = 0;
			for (auto &row : ivalues) {
				auto result = slot->viewFn(scheme, patch.isArray() ? patch.getValue(i) : patch);
				if (!result.empty()) {
					row.values[target] = InputValue(move(result));
				}
				++i;
			}
		}
	};

	for (auto &it : scheme.getFields()) {
		if (it.second.getType() == Type::FullTextView) {
			auto slot = it.second.getSlot<FieldFullTextView>();
			for (auto &p_it : ifields) {
				if (std::find(slot->requireFields.begin(), slot->requireFields.end(),
							p_it.field->getName())
						!= slot->requireFields.end()) {
					addFullTextView(&it.second, slot);
					break;
				}
			}
		}
	}
}


void Binder::setInterface(QueryInterface *iface) { _iface = iface; }
QueryInterface *Binder::getInterface() const { return _iface; }

void Binder::writeBind(StringStream &query, int64_t val) { _iface->bindInt(*this, query, val); }
void Binder::writeBind(StringStream &query, uint64_t val) { _iface->bindUInt(*this, query, val); }
void Binder::writeBind(StringStream &query, double val) { _iface->bindDouble(*this, query, val); }
void Binder::writeBind(StringStream &query, stappler::Time val) {
	_iface->bindUInt(*this, query, val.toMicros());
}
void Binder::writeBind(StringStream &query, stappler::TimeInterval val) {
	_iface->bindUInt(*this, query, val.toMicros());
}
void Binder::writeBind(StringStream &query, const String &val) {
	_iface->bindString(*this, query, val);
}
void Binder::writeBind(StringStream &query, String &&val) {
	_iface->bindMoveString(*this, query, sp::move(val));
}
void Binder::writeBind(StringStream &query, const StringView &val) {
	_iface->bindStringView(*this, query, val);
}
void Binder::writeBind(StringStream &query, const Bytes &val) {
	_iface->bindBytes(*this, query, val);
}
void Binder::writeBind(StringStream &query, Bytes &&val) {
	_iface->bindMoveBytes(*this, query, sp::move(val));
}
void Binder::writeBind(StringStream &query, const stappler::CoderSource &val) {
	_iface->bindCoderSource(*this, query, val);
}
void Binder::writeBind(StringStream &query, const Value &val) {
	_iface->bindValue(*this, query, val);
}
void Binder::writeBind(StringStream &query, const DataField &f) {
	_iface->bindDataField(*this, query, f);
}
void Binder::writeBind(StringStream &query, const TypeString &type) {
	_iface->bindTypeString(*this, query, type);
}
void Binder::writeBind(StringStream &query, const FullTextField &d) {
	_iface->bindFullText(*this, query, d);
}
void Binder::writeBind(StringStream &query, const FullTextFrom &d) {
	_iface->bindFullTextFrom(*this, query, d);
}
void Binder::writeBind(StringStream &query, const FullTextRank &rank) {
	_iface->bindFullTextRank(*this, query, rank);
}
void Binder::writeBind(StringStream &query, const FullTextQueryRef &data) {
	_iface->bindFullTextQuery(*this, query, data);
}
void Binder::writeBind(StringStream &query,
		const stappler::sql::PatternComparator<const Value &> &cmp) {
	if (cmp.value->isString()) {
		switch (cmp.cmp) {
		case Comparation::Prefix: {
			String str;
			str.reserve(cmp.value->getString().size() + 1);
			str.append(cmp.value->getString());
			str.append("%");
			_iface->bindMoveString(*this, query, sp::move(str));
			break;
		}
		case Comparation::Suffix: {
			String str;
			str.reserve(cmp.value->getString().size() + 1);
			str.append("%");
			str.append(cmp.value->getString());
			_iface->bindMoveString(*this, query, sp::move(str));
			break;
		}
		case Comparation::WordPart: {
			String str;
			str.reserve(cmp.value->getString().size() + 2);
			str.append("%");
			str.append(cmp.value->getString());
			str.append("%");
			_iface->bindMoveString(*this, query, sp::move(str));
			break;
		}
		default: _iface->bindValue(*this, query, Value()); break;
		}
	} else {
		_iface->bindValue(*this, query, Value());
	}
}
void Binder::writeBind(StringStream &query,
		const stappler::sql::PatternComparator<const StringView &> &cmp) {
	switch (cmp.cmp) {
	case Comparation::Prefix: {
		String str;
		str.reserve(cmp.value->size() + 1);
		str.append(cmp.value->data(), cmp.value->size());
		str.append("%");
		_iface->bindMoveString(*this, query, sp::move(str));
		break;
	}
	case Comparation::Suffix: {
		String str;
		str.reserve(cmp.value->size() + 1);
		str.append("%");
		str.append(cmp.value->data(), cmp.value->size());
		_iface->bindMoveString(*this, query, sp::move(str));
		break;
	}
	case Comparation::WordPart: {
		String str;
		str.reserve(cmp.value->size() + 2);
		str.append("%");
		str.append(cmp.value->data(), cmp.value->size());
		str.append("%");
		_iface->bindMoveString(*this, query, sp::move(str));
		break;
	}
	default: break;
	}
	_iface->bindMoveString(*this, query, "NULL");
}
void Binder::writeBind(StringStream &query, const Vector<int64_t> &vec) {
	_iface->bindIntVector(*this, query, vec);
}

void Binder::writeBind(StringStream &query, const Vector<double> &vec) {
	_iface->bindDoubleVector(*this, query, vec);
}

void Binder::writeBind(StringStream &query, const Vector<StringView> &vec) {
	_iface->bindStringVector(*this, query, vec);
}

void Binder::writeBindArray(StringStream &query, const Vector<int64_t> &vec) {
	_iface->bindIntVector(*this, query, vec);
}

void Binder::writeBindArray(StringStream &query, const Vector<double> &vec) {
	_iface->bindDoubleVector(*this, query, vec);
}

void Binder::writeBindArray(StringStream &query, const Vector<StringView> &vec) {
	_iface->bindStringVector(*this, query, vec);
}

void Binder::writeBindArray(StringStream &query, const Value &val) {
	if (val.isArray()) {
		if (val.getValue(0).isInteger()) {
			Vector<int64_t> vec;
			for (auto &it : val.asArray()) { vec.emplace_back(it.getInteger()); }
			_iface->bindIntVector(*this, query, vec);
		} else if (val.getValue(0).isDouble()) {
			Vector<double> vec;
			for (auto &it : val.asArray()) { vec.emplace_back(it.getDouble()); }
			_iface->bindDoubleVector(*this, query, vec);
		} else if (val.getValue(0).isString()) {
			Vector<StringView> vec;
			for (auto &it : val.asArray()) { vec.emplace_back(it.getString()); }
			_iface->bindStringVector(*this, query, vec);
		} else {
			log::source().error("db::Binder", "Malformed Value for writeBindArray - not an array");
		}
	} else {
		log::source().error("db::Binder", "Malformed Value for writeBindArray - not an array");
	}
}

void Binder::clear() { _iface->clear(); }

ResultRow::ResultRow(const ResultCursor *res, size_t r) : result(res), row(r) { }

ResultRow::ResultRow(const ResultRow &other) noexcept : result(other.result), row(other.row) { }
ResultRow &ResultRow::operator=(const ResultRow &other) noexcept {
	result = other.result;
	row = other.row;
	return *this;
}

size_t ResultRow::size() const { return result->getFieldsCount(); }
Value ResultRow::toData(const db::Scheme &scheme, const Map<String, db::Field> &viewFields,
		const Vector<const Field *> &virtuals) {
	Value row(Value::Type::DICTIONARY);
	row.asDict().reserve(result->getFieldsCount() + virtuals.size());
	Value *deltaPtr = nullptr;
	for (size_t i = 0; i < result->getFieldsCount(); i++) {
		auto n = result->getFieldName(i);
		if (n == "__oid") {
			if (!isNull(i)) {
				row.setInteger(toInteger(i), n.str<Interface>());
			}
		} else if (n == "__vid") {
			auto val = isNull(i) ? int64_t(0) : toInteger(i);
			row.setInteger(val, n.str<Interface>());
			if (deltaPtr && val == 0) {
				deltaPtr->setString("delete", "action");
			}
		} else if (n == "__d_action") {
			if (!deltaPtr) {
				deltaPtr = &row.emplace("__delta");
			}
			switch (DeltaAction(toInteger(i))) {
			case DeltaAction::Create: deltaPtr->setString("create", "action"); break;
			case DeltaAction::Update: deltaPtr->setString("update", "action"); break;
			case DeltaAction::Delete: deltaPtr->setString("delete", "action"); break;
			case DeltaAction::Append: deltaPtr->setString("append", "action"); break;
			case DeltaAction::Erase: deltaPtr->setString("erase", "action"); break;
			default: break;
			}
		} else if (n == "__d_object") {
			row.setInteger(toInteger(i), "__oid");
		} else if (n == "__d_time") {
			if (!deltaPtr) {
				deltaPtr = &row.emplace("__delta");
			}
			deltaPtr->setInteger(toInteger(i), "time");
		} else if (n.starts_with("__ts_rank_")) {
			auto d = toDouble(i);
			row.setDouble(d, n.sub("__ts_rank_"_len).str<Interface>());
			row.setDouble(d, n.str<Interface>());
		} else if (!isNull(i)) {
			if (auto f_it = scheme.getField(n)) {
				row.setValue(toData(i, *f_it), n.str<Interface>());
			} else {
				auto ef_it = viewFields.find(n);
				if (ef_it != viewFields.end()) {
					row.setValue(toData(i, ef_it->second), n.str<Interface>());
				}
			}
		}
	}

	if (!virtuals.empty()) {
		for (auto &it : virtuals) {
			auto slot = it->getSlot<FieldVirtual>();
			if (slot->readFn) {
				if (auto v = slot->readFn(scheme, row)) {
					row.setValue(sp::move(v), it->getName());
				}
			}
		}
	}

	return row;
}

Value ResultRow::encode() const {
	Value row(Value::Type::DICTIONARY);
	row.asDict().reserve(result->getFieldsCount());

	for (size_t i = 0; i < result->getFieldsCount(); i++) {
		auto n = result->getFieldName(i);
		if (!isNull(i)) {
			row.setValue(toTypedData(i), n);
		}
	}
	return row;
}

StringView ResultRow::front() const { return at(0); }
StringView ResultRow::back() const { return at(result->getFieldsCount() - 1); }

bool ResultRow::isNull(size_t n) const { return result->isNull(n); }

StringView ResultRow::at(size_t n) const { return result->toString(n); }

StringView ResultRow::toString(size_t n) const { return result->toString(n); }
BytesView ResultRow::toBytes(size_t n) const { return result->toBytes(n); }

int64_t ResultRow::toInteger(size_t n) const { return result->toInteger(n); }

double ResultRow::toDouble(size_t n) const { return result->toDouble(n); }

bool ResultRow::toBool(size_t n) const { return result->toBool(n); }

Value ResultRow::toTypedData(size_t n) const { return result->toTypedData(n); }

Value ResultRow::toData(size_t n, const db::Field &f) {
	switch (f.getType()) {
	case db::Type::Integer:
	case db::Type::Object:
	case db::Type::Set:
	case db::Type::Array:
	case db::Type::File:
	case db::Type::Image: return Value(toInteger(n)); break;
	case db::Type::Float: return Value(toDouble(n)); break;
	case db::Type::Boolean: return Value(toBool(n)); break;
	case db::Type::Text: return Value(toString(n)); break;
	case db::Type::Bytes: return Value(toBytes(n)); break;
	case db::Type::Data:
	case db::Type::Extra: return data::read<Interface, BytesView>(toBytes(n)); break;
	case db::Type::Custom: return result->toCustomData(n, f.getSlot<db::FieldCustom>()); break;
	default: break;
	}

	return Value();
}

Result::Result(db::ResultCursor *iface) : _cursor(iface) {
	_success = _cursor->isSuccess();
	if (_success) {
		_nfields = _cursor->getFieldsCount();
	}
}
Result::~Result() { clear(); }

Result::Result(Result &&res)
: _cursor(res._cursor), _success(res._success), _nfields(res._nfields) {
	res._cursor = nullptr;
}
Result &Result::operator=(Result &&res) {
	clear();
	_cursor = res._cursor;
	_success = res._success;
	_nfields = res._nfields;
	res._cursor = nullptr;
	return *this;
}

Result::operator bool() const { return _success; }
bool Result::success() const { return _success; }

Value Result::info() const { return _cursor->getInfo(); }

bool Result::empty() const { return _cursor->isEmpty(); }

int64_t Result::readId() { return _cursor->toId(); }

size_t Result::getAffectedRows() const { return _cursor->getAffectedRows(); }

size_t Result::getRowsHint() const { return _cursor->getRowsHint(); }

void Result::clear() {
	if (_cursor) {
		_cursor->clear();
	}
}

Result::Iter Result::begin() {
	if (_row != 0) {
		_cursor->reset();
		_row = 0;
	}
	if (_cursor->isEmpty()) {
		return Result::Iter(this, stappler::maxOf<size_t>());
	} else {
		return Result::Iter(this, _row);
	}
}

Result::Iter Result::end() { return Result::Iter(this, stappler::maxOf<size_t>()); }

ResultRow Result::current() const { return ResultRow(_cursor, _row); }

bool Result::next() {
	if (_cursor->next()) {
		++_row;
		return true;
	}
	_row = stappler::maxOf<size_t>();
	return false;
}

StringView Result::name(size_t n) const { return _cursor->getFieldName(n); }

Value Result::decode(const db::Scheme &scheme, const Vector<const Field *> &virtuals) {
	Value ret(Value::Type::ARRAY);
	ret.asArray().reserve(getRowsHint());
	for (auto it : *this) { ret.addValue(it.toData(scheme, Map<String, db::Field>(), virtuals)); }
	return ret;
}
Value Result::decode(const db::Field &field, const Vector<const Field *> &virtuals) {
	Value ret;
	if (!empty()) {
		if (field.getType() == db::Type::Array) {
			auto &arrF = static_cast<const db::FieldArray *>(field.getSlot())->tfield;
			for (auto it : *this) { ret.addValue(it.toData(0, arrF)); }
		} else if (field.getType() == db::Type::View) {
			auto v = static_cast<const db::FieldView *>(field.getSlot());
			for (auto it : *this) {
				ret.addValue(it.toData(*v->scheme, Map<String, db::Field>(), virtuals));
			}
		} else {
			for (auto it : *this) { ret.addValue(it.toData(0, field)); }
		}
	}
	return ret;
}

Value Result::decode(const db::FieldView &field) {
	Value ret;
	for (auto it : *this) { ret.addValue(it.toData(*field.scheme, Map<String, db::Field>())); }
	return ret;
}

} // namespace stappler::db
