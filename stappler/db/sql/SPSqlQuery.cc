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

#include "SPSqlQuery.h"
#include "SPSqlHandle.h"
#include "SPSqlDriver.h"
#include "SPDbScheme.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

SqlQuery::SqlQuery(db::QueryInterface *iface, const Driver *driver) {
	binder.setInterface(iface);
	_driver = driver;
}

void SqlQuery::clear() {
	stream.clear();
	binder.clear();
}

static inline bool SqlQuery_comparationIsValid(const Driver *driver, const Field &f, Comparation comp) {
	if (f.isIndexed() || comp == Comparation::IsNull || comp == Comparation::IsNotNull) {
		if (f.getType() == Type::Custom) {
			auto c = f.getSlot<FieldCustom>();
			if (auto info = driver->getCustomFieldInfo(c->getDriverTypeName())) {
				return info->isComparationAllowed(*c, comp);
			}
		} else {
			return db::checkIfComparationIsValid(f.getType(), comp, f.getFlags());
		}
	}
	return false;
}

static inline auto SqlQuery_makeSoftLimitWith(SqlQuery::Context &ictx,
		bool isSubField = false, const StringView &lName = StringView(), uint64_t oid = 0) {
	return [ctx = &ictx, isSubField, lName, oid] (SqlQuery::GenericQuery &subq) {
		SqlQuery &query = *ctx->_this;
		const Scheme &scheme = *ctx->scheme;

		auto writeFullTextSel = [&] (SqlQuery::GenericQuery &subQ) {
			auto sel = subQ.select(SqlQuery::Field(scheme.getName(), "__oid"));
			auto ftsQuery = ctx->getAlt(ctx->softLimitField);
			if (!ftsQuery.empty()) {
				if (sel.state == SqlQuery::State::Some) {
					sel.query->getStream() << ", ";
				}
				sel.query->writeBind(db::Binder::FullTextRank{scheme.getName(), scheme.getField(ctx->softLimitField), ftsQuery});
				sel.query->getStream() << " AS __ts_rank_" << ctx->softLimitField;
				sel.state = SqlQuery::State::Some;
			}
			return sel;
		};

		auto sel = (ctx->softLimitIsFts)
				? writeFullTextSel(subq)
				: subq.select(SqlQuery::Field(scheme.getName(), "__oid"), SqlQuery::Field(scheme.getName(), ctx->softLimitField));
		auto s = isSubField ? sel.from(scheme.getName()).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
			q.where(SqlQuery::Field(scheme.getName(), "__oid"), Comparation::Equal, SqlQuery::Field("s", "id"));
		}) : sel.from(scheme.getName());

		if (auto &val = ctx->query->getSoftLimitValue()) {
			if (ctx->softLimitIsFts) {
				auto ftsQuery = ctx->getAlt(ctx->softLimitField);

				s.query->getStream() << " WHERE(";
				s.query->writeBind(db::Binder::FullTextRank{ctx->scheme->getName(), ctx->scheme->getField(ctx->softLimitField), ftsQuery});
				s.query->getStream() << ((ctx->query->getOrdering() == Ordering::Ascending) ? '>' : '<');
				s.query->writeBind(val);
				s.query->getStream() << ')';

				SqlQuery::SelectWhere w(s.query, SqlQuery::State::Some);
				if (!lName.empty()) {
					w.where(Operator::And, SqlQuery::Field(scheme.getName(), lName), Comparation::Equal, oid);
				}
				query.writeWhere(w, Operator::And, scheme, *ctx->query);

			} else {
				auto w = s.where(SqlQuery::Field(scheme.getName(), ctx->softLimitField),
						ctx->query->getOrdering() == Ordering::Ascending ? Comparation::GreatherThen : Comparation::LessThen, val);
				if (!lName.empty()) {
					w.where(Operator::And, SqlQuery::Field(scheme.getName(), lName), Comparation::Equal, oid);
				}
				query.writeWhere(w, Operator::And, scheme, *ctx->query);
			}
		} else if (ctx->query->hasSelect() || !lName.empty()) {
			auto w = lName.empty() ? s.where() : s.where(SqlQuery::Field(scheme.getName(), lName), Comparation::Equal, oid);
			query.writeWhere(w, Operator::And, scheme, *ctx->query);
		}

		query.writeOrdering(s, scheme, *ctx->query);
	};
}

template <typename Clause>
static void SqlQuery_makeCustomFrom(const Driver *driver, SqlQuery &q, Clause &tmp, const Query &query, const Scheme &scheme) {
	for (auto &it : query.getSelectList()) {
		auto f = scheme.getField(it.field);
		switch (f->getType()) {
		case db::Type::Custom: {
			auto c = f->getSlot<FieldCustom>();
			if (auto info = driver->getCustomFieldInfo(c->getDriverTypeName())) {
				if (info->writeFrom) {
					info->writeFrom(*c, scheme, tmp, it.compare, it.value1, it.value2);
				}
			}
			break;
		}
		case db::Type::FullTextView:
			q.writeFullTextFrom(tmp, scheme, f, it);
			break;
		default:
			break;
		}
	}
}

template <typename Clause>
static inline auto SqlQuery_makeWhereClause(const Driver *driver, SqlQuery::Context &ctx, Clause &tmp, const StringView &lName = StringView(), uint64_t oid = 0) {
	SqlQuery_makeCustomFrom(driver, *ctx._this, tmp, *ctx.query, *ctx.scheme);

	bool isAsc = ctx.query->getOrdering() == Ordering::Ascending;
	if (ctx.query->hasSelect() || !ctx.softLimitField.empty() || !lName.empty()) {
		if (ctx.softLimitField == "__oid" || !ctx.hasAltLimit) {
			if (auto &val = ctx.query->getSoftLimitValue()) {
				auto w = tmp.where(SqlQuery::Field(ctx.scheme->getName(), ctx.softLimitField),
						isAsc ? Comparation::GreatherThen : Comparation::LessThen, val.asInteger());
				if (!lName.empty()) {
					w.where(Operator::And, SqlQuery::Field(ctx.scheme->getName(), lName), Comparation::Equal, oid);
				}
				ctx._this->writeWhere(w, Operator::And, *ctx.scheme, *ctx.query);
			} else if (ctx.query->hasSelect() || !lName.empty()) {
				auto w = lName.empty() ? tmp.where() : tmp.where(SqlQuery::Field(ctx.scheme->getName(), lName), Comparation::Equal, oid);
				ctx._this->writeWhere(w, Operator::And, *ctx.scheme, *ctx.query);
			}
		} else if (ctx.softLimitField.empty()) {
			if (ctx.query->hasSelect() || !lName.empty()) {
				auto whi = lName.empty() ? tmp.where() : tmp.where(SqlQuery::Field(ctx.scheme->getName(), lName), Comparation::Equal, oid);
				ctx._this->writeWhere(whi, db::Operator::And, *ctx.scheme, *ctx.query);
			}
		} else {
			auto softLimitFieldStr = (ctx.softLimitIsFts
				? toString("__ts_rank_", ctx.softLimitField)
				: ctx.softLimitField.str<Interface>());

			// write soft limit query like WHERE __oid IN (SELECT __oid FROM u) OR (field = (SELECT MAX(mtime) FROM u))
			tmp.where(SqlQuery::Field(ctx.scheme->getName(), "__oid"), Comparation::In, Callback<void(SqlQuery::Select &)>([&] (SqlQuery::Select &subQ) {
				subQ.field(SqlQuery::Field("u", "__oid")).from("u").finalize();
			})).parenthesis(Operator::Or, [&] (SqlQuery::WhereBegin &whi) {
				if (ctx.softLimitIsFts) {
					auto ftsQuery = ctx.getAlt(ctx.softLimitField);

					whi.query->getStream() << '(';
					whi.query->writeBind(db::Binder::FullTextRank{ctx.scheme->getName(), ctx.scheme->getField(ctx.softLimitField), ftsQuery});
					whi.query->getStream() << "=(SELECT " << (isAsc ? "MAX" : "MIN") << "(u.\"__ts_rank_" << ctx.softLimitField << "\") FROM u))";

					SqlQuery::WhereContinue w(whi.query, SqlQuery::State::Some);
					if (!lName.empty()) {
						w.where(Operator::And, SqlQuery::Field(ctx.scheme->getName(), lName), Comparation::Equal, oid);
					}
					ctx._this->writeWhere(w, Operator::And, *ctx.scheme, *ctx.query);
				} else {
					auto w = whi.where(SqlQuery::Field(ctx.scheme->getName(), ctx.softLimitField), Comparation::Equal,
							Callback<void(SqlQuery::Select &)>([&] (SqlQuery::Select &subQ) {
						subQ.aggregate(isAsc ? "MAX" : "MIN", SqlQuery::Field("u", softLimitFieldStr)).from("u").finalize();
					}));
					if (!lName.empty()) {
						w.where(Operator::And, SqlQuery::Field(ctx.scheme->getName(), lName), Comparation::Equal, oid);
					}
					ctx._this->writeWhere(w, Operator::And, *ctx.scheme, *ctx.query);
				}
			});
		}
	}
}

bool SqlQuery::writeQuery(Context &ctx) {
	auto sel = (ctx.hasAltLimit)
		? with("u", SqlQuery_makeSoftLimitWith(ctx, false)).select()
		: select();
	auto s = writeSelectFrom(sel, ctx);

	SqlQuery_makeWhereClause(_driver, ctx, s);

	writeOrdering(s, *ctx.scheme, *ctx.query, ctx.hasAltLimit);
	if (ctx.query->isForUpdate()) { s.forUpdate(); }
	s.finalize();

	return true;
}

bool SqlQuery::writeQuery(Context &ctx, const db::Scheme &scheme, uint64_t oid, const db::Field &f) {
	StringView lName;
	if (f.getType() == Type::Set && !f.isReference()) {
		if (auto l = scheme.getForeignLink(f)) {
			lName = l->getName();
		}
	}

	auto writeFields = [&, this] (Select &sel) {
		writeFullTextRank(sel, *ctx.scheme, *ctx.query);
		if (ctx.shouldIncludeAll()) {
			sel = sel.field(SqlQuery::Field(ctx.scheme->getName(), "*"));
		} else {
			ctx.readFields([&] (const StringView &name, const db::Field *) {
				sel = sel.field(SqlQuery::Field(ctx.scheme->getName(), name));
			});
		}
	};

	auto writeSelect = [&, this] () -> Select {
		if (f.getType() == Type::View || (f.getType() == Type::Set && f.isReference())) {
			auto wtmp = with("s", [&] (SqlQuery::GenericQuery &q) {
				q.select(SqlQuery::Distinct::Distinct, SqlQuery::Field(toString(ctx.scheme->getName(), "_id")).as("id"))
					.from(toString(scheme.getName(), "_f_", f.getName(), (f.getType() == Type::View ? "_view" : "")))
					.where(toString(scheme.getName(), "_id"), Comparation::Equal, oid);
			});

			if (ctx.hasAltLimit) {
				wtmp.with("u", SqlQuery_makeSoftLimitWith(ctx, true, lName, oid));
			}

			return wtmp.select();
		} else {
			return (ctx.hasAltLimit)
				? with("u", SqlQuery_makeSoftLimitWith(ctx, false, lName, oid)).select()
				: select();
		}
	};

	auto sel = writeSelect();
	writeFields(sel);

	auto tmp = (f.getType() == Type::View || (f.getType() == Type::Set && f.isReference()))
		? sel.from(ctx.scheme->getName()).innerJoinOn("s", [&] (SqlQuery::WhereBegin &q) {
				q.where(SqlQuery::Field(ctx.scheme->getName(), "__oid"), Comparation::Equal, SqlQuery::Field("s", "id"));
			})
		: sel.from(ctx.scheme->getName());

	SqlQuery_makeWhereClause(_driver, ctx, tmp, lName, oid);

	writeOrdering(tmp, *ctx.scheme, *ctx.query, ctx.hasAltLimit);
	if (ctx.query->isForUpdate()) { tmp.forUpdate(); }
	finalize();

	return true;
}

void SqlQuery::writeWhere(SqlQuery::SelectWhere &w, db::Operator op, const db::Scheme &scheme, const db::Query &q) {
	SqlQuery::WhereContinue iw(w.query, w.state);
	writeWhere(iw, op, scheme, q);
}

void SqlQuery::writeWhere(SqlQuery::WhereContinue &w, db::Operator op, const db::Scheme &scheme, const db::Query &q) {
	if (q.getSingleSelectId()) {
		w.where(op, "__oid", db::Comparation::Equal, q.getSingleSelectId());
	} else if (!q.getSelectIds().empty()) {
		w.where(op, SqlQuery::Field(scheme.getName(), "__oid"), db::Comparation::In, q.getSelectIds());
	} else if (!q.getSelectAlias().empty()) {
		w.parenthesis(op, [&] (SqlQuery::WhereBegin &wh) {
			auto whi = wh.where();
			for (auto &it : scheme.getFields()) {
				if (it.second.getType() == db::Type::Text && it.second.getTransform() == db::Transform::Alias) {
					whi.where(db::Operator::Or, SqlQuery::Field(scheme.getName(), it.first), db::Comparation::Equal, q.getSelectAlias());
				}
			}
		});
	} else if (q.hasSelectName()) {
		// failsafe
		w.parenthesis(op, [&] (SqlQuery::WhereBegin &wh) {
			auto whi = wh.where();
			whi.where(db::Operator::Or, SqlQuery::Field(scheme.getName(), "__oid"), db::Comparation::Equal, Value(0));
		});
	} else if (q.getSelectList().size() > 0) {
		w.parenthesis(op, [&, this] (SqlQuery::WhereBegin &wh) {
			auto whi = wh.where();
			for (auto &it : q.getSelectList()) {
				writeWhereItem(whi, db::Operator::And, scheme, it);
			}
		});
	}
}

static void SqlQuery_writeWhereData(const Driver *driver, SqlQuery::WhereContinue &whi, db::Operator op, const db::Scheme &scheme, const db::Field &f,
		Comparation compare, const Value &value1, const Value &value2) {
	if (SqlQuery_comparationIsValid(driver, f, compare)) {
		auto type = f.getType();
		if (type == Type::Custom) {
			auto c = f.getSlot<FieldCustom>();
			if (auto info = driver->getCustomFieldInfo(c->getDriverTypeName())) {
				info->writeQuery(*c, scheme, whi, op, f.getName(), compare, value1, value2);
			}
		} else {
			if ((compare == Comparation::Equal || compare == db::Comparation::NotEqual)
					&& (type == Type::Integer || type == Type::Float || type == Type::Object || type == Type::Text)
					&& value1.isArray()) {
				switch (type) {
				case Type::Integer:
				case Type::Object: {
					Vector<int64_t> vec;
					for (auto &it : value1.asArray()) {
						vec.emplace_back(it.getInteger());
					}
					whi.where(op, SqlQuery::Field(scheme.getName(), f.getName()),
							(compare == Comparation::Equal) ? Comparation::In : Comparation::NotIn, vec);
					break;
				}
				case Type::Float: {
					Vector<double> vec;
					for (auto &it : value1.asArray()) {
						vec.emplace_back(it.getDouble());
					}
					whi.where(op, SqlQuery::Field(scheme.getName(), f.getName()),
							(compare == Comparation::Equal) ? Comparation::In : Comparation::NotIn, vec);
					break;
				}
				case Type::Text: {
					Vector<StringView> vec;
					for (auto &it : value1.asArray()) {
						vec.emplace_back(it.getString());
					}
					whi.where(op, SqlQuery::Field(scheme.getName(), f.getName()),
							(compare == Comparation::Equal) ? Comparation::In : Comparation::NotIn, vec);
					break;
				}
				default:
					break;
				}
			} else {
				whi.where(op, SqlQuery::Field(scheme.getName(), f.getName()), compare, value1, value2);
			}
		}
	} else {
		driver->getApplicationInterface()->error("Sql", "Condition is not applicable", Value({
			stappler::pair("scheme", Value(scheme.getName())),
			stappler::pair("field", Value(f.getName())),
			stappler::pair("cmp", Value(encodeComparation(compare).first)),
		}));
	}
}

void SqlQuery::writeWhereItem(SqlQuery::WhereContinue &whi, db::Operator op, const db::Scheme &scheme, const db::Query::Select &sel) {
	if (auto f = scheme.getField(sel.field)) {
		auto type = f->getType();
		if (type == db::Type::FullTextView && sel.compare != db::Comparation::IsNull && sel.compare != db::Comparation::IsNotNull) {
			auto ftsQuery = getFullTextQuery(scheme, *f, sel);
			if (!ftsQuery.empty()) {
				writeFullTextWhere(whi, op, scheme, sel, ftsQuery);
			}
		} else if (SqlQuery_comparationIsValid(_driver, *f, sel.compare)) {
			SqlQuery_writeWhereData(_driver, whi, op, scheme, *f, sel.compare, sel.value1, sel.value2);
		}
	}
}

void SqlQuery::writeWhereCond(SqlQuery::WhereContinue &whi, db::Operator op, const db::Scheme &scheme, const db::Worker::ConditionData &sel) {
	SqlQuery_writeWhereData(_driver, whi, op, scheme, *sel.field, sel.compare, sel.value1, sel.value2);
}

void SqlQuery::writeOrdering(SqlQuery::SelectFrom &s, const db::Scheme &scheme, const db::Query &q, bool dropLimits) {
	if (q.hasOrder() || q.hasLimit() || q.hasOffset()) {
		auto ordering = q.getOrdering();
		String orderField;
		String schemeName = scheme.getName().str<Interface>();
		if (q.hasOrder()) {
			if (auto f = scheme.getField(q.getOrderField())) {
				if (f->getType() == db::Type::FullTextView) {
					orderField = toString("__ts_rank_", q.getOrderField());
					schemeName.clear();
				} else {
					orderField = q.getOrderField();
				}
			} else {
				return;
			}
		} else if (q.getSelectList().size() == 1) {
			orderField = q.getSelectList().back().field;
			if (!scheme.getField(orderField)) {
				return;
			}
		} else {
			orderField = "__oid";
		}

		SelectOrder o = s.order(ordering, schemeName.empty() ? SqlQuery::Field(orderField) : SqlQuery::Field(scheme.getName(), orderField),
				ordering == db::Ordering::Descending ? stappler::sql::Nulls::Last : stappler::sql::Nulls::None);

		if (!dropLimits) {
			if (q.hasLimit() && q.hasOffset()) {
				o.limit(q.getLimitValue(), q.getOffsetValue());
			} else if (q.hasLimit()) {
				o.limit(q.getLimitValue());
			} else if (q.hasOffset()) {
				o.offset(q.getOffsetValue());
			}
		}
	}
}

void SqlQuery::writeQueryReqest(SqlQuery::SelectFrom &s, const db::QueryList::Item &item) {
	auto &q = item.query;
	if (!item.all && !item.query.empty()) {
		SqlQuery_makeCustomFrom(_driver, *this, s, item.query, *item.scheme);

		auto w = s.where();
		writeWhere(w, db::Operator::And, *item.scheme, q);
	}

	writeOrdering(s, *item.scheme, q);
}

static void SqlQuery_writeJoin(SqlQuery::SelectFrom &s, const StringView &sqName, const StringView &schemeName, const db::QueryList::Item &item) {
	s.innerJoinOn(sqName, [&] (SqlQuery::WhereBegin &w) {
		StringView fieldName = item.ref
				? ( item.ref->getType() == db::Type::Set ? StringView("__oid") : item.ref->getName() )
				: StringView("__oid");
		w.where(SqlQuery::Field(schemeName, fieldName), db::Comparation::Equal, SqlQuery::Field(sqName, "id"));
	});
}

auto SqlQuery::writeSelectFrom(GenericQuery &q, const db::QueryList::Item &item, bool idOnly,
		const StringView &schemeName, const StringView &fieldName, bool isSimpleGet) -> SelectFrom {
	if (idOnly) {
		auto sel = q.select();
		sel.field(SqlQuery::Field(schemeName, fieldName).as("id"));
		writeFullTextRank(sel, *item.scheme, item.query);
		return sel.from(schemeName);
	}

	auto sel = q.select();
	writeFullTextRank(sel, *item.scheme, item.query);
	FieldResolver resv(*item.scheme, item.query, item.getQueryFields());
	resv.readFields([&] (const StringView &name, const db::Field *) {
		sel = sel.field(SqlQuery::Field(schemeName, name));
	}, isSimpleGet);
	return sel.from(schemeName);
}

auto SqlQuery::writeSelectFrom(Select &sel, Context &ctx) -> SelectFrom {
	writeFullTextRank(sel, *ctx.scheme, *ctx.query);
	ctx.readFields([&] (const StringView &name, const db::Field *) {
		sel = sel.field(SqlQuery::Field(name));
	});
	return sel.from(ctx.scheme->getName());
}

void SqlQuery::writeQueryListItem(GenericQuery &q, const db::QueryList &list, size_t idx, bool idOnly, const db::Field *field, bool forSubquery) {
	auto &items = list.getItems();
	const db::QueryList::Item &item = items.at(idx);
	const db::Field *sourceField = nullptr;
	const db::FieldView *viewField = nullptr;
	String refQueryTag;
	if (idx > 0) {
		sourceField = items.at(idx - 1).field;
	}

	if (idx > 0 && !item.ref && sourceField && sourceField->getType() != db::Type::Object) {
		String prevSq = toString("sq", idx - 1);
		const db::QueryList::Item &prevItem = items.at(idx - 1);

		if (sourceField->getType() == db::Type::View) {
			viewField = static_cast<const db::FieldView *>(sourceField->getSlot());
		}
		String tname = viewField
				? toString(prevItem.scheme->getName(), "_f_", prevItem.field->getName(), "_view")
				: toString(prevItem.scheme->getName(), "_f_", prevItem.field->getName());

		String targetIdField = toString(item.scheme->getName(), "_id");
		String sourceIdField = toString(prevItem.scheme->getName(), "_id");

		if (idOnly && item.query.empty()) { // optimize id-only empty request
			q.select(SqlQuery::Field(targetIdField).as("id"))
					.from(tname)
					.innerJoinOn(prevSq, [&] (WhereBegin &w) {
				w.where(sourceIdField, stappler::sql::Comparation::Equal, SqlQuery::Field(prevSq, "id"));
			});
			return;
		}

		refQueryTag = toString("sq", idx, "_ref");
		q.with(refQueryTag, [&] (GenericQuery &sq) {
			sq.select(SqlQuery::Field(targetIdField).as("id"))
					.from(tname).innerJoinOn(prevSq, [&] (WhereBegin &w) {
				w.where(SqlQuery::Field(sourceIdField), stappler::sql::Comparation::Equal, SqlQuery::Field(prevSq, "id"));
			});
		});
	}

	const db::Field * f = field?field:item.field;

	StringView schemeName(item.scheme->getName());
	StringView fieldName( (f && (
		(f->getType() == db::Type::Object && (forSubquery || !idOnly || idx + 1 == items.size()))
		|| f->isFile()))
			? f->getName()
			: StringView("__oid") );

	auto s = writeSelectFrom(q, item, idOnly, schemeName, fieldName, list.hasFlag(db::QueryList::SimpleGet));
	if (idx > 0) {
		if (refQueryTag.empty()) {
			SqlQuery_writeJoin(s, toString("sq", idx - 1), item.scheme->getName(), item);
		} else {
			SqlQuery_writeJoin(s, refQueryTag, item.scheme->getName(), item);
		}
	}
	writeQueryReqest(s, item);
}

void SqlQuery::writeQueryList(const db::QueryList &list, bool idOnly, size_t count) {
	const db::QueryList::Item &item = list.getItems().back();
	if (item.query.hasDelta() && list.isDeltaApplicable()) {
		if (!list.isView()) {
			writeQueryDelta(*item.scheme, stappler::Time::microseconds(item.query.getDeltaToken()), item.fields.getResolves(), false);
		} else {
			writeQueryViewDelta(list, stappler::Time::microseconds(item.query.getDeltaToken()), item.fields.getResolves(), false);
		}
		return;
	} else if (item.query.hasDelta()) {
		list.getApplicationInterface()->error("Query", "Delta is not applicable for this query");
	}

	auto &items = list.getItems();
	count = std::min(items.size(), count);

	GenericQuery q(this);
	size_t i = 0;
	if (count > 0) {
		for (; i < count - 1; ++ i) {
			q.with(toString("sq", i), [&, this] (GenericQuery &sq) {
				writeQueryListItem(sq, list, i, true, nullptr, true);
			});
		}
	}

	writeQueryListItem(q, list, i, idOnly, nullptr, false);
}

void SqlQuery::writeQueryFile(const ApplicationInterface *app, const db::QueryList &list, const db::Field *field) {
	auto &items = list.getItems();
	auto count = items.size();
	GenericQuery q(this);
	for (size_t i = 0; i < count - 1; ++ i) {
		q.with(toString("sq", i), [&, this] (GenericQuery &sq) {
			writeQueryListItem(sq, list, i, true);
		});
	}

	q.with(toString("sq", count - 1), [&, this] (GenericQuery &sq) {
		writeQueryListItem(sq, list, count - 1, true, field);
	});

	auto fileScheme = app->getFileScheme();
	q.select(SqlQuery::Field::all(fileScheme->getName()))
			.from(fileScheme->getName())
			.innerJoinOn(toString("sq", count - 1), [&] (SqlQuery::WhereBegin &w) {
		w.where(SqlQuery::Field(fileScheme->getName(), "__oid"), db::Comparation::Equal, SqlQuery::Field(toString("sq", count - 1), "id"));
	});
}

void SqlQuery::writeQueryArray(const db::QueryList &list, const db::Field *field) {
	auto &items = list.getItems();
	auto count = items.size();
	GenericQuery q(this);
	for (size_t i = 0; i < count; ++ i) {
		q.with(toString("sq", i), [&, this] (GenericQuery &sq) {
			writeQueryListItem(sq, list, i, true);
		});
	}

	auto scheme = items.back().scheme;

	q.select(SqlQuery::Field("t", "data"))
			.from(SqlQuery::Field(toString(scheme->getName(), "_f_", field->getName())).as("t"))
			.innerJoinOn(toString("sq", count - 1), [&] (SqlQuery::WhereBegin &w) {
		w.where(SqlQuery::Field("t", toString(scheme->getName(), "_id")), db::Comparation::Equal, SqlQuery::Field(toString("sq", count - 1), "id"));
	});
}

void SqlQuery::writeQueryDelta(const db::Scheme &scheme, const stappler::Time &time, const Set<const db::Field *> &fields, bool idOnly) {
	GenericQuery q(this);
	auto s = q.with("d", [&] (SqlQuery::GenericQuery &sq) {
		sq.select()
			.aggregate("max", Field("time").as("time"))
			.aggregate("max", Field("action").as("action"))
			.field("object")
			.from(SqlHandle::getNameForDelta(scheme))
			.where("time", db::Comparation::GreatherThen, time.toMicroseconds())
			.group("object")
			.order(db::Ordering::Descending, "time");
	}).select();
	if (!idOnly) {
		FieldResolver resv(scheme, fields);
		resv.readFields([&] (const StringView &name, const db::Field *field) {
			s.field(SqlQuery::Field("t", name));
		});
	} else {
		s.field(Field("t", "__oid"));
	}
	s.fields(Field("d", "action").as("__d_action"), Field("d", "time").as("__d_time"), Field("d", "object").as("__d_object"))
		.from(Field(scheme.getName()).as("t"))
		.rightJoinOn("d", [&] (SqlQuery::WhereBegin &w) {
			w.where(Field("d", "object"), db::Comparation::Equal, Field("t", "__oid"));
	});
}

void SqlQuery::writeQueryViewDelta(const db::QueryList &list, const stappler::Time &time, const Set<const db::Field *> &fields, bool idOnly) {
	auto &items = list.getItems();
	const db::QueryList::Item &item = items.back();
	auto prevScheme = items.size() > 1 ? items.at(items.size() - 2).scheme : nullptr;
	auto viewField = items.size() > 1 ? items.at(items.size() - 2).field : items.back().field;
	auto view = static_cast<const db::FieldView *>(viewField->getSlot());

	GenericQuery q(this);
	const db::Scheme &scheme = *item.scheme;
	String deltaName = toString(prevScheme->getName(), "_f_", view->name, "_delta");
	String viewName = toString(prevScheme->getName(), "_f_", view->name, "_view");
	auto s = q.with("dv", [&] (SqlQuery::GenericQuery &sq) {
		uint64_t id = 0;
		String sqName;
		// optimize id-only
		if (items.size() != 2 || items.front().query.getSingleSelectId() == 0) {
			size_t i = 0;
			for (; i < items.size() - 1; ++ i) {
				sq.with(toString("sq", i), [&, this] (GenericQuery &sq) {
					writeQueryListItem(sq, list, i, true);
				});
			}
			sqName = toString("sq", i - 1);
		} else {
			id = items.front().query.getSingleSelectId();
		}

		sq.with("d", [&] (SqlQuery::GenericQuery &sq) {
			if (id) {
				sq.select()
					.aggregate("max", Field("time").as("time"))
					.field("object")
					.field("tag")
					.from(deltaName)
					.where(SqlQuery::Field("tag"), db::Comparation::Equal, id)
					.where(db::Operator::And, "time", db::Comparation::GreatherThen, time.toMicroseconds())
					.group("object").field("tag");
			} else {
				sq.select()
					.aggregate("max", Field("time").as("time"))
					.field("object")
					.field(Field(sqName, "id").as("tag"))
					.from(deltaName)
					.innerJoinOn(sqName, [&] (SqlQuery::WhereBegin &w) {
						w.where(SqlQuery::Field(deltaName, "tag"), db::Comparation::Equal, SqlQuery::Field(sqName, "id"));
				})
					.where("time", db::Comparation::GreatherThen, time.toMicroseconds())
					.group("object").field(SqlQuery::Field(sqName, "id"));;
			}
		}).select().fields(Field("d", "time"), Field("d", "object"), Field("__vid"))
			.from(viewName).rightJoinOn("d", [&] (SqlQuery::WhereBegin &w) {
				w.where(SqlQuery::Field("d", "tag"), db::Comparation::Equal, SqlQuery::Field(viewName, toString(prevScheme->getName(), "_id")))
						.where(db::Operator::And, SqlQuery::Field("d", "object"), db::Comparation::Equal, SqlQuery::Field(viewName, toString(scheme.getName(), "_id")));
			});
	}).select();

	if (!idOnly) {
		FieldResolver resv(scheme, fields);
		resv.readFields([&] (const StringView &name, const db::Field *field) {
			s.field(SqlQuery::Field("t", name));
		});
	} else {
		s.field(Field("t", "__oid"));
	}
	s.fields(Field("dv", "time").as("__d_time"), Field("dv", "object").as("__d_object"), Field("dv", "__vid"))
		.from(Field(view->scheme->getName()).as("t"))
		.rightJoinOn("dv", [&] (SqlQuery::WhereBegin &w) {
			w.where(Field("dv", "object"), Comparation::Equal, Field("t", "__oid"));
	});
}

const StringStream &SqlQuery::getQuery() const {
	return stream;
}

db::QueryInterface * SqlQuery::getInterface() const {
	return binder.getInterface();
}

void SqlQuery::writeFullTextFrom(SelectFrom &sel, const Scheme &scheme, const db::Field *f, const db::Query::Select &it) {
	auto ftsQuery = getFullTextQuery(scheme, *f, it);
	if (!ftsQuery.empty()) {
		sel.query->writeBind(db::Binder::FullTextFrom{scheme.getName(), f, ftsQuery});
	}
}

void SqlQuery::writeFullTextRank(Select &sel, const db::Scheme &scheme, const db::Query &q) {
	Set<const db::Field *> fields;
	for (auto &it : q.getSelectList()) {
		if (auto f = scheme.getField(it.field)) {
			if (f->getType() == db::Type::FullTextView) {
				if (fields.find(f) == fields.end()) {
					fields.emplace(f);
					auto ftsQuery = getFullTextQuery(scheme, *f, it);
					if (!ftsQuery.empty()) {
						if (sel.state == SqlQuery::State::Some) {
							sel.query->getStream() << ", ";
						}
						sel.query->writeBind(db::Binder::FullTextRank{scheme.getName(), f, ftsQuery});
						sel.query->getStream() << " AS __ts_rank_" << it.field;
						sel.state = SqlQuery::State::Some;
					}
				}
			}
		}
	}
}

void SqlQuery::writeFullTextWhere(WhereContinue &whi, db::Operator op, const db::Scheme &scheme, const db::Query::Select &sel, StringView ftsQuery) {
	whi.where(op, SqlQuery::Field(scheme.getName(), sel.field), Comparation::Includes, RawStringView{ftsQuery});
}

StringView SqlQuery::getFullTextQuery(const Scheme &scheme, const db::Field &f, const db::Query::Select &it) {
	if (f.getType() != Type::FullTextView) {
		return StringView();
	}

	String key = toString(scheme.getName(), ":", f.getName());

	auto fit = _fulltextQueries.find(key);
	if (fit != _fulltextQueries.end()) {
		return fit->second;
	}

	if (!it.textQuery.empty()) {
		StringStream queryFrom;
		binder.writeBind(queryFrom, Binder::FullTextQueryRef{scheme.getName(), &f, it.textQuery});
		return _fulltextQueries.emplace(sp::move(key), queryFrom.str()).first->second;
	} else if (it.value1) {
		auto d = f.getSlot<db::FieldFullTextView>();
		auto q = d->parseQuery(it.value1);
		if (!q.empty()) {
			auto &it = _parsedQueries.emplace_front(move(q));
			StringStream queryFrom;
			binder.writeBind(queryFrom, Binder::FullTextQueryRef{scheme.getName(), &f, it});
			return _fulltextQueries.emplace(sp::move(key), queryFrom.str()).first->second;
		}
	}

	return StringView();
}


SqlQuery::Context::Context(SqlQuery &sql, const Scheme &s, const Worker &w, const db::Query &q)
: FieldResolver(s, w, q) {
	_this = &sql;

	hasAltLimit = false;
	if (query->isSoftLimit()) {
		auto &field = query->getOrderField();
		auto f = scheme->getField(field);
		if (field == "__oid") {
			softLimitField = field;
		} else if (f) {
			softLimitField = f->getName();
			hasAltLimit = (f->getType() == Type::FullTextView || !f->hasFlag(Flags::Unique));
			softLimitIsFts = (f->getType() == Type::FullTextView);
		} else {
			sql._driver->getApplicationInterface()->error("SqlQuery", "Invalid soft limit field", Value(field));
		}
	}
}

StringView SqlQuery::Context::getAlt(StringView key) {
	auto f = scheme->getField(key);
	for (auto &it : query->getSelectList()) {
		if (it.field == key) {
			return _this->getFullTextQuery(*scheme, *f, it);
		}
	}
	return StringView();
}

}
