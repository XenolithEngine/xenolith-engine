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

#ifndef STAPPLER_DB_SQL_SPSQLQUERY_H_
#define STAPPLER_DB_SQL_SPSQLQUERY_H_

#include "SPDbBackendInterface.h"
#include "SPDbWorker.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

class Driver;

class SP_PUBLIC SqlQuery : public stappler::sql::Query<db::Binder, Interface> {
public:
	struct Context : FieldResolver {
		Context(SqlQuery &, const Scheme &scheme, const Worker &w, const db::Query &q);

		SqlQuery *_this;

		bool hasAltLimit = false;
		bool softLimitIsFts = false;
		StringView softLimitField;

		StringView getAlt(StringView);
	};

	using TypeString = db::Binder::TypeString;

	virtual ~SqlQuery() = default;

	SqlQuery(db::QueryInterface *, const Driver *);

	void clear();

	virtual bool writeQuery(Context &);
	virtual bool writeQuery(Context &, const db::Scheme &scheme, uint64_t, const db::Field &f);

	virtual void writeWhere(SqlQuery::SelectWhere &, db::Operator op, const db::Scheme &, const db::Query &);
	virtual void writeWhere(SqlQuery::WhereContinue &, db::Operator op, const db::Scheme &, const db::Query &);

	virtual void writeWhereItem(SqlQuery::WhereContinue &w, db::Operator op, const db::Scheme &scheme, const db::Query::Select &sel);
	virtual void writeWhereCond(SqlQuery::WhereContinue &w, db::Operator op, const db::Scheme &scheme, const db::Worker::ConditionData &sel);

	virtual void writeOrdering(SqlQuery::SelectFrom &, const db::Scheme &, const db::Query &, bool dropLimits = false);

	virtual SelectFrom writeSelectFrom(GenericQuery &q, const db::QueryList::Item &item, bool idOnly, const StringView &scheme, const StringView &field, bool isSimpleGet = false);
	virtual SelectFrom writeSelectFrom(Select &sel, Context &);

	virtual void writeQueryReqest(SqlQuery::SelectFrom &s, const db::QueryList::Item &item);
	virtual void writeQueryListItem(GenericQuery &sq, const db::QueryList &list, size_t idx, bool idOnly, const db::Field *field = nullptr, bool forSubquery = false);
	virtual void writeQueryList(const db::QueryList &query, bool idOnly, size_t count = stappler::maxOf<size_t>());
	virtual void writeQueryFile(const ApplicationInterface *app, const db::QueryList &query, const db::Field *field);
	virtual void writeQueryArray(const db::QueryList &query, const db::Field *field);

	virtual void writeQueryDelta(const db::Scheme &, const stappler::Time &, const Set<const db::Field *> &fields, bool idOnly);
	virtual void writeQueryViewDelta(const db::QueryList &list, const stappler::Time &, const Set<const db::Field *> &fields, bool idOnly);

	template <typename T>
	friend auto & operator << (SqlQuery &query, const T &value) {
		return query.stream << value;
	}

	const StringStream &getQuery() const;
	db::QueryInterface * getInterface() const;

	virtual void writeFullTextFrom(SelectFrom &sel, const Scheme &scheme, const db::Field *, const db::Query::Select &it);
	virtual void writeFullTextRank(Select &sel, const Scheme &scheme, const db::Query &q);

	virtual void writeFullTextWhere(WhereContinue &w, db::Operator op, const db::Scheme &scheme, const db::Query::Select &sel, StringView ftsQuery);

	virtual StringView getFullTextQuery(const Scheme &scheme, const db::Field &f, const db::Query::Select &it);

protected:
	const Driver *_driver = nullptr;
	std::forward_list<FullTextQuery> _parsedQueries;
	Map<String, String> _fulltextQueries;
};

}

#endif /* STAPPLER_DB_SQL_SPSQLQUERY_H_ */
