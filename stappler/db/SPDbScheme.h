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

#ifndef STAPPLER_DB_SPDBSCHEME_H_
#define STAPPLER_DB_SPDBSCHEME_H_

#include "SPDbWorker.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC Scheme : public AllocBase {
public:
	enum Options : uint32_t {
		None = 0,
		WithDelta = 1 << 0,
		Detouched = 1 << 1,
		Compressed = 1 << 2,
	};

	struct ViewScheme : AllocBase {
		const Scheme *scheme = nullptr;
		const Field *viewField = nullptr;
		Set<const Field *> fields;
		const Field *autoLink = nullptr;
		const AutoFieldScheme *autoField = nullptr;

		ViewScheme(const Scheme *s, const Field *v, const FieldView &) : scheme(s), viewField(v) { }
		ViewScheme(const Scheme *s, const Field *v, const AutoFieldScheme &af)
		: scheme(s), viewField(v), autoField(&af) { }
	};

	struct ParentScheme : AllocPool {
		const Scheme *scheme = nullptr;
		const Field *pointerField = nullptr;
		const Field *backReference = nullptr;

		ParentScheme(const Scheme *s, const Field *v) : scheme(s), pointerField(v) { }
	};

	struct UniqueConstraint {
		StringView name;
		Vector<const Field *> fields;

		UniqueConstraint(StringView n, Vector<const Field *> &&f) : name(n), fields(sp::move(f)) { }
	};

	enum class TransformAction {
		Create,
		Update,
		Compare,
		ProtectedCreate,
		ProtectedUpdate,
		Touch
	};

	using FieldVec = Vector<const Field *>;
	using AccessTable = std::array<AccessRole *, stappler::toInt(AccessRoleId::Max)>;

	// field list to send, when no field is required to return
	static FieldVec EmptyFieldList() { return FieldVec{nullptr}; }

	static bool initSchemes(const Map<StringView, const Scheme *> &);

public:
	Scheme(const StringView &name, Options = Options::None, uint32_t v = 0);
	Scheme(const StringView &name, std::initializer_list<Field> il, Options = Options::None,
			uint32_t v = 0);

	Scheme(const Scheme &) = delete;
	Scheme &operator=(const Scheme &) = delete;

	Scheme(Scheme &&) = default;
	Scheme &operator=(Scheme &&) = default;

	bool hasDelta() const;
	bool isDetouched() const;
	bool isCompressed() const;
	bool hasFullText() const;

	const Scheme &define(std::initializer_list<Field> il);
	const Scheme &define(Vector<Field> &&il);
	const Scheme &define(AccessRole &&role);
	const Scheme &define(UniqueConstraintDef &&);
	const Scheme &define(Bytes &&);

	template <typename T, typename... Args>
	const Scheme &define(T &&il, Args &&...args);

	bool init();

	void addFlags(Options);

	void cloneFrom(Scheme *);

	uint32_t getVersion() const { return _version; }
	Options getFlags() const { return _flags; }
	StringView getName() const;
	bool hasAliases() const;

	bool isProtected(const StringView &) const;
	bool save(const Transaction &, Object *) const;

	bool hasFiles() const;
	bool hasForceExclude() const;
	bool hasAccessControl() const;
	bool hasVirtuals() const;

	const Set<const Field *> &getForceInclude() const;
	const Map<String, Field> &getFields() const;
	const Field *getField(const StringView &str) const;
	const Vector<UniqueConstraint> &getUnique() const;
	BytesView getCompressDict() const;

	const Set<const Field *> &getFullTextFields() const { return _fullTextFields; }

	const Field *getForeignLink(const FieldObject *f) const;
	const Field *getForeignLink(const Field &f) const;
	const Field *getForeignLink(const StringView &f) const;

	void setConfig(InputConfig cfg) { _config = cfg; }
	const InputConfig &getConfig() const { return _config; }

	size_t getMaxRequestSize() const { return _config.maxRequestSize; }
	size_t getMaxVarSize() const { return _config.maxVarSize; }
	size_t getMaxFileSize() const { return std::max(_config.maxFileSize, _config.maxVarSize); }

	bool isAtomicPatch(const Value &) const;

	uint64_t hash(ValidationLevel l = ValidationLevel::NamesAndTypes) const;

	const Vector<ViewScheme *> &getViews() const;
	Vector<const Field *> getPatchFields(const Value &patch) const;

	const AccessTable &getAccessTable() const;
	const AccessRole *getAccessRole(AccessRoleId) const;
	void setAccessRole(AccessRoleId, AccessRole &&);

	Value &transform(Value &, TransformAction = TransformAction::Create) const;

public: // worker interface
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, UpdateFlags = UpdateFlags::None) const -> Value;
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, StringView, UpdateFlags = UpdateFlags::None) const -> Value;
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, std::initializer_list<StringView> &&fields,
			UpdateFlags = UpdateFlags::None) const -> Value;
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, std::initializer_list<const char *> &&fields,
			UpdateFlags = UpdateFlags::None) const -> Value;
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, std::initializer_list<const Field *> &&fields,
			UpdateFlags = UpdateFlags::None) const -> Value;
	template <typename Storage, typename _Value>
	auto get(Storage &&, _Value &&, SpanView<const Field *> fields,
			UpdateFlags = UpdateFlags::None) const -> Value;

	// Select objects via callback iterator
	template <typename T>
	auto foreach (T &&t, const Query &, const Callback<bool(Value &)> &,
			UpdateFlags = UpdateFlags::None) const -> bool;

	// Select objects by query
	// - db::Transaction, db::Query
	template <typename T, typename... Args>
	auto select(T &&t, Args &&...args) const -> Value;

	// Create new object (single for dict or multi for array)
	// - db::Transaction, Value[, UpdateFlags][, Conflict]
	template <typename T, typename... Args>
	auto create(T &&t, Args &&...args) const -> Value;

	// Update object
	// - db::Transaction, Value obj, Value patch[, UpdateFlags]
	// - db::Transaction, int64_t id, Value patch[, UpdateFlags]
	template <typename T, typename... Args>
	auto update(T &&t, Args &&...args) const -> Value;

	// Remove object
	// - db::Transaction, Value obj
	// - db::Transaction, int64_t id
	template <typename T, typename... Args>
	auto remove(T &&t, Args &&...args) const -> bool;

	// Count resulting objects by query
	// - db::Transaction, db::Query
	template <typename T, typename... Args>
	auto count(T &&t, Args &&...args) const -> size_t;

	// Touch object (update autofields)
	// - db::Transaction, Value obj
	// - db::Transaction, int64_t id
	template <typename T, typename... Args>
	auto touch(T &&t, Args &&...args) const -> void;

	template <typename _Storage, typename _Value, typename _Field>
	auto getProperty(_Storage &&, _Value &&, _Field &&,
			std::initializer_list<StringView> fields) const -> Value;

	template <typename _Storage, typename _Value, typename _Field>
	auto getProperty(_Storage &&, _Value &&, _Field &&,
			const Set<const Field *> & = Set<const Field *>()) const -> Value;

	template <typename T, typename... Args>
	auto setProperty(T &&t, Args &&...args) const -> Value;
	template <typename T, typename... Args>
	auto appendProperty(T &&t, Args &&...args) const -> Value;
	template <typename T, typename... Args>
	auto clearProperty(T &&t, Args &&...args) const -> bool;
	template <typename T, typename... Args>
	auto countProperty(T &&t, Args &&...args) const -> size_t;

protected: // CRUD functions
	friend class Worker;

	bool foreachWithWorker(Worker &, const Query &, const Callback<bool(Value &)> &) const;

	// returns Array with zero or more Dictionaries with object data or Null value
	Value selectWithWorker(Worker &, const Query &) const;

	// returns Dictionary with single object data or Null value
	Value createWithWorker(Worker &, const Value &data, bool isProtected = false) const;

	Value updateWithWorker(Worker &, uint64_t oid, const Value &data,
			bool isProtected = false) const;
	Value updateWithWorker(Worker &, const Value &obj, const Value &data,
			bool isProtected = false) const;

	bool removeWithWorker(Worker &, uint64_t oid) const;

	size_t countWithWorker(Worker &, const Query &) const;

	void touchWithWorker(Worker &, uint64_t id) const;
	void touchWithWorker(Worker &, const Value &obj) const;

	Value fieldWithWorker(Action, Worker &, uint64_t oid, const Field &, Value && = Value()) const;
	Value fieldWithWorker(Action, Worker &, const Value &, const Field &, Value && = Value()) const;

	Value setFileWithWorker(Worker &w, uint64_t oid, const Field &, InputFile &) const;

	void initScheme();

	void addView(const Scheme *, const Field *);
	void addAutoField(const Scheme *, const Field *f, const AutoFieldScheme &);
	void addParent(const Scheme *, const Field *);

	Value createFilePatch(const Transaction &, const Value &val, Value &changeSet) const;
	void purgeFilePatch(const Transaction &t, const Value &) const;
	void mergeValues(const Field &f, const Value &obj, Value &original, Value &newVal) const;

	stappler::Pair<bool, Value> prepareUpdate(const Value &data, bool isProtected) const;
	Value updateObject(Worker &, Value &obj, Value &data) const;

	Value doPatch(Worker &w, const Transaction &t, uint64_t obj, Value &patch) const;

	Value patchOrUpdate(Worker &, uint64_t id, Value &patch) const;
	Value patchOrUpdate(Worker &, Value &obj, Value &patch) const;

	void touchParents(const Transaction &, const Value &obj) const;
	void extractParents(Map<int64_t, const Scheme *> &, const Transaction &, const Value &obj,
			bool isChangeSet = false) const;

	// returns:
	// - true if field was successfully removed
	// - null or false if field was not removed, we should abort transaction
	// - value, that will be sent to finalizeField if all fields will be removed
	Value removeField(const Transaction &, Value &, const Field &, const Value &old);
	void finalizeField(const Transaction &, const Field &, const Value &old);

	// call before object is created, used for additional checking or default values
	Value createFile(const Transaction &, const Field &, InputFile &) const;

	// call before object is created, when file is embedded into patch
	Value createFile(const Transaction &, const Field &, const BytesView &, const StringView &type,
			int64_t = 0) const;

	Value makeObjectForPatch(const Transaction &, uint64_t id, const Value &,
			const Value &patch) const;

	void updateLimits();

	bool validateHint(uint64_t, const Value &);
	bool validateHint(const String &alias, const Value &);
	bool validateHint(const Value &);

	Vector<uint64_t> getLinkageForView(const Value &, const ViewScheme &) const;

	void updateView(const Transaction &, const Value &, const ViewScheme *,
			const Vector<uint64_t> &) const;

	Map<String, Field> _fields;
	String _name;

	uint32_t _version = 0;
	Options _flags = Options::None;

	InputConfig _config;

	Vector<ViewScheme *> _views;
	Vector<ParentScheme *> _parents;
	Set<const Field *> _forceInclude;
	Set<const Field *> _fullTextFields;
	Set<const Field *> _autoFieldReq;

	bool _init = false;
	bool _hasFiles = false;
	bool _hasForceExclude = false;
	bool _hasAccessControl = false;
	bool _hasVirtuals = false;

	AccessTable _roles;
	Field _oidField;
	Vector<UniqueConstraint> _unique;
	Bytes _compressDict;
};

SP_DEFINE_ENUM_AS_MASK(Scheme::Options)

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s)).get(std::forward<_Value>(v), flags);
}

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, StringView it, UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s)).get(std::forward<_Value>(v), it, flags);
}

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, std::initializer_list<StringView> &&fields,
		UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s))
			.get(std::forward<_Value>(v), sp::move(fields), flags);
}

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, std::initializer_list<const char *> &&fields,
		UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s))
			.get(std::forward<_Value>(v), sp::move(fields), flags);
}

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, std::initializer_list<const Field *> &&fields,
		UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s))
			.get(std::forward<_Value>(v), sp::move(fields), flags);
}

template <typename Storage, typename _Value>
inline auto Scheme::get(Storage &&s, _Value &&v, SpanView<const Field *> fields,
		UpdateFlags flags) const -> Value {
	return Worker(*this, std::forward<Storage>(s)).get(std::forward<_Value>(v), fields, flags);
}

template <typename T>
inline auto Scheme::foreach (T &&s, const Query &q, const Callback<bool(Value &)> &cb,
		UpdateFlags flags) const -> bool {
	return Worker(*this, std::forward<T>(s)).foreach (q, cb, flags);
}

template <typename T, typename... Args>
inline auto Scheme::select(T &&t, Args &&...args) const -> Value {
	return Worker(*this, std::forward<T>(t)).select(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::create(T &&t, Args &&...args) const -> Value {
	return Worker(*this, std::forward<T>(t)).create(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::update(T &&t, Args &&...args) const -> Value {
	return Worker(*this, std::forward<T>(t)).update(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::remove(T &&t, Args &&...args) const -> bool {
	return Worker(*this, std::forward<T>(t)).remove(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::count(T &&t, Args &&...args) const -> size_t {
	return Worker(*this, std::forward<T>(t)).count(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::touch(T &&t, Args &&...args) const -> void {
	Worker(*this, std::forward<T>(t)).touch(std::forward<Args>(args)...);
}


template <typename _Storage, typename _Value, typename _Field>
inline auto Scheme::getProperty(_Storage &&s, _Value &&v, _Field &&f,
		std::initializer_list<StringView> fields) const -> Value {
	return Worker(*this, std::forward<_Storage>(s))
			.getField(std::forward<_Value>(v), std::forward<_Field>(f), fields);
}

template <typename _Storage, typename _Value, typename _Field>
auto Scheme::getProperty(_Storage &&s, _Value &&v, _Field &&f,
		const Set<const Field *> &fields) const -> Value {
	return Worker(*this, std::forward<_Storage>(s))
			.getField(std::forward<_Value>(v), std::forward<_Field>(f), fields);
}

template <typename T, typename... Args>
inline auto Scheme::setProperty(T &&t, Args &&...args) const -> Value {
	return Worker(*this, std::forward<T>(t)).setField(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::appendProperty(T &&t, Args &&...args) const -> Value {
	return Worker(*this, std::forward<T>(t)).appendField(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::clearProperty(T &&t, Args &&...args) const -> bool {
	return Worker(*this, std::forward<T>(t)).clearField(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto Scheme::countProperty(T &&t, Args &&...args) const -> size_t {
	return Worker(*this, std::forward<T>(t)).countField(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
const Scheme &Scheme::define(T &&il, Args &&...args) {
	define(std::forward<T>(il));
	define(std::forward<Args>(args)...);
	return *this;
}

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDBSCHEME_H_ */
