/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPDbFieldExtensions.h"
#include "SPSqlQuery.h"
#include "SPDbScheme.h"
#include "SPPqHandle.h"
#include "SPSqliteHandle.h"

namespace STAPPLER_VERSIONIZED stappler::db {

static Value readPgIntArray(BytesViewNetwork r) {
	SPUNUSED auto ndim = r.readUnsigned32();
	r.offset(4); // ignored;
	SPUNUSED auto oid = r.readUnsigned32();
	auto size = r.readUnsigned32();
	SPUNUSED auto index = r.readUnsigned32();

	if (size > 0) {
		Value ret(Value::Type::ARRAY); ret.getArray().reserve(size);
		while (!r.empty()) {
			auto width = r.readUnsigned32();
			switch (width) {
			case 1: ret.addInteger(r.readUnsigned()); break;
			case 2: ret.addInteger(r.readUnsigned16()); break;
			case 4: ret.addInteger(r.readUnsigned32()); break;
			case 8: ret.addInteger(r.readUnsigned64()); break;
			default: break;
			}
		}
		return ret;
	}
	return Value();
}

static bool writePgIntArray(StringStream &query, const Value &val) {
	if (val.isArray()) {
		query << "'{";
		bool init = true;
		for (auto &it : val.asArray()) {
			if (init) { init = false; } else { query << ","; }
			query << it.asInteger();
		}
		query << "}'";
		return true;
	}
	return false;
}

bool FieldIntArray::registerForPostgres(CustomFieldInfo &info) {
	info.isIndexable = true;
	info.typeName = "integer[]";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		return readPgIntArray(BytesViewNetwork(iface.toBytes(field)));
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		return writePgIntArray(query, val);
	};
	info.getIndexName = [] (const FieldCustom &field) {
		return toString(field.name, "_gin_int");
	};
	info.getIndexDefinition = [] (const FieldCustom &field) {
		return toString("USING GIN ( \"", field.name, "\"  gin__int_ops)");
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isInteger()) {
				whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "@>", db::sql::SqlQuery::RawString{toString("ARRAY[", val.asInteger(), ']')});
			} else if (val.isArray()) {
				StringStream str; str << "ARRAY[";
				bool init = false;
				for (auto &it : val.asArray()) {
					if (it.isInteger()) {
						if (init) { str << ","; } else { init = true; }
						str << it.getInteger();
					}
				}
				str << "]";
				if (init) {
					whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "&&", db::sql::SqlQuery::RawString{str.str()});
				}
			}
		}
	};
	return true;
}

bool FieldIntArray::registerForSqlite(CustomFieldInfo &info) {
	info.isIndexable = false;
	info.typeName = "BLOB";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto d = BytesViewNetwork(iface.toBytes(field));
		return data::read<Interface>(d);
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		auto &it = static_cast<sqlite::SqliteQueryInterface &>(iface);
		it.push(query, val, true, false);
		return true;
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isInteger()) {
				auto unwrapTable = StringView(toString(s.getName(), "_", f, "_unwrap")).pdup();
				whi.where(op, db::sql::SqlQuery::Field(unwrapTable, StringView("__unwrap_value")), "=?", Value(val));
			}
		}
	};
	info.writeFrom = [] (const FieldCustom &field, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::SelectFrom &from,
			Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
		} else {
			if (val.isString()) {
				auto name = toString("sp_unwrap(", s.getName(), ".\"", field.name, "\")");
				auto unwrapTable = toString(s.getName(), "_", field.name, "_unwrap");
				from.from(stappler::sql::Query<db::Binder, Interface>::Field(name).as(unwrapTable));
			}
		}
	};
	return true;
}

bool FieldIntArray::transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const {
	if (val.isArray()) {
		for (auto &it : val.asArray()) {
			if (!it.isInteger()) {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool FieldIntArray::isSimpleLayout() const {
	return true;
}

bool FieldBigIntArray::registerForPostgres(CustomFieldInfo &info) {
	info.isIndexable = true;
	info.typeName = "bigint[]";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		return readPgIntArray(BytesViewNetwork(iface.toBytes(field)));
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		return writePgIntArray(query, val);
	};
	info.getIndexName = [] (const FieldCustom &field) {
		return toString(field.name, "_gin_bigint");
	};
	info.getIndexDefinition = [] (const FieldCustom &field) {
		return toString("USING GIN ( \"", field.name, "\"  array_ops)");
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isInteger()) {
				whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "@>", db::sql::SqlQuery::RawString{toString("ARRAY[", val.asInteger(), "::bigint]")});
			} else if (val.isArray()) {
				StringStream str; str << "ARRAY[";
				bool init = false;
				for (auto &it : val.asArray()) {
					if (it.isInteger()) {
						if (init) { str << ","; } else { init = true; }
						str << it.getInteger() << "::bigint";
					}
				}
				str << "]";
				if (init) {
					whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "&&", db::sql::SqlQuery::RawString{str.str()});
				}
			}
		}
	};
	return true;
}

bool FieldBigIntArray::registerForSqlite(CustomFieldInfo &info) {
	info.isIndexable = false;
	info.typeName = "BLOB";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto d = BytesViewNetwork(iface.toBytes(field));
		return data::read<Interface>(d);
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		auto &it = static_cast<sqlite::SqliteQueryInterface &>(iface);
		it.push(query, val, true, false);
		return true;
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isInteger()) {
				auto unwrapTable = StringView(toString(s.getName(), "_", f, "_unwrap")).pdup();
				whi.where(op, db::sql::SqlQuery::Field(unwrapTable, StringView("__unwrap_value")), "=?", Value(val));
			}
		}
	};
	info.writeFrom = [] (const FieldCustom &field, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::SelectFrom &from,
			Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
		} else {
			if (val.isString()) {
				auto name = toString("sp_unwrap(", s.getName(), ".\"", field.name, "\")");
				auto unwrapTable = toString(s.getName(), "_", field.name, "_unwrap");
				from.from(stappler::sql::Query<db::Binder, Interface>::Field(name).as(unwrapTable));
			}
		}
	};
	return true;
}

bool FieldBigIntArray::transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const {
	if (val.isArray()) {
		for (auto &it : val.asArray()) {
			if (!it.isInteger()) {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool FieldBigIntArray::isSimpleLayout() const {
	return true;
}

bool FieldPoint::registerForPostgres(CustomFieldInfo &info) {
	info.isIndexable = true;
	info.typeName = "point";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto r = stappler::BytesViewNetwork(iface.toBytes(field));

		if (r.size() == 16) {
			auto x = r.readFloat64();
			auto y = r.readFloat64();

			return Value({
				Value(x),
				Value(y),
			});
		}
		return Value();
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		if (val.isArray() && val.size() == 2 && val.isDouble(0) && val.isDouble(1)) {
			query << std::setprecision(std::numeric_limits<double>::max_digits10) << "point(" << val.getDouble(0) << "," << val.getDouble(1) << ")";
			return true;
		}
		return false;
	};
	info.getIndexName = [] (const FieldCustom &field) {
		return toString(field.name, "_gist_point");
	};
	info.getIndexDefinition = [] (const FieldCustom &field) {
		return toString("USING GIST( \"", field.name, "\")");
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		return c == db::Comparation::Includes || c == db::Comparation::Equal || c == db::Comparation::In;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (val.isArray() && val.size() == 4) {
			if (whi.state == stappler::sql::Query<db::Binder, Interface>::State::None) {
				whi.state = stappler::sql::Query<db::Binder, Interface>::State::Some;
			} else {
				Query_writeOperator(whi.query->getStream(), op);
			}
			auto &stream = whi.query->getStream();
			stream << "(" << s.getName() << ".\"" << f << "\" <@ box '("
				<< std::setprecision(std::numeric_limits<double>::max_digits10)
				<< val.getDouble(0) << "," << val.getDouble(1) << "),(" << val.getDouble(2) << "," << val.getDouble(3) << ")')";
		}
	};
	return true;
}

bool FieldPoint::registerForSqlite(CustomFieldInfo &info) {
	info.isIndexable = false;
	info.typeName = "BLOB";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto d = BytesViewNetwork(iface.toBytes(field));
		return data::read<Interface>(d);
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		auto &it = static_cast<sqlite::SqliteQueryInterface &>(iface);
		it.push(query, val, true, false);
		return true;
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) { };
	return true;
}

bool FieldPoint::transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const {
	if (val.isArray() && val.size() == 2 && val.isDouble(0) && val.isDouble(1)) {
		return true;
	}
	return false;
}

bool FieldPoint::isSimpleLayout() const {
	return true;
}

bool FieldTextArray::registerForPostgres(CustomFieldInfo &info) {
	info.isIndexable = true;
	info.typeName = "text[]";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto r = stappler::BytesViewNetwork(iface.toBytes(field));
		SPUNUSED auto ndim = r.readUnsigned32();
		r.offset(4); // ignored;
		SPUNUSED auto oid = r.readUnsigned32();
		auto size = r.readUnsigned32();
		SPUNUSED auto index = r.readUnsigned32();

		if (size > 0) {
			Value ret(Value::Type::ARRAY); ret.getArray().reserve(size);
			while (!r.empty()) {
				auto size = r.readUnsigned32();
				auto str = r.readString(size);

				ret.addString(str);
			}
			return ret;
		}
		return Value();
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		if (val.isArray()) {
			query << "ARRAY[";
			bool init = true;
			for (auto &it : val.asArray()) {
				if (init) { init = false; } else { query << ","; }
				if (auto q = static_cast<db::pq::PgQueryInterface *>(&iface)) {
					q->push(query, it, false, false);
				}
			}
			query << "]";
			return true;
		}
		return false;
	};
	info.getIndexName = [] (const FieldCustom &field) {
		return toString(field.name, "_gin_text");
	};
	info.getIndexDefinition = [] (const FieldCustom &field) {
		return toString("USING GIN ( \"", field.name, "\"  array_ops)");
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isString()) {
				if (auto q = static_cast<db::pq::PgQueryInterface *>(whi.query->getBinder().getInterface())) {
					auto id = q->push(val.asString());
					whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "@>", db::sql::SqlQuery::RawString{toString("ARRAY[$", id, "::text]")});
				}
			} else if (val.isArray()) {
				if (auto q = static_cast<db::pq::PgQueryInterface *>(whi.query->getBinder().getInterface())) {
					StringStream str; str << "ARRAY[";
					bool init = false;
					for (auto &it : val.asArray()) {
						if (it.isInteger()) {
							if (init) { str << ","; } else { init = true; }
							str << "$" << q->push(val.asString()) << "::text";
						}
					}
					str << "]";
					if (init) {
						whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), "&&", db::sql::SqlQuery::RawString{str.str()});
					}
				}
			}
		}
	};
	return true;
}

bool FieldTextArray::registerForSqlite(CustomFieldInfo &info) {
	info.isIndexable = false;
	info.typeName = "BLOB";
	info.readFromStorage = [] (const FieldCustom &, const ResultCursor &iface, size_t field) -> Value {
		auto d = BytesViewNetwork(iface.toBytes(field));
		return data::read<Interface>(d);
	};
	info.writeToStorage = [] (const FieldCustom &, QueryInterface &iface, StringStream &query, const Value &val) -> bool {
		auto &it = static_cast<sqlite::SqliteQueryInterface &>(iface);
		it.push(query, val, true, false);
		return true;
	};
	info.isComparationAllowed = [] (const FieldCustom &, Comparation c) {
		switch (c) {
		case db::Comparation::Includes:
		case db::Comparation::Equal:
		case db::Comparation::IsNotNull:
		case db::Comparation::IsNull:
			return true;
			break;
		default:
			break;
		}
		return false;
	};
	info.writeQuery = [] (const FieldCustom &field, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::WhereContinue &whi,
			Operator op, const StringView &f, Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
			whi.where(op, db::sql::SqlQuery::Field(s.getName(), f), cmp, val);
		} else {
			if (val.isString()) {
				if (auto q = static_cast<db::sqlite::SqliteQueryInterface *>(whi.query->getBinder().getInterface())) {
					auto id = q->push(val.asString());
					auto unwrapTable = toString(s.getName(), "_", f, "_unwrap");
					whi.where(op, db::sql::SqlQuery::Field(unwrapTable, StringView("__unwrap_value")), "=?", Value(id));
				}
			}
		}
	};
	info.writeFrom = [] (const FieldCustom &field, const Scheme &s, stappler::sql::Query<db::Binder, Interface>::SelectFrom &from,
			Comparation cmp, const Value &val, const Value &) {
		if (cmp == db::Comparation::IsNull || cmp == db::Comparation::IsNotNull) {
		} else {
			if (val.isString()) {
				auto name = toString("sp_unwrap(", s.getName(), ".\"", field.name, "\")");
				auto unwrapTable = toString(s.getName(), "_", field.name, "_unwrap");
				from.from(stappler::sql::Query<db::Binder, Interface>::Field(name).as(unwrapTable));
			}
		}
	};
	return true;
}

bool FieldTextArray::transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const {
	if (val.isArray()) {
		for (auto &it : val.asArray()) {
			if (!it.isString()) {
				auto str = it.asString();
				if (!str.empty()) {
					it = Value(str);
				} else {
					return false;
				}
			}
		}
		return true;
	}
	return false;
}

bool FieldTextArray::isSimpleLayout() const {
	return true;
}

}
