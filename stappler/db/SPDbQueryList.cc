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

#include "SPDbQueryList.h"
#include "SPDbFile.h"
#include "SPDbScheme.h"
#include "SPValid.h"

namespace STAPPLER_VERSIONIZED stappler::db {

static const Field *getFieldFormMap(const Map<String, Field> &fields, const StringView &name) {
	auto it = fields.find(name);
	if (it != fields.end()) {
		return &it->second;
	}
	return nullptr;
}

static const Query::FieldsVec *getFieldsVec(const Query::FieldsVec *vec, const StringView &name) {
	if (vec) {
		for (auto &it : *vec) {
			if (it.name == name) {
				return it.fields.empty() ? nullptr : &it.fields;
			}
		}
	}
	return nullptr;
}

static void QueryFieldResolver_resolveByName(Set<const Field *> &ret, const Map<String, Field> &fields, const StringView &name) {
	if (!name.empty() && name.front() == '$') {
		auto res = Query::decodeResolve(name);
		switch (res) {
		case Resolve::Files:
			for (auto &it : fields) {
				if (it.second.isFile()) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Sets:
			for (auto &it : fields) {
				if (it.second.getType() == Type::Set) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Objects:
			for (auto &it : fields) {
				if (it.second.getType() == Type::Object) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Arrays:
			for (auto &it : fields) {
				if (it.second.getType() == Type::Array) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Basics:
			for (auto &it : fields) {
				if (it.second.isSimpleLayout() && !it.second.isDataLayout() && !it.second.hasFlag(db::Flags::ForceExclude)) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Defaults:
			for (auto &it : fields) {
				if (it.second.isSimpleLayout() && !it.second.hasFlag(db::Flags::ForceExclude)) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::All:
			for (auto &it : fields) {
				if (!it.second.hasFlag(db::Flags::ForceExclude)) {
					ret.emplace(&it.second);
				}
			}
			break;
		case Resolve::Ids:
			for (auto &it : fields) {
				if (it.second.isFile() || it.second.getType() == Type::Object) {
					ret.emplace(&it.second);
				}
			}
			break;
		default: break;
		}
	} else {
		if (auto field = getFieldFormMap(fields, name)) {
			ret.emplace(field);
		}
	}
}

QueryFieldResolver::QueryFieldResolver() : root(nullptr) { }

QueryFieldResolver::QueryFieldResolver(const ApplicationInterface * app, const Scheme &scheme, const Query &query, const Vector<StringView> &extraFields) {
	root = new Data{&scheme, &scheme.getFields(), &query.getIncludeFields(), &query.getExcludeFields()};
	doResolve(app, root, extraFields, 0, query.getResolveDepth());
}

const Field *QueryFieldResolver::getField(const String &name) const {
	if (root && root->fields) {
		auto it = root->fields->find(name);
		if (it != root->fields->end()) {
			return &it->second;
		}
	}
	return nullptr;
}

const Scheme *QueryFieldResolver::getScheme() const {
	if (root) {
		return root->scheme;
	}
	return nullptr;
}
const Map<String, Field> *QueryFieldResolver::getFields() const {
	if (root) {
		return root->fields;
	}
	return nullptr;
}

QueryFieldResolver::Meta QueryFieldResolver::getMeta() const {
	return root ? root->meta : Meta::None;
}

const Set<const Field *> &QueryFieldResolver::getResolves() const {
	return root->resolved;
}

const Set<StringView> &QueryFieldResolver::getResolvesData() const {
	return root->resolvedData;
}

const Query::FieldsVec *QueryFieldResolver::getIncludeVec() const {
	if (root) {
		return root->include;
	}
	return nullptr;
}

const Query::FieldsVec *QueryFieldResolver::getExcludeVec() const {
	if (root) {
		return root->exclude;
	}
	return nullptr;
}

QueryFieldResolver QueryFieldResolver::next(const StringView &f) const {
	if (root) {
		auto it = root->next.find(f);
		if (it != root->next.end()) {
			return QueryFieldResolver(&it->second);
		}
	}
	return QueryFieldResolver();
}

QueryFieldResolver::operator bool () const {
	return root && root->scheme != nullptr && (root->fields != nullptr || !root->resolvedData.empty());
}

QueryFieldResolver::QueryFieldResolver(Data *data) : root(data) { }

void QueryFieldResolver::doResolve(const ApplicationInterface *app, Data *data, const Vector<StringView> &extra, uint16_t depth, uint16_t max) {
	if (!data->fields) {
		return;
	}

	if (data->include && !data->include->empty()) {
		for (const Query::Field &it : *data->include) {
			if (it.name == "$meta") {
				for (auto &f_it : it.fields) {
					if (f_it.name == "time") {
						data->meta |= Meta::Time;
					} else if (f_it.name == "action") {
						data->meta |= Meta::Action;
					} else if (f_it.name == "view") {
						data->meta |= Meta::View;
					}
				}
			} else {
				QueryFieldResolver_resolveByName(data->resolved, *data->fields, it.name);
			}
		}
	}

	if (!data->include || data->include->empty() || (data->include->size() == 1 && data->include->front().name == "$meta")) {
		for (auto &it : *data->fields) {
			if (it.second.isSimpleLayout()) {
				if (!it.second.hasFlag(Flags::ForceExclude)) {
					data->resolved.emplace(&it.second);
				}
			}
		}
	}

	if (!extra.empty()) {
		for (auto &it : extra) {
			QueryFieldResolver_resolveByName(data->resolved, *data->fields, it);
		}
	}

	if (data->exclude) {
		for (const Query::Field &it : *data->exclude) {
			if (it.fields.empty()) {
				if (auto field = getFieldFormMap(*data->fields, it.name)) {
					data->resolved.erase(field);
				}
			}
		}
	}

	for (const Field *it : data->resolved) {
		const Scheme *scheme = nullptr;
		const Map<String, Field> *fields = nullptr;

		if (auto s = it->getForeignScheme()) {
			scheme = s;
			fields = &s->getFields();
		} else if (it->getType() == Type::Extra) {
			scheme = data->scheme;
			fields = &static_cast<const FieldExtra *>(it->getSlot())->fields;
		} else if (it->getType() == Type::Data || it->getType() == Type::Virtual) {
			scheme = data->scheme;
			if (depth < max) {
				auto n_it = data->next.emplace(it->getName().str<Interface>(), Data{scheme, nullptr, getFieldsVec(data->include, it->getName()), getFieldsVec(data->exclude, it->getName())}).first;
				doResolveData(&n_it->second, depth + 1, max);
			}
		} else if (it->isFile()) {
			scheme = app->getFileScheme();
			fields = &scheme->getFields();
		}
		if (scheme && fields && depth < max) {
			auto n_it = data->next.emplace(it->getName().str<Interface>(), Data{scheme, fields, getFieldsVec(data->include, it->getName()), getFieldsVec(data->exclude, it->getName())}).first;
			doResolve(app, &n_it->second, extra, depth + 1, max);
		}
	}
}

void QueryFieldResolver::doResolveData(Data *data, uint16_t depth, uint16_t max) {
	if (data->include && !data->include->empty()) {
		for (const Query::Field &it : *data->include) {
			data->resolvedData.emplace(StringView(it.name));
		}
	}

	if (data->exclude) {
		for (const Query::Field &it : *data->exclude) {
			data->resolvedData.erase(it.name);
		}
	}

	for (const StringView &it : data->resolvedData) {
		auto scheme = data->scheme;
		if (depth < max) {
			auto n_it = data->next.emplace(it.str<Interface>(), Data{scheme, nullptr, getFieldsVec(data->include, it), getFieldsVec(data->exclude, it)}).first;
			doResolveData(&n_it->second, depth + 1, max);
		}
	}
}

const Set<const Field *> &QueryList::Item::getQueryFields() const {
	return fields.getResolves();
}

/*void QueryList::readFields(const Scheme &scheme, const Set<const Field *> &fields, const FieldCallback &cb, bool isSimpleGet) {
	if (!fields.empty()) {
		cb("__oid", nullptr);
		for (auto &it : fields) {
			if (it != nullptr) {
				auto type = it->getType();
				if (type == Type::Virtual) {
					auto slot = it->getSlot<FieldVirtual>();
					for (auto &it : slot->requires) {
						if (auto f = scheme.getField(it)) {
							if (!f->hasFlag(db::Flags::ForceInclude) && fields.find(f) == fields.end()) {
								cb(f->getName(), f);
							}
						}
					}
				} else if (type != Type::Set && type != Type::Array && type != Type::View) {
					cb(it->getName(), it);
				}
			}
		}
		auto &forced = scheme.getForceInclude();
		for (auto &it : scheme.getFields()) {
			if (it.second.getType() != Type::Virtual) {
				if ((it.second.hasFlag(db::Flags::ForceInclude) || (!isSimpleGet && forced.find(&it.second) != forced.end()))
						&& fields.find(&it.second) == fields.end()) {
					cb(it.first, &it.second);
				}
			}
		}
	} else {
		cb("*", nullptr);
	}
}

void QueryList::Item::readFields(const FieldCallback &cb, bool isSimpleGet) const {
	QueryList::readFields(*scheme, getQueryFields(), cb, isSimpleGet);
}*/

QueryList::QueryList(const ApplicationInterface *app, const Scheme *scheme)
: _application(app) {
	queries.reserve(config::RESOURCE_RESOLVE_MAX_DEPTH);
	queries.emplace_back(Item{scheme});
}

bool QueryList::selectById(const Scheme *scheme, uint64_t id) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getSelectIds().empty() && b.query.getSelectAlias().empty()) {
		b.query.select(id);
		return true;
	}
	return false;
}

bool QueryList::selectByName(const Scheme *scheme, const StringView &f) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getSelectIds().empty() && b.query.getSelectAlias().empty()) {
		b.query.select(f);
		return true;
	}
	return false;
}

bool QueryList::selectByQuery(const Scheme *scheme, Query::Select &&f) {
	Item &b = queries.back();
	if (b.scheme == scheme && (b.query.empty() || !b.query.getSelectList().empty())) {
		b.query.select(sp::move(f));
		return true;
	}
	return false;
}

bool QueryList::order(const Scheme *scheme, const StringView &f, Ordering o) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getOrderField().empty()) {
		b.query.order(f, o);
		return true;
	}

	return false;
}
bool QueryList::first(const Scheme *scheme, const StringView &f, size_t v) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getOrderField().empty() && b.query.getLimitValue() > v && b.query.getOffsetValue() == 0) {
		b.query.order(f, Ordering::Ascending);
		b.query.limit(v, 0);
		return true;
	}

	return false;
}
bool QueryList::last(const Scheme *scheme, const StringView &f, size_t v) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getOrderField().empty() && b.query.getLimitValue() > v && b.query.getOffsetValue() == 0) {
		b.query.order(f, Ordering::Descending);
		b.query.limit(v, 0);
		return true;
	}

	return false;
}
bool QueryList::limit(const Scheme *scheme, size_t limit) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getLimitValue() > limit) {
		b.query.limit(limit);
		return true;
	}

	return false;
}
bool QueryList::offset(const Scheme *scheme, size_t offset) {
	Item &b = queries.back();
	if (b.scheme == scheme && b.query.getOffsetValue() == 0) {
		b.query.offset(offset);
		return true;
	}

	return false;
}

bool QueryList::setFullTextQuery(const Field *field, FullTextQuery &&data) {
	if (queries.size() > 0 && field->getType() == Type::FullTextView) {
		Item &b = queries.back();
		b.query.select(field->getName(), sp::move(data));
		b.field = field;
		return true;
	}
	return false;
}

bool QueryList::setAll() {
	Item &b = queries.back();
	if (!b.all) {
		b.all = true;
		return true;
	}
	return false;
}

bool QueryList::setField(const Scheme *scheme, const Field *field) {
	if (queries.size() < config::RESOURCE_RESOLVE_MAX_DEPTH) {
		Item &prev = queries.back();
		prev.field = field;
		queries.emplace_back(Item{scheme, prev.scheme->getForeignLink(*prev.field)});
		return true;
	}
	return false;
}

bool QueryList::setProperty(const Field *field) {
	queries.back().query.include(Query::Field(field->getName().str<Interface>()));
	return true;
}

StringView QueryList::setQueryAsMtime() {
	if (auto scheme = getScheme()) {
		for (auto &it : scheme->getFields()) {
			if (it.second.hasFlag(db::Flags::AutoMTime)) {
				auto &q = queries.back();
				q.query.clearFields().include(it.first);
				q.fields = QueryFieldResolver(_application, *q.scheme, q.query, Vector<StringView>());
				return it.first;
			}
		}
	}
	return StringView();
}

void QueryList::clearFlags() {
	_flags = None;
}

void QueryList::addFlag(Flags flags) {
	_flags |= flags;
}

bool QueryList::hasFlag(Flags flags) const {
	return (_flags & flags) != Flags::None;
}

bool QueryList::isAll() const {
	return queries.back().all;
}

bool QueryList::isRefSet() const {
	return (queries.size() > 1 && !queries.back().ref && !queries.back().all);
}

bool QueryList::isObject() const {
	const Query &b = queries.back().query;
	return b.getSelectIds().size() == 1 || !b.getSelectAlias().empty() || b.getLimitValue() == 1;
}
bool QueryList::isView() const {
	if (queries.size() > 1 && queries.at(queries.size() - 2).field) {
		return queries.at(queries.size() - 2).field->getType() == Type::View;
	} else if (!queries.empty() && queries.back().field) {
		return queries.back().field->getType() == Type::View;
	}
	return false;
}
bool QueryList::empty() const {
	return queries.size() == 1 && queries.front().query.empty();
}

bool QueryList::isDeltaApplicable() const {
	const QueryList::Item &item = getItems().back();
	if ((queries.size() == 1 || (isView() && queries.size() == 2
			&& (queries.front().query.getSelectIds().size() == 1 || queries.front().query.getLimitValue() == 1)))
			&& !item.query.hasSelectName() && !item.query.hasSelectList()) {
		return true;
	}
	return false;
}

size_t QueryList::size() const {
	return queries.size();
}

const Scheme *QueryList::getPrimaryScheme() const {
	return queries.front().scheme;
}
const Scheme *QueryList::getSourceScheme() const {
	if (queries.size() >= 2) {
		return queries.at(queries.size() - 2).scheme;
	}
	return getPrimaryScheme();
}
const Scheme *QueryList::getScheme() const {
	return queries.back().scheme;
}

const Field *QueryList::getField() const {
	if (queries.size() >= 2) {
		return queries.at(queries.size() - 2).field;
	}
	return nullptr;
}

const Query &QueryList::getTopQuery() const {
	return queries.back().query;
}

const Vector<QueryList::Item> &QueryList::getItems() const {
	return queries;
}

void QueryList::decodeSelect(const Scheme &scheme, Query &q, const Value &val) {
	if (val.isInteger()) {
		q.select(val.asInteger());
	} else if (val.isString()) {
		q.select(val.getString());
	} else if (val.isArray() && val.size() > 0) {
		auto cb = [&, this] (const Value &iit) {
			if (iit.isArray() && iit.size() >= 3) {
				auto field = iit.getValue(0).asString();
				if (auto f = scheme.getField(field)) {
					auto cmp = iit.getValue(1).asString();
					auto d = decodeComparation(cmp);
					if (f->isIndexed() || d.first == Comparation::IsNotNull || d.first == Comparation::IsNull) {
						auto &val = iit.getValue(2);
						if (d.second && iit.size() >= 4) {
							auto &val2 = iit.getValue(4);
							q.select(field, d.first, Value(val), val2);
						} else {
							q.select(field, d.first, Value(val), Value());
						}
					} else {
						_application->error("QueryList", "Invalid field for select", Value(field));
					}
				} else {
					_application->error("QueryList", "Invalid field for select", Value(field));
				}
			}
		};

		if (val.getValue(0).isString()) {
			cb(val);
		} else if (val.getValue(0).isArray()) {
			for (auto &iit : val.asArray()) {
				cb(iit);
			}
		}
	}
}

void QueryList::decodeOrder(const Scheme &scheme, Query &q, const String &str, const Value &val) {
	String field;
	Ordering ord = Ordering::Ascending;
	size_t limit = stappler::maxOf<size_t>();
	size_t offset = 0;
	if (val.isArray() && val.size() > 0) {
		size_t target = 1;
		auto size = val.size();
		field = val.getValue(0).asString();
		if (str == "order") {
			if (size > target) {
				auto dir = val.getValue(target).asString();
				if (dir == "desc") {
					ord = Ordering::Descending;
				}
				++ target;
			}
		} else if (str == "last") {
			ord = Ordering::Descending;
			limit = 1;
		} else if (str == "first") {
			ord = Ordering::Ascending;
			limit = 1;
		}

		if (size > target) {
			limit = val.getInteger(target);
			++ target;
			if (size > target) {
				offset = val.getInteger(target);
				++ target;
			}
		}
	} else if (val.isString()) {
		field = val.asString();
		if (str == "last") {
			ord = Ordering::Descending;
			limit = 1;
		} else if (str == "first") {
			ord = Ordering::Ascending;
			limit = 1;
		}
	}
	if (!field.empty()) {
		if (auto f = scheme.getField(field)) {
			if (f->isIndexed()) {
				q.order(field, ord);
				if (limit != stappler::maxOf<size_t>() && !q.hasLimit()) {
					q.limit(limit);
				}
				if (offset != 0 && !q.hasOffset()) {
					q.offset(offset);
				}
				return;
			}
		}
	}
	_application->error("QueryList", "Invalid field for ordering", Value(field));
}

const Field *QueryList_getField(const Scheme &scheme, const Field *f, const String &name) {
	if (!f) {
		return scheme.getField(name);
	} else if (f->getType() == Type::Extra) {
		auto slot = static_cast<const FieldExtra *>(f->getSlot());
		auto it = slot->fields.find(name);
		if (it != slot->fields.end()) {
			return &it->second;
		}
	}
	return nullptr;
}

static uint16_t QueryList_emplaceItem(const ApplicationInterface *app, const Scheme &scheme, const Field *f, Vector<Query::Field> &dec, const String &name) {
	if (!name.empty() && name.front() == '$') {
		dec.emplace_back(String(name));
		return 1;
	}
	if (!f) {
		if (auto field = scheme.getField(name)) {
			dec.emplace_back(String(name));
			return (field->isFile() || field->getForeignScheme()) ? 1 : 0;
		}
	} else if (f->getType() == Type::Extra) {
		auto slot = static_cast<const FieldExtra *>(f->getSlot());
		if (slot->fields.find(name) != slot->fields.end()) {
			dec.emplace_back(String(name));
			return 0;
		}
	} else if (f->getType() == Type::Data || f->getType() == Type::Virtual) {
		dec.emplace_back(String(name));
		return 0;
	}
	if (!f) {
		app->error("QueryList", toString("Invalid field name in 'include' for scheme ", scheme.getName()), Value(name));
	} else {
		app->error("QueryList",
				toString("Invalid field name in 'include' for scheme ", scheme.getName(), " and field ", f->getName()), Value(name));
	}
	return 0;
}

static uint16_t QueryList_decodeIncludeItem(const ApplicationInterface *app, const Scheme &scheme, const Field *f, Vector<Query::Field> &dec, const Value &val) {
	uint16_t depth = 0;
	if (val.isString()) {
		return QueryList_emplaceItem(app, scheme, f, dec, val.getString());
	} else if (val.isArray()) {
		for (auto &iit : val.asArray()) {
			if (iit.isString()) {
				depth = std::max(depth, QueryList_emplaceItem(app, scheme, f, dec, iit.getString()));
			}
		}
	}
	return depth;
}

static void QueryList_decodeMeta(Vector<Query::Field> &dec, const Value &val) {
	if (val.isArray()) {
		for (auto &it : val.asArray()) {
			auto str = it.asString();
			if (!str.empty()) {
				dec.emplace_back(sp::move(str));
			}
		}
	} else if (val.isDictionary()) {
		for (auto &it : val.asDict()) {
			dec.emplace_back(String(it.first));
			QueryList_decodeMeta(dec.back().fields, it.second);
		}
	} else if (val.isString()) {
		dec.emplace_back(val.asString());
	}
}

static uint16_t QueryList_decodeInclude(const ApplicationInterface *app, const Scheme &scheme, const Field *f, Vector<Query::Field> &dec, const Value &val) {
	uint16_t depth = 0;
	if (val.isDictionary()) {
		for (auto &it : val.asDict()) {
			if (!it.first.empty()) {
				if (it.second.isBool() && it.second.asBool()) {
					QueryList_emplaceItem(app, scheme, f, dec, it.first);
				} else if (it.second.isArray() || it.second.isDictionary() || it.second.isString()) {
					if (it.first.front() == '$') {
						dec.emplace_back(String(it.first));
						QueryList_decodeMeta(dec.back().fields, it.second);
					} else if (auto target = QueryList_getField(scheme, f, it.first)) {
						if (auto ts = target->getForeignScheme()) {
							dec.emplace_back(String(it.first));
							depth = std::max(depth, QueryList_decodeInclude(app, *ts, nullptr, dec.back().fields, it.second));
						} else if (target->isFile()) {
							dec.emplace_back(String(it.first));
							depth = std::max(depth, QueryList_decodeInclude(app, *app->getFileScheme(), nullptr, dec.back().fields, it.second));
						} else {
							dec.emplace_back(String(it.first));
							depth = std::max(depth, QueryList_decodeInclude(app, scheme, target, dec.back().fields, it.second));
						}
					}
				}
			}
		}
		depth = (depth + 1);
	} else {
		depth = std::max(depth, QueryList_decodeIncludeItem(app, scheme, f, dec, val));
	}
	return depth;
}

static Ordering QueryList_getTokenOrdering(const Value &v) {
	return ((v.isInteger() && v.getInteger() == 1) || (v.isString() && v.getString() == "desc")) ? Ordering::Descending : Ordering::Ascending;
}

static ContinueToken QueryList_decodeToken(const ApplicationInterface *app, const Scheme &scheme, const Value &val) {
	StringView fStr = StringView("__oid");
	int64_t count = QueryList::DefaultSoftLimit;
	Ordering ord = Ordering::Ascending;

	if (val.isArray() && val.size() == 3) {
		fStr = StringView(val.getString(0));
		count = val.getInteger(1);
		ord = QueryList_getTokenOrdering(val.getValue(2));
	} else if (val.isArray() && val.size() == 2) {
		if (val.getValue(0).isString()) {
			fStr = StringView(val.getValue(0).getString());
			count = val.getInteger(1);
		} else if (val.getValue(0).isInteger()) {
			count = val.getInteger(0);
			ord = QueryList_getTokenOrdering(val.getValue(1));
		}
	} else {
		auto &v = (val.isArray() && val.size() == 1) ? val.getValue(0) : val;
		if (v.isInteger()) {
			count = v.getInteger();
		} else if (v.isString()) {
			auto &vStr = v.getString();
			if (stappler::valid::validateNumber(vStr)) {
				count = v.getInteger();
			} else if (vStr == "asc") {
				ord = Ordering::Ascending;
			} else if (vStr == "desc") {
				ord = Ordering::Descending;
			} else if (scheme.getField(vStr)) {
				fStr = vStr;
			} else {
				app->error("QueryList", "Invalid token field", Value(val));
			}
		}
	}

	count = stappler::math::clamp(count, QueryList::MinSoftLimit, QueryList::MaxSoftLimit);
	if (scheme.getField(fStr) == nullptr) {
		app->error("QueryList", "Invalid token field", Value(val));
	}

	return ContinueToken(fStr, count, ord == Ordering::Descending);
}

bool QueryList::apply(const Value &val) {
	Item &item = queries.back();
	Query &q = item.query;
	const Scheme &scheme = *item.scheme;

	for (auto &it : val.asDict()) {
		if (it.first == "select") {
			decodeSelect(scheme, q, it.second);
		} else if (it.first == "order" || it.first == "last" || it.first == "first") {
			decodeOrder(scheme, q, it.first, it.second);
		} else if (it.first == "limit") {
			if (it.second.isInteger()) {
				q.limit(it.second.asInteger());
			}
		} else if (it.first == "offset") {
			if (it.second.isInteger()) {
				q.offset(it.second.asInteger());
			}
		} else if (it.first == "fields") {
			Vector<Query::Field> dec;
			q.depth(std::min(QueryList_decodeInclude(_application, scheme, nullptr, dec, it.second), config::RESOURCE_RESOLVE_MAX_DEPTH));
			for (auto &it : dec) {
				q.include(sp::move(it));
			}
		} else if (it.first == "include") {
			Vector<Query::Field> dec;
			q.depth(std::min(QueryList_decodeInclude(_application, scheme, nullptr, dec, it.second), config::RESOURCE_RESOLVE_MAX_DEPTH));
			for (auto &it : dec) {
				q.include(sp::move(it));
			}
		} else if (it.first == "exclude") {
			Vector<Query::Field> dec;
			q.depth(std::min(QueryList_decodeInclude(_application, scheme, nullptr, dec, it.second), config::RESOURCE_RESOLVE_MAX_DEPTH));
			for (auto &it : dec) {
				q.exclude(sp::move(it));
			}
		} else if (it.first == "delta") {
			if (it.second.isString()) {
				q.delta(it.second.asString());
			} else if (it.second.isInteger()) {
				q.delta(it.second.asInteger());
			}
		} else if (it.first == "forUpdate") {
			q.forUpdate();
		} else if (it.first == "continue") {
			if (auto t = ContinueToken(it.second.asString())) {
				token = t;
			} else {
				_application->error("QueryList", "Invalid token", Value(it.second));
				failed = true;
			}
		} else if (it.first == "begin") {
			if (auto t = QueryList_decodeToken(_application, scheme, it.second)) {
				token = t;
			} else {
				failed = true;
			}
		} else {
			extraData.setValue(it.second, it.first);
		}
	}

	return true;
}

void QueryList::resolve(const Vector<StringView> &vec) {
	Item &b = queries.back();
	b.fields = QueryFieldResolver(_application, *b.scheme, b.query, vec);
}

uint16_t QueryList::getResolveDepth() const {
	return queries.back().query.getResolveDepth();
}

void QueryList::setResolveDepth(uint16_t d) {
	queries.back().query.depth(d);
}

void QueryList::setDelta(stappler::Time d) {
	queries.back().query.delta(d.toMicroseconds());
}

stappler::Time QueryList::getDelta() const {
	return stappler::Time::microseconds(queries.back().query.getDeltaToken());
}

const Query::FieldsVec &QueryList::getIncludeFields() const {
	return queries.back().query.getIncludeFields();
}
const Query::FieldsVec &QueryList::getExcludeFields() const {
	return queries.back().query.getExcludeFields();
}

QueryFieldResolver QueryList::getFields() const {
	return QueryFieldResolver(queries.back().fields);
}

const Value &QueryList::getExtraData() const {
	return extraData;
}

ContinueToken &QueryList::getContinueToken() const {
	return token;
}

}
