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

#include "SPDbScheme.h"

#include "SPDbAdapter.h"
#include "SPDbFile.h"
#include "SPDbObject.h"
#include "SPDbTransaction.h"
#include "SPDbWorker.h"

#if MODULE_STAPPLER_BITMAP
#include "SPBitmap.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::db {

SPUNUSED static void prepareGetQuery(Query &query, uint64_t oid, bool forUpdate);

static void Scheme_setOwner(const Scheme *scheme, const Map<String, Field> &map) {
	for (auto &it : map) {
		const_cast<Field::Slot *>(it.second.getSlot())->owner = scheme;
		if (it.second.getType() == Type::Extra) {
			auto slot = static_cast<const FieldExtra *>(it.second.getSlot());
			Scheme_setOwner(scheme, slot->fields);
		}
	}
}

bool Scheme::initSchemes(const Map<StringView, const Scheme *> &schemes) {
	for (auto &it : schemes) { const_cast<Scheme *>(it.second)->init(); }
	return true;
}

Scheme::Scheme(const StringView &ns, Options f, uint32_t v)
: _name(ns.str<Interface>())
, _version(v)
, _flags(f)
, _oidField(Field::Integer("__oid", Flags::Indexed | Flags::ForceInclude)) {
	const_cast<Field::Slot *>(_oidField.getSlot())->owner = this;
	for (size_t i = 0; i < _roles.size(); ++i) { _roles[i] = nullptr; }
}

Scheme::Scheme(const StringView &name, std::initializer_list<Field> il, Options f, uint32_t v)
: Scheme(name, f, v) {
	for (auto &it : il) {
		auto fname = it.getName();
		_fields.emplace(fname.str<Interface>(), sp::move(const_cast<Field &>(it)));
	}

	updateLimits();
}

bool Scheme::hasDelta() const { return (_flags & Options::WithDelta) != Options::None; }

bool Scheme::isDetouched() const { return (_flags & Options::Detouched) != Options::None; }

bool Scheme::isCompressed() const { return (_flags & Options::Compressed) != Options::None; }

bool Scheme::hasFullText() const { return !_fullTextFields.empty(); }

const Scheme &Scheme::define(std::initializer_list<Field> il) {
	for (auto &it : il) {
		auto fname = it.getName();
		if (it.getType() == Type::Image) {
			auto image = static_cast<const FieldImage *>(it.getSlot());
			auto &thumbnails = image->thumbnails;
			for (auto &thumb : thumbnails) {
				auto new_f = _fields.emplace(thumb.name,
											Field::Image(String(thumb.name),
													MaxImageSize(thumb.width, thumb.height)))
									 .first;
				((FieldImage *)(new_f->second.getSlot()))->primary = false;
			}
		}
		if (it.hasFlag(Flags::ForceExclude)) {
			_hasForceExclude = true;
		}
		if (it.getType() == Type::Virtual) {
			_hasVirtuals = true;
		}
		if (it.isFile()) {
			_hasFiles = true;
		}
		_fields.emplace(fname.str<Interface>(), sp::move(const_cast<Field &>(it)));
	}

	updateLimits();
	return *this;
}

const Scheme &Scheme::define(Vector<Field> &&il) {
	for (auto &it : il) {
		auto fname = it.getName();
		if (it.getType() == Type::Image) {
			auto image = static_cast<const FieldImage *>(it.getSlot());
			auto &thumbnails = image->thumbnails;
			for (auto &thumb : thumbnails) {
				auto new_f = _fields.emplace(thumb.name,
											Field::Image(String(thumb.name),
													MaxImageSize(thumb.width, thumb.height)))
									 .first;
				((FieldImage *)(new_f->second.getSlot()))->primary = false;
			}
		}
		if (it.hasFlag(Flags::ForceExclude)) {
			_hasForceExclude = true;
		}
		if (it.getType() == Type::Virtual) {
			_hasVirtuals = true;
		}
		if (it.isFile()) {
			_hasFiles = true;
		}
		_fields.emplace(fname.str<Interface>(), sp::move(const_cast<Field &>(it)));
	}

	updateLimits();
	return *this;
}

const Scheme &Scheme::define(AccessRole &&role) {
	if (role.users.count() == 1) {
		for (size_t i = 0; i < role.users.size(); ++i) {
			if (role.users.test(i)) {
				setAccessRole(AccessRoleId(i), sp::move(role));
				break;
			}
		}
	} else {
		for (size_t i = 0; i < role.users.size(); ++i) {
			if (role.users.test(i)) {
				setAccessRole(AccessRoleId(i), AccessRole(role));
			}
		}
	}
	return *this;
}

const Scheme &Scheme::define(UniqueConstraintDef &&def) {
	Vector<const Field *> fields;
	fields.reserve(def.fields.size());
	for (auto &it : def.fields) {
		if (auto f = getField(it)) {
			auto iit = std::lower_bound(fields.begin(), fields.end(), f);
			if (iit == fields.end()) {
				fields.emplace_back(f);
			} else if (*iit != f) {
				fields.emplace(iit, f);
			}
		} else {
			log::source().error("Scheme", "Field for unique constraint not found",
					data::EncodeFormat::Pretty, Value(it));
		}
	}

	_unique.emplace_back(
			StringView(toString(_name, "_", string::tolower<Interface>(def.name), "_unique"))
					.pdup(_unique.get_allocator()),
			sp::move(fields));
	return *this;
}

const Scheme &Scheme::define(Bytes &&dict) {
	_compressDict = sp::move(dict);
	return *this;
}

bool Scheme::init() {
	if (_init) {
		return true;
	}

	memory::context<pool_t *> ctx(_fields.get_allocator(), memory::context<pool_t *>::conditional);

	// init non-linked object fields as StrongReferences
	for (auto &fit : _fields) {
		const_cast<Field::Slot *>(fit.second.getSlot())->owner = this;
		switch (fit.second.getType()) {
		case Type::Object:
		case Type::Set:
			if (auto slot = fit.second.getSlot<FieldObject>()) {
				if (slot->linkage == Linkage::Auto && slot->onRemove == RemovePolicy::Null
						&& !slot->hasFlag(Flags::Reference)) {
					if (!getForeignLink(slot)) {
						// assume strong reference
						auto mutSlot = const_cast<FieldObject *>(slot);
						mutSlot->onRemove = RemovePolicy::StrongReference;
						mutSlot->flags |= Flags::Reference;
					}
				}
			}
			break;
		case Type::FullTextView: {
			auto slot = static_cast<const FieldFullTextView *>(fit.second.getSlot());
			for (auto &req_it : slot->requireFields) {
				if (auto f = getField(req_it)) {
					_fullTextFields.emplace(f);
				}
			}
			break;
		}
		case Type::Extra: {
			auto slot = static_cast<const FieldExtra *>(fit.second.getSlot());
			Scheme_setOwner(this, slot->fields);
			break;
		}
		case Type::Array: {
			auto slot = static_cast<const FieldArray *>(fit.second.getSlot());
			auto arraySlot = slot->tfield.getSlot();
			const_cast<Field::Slot *>(arraySlot)->owner = this;
			if (arraySlot->type == Type::Extra) {
				auto extraSlot = static_cast<const FieldExtra *>(arraySlot);
				Scheme_setOwner(this, extraSlot->fields);
			}
			break;
		}
		case Type::View: {
			auto slot = static_cast<const FieldView *>(fit.second.getSlot());
			if (slot->scheme) {
				const_cast<Scheme *>(slot->scheme)->addView(this, &fit.second);
			}
			break;
		}
		default: break;
		}
		if (fit.second.getSlot()->autoField.defaultFn) {
			auto &autoF = fit.second.getSlot()->autoField;
			for (auto &a_it : autoF.schemes) {
				const_cast<Scheme &>(a_it.scheme).addAutoField(this, &fit.second, a_it);
			}
		}
		if (fit.second.hasFlag(Flags::Composed)
				&& (fit.second.getType() == Type::Object || fit.second.getType() == Type::Set)) {
			auto slot = static_cast<const FieldObject *>(fit.second.getSlot());
			if (slot->scheme) {
				const_cast<Scheme *>(slot->scheme)->addParent(this, &fit.second);
			}
		}
	}

	_init = true;
	return true;
}

void Scheme::addFlags(Options opts) { _flags |= opts; }

void Scheme::cloneFrom(Scheme *source) {
	for (auto &it : source->_fields) { _fields.emplace(it.first, it.second); }
}

StringView Scheme::getName() const { return _name; }
bool Scheme::hasAliases() const {
	for (auto &it : _fields) {
		if (it.second.getType() == Type::Text && it.second.getTransform() == Transform::Alias) {
			return true;
		}
	}
	return false;
}

bool Scheme::isProtected(const StringView &key) const {
	auto it = _fields.find(key);
	if (it != _fields.end()) {
		return it->second.isProtected();
	}
	return false;
}

const Set<const Field *> &Scheme::getForceInclude() const { return _forceInclude; }

const Map<String, Field> &Scheme::getFields() const { return _fields; }

const Field *Scheme::getField(const StringView &key) const {
	auto it = _fields.find(key);
	if (it != _fields.end()) {
		return &it->second;
	}
	if (key == "__oid") {
		return &_oidField;
	}
	return nullptr;
}

const Vector<Scheme::UniqueConstraint> &Scheme::getUnique() const { return _unique; }

BytesView Scheme::getCompressDict() const { return _compressDict; }

const Field *Scheme::getForeignLink(const FieldObject *f) const {
	if (!f || f->onRemove == RemovePolicy::Reference
			|| f->onRemove == RemovePolicy::StrongReference) {
		return nullptr;
	}
	auto &link = f->link;
	auto nextScheme = f->scheme;
	if (f->linkage == Linkage::Auto) {
		auto &nextFields = nextScheme->getFields();
		for (auto &it : nextFields) {
			auto &nextField = it.second;
			if (nextField.getType() == Type::Object
					|| (nextField.getType() == Type::Set && f->getType() == Type::Object)) {
				auto nextSlot = static_cast<const FieldObject *>(nextField.getSlot());
				if (nextSlot->scheme == this) {
					return &nextField;
				}
			}
		}
	} else if (f->linkage == Linkage::Manual) {
		auto nextField = nextScheme->getField(link);
		if (nextField
				&& (nextField->getType() == Type::Object
						|| (nextField->getType() == Type::Set && f->getType() == Type::Object))) {
			auto nextSlot = static_cast<const FieldObject *>(nextField->getSlot());
			if (nextSlot->scheme == this) {
				return nextField;
			}
		}
	}
	return nullptr;
}

const Field *Scheme::getForeignLink(const Field &f) const {
	if (f.getType() == Type::Set || f.getType() == Type::Object) {
		auto slot = static_cast<const FieldObject *>(f.getSlot());
		return getForeignLink(slot);
	}
	return nullptr;
}
const Field *Scheme::getForeignLink(const StringView &fname) const {
	auto f = getField(fname);
	if (f) {
		return getForeignLink(*f);
	}
	return nullptr;
}

bool Scheme::isAtomicPatch(const Value &val) const {
	if (val.isDictionary()) {
		for (auto &it : val.asDict()) {
			auto f = getField(it.first);

			if (f
					&& (
							// extra field should use select-update
							f->getType() == Type::Extra

							// virtual field should use select-update
							|| f->getType() == Type::Virtual

							// force-includes used to update views, so, we need select-update
							|| _forceInclude.find(f) != _forceInclude.end()

							// for full-text views update
							|| _fullTextFields.find(f) != _fullTextFields.end()

							// auto fields requires select-update
							|| _autoFieldReq.find(f) != _autoFieldReq.end()

							// select-update required for replace filters
							|| f->getSlot()->replaceFilterFn)) {
				return false;
			}
		}
		return true;
	}
	return false;
}

uint64_t Scheme::hash(ValidationLevel l) const {
	StringStream stream;
	for (auto &it : _fields) { it.second.hash(stream, l); }
	return std::hash<String>{}(stream.weak());
}

const Vector<Scheme::ViewScheme *> &Scheme::getViews() const { return _views; }

Vector<const Field *> Scheme::getPatchFields(const Value &patch) const {
	Vector<const Field *> ret;
	ret.reserve(patch.size());
	for (auto &it : patch.asDict()) {
		if (auto f = getField(it.first)) {
			ret.emplace_back(f);
		}
	}
	return ret;
}

const Scheme::AccessTable &Scheme::getAccessTable() const { return _roles; }

const AccessRole *Scheme::getAccessRole(AccessRoleId id) const {
	return _roles[stappler::toInt(id)];
}

void Scheme::setAccessRole(AccessRoleId id, AccessRole &&r) {
	if (stappler::toInt(id) < stappler::toInt(AccessRoleId::Max)) {
		_roles[stappler::toInt(id)] = new (std::nothrow) AccessRole(sp::move(r));
		_hasAccessControl = true;
	}
}

bool Scheme::save(const Transaction &t, Object *obj) const {
	Worker w(*this, t);
	Set<const Field *> fields;

	Value tmp(obj->_data);
	return t.save(w, obj->getObjectId(), tmp, obj->_data, fields) ? true : false;
}

bool Scheme::hasFiles() const { return _hasFiles; }

bool Scheme::hasForceExclude() const { return _hasForceExclude; }

bool Scheme::hasAccessControl() const { return _hasAccessControl; }

bool Scheme::hasVirtuals() const { return _hasVirtuals; }

Value Scheme::createWithWorker(Worker &w, const Value &data, bool isProtected) const {
	if (!data.isDictionary() && !data.isArray()) {
		w.getApplicationInterface()->error("Storage", "Invalid data for object");
		return Value();
	}

	auto checkRequired = [&](StringView f, const Value &changeSet) {
		auto &val = changeSet.getValue(f);
		if (val.isNull()) {
			w.getApplicationInterface()->error("Storage", "No value for required field",
					Value({std::make_pair("field", Value(f))}));
			return false;
		}
		return true;
	};

	Value changeSet = data;
	if (data.isDictionary()) {
		transform(changeSet,
				isProtected ? TransformAction::ProtectedCreate : TransformAction::Create);
	} else {
		for (auto &it : changeSet.asArray()) {
			if (it) {
				transform(it,
						isProtected ? TransformAction::ProtectedCreate : TransformAction::Create);
			}
		}
	}

	bool stop = false;
	for (auto &it : _fields) {
		if (it.second.hasFlag(Flags::Required)) {
			if (changeSet.isDictionary()) {
				if (!checkRequired(it.first, changeSet)) {
					stop = true;
				}
			} else {
				for (auto &iit : changeSet.asArray()) {
					if (!checkRequired(it.first, iit)) {
						iit = Value();
					}
				}
			}
		}
	}

	if (stop) {
		return Value();
	}

	Value retVal;
	if (w.perform([&, this](const Transaction &t) -> bool {
		Value patch(createFilePatch(t, data, changeSet));
		if (auto ret = t.create(w, changeSet)) {
			touchParents(t, ret);
			for (auto &it : _views) { updateView(t, ret, it, Vector<uint64_t>()); }
			retVal = sp::move(ret);
			return true;
		} else {
			if (patch.isDictionary() || patch.isArray()) {
				purgeFilePatch(t, patch);
			}
		}
		return false;
	})) {
		return retVal;
	}

	return Value();
}

Value Scheme::updateWithWorker(Worker &w, uint64_t oid, const Value &data, bool isProtected) const {
	bool success = false;
	Value changeSet;

	std::tie(success, changeSet) = prepareUpdate(data, isProtected);
	if (!success) {
		return Value();
	}

	Value ret;
	w.perform([&, this](const Transaction &t) -> bool {
		Value filePatch(createFilePatch(t, data, changeSet));
		if (changeSet.empty()) {
			w.getApplicationInterface()->error("Storage", "Empty changeset for id",
					Value({std::make_pair("oid", Value((int64_t)oid))}));
			return false;
		}

		ret = patchOrUpdate(w, oid, changeSet);
		if (ret.isNull()) {
			if (filePatch.isDictionary()) {
				purgeFilePatch(t, filePatch);
			}
			w.getApplicationInterface()->error("Storage", "Fail to update object for id",
					Value({std::make_pair("oid", Value((int64_t)oid))}));
			return false;
		}
		return true;
	});

	return ret;
}

Value Scheme::updateWithWorker(Worker &w, const Value &obj, const Value &data,
		bool isProtected) const {
	uint64_t oid = obj.getInteger("__oid");
	if (!oid) {
		w.getApplicationInterface()->error("Storage", "Invalid data for object");
		return Value();
	}

	bool success = false;
	Value changeSet;

	std::tie(success, changeSet) = prepareUpdate(data, isProtected);
	if (!success) {
		return Value();
	}

	Value ret;
	w.perform([&, this](const Transaction &t) -> bool {
		Value filePatch(createFilePatch(t, data, changeSet));
		if (changeSet.empty()) {
			w.getApplicationInterface()->error("Storage", "Empty changeset for id",
					Value({std::make_pair("oid", Value((int64_t)oid))}));
			return false;
		}

		Value tmp(obj);
		ret = patchOrUpdate(w, tmp, changeSet);
		if (ret.isNull()) {
			if (filePatch.isDictionary()) {
				purgeFilePatch(t, filePatch);
			}
			w.getApplicationInterface()->error("Storage", "No object for id to update",
					Value({std::make_pair("oid", Value((int64_t)oid))}));
			return false;
		}
		return true;
	});

	return ret;
}

stappler::Pair<bool, Value> Scheme::prepareUpdate(const Value &data, bool isProtected) const {
	if (!data.isDictionary()) {
		log::source().error("Storage", "Invalid changeset data for object");
		return stappler::pair(false, Value());
	}

	Value changeSet = data;
	transform(changeSet, isProtected ? TransformAction::ProtectedUpdate : TransformAction::Update);

	bool stop = false;
	for (auto &it : _fields) {
		if (changeSet.hasValue(it.first)) {
			auto &val = changeSet.getValue(it.first);
			if (val.isNull() && it.second.hasFlag(Flags::Required)) {
				log::source().error("Storage", "Value for required field can not be removed",
						data::EncodeFormat::Pretty,
						Value({std::make_pair("field", Value(it.first))}));
				stop = true;
			}
		}
	}

	if (stop) {
		return stappler::pair(false, Value());
	}

	return stappler::pair(true, changeSet);
}

void Scheme::touchParents(const Transaction &t, const Value &obj) const {
	t.performAsSystem([&, this]() -> bool {
		if (!_parents.empty()) {
			Map<int64_t, const Scheme *> parentsToUpdate;
			extractParents(parentsToUpdate, t, obj, false);
			for (auto &it : parentsToUpdate) { Worker(*it.second, t).touch(it.first); }
		}
		return true;
	});
}

void Scheme::extractParents(Map<int64_t, const Scheme *> &parentsToUpdate, const Transaction &t,
		const Value &obj, bool isChangeSet) const {
	auto id = obj.getInteger("__oid");
	for (auto &it : _parents) {
		if (it->backReference) {
			if (auto value = obj.getInteger(it->backReference->getName())) {
				parentsToUpdate.emplace(value, it->scheme);
			}
		} else if (!isChangeSet && id) {
			auto vec = t.getAdapter().getReferenceParents(*this, id, it->scheme, it->pointerField);
			for (auto &value : vec) { parentsToUpdate.emplace(value, it->scheme); }
		}
	}
}

Value Scheme::updateObject(Worker &w, Value &obj, Value &changeSet) const {
	Set<const Field *> fieldsToUpdate;

	Vector<stappler::Pair<const ViewScheme *, Vector<uint64_t>>> viewsToUpdate;
	viewsToUpdate.reserve(_views.size());
	Map<int64_t, const Scheme *> parentsToUpdate;

	Value replacements;

	if (!_parents.empty()) {
		extractParents(parentsToUpdate, w.transaction(), obj);
		extractParents(parentsToUpdate, w.transaction(), changeSet, true);
	}

	// find what fields and views should be updated
	for (auto &it : changeSet.asDict()) {
		auto &fieldName = it.first;
		if (auto f = getField(fieldName)) {
			auto slot = f->getSlot();
			auto &val = obj.getValue(it.first);
			if (!slot->replaceFilterFn || slot->replaceFilterFn(*this, obj, val, it.second)) {
				fieldsToUpdate.emplace(f);

				if (_forceInclude.find(f) != _forceInclude.end()
						|| _autoFieldReq.find(f) != _autoFieldReq.end()) {
					for (auto &it : _views) {
						if (it->fields.find(f) != it->fields.end()) {
							auto lb = std::lower_bound(viewsToUpdate.begin(), viewsToUpdate.end(),
									it,
									[](stappler::Pair<const ViewScheme *, Vector<uint64_t>> &l,
											const ViewScheme *r) -> bool { return l.first < r; });
							if (lb == viewsToUpdate.end() && lb->first != it) {
								viewsToUpdate.emplace(lb, stappler::pair(it, Vector<uint64_t>()));
							}
						}
					}
				}
			}
		}
	}

	// acquire current views state
	for (auto &it : viewsToUpdate) { it.second = getLinkageForView(obj, *it.first); }

	if (!viewsToUpdate.empty() || !parentsToUpdate.empty()) {
		if (w.perform([&, this](const Transaction &t) {
			if (t.save(w, obj.getInteger("__oid"), obj, changeSet, fieldsToUpdate)) {
				t.performAsSystem([&]() -> bool {
					for (auto &it : parentsToUpdate) { Worker(*it.second, t).touch(it.first); }
					return true;
				});
				for (auto &it : viewsToUpdate) { updateView(t, obj, it.first, it.second); }
				return true;
			}
			return false;
		})) {
			return obj;
		}
	} else if (auto ret = w.transaction().save(w, obj.getInteger("__oid"), obj, changeSet,
					   fieldsToUpdate)) {
		return ret;
	}

	return Value();
}

void Scheme::touchWithWorker(Worker &w, uint64_t id) const {
	Value patch;
	transform(patch, TransformAction::Touch);
	w.includeNone();
	patchOrUpdate(w, id, patch);
}

void Scheme::touchWithWorker(Worker &w, const Value &obj) const {
	Value tmp(obj);
	Value patch;
	transform(patch, TransformAction::Touch);
	w.includeNone();
	patchOrUpdate(w, tmp, patch);
}

Value Scheme::fieldWithWorker(Action a, Worker &w, uint64_t oid, const Field &f,
		Value &&patch) const {
	switch (a) {
	case Action::Get:
	case Action::Count: return w.transaction().field(a, w, oid, f, sp::move(patch)); break;
	case Action::Set:
		if (f.transform(*this, oid, patch)) {
			Value ret;
			w.perform([&](const Transaction &t) -> bool {
				ret = t.field(a, w, oid, f, sp::move(patch));
				return !ret.isNull();
			});
			return ret;
		}
		break;
	case Action::Remove:
		return Value(w.perform([&](const Transaction &t) -> bool {
			return t.field(a, w, oid, f, sp::move(patch)) ? true : false;
		}));
		break;
	case Action::Append:
		if (f.transform(*this, oid, patch)) {
			Value ret;
			w.perform([&](const Transaction &t) -> bool {
				ret = t.field(a, w, oid, f, sp::move(patch));
				return !ret.isNull();
			});
			return ret;
		}
		break;
	}
	return Value();
}

Value Scheme::fieldWithWorker(Action a, Worker &w, const Value &obj, const Field &f,
		Value &&patch) const {
	switch (a) {
	case Action::Get:
	case Action::Count: return w.transaction().field(a, w, obj, f, sp::move(patch)); break;
	case Action::Set:
		if (f.transform(*this, obj, patch)) {
			Value ret;
			w.perform([&](const Transaction &t) -> bool {
				ret = t.field(a, w, obj, f, sp::move(patch));
				return !ret.isNull();
			});
			return ret;
		}
		break;
	case Action::Remove:
		return Value(w.perform([&](const Transaction &t) -> bool {
			return t.field(a, w, obj, f, sp::move(patch)).asBool();
		}));
		break;
	case Action::Append:
		if (f.transform(*this, obj, patch)) {
			Value ret;
			w.perform([&](const Transaction &t) -> bool {
				ret = t.field(a, w, obj, f, sp::move(patch));
				return !ret.isNull();
			});
			return ret;
		}
		break;
	}
	return Value();
}

Value Scheme::setFileWithWorker(Worker &w, uint64_t oid, const Field &f, InputFile &file) const {
	Value ret;
	w.perform([&, this](const Transaction &t) -> bool {
		Value patch;
		transform(patch, TransformAction::Update);
		auto d = createFile(t, f, file);
		if (d.isInteger()) {
			patch.setValue(d, f.getName().str<Interface>());
		} else {
			patch.setValue(sp::move(d));
		}
		if (patchOrUpdate(w, oid, patch)) {
			// resolve files
			ret = File::getData(t, patch.getInteger(f.getName()));
			return true;
		} else {
			purgeFilePatch(t, patch);
			return false;
		}
	});
	return ret;
}

Value Scheme::doPatch(Worker &w, const Transaction &t, uint64_t id, Value &patch) const {
	if (auto ret = t.patch(w, id, patch)) {
		touchParents(t, ret);
		return ret;
	}
	return Value();
}

Value Scheme::patchOrUpdate(Worker &w, uint64_t id, Value &patch) const {
	if (!patch.empty()) {
		Value ret;
		w.perform([&, this](const Transaction &t) {
			auto r = getAccessRole(t.getRole());
			auto d = getAccessRole(AccessRoleId::Default);
			// if we have save callback - we unable to do patches
			if (!isAtomicPatch(patch) || (r && r->onSave && !r->onPatch)
					|| (d && d->onSave && !d->onPatch)) {
				if (auto obj = makeObjectForPatch(t, id, Value(), patch)) {
					t.setObject(id, Value(obj));
					if ((ret = updateObject(w, obj, patch))) {
						return true;
					}
				}
			} else {
				ret = doPatch(w, w.transaction(), id, patch);
				if (ret) {
					return true;
				}
			}
			return false;
		});
		return ret;
	}
	return Value();
}

Value Scheme::patchOrUpdate(Worker &w, Value &obj, Value &patch) const {
	auto isObjectValid = [&, this](const Value &obj) -> bool {
		for (auto &it : patch.asDict()) {
			if (!obj.hasValue(it.first)) {
				return false;
			}
		}
		for (auto &it : _forceInclude) {
			if (!obj.hasValue(it->getName())) {
				return false;
			}
		}
		return true;
	};

	if (!patch.empty()) {
		Value ret;
		w.perform([&, this](const Transaction &t) {
			if (isAtomicPatch(patch)) {
				ret = doPatch(w, t, obj.getInteger("__oid"), patch);
				if (ret) {
					return true;
				}
			} else {
				auto id = obj.getInteger("__oid");
				if (isObjectValid(obj)) {
					if ((ret = updateObject(w, obj, patch))) {
						return true;
					}
				} else if (auto patchObj = makeObjectForPatch(t, id, obj, patch)) {
					t.setObject(id, Value(patchObj));
					if ((ret = updateObject(w, patchObj, patch))) {
						return true;
					}
				}
			}
			return false;
		});
		return ret;
	}
	return Value();
}

bool Scheme::removeWithWorker(Worker &w, uint64_t oid) const {
	bool hasAuto = false;
	for (auto &it : _views) {
		if (it->autoField) {
			hasAuto = true;
			break;
		}
	}

	if (!_parents.empty() || hasAuto) {
		return w.perform([&, this](const Transaction &t) {
			Query query;
			prepareGetQuery(query, oid, true);
			for (auto &it : _parents) {
				if (it->backReference) {
					query.include(it->backReference->getName());
				}
			}
			if (auto obj = Worker(*this, t).asSystem().reduceGetQuery(query, true)) {
				touchParents(t, obj); // if transaction fails - all changes will be rolled back

				for (auto &it : _views) {
					if (it->autoField) {
						Vector<uint64_t> ids = getLinkageForView(obj, *it);
						for (auto &id : ids) {
							t.scheduleAutoField(*it->scheme, *it->viewField, id);
						}
					}
				}

				return t.remove(w, oid);
			}
			return false;
		});
	} else {
		return w.perform([&](const Transaction &t) { return t.remove(w, oid); });
	}
}

bool Scheme::foreachWithWorker(Worker &w, const Query &q, const Callback<bool(Value &)> &cb) const {
	return w.transaction().foreach (w, q, cb);
}

Value Scheme::selectWithWorker(Worker &w, const Query &q) const {
	return w.transaction().select(w, q);
}

size_t Scheme::countWithWorker(Worker &w, const Query &q) const {
	return w.transaction().count(w, q);
}

Value &Scheme::transform(Value &d, TransformAction a) const {
	// drop readonly and not existed fields
	auto &dict = d.asDict();
	auto it = dict.begin();
	while (it != dict.end()) {
		auto &fname = it->first;
		auto f_it = _fields.find(fname);
		if (f_it == _fields.end()
				|| f_it->second.getType() == Type::FullTextView

				// we can write into readonly field only in protected mode
				|| (f_it->second.hasFlag(Flags::ReadOnly) && a != TransformAction::ProtectedCreate
						&& a != TransformAction::ProtectedUpdate)

				// we can drop files in all modes...
				|| (f_it->second.isFile()
						&& !it->second.isNull()
						// but we can write files as ints only in protected mode
						&& ((a != TransformAction::ProtectedCreate
									&& a != TransformAction::ProtectedUpdate)
								|| !it->second.isInteger()))) {
			it = dict.erase(it);
		} else {
			it++;
		}
	}

	// write defaults
	for (auto &it : _fields) {
		auto &field = it.second;
		if (a == TransformAction::Create || a == TransformAction::ProtectedCreate) {
			if (field.hasFlag(Flags::AutoMTime) && !d.hasValue(it.first)) {
				d.setInteger(stappler::Time::now().toMicroseconds(), it.first);
			} else if (field.hasFlag(Flags::AutoCTime) && !d.hasValue(it.first)) {
				d.setInteger(stappler::Time::now().toMicroseconds(), it.first);
			} else if (field.hasDefault() && !d.hasValue(it.first)) {
				if (auto def = field.getDefault(d)) {
					d.setValue(sp::move(def), it.first);
				}
			}
		} else if ((a == TransformAction::Update || a == TransformAction::ProtectedUpdate
						   || a == TransformAction::Touch)
				&& field.hasFlag(Flags::AutoMTime)) {
			if ((!d.empty() && !d.hasValue(it.first)) || a == TransformAction::Touch) {
				d.setInteger(stappler::Time::now().toMicroseconds(), it.first);
			}
		}
	}

	if (!d.empty()) {
		auto &dict = d.asDict();
		auto it = dict.begin();
		while (it != dict.end()) {
			auto &field = _fields.at(it->first);
			if (it->second.isNull()
					&& (a == TransformAction::Update || a == TransformAction::ProtectedUpdate
							|| a == TransformAction::Touch)) {
				it++;
			} else if (!field.transform(*this, d, it->second,
							   (a == TransformAction::Create
									   || a == TransformAction::ProtectedCreate))) {
				it = dict.erase(it);
			} else {
				it++;
			}
		}
	}

	return d;
}

Value Scheme::createFile(const Transaction &t, const Field &field, InputFile &file) const {
	//check if content type is valid
#if MODULE_STAPPLER_BITMAP
	if (field.getType() == Type::Image) {
		if (file.type == "application/octet-stream" || file.type.empty()) {
			file.type = bitmap::getMimeType(bitmap::detectFormat(file.file).first).str<Interface>();
		}
	}
#endif

	if (!File::validateFileField(t.getAdapter().getApplicationInterface(), field, file)) {
		return Value();
	}

	if (field.getType() == Type::File) {
		return File::createFile(t, field, file);
	} else if (field.getType() == Type::Image) {
		return File::createImage(t, field, file);
	}
	return Value();
}

Value Scheme::createFile(const Transaction &t, const Field &field, const BytesView &data,
		const StringView &itype, int64_t mtime) const {
	//check if content type is valid
	String type(itype.str<Interface>());
#if MODULE_STAPPLER_BITMAP
	if (field.getType() == Type::Image) {
		if (type == "application/octet-stream" || type.empty()) {
			stappler::CoderSource source(data);
			type = bitmap::getMimeType(bitmap::detectFormat(source).first).str<Interface>();
		}
	}
#endif

	if (!File::validateFileField(t.getAdapter().getApplicationInterface(), field, type, data)) {
		return Value();
	}

	if (field.getType() == Type::File) {
		return File::createFile(t, type, data, mtime);
	} else if (field.getType() == Type::Image) {
		return File::createImage(t, field, type, data, mtime);
	}
	return Value();
}

Value Scheme::makeObjectForPatch(const Transaction &t, uint64_t oid, const Value &obj,
		const Value &patch) const {
	Set<const Field *> includeFields;

	Query query;
	prepareGetQuery(query, oid, true);

	for (auto &it : patch.asDict()) {
		if (auto f = getField(it.first)) {
			if (!obj.hasValue(it.first)) {
				includeFields.emplace(f);
			}
		}
	}

	for (auto &it : _forceInclude) {
		if (!obj.hasValue(it->getName())) {
			includeFields.emplace(it);
		}
	}

	for (auto &it : _fields) {
		if (it.second.getType() == Type::FullTextView) {
			auto slot = it.second.getSlot<FieldFullTextView>();
			for (auto &p_it : patch.asDict()) {
				auto req_it = std::find(slot->requireFields.begin(), slot->requireFields.end(),
						p_it.first);
				if (req_it != slot->requireFields.end()) {
					for (auto &it : slot->requireFields) {
						if (auto f = getField(it)) {
							includeFields.emplace(f);
						}
					}
				}
			}
		}
	}

	for (auto &it : includeFields) { query.include(Query::Field(it->getName())); }

	auto ret = Worker(*this, t).asSystem().reduceGetQuery(query, false);
	if (!obj) {
		return ret;
	} else {
		for (auto &it : obj.asDict()) {
			if (!ret.hasValue(it.first)) {
				ret.setValue(it.second, it.first);
			}
		}
		return ret;
	}
}

Value Scheme::removeField(const Transaction &t, Value &obj, const Field &f, const Value &value) {
	if (f.isFile()) {
		auto scheme = t.getAdapter().getApplicationInterface()->getFileScheme();
		int64_t id = 0;
		if (value.isInteger()) {
			id = value.asInteger();
		} else if (value.isInteger("__oid")) {
			id = value.getInteger("__oid");
		}

		if (id) {
			if (Worker(*scheme, t).remove(id)) {
				return Value(id);
			}
		}
		return Value();
	}
	return Value(true);
}
void Scheme::finalizeField(const Transaction &t, const Field &f, const Value &value) {
	if (f.isFile()) {
		File::removeFile(t.getAdapter().getApplicationInterface(), value);
	}
}

void Scheme::updateLimits() { _config.updateLimits(_fields); }

bool Scheme::validateHint(uint64_t oid, const Value &hint) {
	if (!hint.isDictionary()) {
		return false;
	}
	auto hoid = hint.getInteger("__oid");
	if (hoid > 0 && (uint64_t)hoid == oid) {
		return validateHint(hint);
	}
	return false;
}

bool Scheme::validateHint(const String &alias, const Value &hint) {
	if (!hint.isDictionary()) {
		return false;
	}
	for (auto &it : _fields) {
		if (it.second.getType() == Type::Text && it.second.getTransform() == Transform::Alias) {
			if (hint.getString(it.first) == alias) {
				return validateHint(hint);
			}
		}
	}
	return false;
}

bool Scheme::validateHint(const Value &hint) {
	if (hint.size() > 1) {
		// all required fields should exists
		for (auto &it : _fields) {
			if (it.second.hasFlag(Flags::Required)) {
				if (!hint.hasValue(it.first)) {
					return false;
				}
			}
		}

		// no fields other then in schemes fields
		for (auto &it : hint.asDict()) {
			if (it.first != "__oid" && _fields.find(it.first) == _fields.end()) {
				return false;
			}
		}

		return true;
	}
	return false;
}

Value Scheme::createFilePatch(const Transaction &t, const Value &ival, Value &iChangeSet) const {
	auto createPatch = [&, this](const Value &val, Value &changeSet) {
		Value patch;
		for (auto &it : val.asDict()) {
			auto f = getField(it.first);
			if (f
					&& (f->getType() == Type::File
							|| (f->getType() == Type::Image
									&& static_cast<const FieldImage *>(f->getSlot())->primary))) {
				if (it.second.isInteger() && it.second.getInteger() < 0) {
					auto file = t.getAdapter().getApplicationInterface()->getFileFromContext(
							it.second.getInteger());
					if (file && file->isOpen()) {
						auto d = createFile(t, *f, *file);
						if (d.isInteger()) {
							patch.setValue(d, f->getName().str<Interface>());
						} else if (d.isDictionary()) {
							for (auto &it : d.asDict()) {
								patch.setValue(sp::move(it.second), it.first);
							}
						}
					}
				} else if (it.second.isDictionary()) {
					if ((it.second.isBytes("content") || it.second.isString("content"))
							&& it.second.isString("type")) {
						auto &c = it.second.getValue("content");
						Value d;
						if (c.isBytes()) {
							d = createFile(t, *f, c.getBytes(), it.second.getString("type"),
									it.second.getInteger("mtime"));
						} else {
							auto &str = it.second.getString("content");
							d = createFile(t, *f,
									BytesView((const uint8_t *)str.data(), str.size()),
									it.second.getString("type"), it.second.getInteger("mtime"));
						}
						if (d.isInteger()) {
							patch.setValue(d, f->getName().str<Interface>());
						} else if (d.isDictionary()) {
							for (auto &it : d.asDict()) {
								patch.setValue(sp::move(it.second), it.first);
							}
						}
					}
				}
			}
		}
		if (patch.isDictionary()) {
			for (auto &it : patch.asDict()) { changeSet.setValue(it.second, it.first); }
		}
		return patch;
	};

	if (ival.isDictionary()) {
		return createPatch(ival, iChangeSet);
	} else {
		size_t i = 0;
		Value ret;
		for (auto &it : ival.asArray()) {
			auto &changeSet = iChangeSet.getValue(i);
			if (!changeSet.isNull()) {
				if (auto vl = createPatch(it, changeSet)) {
					ret.addValue(sp::move(vl));
				}
			}
			++i;
		}
		return ret;
	}
}

void Scheme::purgeFilePatch(const Transaction &t, const Value &patch) const {
	if (patch.isDictionary()) {
		for (auto &it : patch.asDict()) {
			if (getField(it.first)) {
				File::purgeFile(t, it.second);
			}
		}
	} else if (patch.isArray()) {
		for (auto &v : patch.asArray()) {
			for (auto &it : v.asDict()) {
				if (getField(it.first)) {
					File::purgeFile(t, it.second);
				}
			}
		}
	}
}

void Scheme::addView(const Scheme *s, const Field *f) {
	memory::context<pool_t *> ctx(_views.get_allocator(), memory::context<pool_t *>::conditional);

	if (auto view = static_cast<const FieldView *>(f->getSlot())) {
		_views.emplace_back(new (std::nothrow) ViewScheme{s, f, *view});
		auto viewScheme = _views.back();

		bool linked = false;
		for (auto &it : view->requireFields) {
			auto fit = _fields.find(it);
			if (fit != _fields.end()) {
				if (fit->second.getType() == Type::Object && !view->linkage && !linked) {
					// try to autolink from required field
					auto nextSlot = static_cast<const FieldObject *>(fit->second.getSlot());
					if (nextSlot->scheme == s) {
						viewScheme->autoLink = &fit->second;
						linked = true;
					}
				}
				viewScheme->fields.emplace(&fit->second);
				_forceInclude.emplace(&fit->second);
			} else {
				log::source().error("Scheme", "Field for view not foumd",
						data::EncodeFormat::Pretty,
						Value({stappler::pair("view",
									   Value(toString(s->getName(), ".", f->getName()))),
							stappler::pair("field", Value(toString(getName(), ".", it)))}));
			}
		}
		if (!view->linkage && !linked) {
			// try to autolink from other fields
			for (auto &it : _fields) {
				auto &field = it.second;
				if (field.getType() == Type::Object) {
					auto nextSlot = static_cast<const FieldObject *>(field.getSlot());
					if (nextSlot->scheme == s) {
						viewScheme->autoLink = &field;
						viewScheme->fields.emplace(&field);
						_forceInclude.emplace(&field);
						linked = true;
						break;
					}
				}
			}
		}
		if (view->linkage) {
			linked = true;
		}
		if (!linked) {
			log::source().error("Scheme", "Failed to autolink view field",
					data::EncodeFormat::Pretty,
					Value({
						stappler::pair("view", Value(toString(s->getName(), ".", f->getName()))),
					}));
		}
	}
}

void Scheme::addAutoField(const Scheme *s, const Field *f, const AutoFieldScheme &a) {
	memory::context<pool_t *> ctx(_views.get_allocator(), memory::context<pool_t *>::conditional);

	_views.emplace_back(new (std::nothrow) ViewScheme{s, f, a});
	auto viewScheme = _views.back();

	if (this == s && !a.linkage) {
		for (auto &it : a.requiresForAuto) {
			if (auto f = getField(it)) {
				viewScheme->fields.emplace(f);
				_autoFieldReq.emplace(f);
			} else {
				log::source().error("Scheme", "Field for view not foumd",
						data::EncodeFormat::Pretty,
						Value({stappler::pair("view", Value(toString(s->getName(), ".", it))),
							stappler::pair("field", Value(toString(getName(), ".", it)))}));
			}
		}
	} else {
		bool linked = false;
		for (auto &it : a.requiresForLinking) {
			if (auto f = getField(it)) {
				if (f->getType() == Type::Object && !a.linkage && !linked) {
					// try to autolink from required field
					auto nextSlot = static_cast<const FieldObject *>(f->getSlot());
					if (nextSlot->scheme == s) {
						viewScheme->autoLink = f;
						linked = true;
					}
				}
				viewScheme->fields.emplace(f);
				_forceInclude.emplace(f);
			} else {
				log::source().error("Scheme", "Field for view not foumd",
						data::EncodeFormat::Pretty,
						Value({stappler::pair("view", Value(toString(s->getName(), ".", it))),
							stappler::pair("field", Value(toString(getName(), ".", it)))}));
			}
		}
		for (auto &it : a.requiresForAuto) {
			if (auto f = getField(it)) {
				viewScheme->fields.emplace(f);
				_autoFieldReq.emplace(f);
			} else {
				log::source().error("Scheme", "Field for view not foumd",
						data::EncodeFormat::Pretty,
						Value({stappler::pair("view", Value(toString(s->getName(), ".", it))),
							stappler::pair("field", Value(toString(getName(), ".", it)))}));
			}
		}
		if (!a.linkage && !linked) {
			// try to autolink from other fields
			for (auto &it : _fields) {
				auto &field = it.second;
				if (field.getType() == Type::Object) {
					auto nextSlot = static_cast<const FieldObject *>(field.getSlot());
					if (nextSlot->scheme == s) {
						viewScheme->autoLink = &field;
						viewScheme->fields.emplace(&field);
						_forceInclude.emplace(&field);
						linked = true;
						break;
					}
				}
			}
		}
		if (a.linkage) {
			linked = true;
		}
		if (!linked) {
			log::source().error("Scheme", "Failed to autolink view field",
					data::EncodeFormat::Pretty,
					Value({
						stappler::pair("view", Value(toString(s->getName(), ".", f->getName()))),
					}));
		}
	}
}

void Scheme::addParent(const Scheme *s, const Field *f) {
	memory::context<pool_t *> ctx(_parents.get_allocator(), memory::context<pool_t *>::conditional);

	_parents.emplace_back(new (std::nothrow) ParentScheme(s, f));
	auto &p = _parents.back();

	auto slot = static_cast<const FieldObject *>(f->getSlot());
	if (f->getType() == Type::Set) {
		auto link = s->getForeignLink(slot);
		if (link) {
			p->backReference = link;
			_forceInclude.emplace(p->backReference);
		}
	}
}

Vector<uint64_t> Scheme::getLinkageForView(const Value &obj, const ViewScheme &s) const {
	Vector<uint64_t> ids;
	ids.reserve(1);
	if (s.autoLink && s.autoLink->getSlot()) {
		if (auto id = obj.getInteger(s.autoLink->getName())) {
			ids.push_back(id);
		}
	} else if (s.autoField) {
		if (s.autoField->linkage) {
			ids = s.autoField->linkage(*s.scheme, *this, obj);
		} else if (&s.autoField->scheme == this) {
			ids.push_back(obj.getInteger("__oid"));
		}
	} else {
		auto view = s.viewField->getSlot<FieldView>();
		if (!view->viewFn) {
			return Vector<uint64_t>();
		}

		if (view->linkage) {
			ids = view->linkage(*s.scheme, *this, obj);
		}
	}
	return ids;
}

void Scheme::updateView(const Transaction &t, const Value &obj, const ViewScheme *scheme,
		const Vector<uint64_t> &orig) const {
	const FieldView *view = nullptr;
	if (scheme->viewField->getType() == Type::View) {
		view = static_cast<const FieldView *>(scheme->viewField->getSlot());
	}
	if ((!view || !view->viewFn) && !scheme->autoField) {
		return;
	}

	auto objId = obj.getInteger("__oid");

	// list of objects, that view fields should contain this object
	Vector<uint64_t> ids = getLinkageForView(obj, *scheme);

	if (scheme->autoField) {
		for (auto &it : orig) {
			auto ids_it = std::find(ids.begin(), ids.begin(), it);
			if (ids_it != ids.end()) {
				ids.erase(ids_it);
			}
			t.scheduleAutoField(*scheme->scheme, *scheme->viewField, it);
		}

		for (auto &it : ids) { t.scheduleAutoField(*scheme->scheme, *scheme->viewField, it); }
	} else {
		t.performAsSystem([&, this]() -> bool {
			t.removeFromView(*scheme->scheme, *view, objId, obj);

			if (!ids.empty()) {
				if (view->viewFn(*this, obj)) {
					for (auto &id : ids) {
						Value it;
						it.setInteger(objId, toString(getName(), "_id"));
						if (scheme->scheme) {
							it.setInteger(id, toString(scheme->scheme->getName(), "_id"));
						}
						t.addToView(*scheme->scheme, *view, id, obj, it);
					}
				}
			}
			return true;
		});
	}
}

} // namespace stappler::db
