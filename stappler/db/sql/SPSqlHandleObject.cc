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

#include "SPSqlHandle.h"
#include "SPSqlDriver.h"
#include "SPDbScheme.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

template <typename Clause>
static void SqlQuery_makeCustomFrom(const Driver *driver, SqlQuery &q, Clause &tmp, const Query &query, const Scheme &scheme);

static bool Handle_hasPostUpdate(const SpanView<InputField> &idata, const SpanView<InputRow> &inputRows) {
	size_t i = 0;
	for (auto &it : idata) {
		auto t = it.field->getType();
		switch (t) {
		case db::Type::Array:
		case db::Type::Set:
			for (auto &row : inputRows) {
				if (row.values[i].hasValue()) {
					return true;
				}
			}
			return true;
			break;
		case db::Type::Object:
			for (auto &row : inputRows) {
				if (row.values[i].hasValue() && !row.values[i].value.isBasicType()) {
					return true;
				}
			}
			break;
		default:
			break;
		}
		if (t == db::Type::Array || t == db::Type::Set || t == db::Type::Object) {
			return true;
		}
		++ i;
	}
	return false;
}

static Value Handle_preparePostUpdate(const Vector<InputField> &inputFields, InputRow &row) {
	Value postUpdate(Value::Type::DICTIONARY);
	size_t i = 0;
	for (auto &field : inputFields) {
		switch (field.field->getType()) {
		case db::Type::Array:
		case db::Type::Set:
			if (row.values[i].hasValue()) {
				postUpdate.setValue(sp::move(row.values[i].value), field.field->getName());
			}
			break;
		case db::Type::Object:
			if (row.values[i].hasValue() && !row.values[i].value.isBasicType()) {
				postUpdate.setValue(sp::move(row.values[i].value), field.field->getName());
			}
			break;
		default:
			break;
		}
		++ i;
	}

	return postUpdate;
}

bool SqlHandle::foreach(Worker &worker, const Query &q, const Callback<bool(Value &)> &cb) {
	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());
	bool ret = false;
	auto &scheme = worker.scheme();
	makeQuery([&, this] (SqlQuery &query) {
		auto ordField = q.getQueryField();
		if (ordField.empty()) {
			SqlQuery::Context ctx(query, scheme, worker, q);
			query.writeQuery(ctx);
			ret = selectQuery(query, [&] (Result &res) -> bool {
				auto virtuals = ctx.getVirtuals();
				for (auto it : res) {
					auto d = it.toData(scheme, Map<String, db::Field>(), virtuals);
					if (!cb(d)) {
						return false;
					}
				}
				return true;
			});
		} else if (auto f = scheme.getField(ordField)) {
			switch (f->getType()) {
			case Type::Set: {
				SqlQuery::Context ctx(query, *f->getForeignScheme(), worker, q);
				if (query.writeQuery(ctx, scheme, q.getQueryId(), *f)) {
					ret = selectQuery(query, [&] (Result &res) -> bool {
						auto virtuals = ctx.getVirtuals();
						for (auto it : res) {
							auto d = it.toData(*f->getForeignScheme(), Map<String, db::Field>(), virtuals);
							if (!cb(d)) {
								return false;
							}
						}
						return true;
					});
				}
				break;
			}
			default:
				break;
			}
		}
	}, &queryStorage);
	return ret;
}

Value SqlHandle::select(Worker &worker, const db::Query &q) {
	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());

	Value ret;
	auto &scheme = worker.scheme();
	makeQuery([&, this] (SqlQuery &query) {
		auto ordField = q.getQueryField();
		if (ordField.empty()) {
			SqlQuery::Context ctx(query, scheme, worker, q);
			query.writeQuery(ctx);
			ret = selectValueQuery(scheme, query, ctx.getVirtuals());
		} else if (auto f = scheme.getField(ordField)) {
			switch (f->getType()) {
			case Type::Set:
				ret = getSetField(worker, query, q.getQueryId(), *f, q);
				break;
			case Type::View:
				ret = getViewField(worker, query, q.getQueryId(), *f, q);
				break;
			default:
				break;
			}
		}
	}, &queryStorage);
	return ret;
}

Value SqlHandle::create(Worker &worker, const Vector<InputField> &inputFields, Vector<InputRow> &inputRows, bool multiCreate) {
	if (inputRows.empty() || inputFields.empty()) {
		return Value();
	}

	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());

	auto &scheme = worker.scheme();

	auto bindRow = [&] (Value &ret, stappler::sql::Query<Binder,Interface>::InsertValues &val, InputRow &input) {
		for (size_t idx = 0; idx < inputFields.size(); ++ idx) {
			auto f = inputFields[idx].field;
			switch (f->getType()) {
			case Type::Set:
			case Type::Array:
			case Type::Virtual:
				break;
			default:
				switch (input.values[idx].type) {
				case InputValue::Type::Value: {
					auto &v = ret.setValue(input.values[idx].value, f->getName());
					val.value(db::Binder::DataField{f, v, f->isDataLayout(), f->hasFlag(db::Flags::Compressed)});
					break;
				}
				case InputValue::Type::File:
				case InputValue::Type::None:
					val.def();
					break;
				case InputValue::Type::TSV:
					val.value(db::Binder::FullTextField{f, input.values[idx].tsv});
					break;
				}
				break;
			}
		}
	};

	auto perform = [&, this] (InputRow &row) {
		int64_t id = 0;
		Value ret;
		Value postUpdate(Handle_preparePostUpdate(inputFields, row));

		makeQuery([&, this] (SqlQuery &query) {
			auto ins = query.insert(scheme.getName());
			for (auto &it : inputFields) {
				switch (it.field->getType()) {
				case Type::Set:
				case Type::Array:
				case Type::Virtual:
					break;
				default:
					ins.field(it.field->getName());
					break;
				}
			}

			auto val = ins.values();
			bindRow(ret, val, row);

			auto &conflicts = worker.getConflicts();
			for (auto &it : conflicts) {
				if (it.second.isDoNothing()) {
					val.onConflict(it.first->getName()).doNothing();
				} else {
					auto c = val.onConflict(it.first->getName()).doUpdate();
					for (auto &iit : ret.asDict()) {
						auto f = scheme.getField(iit.first);
						if (f && (it.second.mask.empty() || std::find(it.second.mask.begin(), it.second.mask.end(), f) != it.second.mask.end())) {
							c.excluded(iit.first);
						}
					}

					if (it.second.hasCondition()) {
						c.where().parenthesis(db::Operator::And, [&] (SqlQuery::WhereBegin &wh) {
							SqlQuery::WhereContinue iw(wh.query, wh.state);
							query.writeWhereCond(iw, db::Operator::And, worker.scheme(), it.second.condition);
						});
					}
				}
			}

			if (id == 0) {
				val.returning().field(SqlQuery::Field("__oid").as("id")).finalize();
				id = selectQueryId(query);
				if (id) {
					if (worker.shouldIncludeNone() && worker.scheme().hasForceExclude()) {
						for (auto &it : worker.scheme().getFields()) {
							if (it.second.hasFlag(db::Flags::ForceExclude)) {
								ret.erase(it.second.getName());
							}
						}
					}
					ret.setInteger(id, "__oid");
				} else {
					ret = Value();
					return;
				}
			} else {
				val.finalize();
				if (performQuery(query) != 1) {
					ret = Value();
					return;
				}
			}

			if (id > 0) {
				performPostUpdate(worker.transaction(), query, scheme, ret, id, postUpdate, false);
			}
		}, &queryStorage);
		queryStorage.clear();

		return ret;
	};

	if (Handle_hasPostUpdate(inputFields, inputRows)) {
		// process one-by-one
		Value ret;
		for (auto &it : inputRows) {
			if (!multiCreate) {
				ret = perform(it);
			} else {
				ret.addValue(perform(it));
			}
			return ret;
		}
	} else {
		if (!multiCreate) {
			return perform(inputRows.front());
		}

		Value ret;
		makeQuery([&, this] (SqlQuery &query) {
			auto ins = query.insert(scheme.getName());
			for (auto &it : inputFields) {
				ins.field(it.field->getName());
			}

			auto val = ins.values();
			for (auto &row : inputRows) {
				auto &r = ret.emplace();
				bindRow(r, val, row);
				val = val.next();
			}

			auto &conflicts = worker.getConflicts();
			for (auto &it : conflicts) {
				if (it.second.isDoNothing()) {
					val.onConflict(it.first->getName()).doNothing();
				} else {
					auto c = val.onConflict(it.first->getName()).doUpdate();
					for (auto &iit : inputFields) {
						if ((it.second.mask.empty() || std::find(it.second.mask.begin(), it.second.mask.end(), iit.field) != it.second.mask.end())) {
							c.excluded(iit.field->getName());
						}
					}

					if (it.second.hasCondition()) {
						c.where().parenthesis(db::Operator::And, [&] (SqlQuery::WhereBegin &wh) {
							SqlQuery::WhereContinue iw(wh.query, wh.state);
							query.writeWhereCond(iw, db::Operator::And, worker.scheme(), it.second.condition);
						});
					}
				}
			}

			val.returning().field(SqlQuery::Field("__oid").as("id")).finalize();
			selectQuery(query, [&] (Result &res) {
				size_t i = 0;
				for (auto it : res) {
					ret.getValue(i).setInteger(it.toInteger(0), "__oid");
					++ i;
				}

				for (auto &iit : ret.asArray()) {
					if (worker.shouldIncludeNone() && worker.scheme().hasForceExclude()) {
						for (auto &it : worker.scheme().getFields()) {
							if (it.second.hasFlag(db::Flags::ForceExclude)) {
								iit.erase(it.second.getName());
							}
						}
					}
				}
				return true;
			});
		}, &queryStorage);

		return ret;
	}
	return Value();
}

Value SqlHandle::save(Worker &worker, uint64_t oid, const Value &data, const Vector<InputField> &inputFields, InputRow &inputRow) {
	if ((!data.isDictionary() && !data.empty()) || inputFields.empty() || inputRow.values.empty()) {
		return Value();
	}

	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());

	Value ret(data);
	auto &scheme = worker.scheme();

	Value postUpdate(Handle_preparePostUpdate(inputFields, inputRow));

	makeQuery([&, this] (SqlQuery &query) {
		auto upd = query.update(scheme.getName());

		for (size_t idx = 0; idx < inputFields.size(); ++ idx) {
			auto &f = inputFields[idx];
			auto &v = inputRow.values[idx];

			switch (f.field->getType()) {
			case Type::View:
			case Type::Set:
			case Type::Array:
			case Type::Virtual:
				break;
			case Type::Object:
				if (v.hasValue() && v.value.isDictionary() && v.value.isInteger("__oid")) {
					upd.set(f.field->getName(), v.value.getInteger("__oid"));
				} else if (v.value.isInteger()) {
					upd.set(f.field->getName(), v.value.getInteger());
				}
				break;
			default:
				switch (v.type) {
				case InputValue::Type::Value: {
					ret.setValue(v.value, f.field->getName());
					upd.set(f.field->getName(), db::Binder::DataField{f.field, v.value, f.field->isDataLayout(), f.field->hasFlag(db::Flags::Compressed)});
					break;
				}
				case InputValue::Type::TSV:
					upd.set(f.field->getName(), db::Binder::FullTextField{f.field, v.tsv});
					break;
				case InputValue::Type::File:
				case InputValue::Type::None:
					break;
				}
				break;
			}
		}

		auto q = upd.where("__oid", Comparation::Equal, oid);
		auto &cond = worker.getConditions();
		if (!cond.empty()) {
			q.parenthesis(db::Operator::And, [&] (SqlQuery::WhereBegin &wh) {
				SqlQuery::WhereContinue iw(wh.query, wh.state);
				for (auto &it : cond) {
					query.writeWhereCond(iw, db::Operator::And, worker.scheme(), it);
				}
			});
		}

		FieldResolver resv(worker.scheme(), worker);
		if (!worker.shouldIncludeNone()) {
			auto returning = q.returning();
			for (auto &it : data.asDict()) {
				resv.include(it.first);
			}
			resv.readFields([&] (const StringView &name, const Field *) {
				returning.field(name);
			});
			q.finalize();
		} else {
			q.returning().field("__oid").finalize();
		}

		auto retVal = selectValueQuery(worker.scheme(), query, resv.getVirtuals());
		if (retVal.isArray() && retVal.size() == 1) {
			Value obj = sp::move(retVal.getValue(0));
			int64_t id = obj.getInteger("__oid");
			if (id > 0) {
				performPostUpdate(worker.transaction(), query, scheme, obj, id, postUpdate, false);
			}
			ret = sp::move(obj);
		} else if (!cond.empty() && isSuccess()) {
			ret = Value({ stappler::pair("__oid", Value(oid)) });
		} else {
			_driver->getApplicationInterface()->debug("Storage", "Fail to update object", Value({
				std::make_pair("id", Value(oid)),
				std::make_pair("query", Value(query.getStream().weak())),
				std::make_pair("data", Value(data)),
				std::make_pair("ret", Value(ret)),
			}));
		}
	}, &queryStorage);
	return ret;
}

bool SqlHandle::remove(Worker &worker, uint64_t oid) {
	auto &scheme = worker.scheme();

	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());

	bool ret = false;
	makeQuery([&, this] (SqlQuery &query) {
		auto q = query.remove(scheme.getName())
				.where("__oid", Comparation::Equal, oid);
		q.finalize();
		if (performQuery(query) == 1) { // one row affected
			ret = true;
		}
	}, &queryStorage);
	return ret;
}

size_t SqlHandle::count(Worker &worker, const db::Query &q) {
	auto &scheme = worker.scheme();

	auto queryStorage = _driver->makeQueryStorage(worker.scheme().getName());

	size_t ret = 0;
	makeQuery([&, this] (SqlQuery &query) {
		auto ordField = q.getQueryField();
		if (ordField.empty()) {
			auto f = query.select().count().from(scheme.getName());

			SqlQuery_makeCustomFrom(_driver, query, f, q, scheme);

			if (!q.empty()) {
				auto w = f.where();
				query.writeWhere(w, Operator::And, scheme, q);
			}

			query.finalize();
			selectQuery(query, [&] (Result &res) {
				if (!res.empty()) {
					ret = res.current().toInteger(0);
					return true;
				}
				return false;
			});
		} else if (auto f = scheme.getField(ordField)) {
			switch (f->getType()) {
			case Type::Set:
				ret = getSetCount(worker, query, q.getQueryId(), *f, q);
				break;
			case Type::View:
				ret = getViewCount(worker, query, q.getQueryId(), *f, q);
				break;
			default:
				break;
			}
		}
	}, &queryStorage);
	return ret;
}

void SqlHandle::performPostUpdate(const db::Transaction &t, SqlQuery &query, const Scheme &s, Value &data, int64_t id, const Value &upd, bool clear) {
	query.clear();

	auto makeObject = [&] (const Field &field, const Value &obj) {
		int64_t targetId = 0;
		if (obj.isDictionary()) {
			Value val(sp::move(obj));
			if (auto scheme = field.getForeignScheme()) {
				if (auto link = s.getForeignLink(field)) {
					val.setInteger(id, link->getName().str<Interface>());
				}
				val = Worker(*scheme, t).create(val);
				if (val.isInteger("__oid")) {
					targetId = val.getInteger("__oid");
				}
			}
		}

		if (targetId) {
			Worker w(s, t);
			w.includeNone();
			Value patch{ stappler::pair(field.getName().str<Interface>(), Value(targetId)) };
			t.patch(w, id, patch);
			data.setInteger(targetId, field.getName().str<Interface>());
		}
	};

	auto makeSet = [&, this] (const Field &field, const Value &obj) {
		auto f = field.getSlot<db::FieldObject>();
		auto scheme = field.getForeignScheme();

		if (f && scheme && obj.isArray()) {
			Value ret;
			Vector<int64_t> toAdd;

			if (clear && obj) {
				Worker(s, t).clearField(id, field);
			}

			for (auto &arr_it : obj.asArray()) {
				if (arr_it.isDictionary()) {
					Value val(sp::move(arr_it));
					if (auto link = s.getForeignLink(field)) {
						val.setInteger(id, link->getName().str<Interface>());
					}
					val = Worker(*scheme, t).create(val);
					if (val) {
						ret.addValue(sp::move(val));
					}
				} else {
					if (auto tmp = arr_it.asInteger()) {
						if (field.isReference()) {
							toAdd.emplace_back(tmp);
						} else if (auto link = s.getForeignLink(field)) {
							if (auto val = Worker(*scheme, t).update(tmp, Value{stappler::pair(link->getName().str<Interface>(), Value(id))})) {
								ret.addValue(sp::move(val));
							}
						}
					}
				}
			}

			if (!toAdd.empty()) {
				if (field.isReference()) {
					query.clear();
					if (insertIntoRefSet(query, s, id, field, toAdd)) {
						for (auto &add_it : toAdd) {
							ret.addInteger(add_it);
						}
					}
				}
			}
			data.setValue(sp::move(ret), field.getName().str<Interface>());
		}
	};

	const Map<String, Field> &fields = s.getFields();
	for (auto &it : upd.asDict()) {
		auto f_it = fields.find(it.first);
		if (f_it != fields.end()) {
			if (f_it->second.getType() == db::Type::Object) {
				makeObject(f_it->second, it.second);
			} else if (f_it->second.getType() == db::Type::Set) {
				makeSet(f_it->second, it.second);
			} else if (f_it->second.getType() == db::Type::Array) {
				if (clear && it.second) {
					Worker(s, t).clearField(id, f_it->second);
				}
				query.clear();
				auto tmp = it.second;
				if (insertIntoArray(query, s, id, f_it->second, tmp)) {
					data.setValue(tmp, f_it->second.getName());
				}
			}
		}
	}
}

Vector<int64_t> SqlHandle::performQueryListForIds(const QueryList &list, size_t count) {
	Vector<int64_t> ret;

	auto queryStorage = _driver->makeQueryStorage(list.getScheme()->getName());

	makeQuery([&, this] (SqlQuery &query) {
		query.writeQueryList(list, true, count);
		query.finalize();

		selectQuery(query, [&] (Result &res) {
			for (auto it : res) {
				ret.push_back(it.toInteger(0));
				return true;
			}
			return false;
		});
	}, &queryStorage);

	return ret;
}

Value SqlHandle::performQueryList(const QueryList &list, size_t count, bool forUpdate) {
	Value ret;

	auto queryStorage = _driver->makeQueryStorage(list.getScheme()->getName());

	makeQuery([&, this] (SqlQuery &query) {
		FieldResolver resv(*list.getScheme(), list.getTopQuery());
		query.writeQueryList(list, false, count);
		if (forUpdate) {
			query << "FOR UPDATE";
		}
		query.finalize();

		ret = selectValueQuery(*list.getScheme(), query, resv.getVirtuals());
	}, &queryStorage);
	return ret;
}

bool SqlHandle::removeFromView(const db::FieldView &view, const Scheme *scheme, uint64_t oid) {
	bool ret = false;
	if (scheme) {
		String name = toString(scheme->getName(), "_f_", view.name, "_view");

		auto queryStorage = _driver->makeQueryStorage(view.owner->getName());

		makeQuery([&, this] (SqlQuery &query) {
			query << "DELETE FROM " << name << " WHERE \"" << view.scheme->getName() << "_id\"=" << oid << ";";
			ret = performQuery(query) != stappler::maxOf<size_t>();
		}, &queryStorage);
	}
	return ret;
}

bool SqlHandle::addToView(const db::FieldView &view, const Scheme *scheme, uint64_t tag, const Value &data) {
	bool ret = false;
	if (scheme) {
		String name = toString(scheme->getName(), "_f_", view.name, "_view");

		auto queryStorage = _driver->makeQueryStorage(view.owner->getName());

		makeQuery([&, this] (SqlQuery &query) {
			auto ins = query.insert(name);
			for (auto &it : data.asDict()) {
				ins.field(it.first);
			}

			auto val = ins.values();
			for (auto &it : data.asDict()) {
				val.value(db::Binder::DataField{nullptr, it.second, false});
			}

			val.finalize();
			ret = performQuery(query) != stappler::maxOf<size_t>();
		}, &queryStorage);
	}
	return ret;
}

Vector<int64_t> SqlHandle::getReferenceParents(const Scheme &objectScheme, uint64_t oid, const Scheme *parentScheme, const Field *parentField) {
	Vector<int64_t> vec;
	if (parentField->isReference() && parentField->getType() == db::Type::Set) {
		auto schemeName = toString(parentScheme->getName(), "_f_", parentField->getName());
		auto queryStorage = _driver->makeQueryStorage(schemeName);
		makeQuery([&, this] (SqlQuery &q) {
			q.select(toString(parentScheme->getName(), "_id"))
				.from(schemeName)
				.where(toString(objectScheme.getName(), "_id"), Comparation::Equal, oid);

			selectQuery(q, [&] (Result &res) {
				vec.reserve(res.getRowsHint());
				for (auto it : res) {
					if (auto id = it.toInteger(0)) {
						vec.emplace_back(id);
					}
				}
				return true;
			});
		}, &queryStorage);
	}

	return vec;
}

}
