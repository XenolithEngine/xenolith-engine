/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPDbQuery.h"

namespace STAPPLER_VERSIONIZED stappler::db {

Query::Field::Field(Field &&f) : name(sp::move(f.name)), fields(sp::move(f.fields)) { }

Query::Field::Field(const Field &f) : name(f.name), fields(f.fields) { }

Query::Field &Query::Field::operator=(Field &&f) {
	name = sp::move(f.name);
	fields = sp::move(f.fields);
	return *this;
}
Query::Field &Query::Field::operator=(const Field &f) {
	name = f.name;
	fields = f.fields;
	return *this;
}

void Query::Field::setName(const char *n) {
	name = n;
}
void Query::Field::setName(const StringView &n) {
	name = n.str<Interface>();
}
void Query::Field::setName(const String &n) {
	name = n;
}
void Query::Field::setName(String &&n) {
	name = sp::move(n);
}
void Query::Field::setName(const Field &f) {
	name = f.name;
	fields = f.fields;
}
void Query::Field::setName(Field &&f) {
	name = sp::move(f.name);
	fields = sp::move(f.fields);
}

Query::Select::Select(const StringView & f, Comparation c, Value && v1, Value && v2)
: compare(c), value1(sp::move(v1)), value2(sp::move(v2)), field(f.str<Interface>()) { }

Query::Select::Select(const StringView & f, Comparation c, int64_t v1, int64_t v2)
: compare(c), value1(v1), value2(v2), field(f.str<Interface>()) { }

Query::Select::Select(const StringView & f, Comparation c, const String & v)
: compare(Comparation::Equal), value1(v), value2(0), field(f.str<Interface>()) { }

Query::Select::Select(const StringView & f, Comparation c, const StringView & v)
: compare(Comparation::Equal), value1(v), value2(0), field(f.str<Interface>()) { }

Query::Select::Select(const StringView & f, Comparation c, FullTextQuery && v)
: compare(Comparation::Equal), field(f.str<Interface>()), textQuery(sp::move(v)) { }

Resolve Query::decodeResolve(const StringView &str) {
	if (str == "$all") {
		return Resolve::All;
	} else if (str == "$files") {
		return Resolve::Files;
	} else if (str == "$sets") {
		return Resolve::Sets;
	} else if (str == "$objects" || str == "$objs") {
		return Resolve::Objects;
	} else if (str == "$arrays") {
		return Resolve::Arrays;
	} else if (str == "$defaults" || str == "$defs") {
		return Resolve::Defaults;
	} else if (str == "$basics") {
		return Resolve::Basics;
	} else if (str == "$ids") {
		return Resolve::Ids;
	}
	return Resolve::None;
}

String Query::encodeResolve(Resolve res) {
	if ((res & Resolve::All) == Resolve::All) {
		return "$all";
	} else if ((res & Resolve::Files) != Resolve::None) {
		return "$files";
	} else if ((res & Resolve::Sets) != Resolve::None) {
		return "$sets";
	} else if ((res & Resolve::Objects) != Resolve::None) {
		return "$objs";
	} else if ((res & Resolve::Arrays) != Resolve::None) {
		return "$arrays";
	} else if ((res & Resolve::Defaults) != Resolve::None) {
		return "$defs";
	} else if ((res & Resolve::Basics) != Resolve::None) {
		return "$basics";
	}
	return String();
}

Query Query::all() { return Query(); }

Query Query::field(int64_t id, const StringView &f) {
	Query q;
	q.queryField = f.str<Interface>();
	q.queryId = id;
	return q;
}

Query Query::field(int64_t id, const StringView &f, const Query &iq) {
	Query q(iq);
	q.queryField = f.str<Interface>();
	q.queryId = id;
	return q;
}

Query & Query::select(const StringView &alias) {
	selectIds.clear();
	selectAlias = alias.str<Interface>();
	selectList.clear();
	return *this;
}

Query & Query::select(int64_t id) {
	selectIds.clear();
	selectIds.push_back(id);
	selectAlias.clear();
	selectList.clear();
	return *this;
}
Query & Query::select(const Value &val) {
	if (val.isInteger()) {
		selectIds.clear();
		selectIds.push_back(val.getInteger());
		selectAlias.clear();
		selectList.clear();
	} else if (val.isString()) {
		selectIds.clear();
		selectAlias = val.getString();
		selectList.clear();
	} else if (val.isArray()) {
		selectIds.clear();
		selectAlias.clear();
		selectList.clear();
		if (val.asArray().size() > 0) {
			for (auto &it : val.asArray()) {
				selectIds.emplace_back(it.asInteger());
			}
		} else {
			selectIds = Vector<int64_t>{-1};
		}
	} else if (val.isDictionary()) {
		selectIds.clear();
		selectAlias.clear();
		selectList.clear();
		for (auto &it : val.asDict()) {
			selectList.emplace_back(it.first, Comparation::Equal, Value(it.second), Value());
		}
	}
	return *this;
}

Query & Query::select(Vector<int64_t> &&id) {
	if (!id.empty()) {
		selectIds = sp::move(id);
	} else {
		selectIds = Vector<int64_t>{-1};
	}
	selectAlias.clear();
	selectList.clear();
	_selected = true;
	return *this;
}

Query & Query::select(SpanView<int64_t> id) {
	if (!id.empty()) {
		selectIds = id.vec<Interface>();
	} else {
		selectIds = Vector<int64_t>{-1};
	}
	selectAlias.clear();
	selectList.clear();
	_selected = true;
	return *this;
}

Query & Query::select(std::initializer_list<int64_t> &&id) {
	if (id.size() > 0) {
		selectIds = sp::move(id);
	} else {
		selectIds = Vector<int64_t>{-1};
	}
	selectAlias.clear();
	selectList.clear();
	return *this;
}

Query & Query::select(const StringView &f, Comparation c, const Value & v1, const Value &v2) {
	selectList.emplace_back(f, c, Value(v1), Value(v2));
	return *this;
}
Query & Query::select(const StringView &f, const Value & v1) {
	selectList.emplace_back(f, Comparation::Equal, Value(v1), Value());
	return *this;
}
Query & Query::select(const StringView &f, Comparation c, int64_t v1) {
	selectList.emplace_back(f, c, Value(v1), Value());
	return *this;
}
Query & Query::select(const StringView &f, Comparation c, int64_t v1, int64_t v2) {
	selectList.emplace_back(f, c, Value(v1), Value(v2));
	return *this;
}
Query & Query::select(const StringView &f, const String & v) {
	selectList.emplace_back(f, Comparation::Equal, Value(v), Value());
	return *this;
}
Query & Query::select(const StringView &f, String && v) {
	selectList.emplace_back(f, Comparation::Equal, Value(sp::move(v)), Value());
	return *this;
}
Query & Query::select(const StringView &f, const Bytes & v) {
	selectList.emplace_back(f, Comparation::Equal, Value(v), Value());
	return *this;
}
Query & Query::select(const StringView &f, Bytes && v) {
	selectList.emplace_back(f, Comparation::Equal, Value(sp::move(v)), Value());
	return *this;
}
Query & Query::select(const StringView &f, FullTextQuery && v) {
	selectList.emplace_back(f, Comparation::Equal, sp::move(v));
	order(f, Ordering::Descending);
	return *this;
}

Query & Query::select(Select &&q) {
	selectList.emplace_back(sp::move(q));
	return *this;
}

Query & Query::order(const StringView &f, Ordering o, size_t l, size_t off) {
	orderField = f.str<Interface>();
	ordering = o;
	if (l != stappler::maxOf<size_t>()) {
		limitValue = l;
	}
	if (off != 0) {
		offsetValue = off;
	}
	return *this;
}

Query & Query::softLimit(const StringView &field, Ordering ord, size_t limit, Value &&val) {
	orderField = field.str<Interface>();
	ordering = ord;
	limitValue = limit;
	softLimitValue = sp::move(val);
	_softLimit = true;
	return *this;
}

Query & Query::first(const StringView &f, size_t limit, size_t offset) {
	orderField = f.str<Interface>();
	ordering = Ordering::Ascending;
	if (limit != stappler::maxOf<size_t>()) {
		limitValue = limit;
	}
	if (offset != 0) {
		offsetValue = offset;
	}
	return *this;
}
Query & Query::last(const StringView &f, size_t limit, size_t offset) {
	orderField = f.str<Interface>();
	ordering = Ordering::Descending;
	if (limit != stappler::maxOf<size_t>()) {
		limitValue = limit;
	}
	if (offset != 0) {
		offsetValue = offset;
	}
	return *this;
}

Query & Query::limit(size_t l, size_t off) {
	limitValue = l;
	offsetValue = off;
	return *this;
}

Query & Query::limit(size_t l) {
	limitValue = l;
	return *this;
}

Query & Query::offset(size_t l) {
	offsetValue = l;
	return *this;
}

Query & Query::delta(uint64_t id) {
	deltaToken = id;
	return *this;
}

Query & Query::delta(const StringView &str) {
	auto b = base64::decode<Interface>(str);
	stappler::BytesViewNetwork r(b);
	switch (r.size()) {
	case 2: deltaToken = r.readUnsigned16(); break;
	case 4: deltaToken = r.readUnsigned32(); break;
	case 8: deltaToken = r.readUnsigned64();  break;
	}
	return *this;
}

Query & Query::include(Field &&f) {
	fieldsInclude.emplace_back(sp::move(f));
	return *this;
}
Query & Query::exclude(Field &&f) {
	fieldsExclude.emplace_back(sp::move(f));
	return *this;
}

Query & Query::depth(uint16_t d) {
	resolveDepth = std::max(d, resolveDepth);
	return *this;
}

Query & Query::forUpdate() {
	update = true;
	return *this;
}

Query & Query::clearFields() {
	fieldsInclude.clear();
	fieldsExclude.clear();
	return *this;
}

bool Query::empty() const {
	return selectList.empty() && selectIds.empty() && selectAlias.empty();
}

StringView Query::getQueryField() const {
	return queryField;
}

int64_t Query::getQueryId() const {
	return queryId;
}

int64_t Query::getSingleSelectId() const {
	return selectIds.size() == 1 ? selectIds.front() : 0;
}

const Vector<int64_t> & Query::getSelectIds() const {
	return selectIds;
}

StringView Query::getSelectAlias() const {
	return selectAlias;
}

const Vector<Query::Select> &Query::getSelectList() const {
	return selectList;
}

const String & Query::getOrderField() const {
	return orderField;
}

Ordering Query::getOrdering() const {
	return ordering;
}

size_t Query::getLimitValue() const {
	return limitValue;
}

size_t Query::getOffsetValue() const {
	return offsetValue;
}

const Value &Query::getSoftLimitValue() const {
	return softLimitValue;
}

bool Query::hasSelectName() const {
	return !selectIds.empty() || !selectAlias.empty() || _selected;
}
bool Query::hasSelectList() const {
	return !selectList.empty();
}

bool Query::hasSelect() const {
	if (getSingleSelectId() || !getSelectIds().empty() || !getSelectAlias().empty() || !getSelectList().empty()) {
		return true;
	}
	return false;
}

bool Query::hasOrder() const {
	return !orderField.empty();
}

bool Query::hasLimit() const {
	return limitValue != stappler::maxOf<size_t>();
}

bool Query::hasOffset() const {
	return offsetValue != 0;
}

bool Query::hasDelta() const {
	return deltaToken > 0;
}
bool Query::hasFields() const {
	return !fieldsExclude.empty() || !fieldsInclude.empty();
}
bool Query::isForUpdate() const {
	return update;
}
bool Query::isSoftLimit() const {
	return _softLimit;
}

uint64_t Query::getDeltaToken() const {
	return deltaToken;
}

uint16_t Query::getResolveDepth() const {
	return resolveDepth;
}

const Query::FieldsVec &Query::getIncludeFields() const {
	return fieldsInclude;
}
const Query::FieldsVec &Query::getExcludeFields() const {
	return fieldsExclude;
}

bool Query_Field_isFlat(const Vector<Query::Field> &l) {
	for (auto &it : l) {
		if (!it.fields.empty()) {
			return false;
		}
	}
	return true;
}

void Query_encodeFields(Value &d, const Vector<Query::Field> &fields) {
	if (Query_Field_isFlat(fields)) {
		for (auto &it : fields) {
			d.addString(it.name);
		}
	} else {
		for (auto &it : fields) {
			if (it.fields.empty()) {
				d.setBool(true, it.name);
			} else{
				Query_encodeFields(d.emplace(it.name), it.fields);
			}
		}
	}
}

Value Query::encode() const {
	Value ret;
	if (selectIds.size() == 1) {
		ret.setInteger(selectIds.front(), "select");
	} else if (!selectIds.empty()) {
		auto &vals = ret.emplace("select");
		vals.setArray(Value::ArrayType());
		vals.asArray().reserve(selectIds.size());
		for (auto &it : selectIds) {
			vals.addInteger(it);
		}
	} else if (!selectAlias.empty()) {
		ret.setString(selectAlias, "select");
	} else if (!selectList.empty()) {
		auto &sel = ret.emplace("select");
		for (const Select &it : selectList) {
			auto &s = sel.emplace();
			s.addString(it.field);
			bool twoArgs = false;
			StringView val;
			std::tie(val, twoArgs) = encodeComparation(it.compare);
			s.addString(val);
			s.addValue(it.value1);
			if (twoArgs) {
				s.addValue(it.value2);
			}
		}
	}

	if (hasOrder()) {
		auto &ord = ret.emplace("order");
		ord.addString(orderField);
		switch (ordering) {
		case Ordering::Ascending: ord.addString("asc"); break;
		case Ordering::Descending: ord.addString("desc"); break;
		}
		if (hasLimit()) {
			ord.addInteger(limitValue);
			if (hasOffset()) {
				ord.addInteger(offsetValue);
			}
		} else if (hasOffset()) {
			ret.setInteger(offsetValue, "offset");
		}
	}

	if (hasDelta()) {
		ret.setInteger(deltaToken, "delta");
	}

	if (hasFields()) {
		if (!fieldsInclude.empty()) {
			Query_encodeFields(ret.emplace("include"), fieldsInclude);
		}
		if (!fieldsExclude.empty()) {
			Query_encodeFields(ret.emplace("exclude"), fieldsExclude);
		}
	}

	if (update) {
		ret.setBool(update, "forUpdate");
	}

	return ret;
}

}
