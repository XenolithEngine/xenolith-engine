/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_DB_SPDBQUERY_H_
#define STAPPLER_DB_SPDBQUERY_H_

#include "SPDb.h"

namespace STAPPLER_VERSIONIZED stappler::db {

enum class Resolve : uint32_t {
	None = 0,
	Files = 2,
	Sets = 4,
	Objects = 8,
	Arrays = 16,
	Ids = 32,
	Basics = 64,
	Defaults = 128,
	All = Files | Sets | Objects | Arrays | Defaults,
};

SP_DEFINE_ENUM_AS_MASK(Resolve);

class SP_PUBLIC Query : public AllocBase {
public:
	struct Field : public AllocBase {
		String name;
		Vector<Field> fields;

		Field(Field &&);
		Field(const Field &);

		Field &operator=(Field &&);
		Field &operator=(const Field &);

		template <typename Str>
		Field(Str &&);
		template <typename Str>
		Field(Str &&, Vector<String> &&);
		template <typename Str>
		Field(Str &&, std::initializer_list<String> &&);
		template <typename Str>
		Field(Str &&, Vector<Field> &&);
		template <typename Str>
		Field(Str &&, std::initializer_list<Field> &&);

		void setName(const char *);
		void setName(const StringView &);
		void setName(const String &);
		void setName(String &&);
		void setName(const Field &);
		void setName(Field &&);
	};

	using FieldsVec = Vector<Field>;

	struct Select : public AllocBase {
		Comparation compare = Comparation::Equal;
		Value value1;
		Value value2;
		String field;
		FullTextQuery textQuery;

		Select() { }
		Select(const StringView &f, Comparation c, Value &&v1, Value &&v2);
		Select(const StringView &f, Comparation c, int64_t v1, int64_t v2);
		Select(const StringView &f, Comparation c, const String &v);
		Select(const StringView &f, Comparation c, const StringView &v);
		Select(const StringView &f, Comparation c, FullTextQuery &&v);
	};

	struct SoftLimit {
		String field;
		size_t limit;
		Value offset;
	};

	static Query all();
	static Query field(int64_t id, const StringView &);
	static Query field(int64_t id, const StringView &, const Query &);

	static Resolve decodeResolve(const StringView &str);
	static String encodeResolve(Resolve);

	Query &select(const StringView &alias);
	Query &select(int64_t id);
	Query &select(const Value &);
	Query &select(Vector<int64_t> &&id);
	Query &select(SpanView<int64_t> id);
	Query &select(std::initializer_list<int64_t> &&id);

	Query &select(const StringView &f, Comparation c, const Value &v1, const Value &v2 = Value());
	Query &select(const StringView &f, const Value &v1); // special case for equality

	Query &select(const StringView &f, Comparation c, int64_t v1);
	Query &select(const StringView &f, Comparation c, int64_t v1, int64_t v2);
	Query &select(const StringView &f, const String &v);
	Query &select(const StringView &f, String &&v);

	Query &select(const StringView &f, const Bytes &v);
	Query &select(const StringView &f, Bytes &&v);

	Query &select(const StringView &f, FullTextQuery &&v);

	Query &select(Select &&q);

	Query &order(const StringView &f, Ordering o = Ordering::Ascending,
			size_t limit = stappler::maxOf<size_t>(), size_t offset = 0);
	Query &softLimit(const StringView &, Ordering, size_t limit, Value &&);

	Query &first(const StringView &f, size_t limit = 1, size_t offset = 0);
	Query &last(const StringView &f, size_t limit = 1, size_t offset = 0);

	Query &limit(size_t l, size_t off);
	Query &limit(size_t l);
	Query &offset(size_t l);

	Query &delta(uint64_t);
	Query &delta(const StringView &);

	template <typename... Args>
	Query &include(Field &&, Args &&...);

	Query &include(Field &&);
	Query &exclude(Field &&);

	Query &depth(uint16_t);

	Query &forUpdate();

	Query &clearFields();

	bool empty() const;

	StringView getQueryField() const;
	int64_t getQueryId() const;

	int64_t getSingleSelectId() const;
	const Vector<int64_t> &getSelectIds() const;
	StringView getSelectAlias() const;
	const Vector<Select> &getSelectList() const;

	const String &getOrderField() const;
	Ordering getOrdering() const;

	size_t getLimitValue() const;
	size_t getOffsetValue() const;

	const Value &getSoftLimitValue() const;

	bool hasSelectName() const; // id or alias
	bool hasSelectList() const;

	bool hasSelect() const;
	bool hasOrder() const;
	bool hasLimit() const;
	bool hasOffset() const;
	bool hasDelta() const;
	bool hasFields() const;
	bool isForUpdate() const;
	bool isSoftLimit() const;

	uint64_t getDeltaToken() const;

	uint16_t getResolveDepth() const;

	const FieldsVec &getIncludeFields() const;
	const FieldsVec &getExcludeFields() const;

	Value encode() const;

protected:
	String queryField;
	int64_t queryId = 0;

	Vector<int64_t> selectIds;
	String selectAlias;
	Vector<Select> selectList;

	Ordering ordering = Ordering::Ascending;
	String orderField;

	size_t limitValue = stappler::maxOf<size_t>();
	size_t offsetValue = 0;
	Value softLimitValue;

	uint64_t deltaToken;

	uint16_t resolveDepth = 1;

	FieldsVec fieldsInclude;
	FieldsVec fieldsExclude;
	bool update = false;
	bool _softLimit = false;
	bool _selected = false;
};

template <typename Str>
inline Query::Field::Field(Str &&str) {
	setName(std::forward<Str>(str));
}

template <typename Str>
inline Query::Field::Field(Str &&str, Vector<String> &&l) {
	setName(std::forward<Str>(str));
	for (auto &it : l) { fields.emplace_back(sp::move(it)); }
}

template <typename Str>
inline Query::Field::Field(Str &&str, std::initializer_list<String> &&l) {
	setName(std::forward<Str>(str));
	for (auto &it : l) { fields.emplace_back(String(sp::move(it))); }
}

template <typename Str>
inline Query::Field::Field(Str &&str, Vector<Field> &&l) : fields(sp::move(l)) {
	setName(std::forward<Str>(str));
}

template <typename Str>
inline Query::Field::Field(Str &&str, std::initializer_list<Field> &&l) {
	setName(std::forward<Str>(str));
	for (auto &it : l) { fields.emplace_back(sp::move(it)); }
}

template <typename... Args>
Query &Query::include(Field &&f, Args &&...args) {
	include(sp::move(f));
	include(std::forward<Args>(args)...);
	return *this;
}

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDBQUERY_H_ */
