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

SPUNUSED static void Handle_writeSelectViewDataQuery(SqlQuery &q, const db::Scheme &s, uint64_t oid, const db::FieldView &f, const Value &data);

Value SqlHandle::getFileField(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f) {
	if (auto fs = w.getApplicationInterface()->getFileScheme()) {
		auto sel = targetId ? query.select() : query.with("s", [&] (SqlQuery::GenericQuery &q) {
			q.select(f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid);
		}).select();
		String alias("t"); // do not touch;

		FieldResolver resv(*fs, w);

		resv.readFields([&] (const StringView &name, const Field *) {
			sel = sel.field(SqlQuery::Field("t", name));
		});

		if (targetId) {
			sel.from(SqlQuery::Field("__files").as(alias))
				.where(SqlQuery::Field(alias, "__oid"), Comparation::Equal, targetId).finalize();
		} else {
			sel.from(SqlQuery::Field("__files").as(alias)).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
				q.where(SqlQuery::Field(alias,"__oid"), Comparation::Equal, SqlQuery::Field("s", f.getName()));
			}).finalize();
		}

		auto ret = selectValueQuery(*fs, query, resv.getVirtuals());
		if (ret.isArray()) {
			ret = sp::move(ret.getValue(0));
		}
		return ret;
	}
	return Value();
}

size_t SqlHandle::getFileCount(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f) {
	size_t ret = 0;
	auto sel = (targetId ? query.select() : query.with("s", [&] (SqlQuery::GenericQuery &q) {
		q.select(f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid);
	}).select()).aggregate("COUNT", "*");
	String alias("t"); // do not touch;

	if (targetId) {
		sel.from(SqlQuery::Field("__files").as(alias))
			.where(SqlQuery::Field(alias, "__oid"), Comparation::Equal, targetId).finalize();
	} else {
		sel.from(SqlQuery::Field("__files").as(alias)).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
			q.where(SqlQuery::Field(alias,"__oid"), Comparation::Equal, SqlQuery::Field("s", f.getName()));
		}).finalize();
	}

	selectQuery(query, [&] (Result &result) {
		if (!result.empty()) {
			ret = size_t(result.current().toInteger(0));
			return true;
		}
		return false;
	});
	return ret;
}

Value SqlHandle::getArrayField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f) {
	query.select("data").from(toString(w.scheme().getName(), "_f_", f.getName()))
		.where(toString(w.scheme().getName(), "_id"), Comparation::Equal, oid).finalize();
	return selectValueQuery(f, query, Vector<const Field *>());
}

size_t SqlHandle::getArrayCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f) {
	size_t ret = 0;
	query.select().aggregate("COUNT", "*").from(toString(w.scheme().getName(), "_f_", f.getName()))
		.where(toString(w.scheme().getName(), "_id"), Comparation::Equal, oid).finalize();

	selectQuery(query, [&] (Result &result) {
		if (!result.empty()) {
			ret = size_t(result.current().toInteger(0));
			return true;
		}
		return false;
	});
	return ret;
}

Value SqlHandle::getObjectField(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f) {
	if (auto fs = f.getForeignScheme()) {
		auto sel = targetId ? query.select() : query.with("s", [&] (SqlQuery::GenericQuery &q) {
			q.select(f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid);
		}).select();
		String alias("t"); // do not touch;

		FieldResolver resv(*fs, w);
		resv.readFields([&] (const StringView &name, const Field *) {
			sel = sel.field(SqlQuery::Field("t", name));
		});

		if (targetId) {
			sel.from(SqlQuery::Field(fs->getName()).as(alias))
				.where(SqlQuery::Field(alias, "__oid"), Comparation::Equal, targetId).finalize();
		} else {
			sel.from(SqlQuery::Field(fs->getName()).as(alias)).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
				q.where(SqlQuery::Field("t", "__oid"), Comparation::Equal, SqlQuery::Field("s", f.getName()));
			}).finalize();
		}

		UpdateFlags flags = UpdateFlags::None;
		if (w.shouldIncludeAll()) { flags |= UpdateFlags::GetAll; }

		auto ret = selectValueQuery(*fs, query, resv.getVirtuals());
		if (ret.isArray()) {
			ret = sp::move(ret.getValue(0));
		}
		return ret;
	}
	return Value();
}

size_t SqlHandle::getObjectCount(Worker &w, SqlQuery &query, uint64_t oid, uint64_t targetId, const Field &f) {
	size_t ret = 0;
	if (auto fs = f.getForeignScheme()) {
		auto sel = (targetId ? query.select() : query.with("s", [&] (SqlQuery::GenericQuery &q) {
			q.select(f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid);
		}).select()).aggregate("COUNT", "*");
		String alias("t"); // do not touch;

		if (targetId) {
			sel.from(SqlQuery::Field(fs->getName()).as(alias))
				.where(SqlQuery::Field(alias, "__oid"), Comparation::Equal, targetId).finalize();
		} else {
			sel.from(SqlQuery::Field(fs->getName()).as(alias)).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
				q.where(SqlQuery::Field("t", "__oid"), Comparation::Equal, SqlQuery::Field("s", f.getName()));
			}).finalize();
		}

		selectQuery(query, [&] (Result &result) {
			if (!result.empty()) {
				ret = size_t(result.current().toInteger(0));
				return true;
			}
			return false;
		});
	}
	return ret;
}

Value SqlHandle::getSetField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &q) {
	auto fs = f.getForeignScheme();
	if (!fs) {
		return Value();
	}

	SqlQuery::Context ctx(query, *fs, w, q);
	if (query.writeQuery(ctx, w.scheme(), oid, f)) {
		return selectValueQuery(*f.getForeignScheme(), query, ctx.getVirtuals());
	}
	return Value();
}

size_t SqlHandle::getSetCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &q) {
	size_t ret = 0;
	if (auto fs = f.getForeignScheme()) {
		if (f.isReference()) {
			auto sel = query.with("s", [&] (SqlQuery::GenericQuery &q) {
				q.select(SqlQuery::Field(toString(fs->getName(), "_id")).as("id"))
						.from(toString(w.scheme().getName(), "_f_", f.getName()))
						.where(toString(w.scheme().getName(), "_id"), Comparation::Equal, oid);
			}).select();
			query.writeFullTextRank(sel, *fs, q);
			sel.aggregate("COUNT", "*");

			auto tmp = sel.from(fs->getName())
					.innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
				q.where(SqlQuery::Field(fs->getName(), "__oid"), Comparation::Equal, SqlQuery::Field("s", "id"));
			});

			if (q.hasSelect()) {
				auto whi = tmp.where();
				query.writeWhere(whi, db::Operator::And, *fs, q);
			}
			query.finalize();
		} else if (auto l = w.scheme().getForeignLink(f)) {
			auto sel = query.select();
			query.writeFullTextRank(sel, *fs, q);
			sel.aggregate("COUNT", "*");

			auto whi = sel.from(fs->getName())
				.where(l->getName(), Comparation::Equal, oid);
			query.writeWhere(whi, db::Operator::And, *fs, q);
			query.finalize();
		} else {
			return 0;
		}
		selectQuery(query, [&] (Result &result) {
			if (!result.empty()) {
				ret = size_t(result.current().toInteger(0));
				return true;
			}
			return false;
		});
	}
	return ret;
}

Value SqlHandle::getViewField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &q) {
	auto fs = f.getForeignScheme();
	if (!fs) {
		return Value();
	}

	SqlQuery::Context ctx(query, *fs, w, q);
	if (query.writeQuery(ctx, w.scheme(), oid, f)) {
		auto ret = selectValueQuery(*ctx.scheme, query, ctx.getVirtuals());
		if (ret.isArray() && ret.size() > 0) {
			query.clear();

			auto v = f.getSlot<FieldView>();

			Handle_writeSelectViewDataQuery(query, w.scheme(), oid, *v, ret);
			selectValueQuery(ret, *v, query);
			return ret;
		}
	}
	return Value();
}

size_t SqlHandle::getViewCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f, const db::Query &q) {
	size_t ret = 0;
	if (auto fs = f.getForeignScheme()) {
		auto sel = query.with("s", [&] (SqlQuery::GenericQuery &q) {
			q.select(SqlQuery::Distinct::Distinct, SqlQuery::Field(toString(fs->getName(), "_id")).as("__id"))
					.from(toString(w.scheme().getName(), "_f_", f.getName(), "_view"))
					.where(toString(w.scheme().getName(), "_id"), Comparation::Equal, oid);
		}).select();
		query.writeFullTextRank(sel, *fs, q);
		sel.aggregate("COUNT", "*");

		auto tmp = sel.from(fs->getName())
				.innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
			q.where(SqlQuery::Field(fs->getName(), "__oid"), Comparation::Equal, SqlQuery::Field("s", "__id"));
		});

		if (q.hasSelect()) {
			auto whi = tmp.where();
			query.writeWhere(whi, db::Operator::And, *fs, q);
		}
		query.finalize();

		selectQuery(query, [&] (Result &result) {
			if (!result.empty()) {
				ret = size_t(result.current().toInteger(0));
				return true;
			}
			return false;
		});
	}
	return ret;
}

Value SqlHandle::getSimpleField(Worker &w, SqlQuery &query, uint64_t oid, const Field &f) {
	if (f.getType() == Type::Virtual) {
		auto v = f.getSlot<FieldVirtual>();
		auto sel = query.select("__oid");
		for (auto &it : v->requireFields) {
			sel.field(it);
		}
		sel.from(w.scheme().getName()).where("__oid", Comparation::Equal, oid).finalize();
		auto ret = selectValueQuery(w.scheme(), query, Vector<const Field *>({&f}));
		if (ret.isArray()) {
			ret = sp::move(ret.getValue(0));
		}
		if (ret.isDictionary()) {
			ret = ret.getValue(f.getName());
		}
		return ret;
	} else {
		query.select(f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid).finalize();
		auto ret = selectValueQuery(w.scheme(), query, Vector<const Field *>());
		if (ret.isArray()) {
			ret = sp::move(ret.getValue(0));
		}
		if (ret.isDictionary()) {
			ret = ret.getValue(f.getName());
		}
		return ret;
	}
}

size_t SqlHandle::getSimpleCount(Worker &w, SqlQuery &query, uint64_t oid, const Field &f) {
	size_t ret = 0;
	query.select().aggregate("COUNT", f.getName()).from(w.scheme().getName()).where("__oid", Comparation::Equal, oid).finalize();
	selectQuery(query, [&] (Result &result) {
		if (!result.empty()) {
			ret = size_t(result.current().toInteger(0));
			return true;
		}
		return false;
	});
	return ret;
}

bool SqlHandle::insertIntoSet(SqlQuery &query, const Scheme &s, int64_t id, const db::FieldObject &field, const Field &ref, const Value &d) {
	if (field.type == db::Type::Object) {
		if (ref.getType() == db::Type::Object) {
			// object to object is not implemented
		} else {
			// object to set is maintained by trigger
		}
	} else if (field.type == db::Type::Set) {
		if (ref.getType() == db::Type::Object) {
			if (d.isArray() && d.getValue(0).isInteger()) {
				auto w = query.update(field.scheme->getName()).set(ref.getName(), id).where();
				for (auto & it : d.asArray()) {
					if (it.isInteger()) {
						w.where(Operator::Or, "__oid", Comparation::Equal, it.getInteger());
					}
				}
				w.finalize();
				return performQuery(query) != stappler::maxOf<size_t>();
			}
		} else {
			// set to set is not implemented
		}
	}
	return false;
}

bool SqlHandle::insertIntoArray(SqlQuery &query, const Scheme &scheme, int64_t id, const Field &field, Value &d) {
	if (d.isNull()) {
		query.remove(toString(scheme.getName(), "_f_", field.getName()))
				.where(toString(scheme.getName(), "_id"), Comparation::Equal, id).finalize();
		return performQuery(query) != stappler::maxOf<size_t>();
	} else {
		if (field.transform(scheme, id, const_cast<Value &>(d))) {
			auto &arrf = static_cast<const db::FieldArray *>(field.getSlot())->tfield;
			if (!d.empty()) {
				auto vals = query.insert(toString(scheme.getName(), "_f_", field.getName()))
						.fields(toString(scheme.getName(), "_id"), "data").values();
				for (auto &it : d.asArray()) {
					vals.values(id, db::Binder::DataField {&arrf, it, arrf.isDataLayout(), arrf.hasFlag(db::Flags::Compressed)});
				}
				if (field.hasFlag(Flags::Unique)) {
					vals.finalize();
				} else {
					vals.onConflictDoNothing().finalize();
				}
				return performQuery(query) != stappler::maxOf<size_t>();
			}
		}
	}
	return false;
}

bool SqlHandle::insertIntoRefSet(SqlQuery &query, const Scheme &scheme, int64_t id, const Field &field, const Vector<int64_t> &ids) {
	auto fScheme = field.getForeignScheme();
	if (!ids.empty() && fScheme) {
		auto vals = query.insert(toString(scheme.getName(), "_f_", field.getName()))
				.fields(toString(scheme.getName(), "_id"), toString(fScheme->getName(), "_id")).values();
		for (auto &it : ids) {
			vals.values(id, it);
		}
		vals.onConflictDoNothing().finalize();
		performQuery(query);
		return true;
	}
	return false;
}

bool SqlHandle::cleanupRefSet(SqlQuery &query, const Scheme &scheme, uint64_t oid, const Field &field, const Vector<int64_t> &ids) {
	auto objField = static_cast<const db::FieldObject *>(field.getSlot());
	auto fScheme = objField->scheme;
	if (!ids.empty() && fScheme) {
		if (objField->onRemove == db::RemovePolicy::Reference) {
			auto w = query.remove(toString(scheme.getName(), "_f_", field.getName()))
					.where(toString(scheme.getName(), "_id"), Comparation::Equal, oid);
			w.parenthesis(Operator::And, [&] (SqlQuery::WhereBegin &wh) {
				auto whi = wh.where();
				for (auto &it : ids) {
					whi.where(Operator::Or, toString(fScheme->getName(), "_id"), Comparation::Equal, it);
				}
			}).finalize();
			performQuery(query);
			return true;
		} else if (objField->onRemove == db::RemovePolicy::StrongReference) {
			auto w = query.remove(fScheme->getName()).where();
			for (auto &it : ids) {
				w.where(Operator::Or, "__oid", Comparation::Equal, it);
			}
			w.finalize();
			performQuery(query);
			return true;
		}
	}
	return false;
}

Value SqlHandle::field(db::Action a, Worker &w, uint64_t oid, const Field &f, Value &&val) {
	auto queryStorage = _driver->makeQueryStorage(w.scheme().getName());

	Value ret;
	switch (a) {
	case db::Action::Get:
		makeQuery([&, this] (SqlQuery &query) {
			switch (f.getType()) {
			case db::Type::File:
			case db::Type::Image: ret = getFileField(w, query, oid, 0, f); break;
			case db::Type::Array: ret = getArrayField(w, query, oid, f); break;
			case db::Type::Object: ret = getObjectField(w, query, oid, 0, f); break;
			case db::Type::Set: {
				db::Query db;
				auto &fields = w.getRequiredFields();
				for (auto &it : fields.includeFields) {
					if (it) { db.include(it->getName()); }
				}
				for (auto &it : fields.excludeFields) {
					if (it) { db.exclude(it->getName()); }
				}
				ret = getSetField(w, query, oid, f, db);
				break;
			}
			case db::Type::View: {
				db::Query db;
				auto &fields = w.getRequiredFields();
				for (auto &it : fields.includeFields) {
					if (it) { db.include(it->getName()); }
				}
				for (auto &it : fields.excludeFields) {
					if (it) { db.exclude(it->getName()); }
				}
				ret = getViewField(w, query, oid, f, db);
				break;
			}
			default: ret = getSimpleField(w, query, oid, f); break;
			}
		}, &queryStorage);
		break;
	case db::Action::Count:
		makeQuery([&, this] (SqlQuery &query) {
			switch (f.getType()) {
			case db::Type::File:
			case db::Type::Image: ret = Value(getFileCount(w, query, oid, 0, f)); break;
			case db::Type::Array: ret = Value(getArrayCount(w, query, oid, f)); break;
			case db::Type::Object: ret = Value(getObjectCount(w, query, oid, 0, f)); break;
			case db::Type::Set: ret = Value(getSetCount(w, query, oid, f, db::Query())); break;
			case db::Type::View: ret = Value(getViewCount(w, query, oid, f, db::Query())); break;
			default: ret = Value(getSimpleCount(w, query, oid, f)); break;
			}
		}, &queryStorage);
		break;
	case db::Action::Set:
		switch (f.getType()) {
		case db::Type::File:
		case db::Type::Image:
		case db::Type::View:
		case db::Type::FullTextView: return Value(); break; // file update should be done by scheme itself
		case db::Type::Array:
			if (val.isArray()) {
				field(db::Action::Remove, w, oid, f, Value());
				bool success = false;
				makeQuery([&, this] (SqlQuery &query) {
					success = insertIntoArray(query, w.scheme(), oid, f, val);
				}, &queryStorage);
				if (success) {
					ret = sp::move(val);
				}
			}
			break;
		case db::Type::Set:
			if (f.isReference()) {
				auto objField = static_cast<const db::FieldObject *>(f.getSlot());
				if (objField->onRemove == db::RemovePolicy::Reference) {
					field(db::Action::Remove, w, oid, f, Value());
				} else {
					makeQuery([&, this] (SqlQuery &query) {
						auto obj = static_cast<const db::FieldObject *>(f.getSlot());

						auto source = w.scheme().getName();
						auto target = obj->scheme->getName();

						query << "DELETE FROM " << target << " WHERE __oid IN (SELECT " << target << "_id FROM "
								<< w.scheme().getName() << "_f_" << f.getName() << " WHERE "<< source << "_id=" << oid << ")";

						for (auto &it : val.asArray()) {
							if (it.isInteger()) {
								query << " AND __oid != " << it.asInteger();
							}
						}
						query << ";";
						performQuery(query);
					}, &queryStorage);
				}
				ret = field(db::Action::Append, w, oid, f, sp::move(val));
			}
			// not implemented
			break;
		default: {
			Value patch;
			patch.setValue(val, f.getName().str<Interface>());
			Worker(w.scheme(), w.transaction()).update(oid, patch);
			return val;
			break;
		}
		}
		break;
	case db::Action::Append:
		switch (f.getType()) {
		case db::Type::Array:
			if (!val.isNull()) {
				Worker(w).touch(oid);
				bool success = false;
				makeQuery([&, this] (SqlQuery &query) {
					success = insertIntoArray(query, w.scheme(), oid, f, val);
				}, &queryStorage);
				if (success) {
					ret = sp::move(val);
				}
			}
			break;
		case db::Type::Set:
			if (f.isReference()) {
				Worker(w).touch(oid);
				Vector<int64_t> toAdd;
				if (val.isArray()) {
					for (auto &it : val.asArray()) {
						if (auto id = it.asInteger()) {
							toAdd.push_back(id);
						}
					}
				} else if (val.isInteger()) {
					toAdd.push_back(val.asInteger());
				}
				bool success = false;
				makeQuery([&, this] (SqlQuery &query) {
					success = insertIntoRefSet(query, w.scheme(), oid, f, toAdd);
				}, &queryStorage);
				if (success) {
					ret = sp::move(val);
				}
			}
			break;
		default:
			break;
		}
		break;
	case db::Action::Remove:
		switch (f.getType()) {
		case db::Type::File:
		case db::Type::Image:
		case db::Type::View:
		case db::Type::FullTextView: return Value(); break; // file update should be done by scheme itself
		case db::Type::Array:
			Worker(w).touch(oid);
			makeQuery([&, this] (SqlQuery &query) {
				query << "DELETE FROM " << w.scheme().getName() << "_f_" << f.getName() << " WHERE " << w.scheme().getName() << "_id=" << oid << ";";
				if (performQuery(query) != stappler::maxOf<size_t>()) {
					ret = Value(true);
				}
			}, &queryStorage);
			break;
		case db::Type::Set:
			if (f.isReference()) {
				Worker(w).touch(oid);
				auto objField = static_cast<const db::FieldObject *>(f.getSlot());
				if (!val.isArray()) {
					makeQuery([&, this] (SqlQuery &query) {
						if (objField->onRemove == db::RemovePolicy::Reference) {
							query.remove(toString(w.scheme().getName(), "_f_", f.getName()))
									.where(toString(w.scheme().getName(), "_id"), Comparation::Equal, oid)
									.finalize();
							ret = Value(performQuery(query) != stappler::maxOf<size_t>());
						} else {
							// for strong refs
							auto obj = static_cast<const db::FieldObject *>(f.getSlot());

							auto source = w.scheme().getName();
							auto target = obj->scheme->getName();

							query << "DELETE FROM " << target << " WHERE __oid IN (SELECT " << target << "_id FROM "
									<< w.scheme().getName() << "_f_" << f.getName() << " WHERE "<< source << "_id=" << oid << ");";
							ret = Value(performQuery(query) != stappler::maxOf<size_t>());
						}
					}, &queryStorage);
				} else {
					Vector<int64_t> toRemove;
					for (auto &it : val.asArray()) {
						if (auto id = it.asInteger()) {
							toRemove.push_back(id);
						}
					}

					makeQuery([&, this] (SqlQuery &query) {
						ret = Value(cleanupRefSet(query, w.scheme(), oid, f, toRemove));
					}, &queryStorage);
				}
			}
			break;
		case db::Type::Object: {
			if (f.isReference()) {
				auto ref = static_cast<const FieldObject *>(f.getSlot());
				if (ref->onRemove == RemovePolicy::StrongReference) {
					if (auto obj = Worker(w).get(oid, { f.getName() })) {
						Worker(*ref->scheme, w.transaction()).remove(obj.getInteger("__oid"));
					}
				}
			}
			Value patch;
			patch.setValue(Value(), f.getName().str<Interface>());
			ret = Worker(w).update(oid, patch);
			break;
		}
		default:
			break;
		}
		break;
	}
	return ret;
}

Value SqlHandle::field(db::Action a, Worker &w, const Value &obj, const Field &f, Value &&val) {
	auto queryStorage = _driver->makeQueryStorage(w.scheme().getName());

	Value ret;
	auto oid = obj.isInteger() ? obj.asInteger() : obj.getInteger("__oid");
	auto targetId = obj.isInteger() ? obj.asInteger() : obj.getInteger(f.getName());
	switch (a) {
	case db::Action::Get:
		makeQuery([&, this] (SqlQuery &query) {
			switch (f.getType()) {
			case db::Type::File:
			case db::Type::Image: ret = getFileField(w, query, oid, targetId, f); break;
			case db::Type::Array: ret = getArrayField(w, query, oid, f); break;
			case db::Type::Object: ret = getObjectField(w, query, oid, targetId, f); break;
			case db::Type::Set: {
				db::Query db;
				auto &fields = w.getRequiredFields();
				for (auto &it : fields.includeFields) {
					if (it) { db.include(it->getName()); }
				}
				for (auto &it : fields.excludeFields) {
					if (it) { db.exclude(it->getName()); }
				}
				ret = getSetField(w, query, oid, f, db);
				break;
			}
			case db::Type::View: {
				db::Query db;
				auto &fields = w.getRequiredFields();
				for (auto &it : fields.includeFields) {
					if (it) { db.include(it->getName()); }
				}
				for (auto &it : fields.excludeFields) {
					if (it) { db.exclude(it->getName()); }
				}
				ret = getViewField(w, query, oid, f, db);
				break;
			}
			default:
				if (auto val = obj.getValue(f.getName())) {
					ret = sp::move(val);
				} else {
					ret = getSimpleField(w, query, oid, f);
				}
				break;
			}
		}, &queryStorage);
		break;
	case db::Action::Count:
		makeQuery([&, this] (SqlQuery &query) {
			switch (f.getType()) {
			case db::Type::File:
			case db::Type::Image: ret = Value(getFileCount(w, query, oid, 0, f)); break;
			case db::Type::Array: ret = Value(getArrayCount(w, query, oid, f)); break;
			case db::Type::Object: ret = Value(getObjectCount(w, query, oid, 0, f)); break;
			case db::Type::Set: ret = Value(getSetCount(w, query, oid, f, db::Query())); break;
			case db::Type::View: ret = Value(getViewCount(w, query, oid, f, db::Query())); break;
			default:
				if (auto val = obj.getValue(f.getName())) {
					ret = Value(1);
				} else {
					ret = Value(getSimpleCount(w, query, oid, f));
				}
				break;
			}
		}, &queryStorage);
		break;
	case db::Action::Set:
	case db::Action::Remove:
	case db::Action::Append:
		ret = field(a, w, oid, f, sp::move(val));
		break;
	}
	return ret;
}

}
