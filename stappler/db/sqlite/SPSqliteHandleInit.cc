/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPSqliteHandle.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

constexpr static uint32_t getDefaultFunctionVersion() { return 10; }

static BackendInterface::StorageType getStorageType(StringView type) {
	if (type == "BIGINT") {
		return BackendInterface::StorageType::Int8;
	} else if (type == "INT" || type == "INTEGER") {
		return BackendInterface::StorageType::Int4;
	} else if (type == "NUMERIC") {
		return BackendInterface::StorageType::Numeric;
	} else if (type == "BOOLEAN") {
		return BackendInterface::StorageType::Bool;
	} else if (type == "BLOB") {
		return BackendInterface::StorageType::Bytes;
	} else if (type == "TEXT") {
		return BackendInterface::StorageType::Text;
	} else if (type == "REAL" || type == "DOUBLE") {
		return BackendInterface::StorageType::Float8;
	}
	return BackendInterface::StorageType::Unknown;
}

struct ColRec {
	using Type = BackendInterface::StorageType;

	enum Flags : uint32_t {
		None,
		IsNotNull = 1 << 0,
		PrimaryKey = 1 << 1
	};

	Type type = Type::Unknown;
	String custom;
	Flags flags = Flags::None;

	bool isNotNull() const;

	ColRec(Type t, Flags flags = Flags::None) : type(t), flags(flags) { }
	ColRec(const StringView &t, Flags flags = Flags::None)
	: custom(t.str<Interface>()), flags(flags) {
		type = getStorageType(custom);
	}
};

SP_DEFINE_ENUM_AS_MASK(ColRec::Flags)

struct IndexRec {
	Vector<String> fields;
	bool unique = false;

	IndexRec(StringView str, bool unique = false)
	: fields(Vector<String>({str.str<Interface>()})), unique(unique) { }
	IndexRec(Vector<String> &&fields, bool unique = false)
	: fields(sp::move(fields)), unique(unique) { }
};

struct TriggerRec {
	enum Type {
		Delete,
		Update,
		Insert
	};

	enum Bind {
		Before,
		After
	};

	Type type = Type::Delete;
	Bind bind = Bind::Before;
	const Field *rootField = nullptr;
	const Scheme *rootScheme = nullptr;
	String sourceTable;
	String sourceField;
	String targetTable;
	String targetField;
	String tagField;
	RemovePolicy onRemove = RemovePolicy::Null;

	TriggerRec(StringView def) {
		uint32_t valueIdx = 0;
		while (!def.empty()) {
			auto value = def.readUntil<StringView::Chars<':'>>();
			switch (valueIdx) {
			case 0:
				if (value == "BEFORE") {
					bind = Before;
				} else if (value == "AFTER") {
					bind = After;
				} else {
					return;
				}
				break;
			case 1:
				if (value == "DELETE") {
					type = Delete;
				} else if (value == "UPDATE") {
					type = Update;
				} else if (value == "INSERT") {
					type = Insert;
				} else {
					return;
				}
				break;
			case 2: {
				auto table = value.readUntil<StringView::Chars<'@'>>();
				if (value.is('@')) {
					++value;
					sourceTable = table.str<Interface>();
					sourceField = value.str<Interface>();
				} else {
					return;
				}
				break;
			}
			case 3: {
				auto table = value.readUntil<StringView::Chars<'@'>>();
				if (value.is('@')) {
					++value;
					targetTable = table.str<Interface>();
					targetField = value.str<Interface>();
				} else {
					return;
				}
				break;
			}
			case 4:
				if (value == "CASCADE") {
					onRemove = RemovePolicy::Cascade;
				} else if (value == "RESTRICT") {
					onRemove = RemovePolicy::Restrict;
				} else if (value == "REF") {
					onRemove = RemovePolicy::Reference;
				} else if (value == "SREF") {
					onRemove = RemovePolicy::StrongReference;
				} else {
					return;
				}
				break;
			default: break;
			}
			if (def.is(':')) {
				++def;
				++valueIdx;
			}
		}
	}

	TriggerRec(Type t, Bind b, StringView sourceTable, StringView sourceField,
			StringView targetTable, StringView targetField, const Field *f = nullptr)
	: type(t)
	, bind(b)
	, rootField(f)
	, sourceTable(sourceTable.str<Interface>())
	, sourceField(sourceField.str<Interface>())
	, targetTable(targetTable.str<Interface>())
	, targetField(targetField.str<Interface>()) { }

	String makeName() const {
		StringStream stream;
		switch (bind) {
		case Bind::Before: stream << "ST_TRIGGER:BEFORE:"; break;
		case Bind::After: stream << "ST_TRIGGER:AFTER:"; break;
		}
		switch (type) {
		case Type::Delete: stream << "DELETE:"; break;
		case Type::Update: stream << "UPDATE:"; break;
		case Type::Insert: stream << "INSERT:"; break;
		}
		stream << sourceTable << "@" << sourceField << ":";
		stream << targetTable << "@" << targetField;
		switch (onRemove) {
		case RemovePolicy::Null: break;
		case RemovePolicy::Cascade: stream << ":CASCADE"; break;
		case RemovePolicy::Restrict: stream << ":RESTRICT"; break;
		case RemovePolicy::Reference: stream << ":REF"; break;
		case RemovePolicy::StrongReference: stream << ":SREF"; break;
		}
		return stream.str();
	}
};

struct TableRec {
	using Scheme = db::Scheme;

	static Map<StringView, TableRec> parse(const Driver *driver,
			const BackendInterface::Config &cfg, const Map<StringView, const Scheme *> &s);
	static Map<StringView, TableRec> get(Handle &h, StringStream &stream);

	static void writeCompareResult(Handle &h, StringStream &stream,
			Map<StringView, TableRec> &required, Map<StringView, TableRec> &existed,
			const Map<StringView, const db::Scheme *> &s);

	TableRec();
	TableRec(const Driver *driver, const BackendInterface::Config &cfg, const db::Scheme *scheme);

	Map<String, ColRec> cols;
	Map<String, IndexRec> indexes;
	Map<String, TriggerRec> triggers;
	uint32_t version = 0;
	bool exists = false;
	bool valid = false;
	bool withOids = false;
	bool detached = false;

	const db::Scheme *viewScheme = nullptr;
	const db::FieldView *viewField = nullptr;
};

static StringView getStorageTypeName(BackendInterface::StorageType type, StringView custom) {
	switch (type) {
	case ColRec::Type::Unknown: return custom; break;
	case ColRec::Type::Bool: return "BOOLEAN"; break;
	case ColRec::Type::Float4: return "DOUBLE"; break;
	case ColRec::Type::Float8: return "DOUBLE"; break;
	case ColRec::Type::Int2: return "INT"; break;
	case ColRec::Type::Int4: return "INT"; break;
	case ColRec::Type::Int8: return "BIGINT"; break;
	case ColRec::Type::Text: return "TEXT"; break;
	case ColRec::Type::VarChar: return "TEXT"; break;
	case ColRec::Type::Numeric: return "NUMERIC"; break;
	case ColRec::Type::Bytes: return "BLOB"; break;
	default: break;
	}
	return StringView();
}

bool ColRec::isNotNull() const { return (flags & Flags::IsNotNull) != Flags::None; }

void TableRec::writeCompareResult(Handle &h, StringStream &outstream,
		Map<StringView, TableRec> &required, Map<StringView, TableRec> &existed,
		const Map<StringView, const db::Scheme *> &s) {

	auto writeTriggerHeader = [&](StringView name, TriggerRec &t, StringView updateField) {
		outstream << "CREATE TRIGGER IF NOT EXISTS \"" << name << "\"";
		switch (t.bind) {
		case TriggerRec::Before: outstream << " BEFORE"; break;
		case TriggerRec::After: outstream << " AFTER"; break;
		}
		switch (t.type) {
		case TriggerRec::Delete: outstream << " DELETE"; break;
		case TriggerRec::Update:
			outstream << " UPDATE";
			if (!updateField.empty()) {
				outstream << " OF \"" << updateField << "\"";
			}
			break;
		case TriggerRec::Insert: outstream << " INSERT"; break;
		}
		outstream << " ON \"" << t.sourceTable << "\" FOR EACH ROW";
	};

	auto writeTrigger = [&](StringView name, TriggerRec &t) {
		if (t.rootField) {
			switch (t.rootField->getType()) {
			case Type::Array:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN\n\tDELETE FROM \"" << t.targetTable << "\"" " WHERE \""
						  << t.targetTable << "\".\"" << t.targetField << "\"=OLD.__oid;\nEND;\n";
				break;
			case Type::File:
			case Type::Image:
				writeTriggerHeader(name, t, StringView());
				switch (t.type) {
				case TriggerRec::Delete:
					outstream << " WHEN OLD.\"" << t.sourceField << "\" IS NOT NULL BEGIN"
							"\n\tINSERT OR IGNORE INTO __removed (__oid) VALUES (OLD.\"" << t.sourceField << "\");\nEND;\n";
					break;
				case TriggerRec::Update:
					outstream << " WHEN OLD.\"" << t.sourceField << "\" IS NOT NULL BEGIN"
							"\n\tINSERT OR IGNORE INTO __removed (__oid) VALUES (OLD.\"" << t.sourceField << "\");\nEND;\n";
					break;
				default: break;
				}
				break;
			case Type::Set:
				switch (static_cast<const db::FieldObject *>(t.rootField->getSlot())->onRemove) {
				case RemovePolicy::Reference:
				case RemovePolicy::StrongReference:
					writeTriggerHeader(name, t, StringView());
					outstream << " BEGIN\n\tDELETE FROM \"" << t.targetTable << "\" WHERE \""
							  << t.targetTable << "\".\"" << t.targetField << "\"=OLD.\""
							  << t.sourceField << "\";\nEND;\n";
					break;
				default: break;
				}
				break;
			case Type::View:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN\n\tDELETE FROM \"" << t.targetTable << "\" WHERE \""
						  << t.targetTable << "\".\"" << t.targetField << "\"=OLD.\""
						  << t.sourceField << "\";\nEND;\n";
				break;
			case Type::Object:
				switch (t.onRemove) {
				case RemovePolicy::Cascade:
					writeTriggerHeader(name, t, StringView());
					outstream << " BEGIN\n\tDELETE FROM \"" << t.targetTable << "\" WHERE \""
							  << t.targetTable << "\".\"" << t.targetField << "\"=OLD.\""
							  << t.sourceField << "\";\nEND;\n";
					break;
				case RemovePolicy::Restrict:
					writeTriggerHeader(name, t, StringView());
					outstream << " BEGIN\n\tSELECT RAISE(ABORT, 'Restrict constraint failed on "
							  << t.targetTable << "." << t.targetField << "' FROM \""
							  << t.targetTable << "\" WHERE \"" << t.targetTable << "\".\""
							  << t.targetField << "\"=OLD.\"" << t.sourceField << "\";\nEND;\n";
					break;
				case RemovePolicy::Null:
				case RemovePolicy::Reference:
					writeTriggerHeader(name, t, StringView());
					outstream << " BEGIN\n\tUPDATE \"" << t.targetTable << "\" SET \""
							  << t.targetField << "\"=NULL WHERE \"" << t.targetTable << "\".\""
							  << t.targetField << "\"=OLD.\"" << t.sourceField << "\";\nEND;\n";
					break;
				case RemovePolicy::StrongReference:
					// Reverse trigger
					switch (t.type) {
					case TriggerRec::Delete:
						writeTriggerHeader(name, t, StringView());
						outstream << " BEGIN\n\tDELETE FROM \"" << t.targetTable << "\" WHERE \""
								  << t.targetTable << "\".\"" << t.targetField << "\"=OLD.\""
								  << t.sourceField << "\";\nEND;\n";
						break;
					case TriggerRec::Update:
						writeTriggerHeader(name, t, t.sourceField);
						outstream << " WHEN OLD.\"" << t.sourceField
								  << "\" IS NOT NULL BEGIN" "\n\tDELETE FROM \"" << t.targetTable
								  << "\" WHERE \"" << t.targetTable << "\".\"" << t.targetField
								  << "\"=OLD.\"" << t.sourceField << "\";\nEND;\n";
						break;
					default: break;
					}
					break;
				}
				break;
			case Type::FullTextView:
				writeTriggerHeader(name, t, t.sourceField);
				switch (t.type) {
				case TriggerRec::Delete:
					outstream << " WHEN OLD.\"" << t.sourceField
							  << "\" IS NOT NULL BEGIN\n" "\tSELECT sp_ts_update(OLD.__oid, OLD.\""
							  << t.sourceField << "\", " "'" << t.sourceTable << "', '"
							  << t.sourceField << "', '" << t.targetTable << "', 2);" "\nEND;\n";
					break;
				case TriggerRec::Update:
					outstream << " BEGIN\n" "\tSELECT sp_ts_update(OLD.__oid, NEW.\""
							  << t.sourceField << "\", " "'" << t.sourceTable << "', '"
							  << t.sourceField << "', '" << t.targetTable << "', 1);" "\nEND;\n";
					break;
				case TriggerRec::Insert:
					outstream << " WHEN NEW.\"" << t.sourceField
							  << "\" IS NOT NULL BEGIN\n" "\tSELECT sp_ts_update(NEW.__oid, NEW.\""
							  << t.sourceField << "\", " "'" << t.sourceTable << "', '"
							  << t.sourceField << "', '" << t.targetTable << "', 0);" "\nEND;\n";
					break;
				}
				break;
			default: break;
			}
		} else if (t.rootScheme && t.rootScheme->hasDelta()) {
			switch (t.type) {
			case TriggerRec::Delete:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"object\",\"action\",\"time\",\"user\") VALUES" "(OLD.__oid,"
						  << stappler::toInt(DeltaAction::Delete)
						  << ",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			case TriggerRec::Update:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"object\",\"action\",\"time\",\"user\") VALUES" "(NEW.__oid,"
						  << stappler::toInt(DeltaAction::Update)
						  << ",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			case TriggerRec::Insert:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"object\",\"action\",\"time\",\"user\") VALUES" "(NEW.__oid,"
						  << stappler::toInt(DeltaAction::Create)
						  << ",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			}
		} else if (t.sourceField == "__delta") {
			switch (t.type) {
			case TriggerRec::Delete:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"tag\",\"object\",\"time\",\"user\") VALUES" "(OLD.\"" << t.tagField
						  << "\",OLD.\"" << t.targetField
						  << "\",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			case TriggerRec::Update:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"tag\",\"object\",\"time\",\"user\") VALUES" "(NEW.\"" << t.tagField
						  << "\",NEW.\"" << t.targetField
						  << "\",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			case TriggerRec::Insert:
				writeTriggerHeader(name, t, StringView());
				outstream << " BEGIN" "\n\tINSERT INTO " << t.targetTable
						  << "(\"tag\",\"object\",\"time\",\"user\") VALUES" "(NEW.\"" << t.tagField
						  << "\",NEW.\"" << t.targetField
						  << "\",sp_sqlite_now(),sp_sqlite_user());" "\nEND;\n";
				break;
			}
		}
	};

	for (auto &ex_it : existed) {
		auto req_it = required.find(ex_it.first);
		if (req_it != required.end()) {
			if (ex_it.second.version > req_it->second.version) {
				continue;
			}

			bool updated = false;
			auto &req_t = req_it->second;
			auto &ex_t = ex_it.second;
			req_t.exists = true;

			for (auto &ex_idx_it : ex_t.indexes) {
				auto req_idx_it = req_t.indexes.find(ex_idx_it.first);
				if (req_idx_it == req_t.indexes.end()) {
					// index is not required any more, drop it
					updated = true;
					outstream << "DROP INDEX IF EXISTS \"" << ex_idx_it.first << "\";\n";
				} else {
					req_t.indexes.erase(req_idx_it);
				}
			}

			for (auto &ex_col_it : ex_t.cols) {
				if (ex_col_it.first == "__oid") {
					continue;
				}

				auto req_col_it = req_t.cols.find(ex_col_it.first);
				if (req_col_it == req_t.cols.end()) {
					updated = true;
					outstream << "ALTER TABLE \"" << ex_it.first << "\" DROP COLUMN \""
							  << ex_col_it.first << "\";\n";
				} else {
					auto &req_col = req_col_it->second;
					auto &ex_col = ex_col_it.second;

					auto req_type = req_col.type;

					if (req_type != ex_col.type) {
						updated = true;
						outstream << "ALTER TABLE \"" << ex_it.first << "\" DROP COLUMN \""
								  << ex_col_it.first << "\";\n";
					} else if (ex_col.type == ColRec::Type::Unknown
							&& req_type == ColRec::Type::Unknown
							&& ex_col.custom != req_col.custom) {
						updated = true;
						outstream << "ALTER TABLE \"" << ex_it.first << "\" DROP COLUMN \""
								  << ex_col_it.first << "\";\n";
					} else {
						req_t.cols.erase(req_col_it);
					}
				}
			}

			for (auto &ex_tgr_it : ex_t.triggers) {
				auto req_tgr_it = req_t.triggers.find(ex_tgr_it.first);
				if (req_tgr_it == req_t.triggers.end()) {
					updated = true;
					outstream << "DROP TRIGGER IF EXISTS \"" << ex_tgr_it.first << "\";\n";
				} else {
					req_t.triggers.erase(ex_tgr_it.first);
				}
			}

			if (updated) {
				outstream << "INSERT INTO __versions(name,version) VALUES('" << ex_it.first << "',"
						  << ex_t.version << ")"
						  << " ON CONFLICT(name) DO UPDATE SET version = EXCLUDED.version;\n";
			}
		}
	}

	// write table structs
	for (auto &it : required) {
		auto &t = it.second;
		if (!it.second.exists) {
			outstream << "CREATE TABLE IF NOT EXISTS \"" << it.first << "\" (\n";

			bool first = true;
			if (it.second.withOids) {
				first = false;
				if (it.second.detached) {
					outstream << "\t\"__oid\" INTEGER PRIMARY KEY AUTOINCREMENT";
				} else {
					outstream << "\t\"__oid\" INTEGER DEFAULT (sp_sqlite_next_oid())";
				}
			}

			for (auto cit = t.cols.begin(); cit != t.cols.end(); cit++) {
				if (first) {
					first = false;
				} else {
					outstream << ",\n";
				}
				outstream << "\t\"" << cit->first << "\" "
						  << getStorageTypeName(cit->second.type, cit->second.custom);

				if ((cit->second.flags & ColRec::IsNotNull) != ColRec::None) {
					outstream << " NOT NULL";
				}

				if (!it.second.withOids
						&& (cit->second.flags & ColRec::PrimaryKey) != ColRec::Flags::None) {
					outstream << " PRIMARY KEY";
				}
			}

			outstream << "\n);\n";
		} else {
			for (auto cit : t.cols) {
				if (cit.first != "__oid") {
					outstream << "ALTER TABLE \"" << it.first << "\" ADD COLUMN \"" << cit.first
							  << "\" " << getStorageTypeName(cit.second.type, cit.second.custom);
					if ((cit.second.flags & ColRec::Flags::IsNotNull) != ColRec::Flags::None) {
						outstream << " NOT NULL";
					}
					outstream << ";\n";
				}
			}
		}

		outstream << "INSERT INTO __versions(name,version) VALUES('" << it.first << "',"
				  << it.second.version << ")"
				  << " ON CONFLICT(name) DO UPDATE SET version = EXCLUDED.version;\n";
	}

	// indexes
	for (auto &it : required) {
		for (auto &cit : it.second.indexes) {
			outstream << "CREATE";
			if (cit.second.unique) {
				outstream << " UNIQUE";
			}
			outstream << " INDEX IF NOT EXISTS \"" << cit.first << "\" ON \"" << it.first << "\"";
			if (cit.second.fields.size() == 1 && cit.second.fields.front().back() == ')') {
				outstream << " " << cit.second.fields.front() << ";\n";
			} else {
				outstream << " (";
				bool first = true;
				for (auto &field : cit.second.fields) {
					if (first) {
						first = false;
					} else {
						outstream << ",";
					}
					outstream << "\"" << field << "\"";
				}
				outstream << ");\n";
			}
		}

		if (!it.second.triggers.empty()) {
			for (auto &tit : it.second.triggers) { writeTrigger(tit.first, tit.second); }

			/*auto scheme_it = s.find(it.first);
			if (scheme_it != s.end()) {
				for (auto & tit : it.second.triggers) {
					if (StringView(tit).starts_with("_tr_a_")) {
						writeAfterTrigger(stream, scheme_it->second, tit);
					} else {
						writeBeforeTrigger(stream, scheme_it->second, tit);
					}
				}
			} else if (it.second.viewField) {
				for (auto & tit : it.second.triggers) {
					writeDeltaTrigger(stream, it.first, it.second, tit);
				}
			}*/
		}
	}
}

Map<StringView, TableRec> TableRec::parse(const Driver *driver, const BackendInterface::Config &cfg,
		const Map<StringView, const db::Scheme *> &s) {
	Map<StringView, TableRec> tables;
	for (auto &it : s) {
		auto scheme = it.second;
		tables.emplace(scheme->getName(), TableRec(driver, cfg, scheme));
	}

	for (auto &it : s) {
		auto scheme = it.second;
		auto tableIt = tables.find(scheme->getName());
		if (tableIt == tables.end()) {
			continue;
		}
		auto schemeTable = &tableIt->second;

		// check for extra tables
		for (auto &fit : scheme->getFields()) {
			auto &f = fit.second;
			auto type = fit.second.getType();

			switch (type) {
			case db::Type::Set: {
				auto ref = static_cast<const db::FieldObject *>(f.getSlot());
				if (ref->onRemove == db::RemovePolicy::Reference
						|| ref->onRemove == db::RemovePolicy::StrongReference) {
					// create many-to-many table link
					String name = toString(it.first, "_f_", fit.first);
					auto &source = it.first;
					auto target = ref->scheme->getName();
					TableRec table;
					table.cols.emplace(toString(source, "_id"),
							ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
					table.cols.emplace(toString(target, "_id"),
							ColRec(ColRec::Type::Int8, ColRec::IsNotNull));

					table.indexes.emplace(toString(name, "_idx_", source), toString(source, "_id"));
					table.indexes.emplace(toString(name, "_idx_", target), toString(target, "_id"));

					auto extraTable =
							&tables.emplace(StringView(name).pdup(), sp::move(table)).first->second;

					do {
						TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before, source, "__oid",
								name, toString(source, "_id"));
						trigger.rootField = &fit.second;
						auto triggerName = trigger.makeName();
						schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
					} while (0);

					do {
						auto targetIt = tables.find(target);
						if (targetIt != tables.end()) {
							TriggerRec trigger(TriggerRec::Delete, TriggerRec::After, target,
									"__oid", name, toString(target, "_id"));
							trigger.rootField = &fit.second;
							trigger.rootScheme = scheme;
							auto triggerName = trigger.makeName();
							targetIt->second.triggers.emplace(sp::move(triggerName),
									sp::move(trigger));
						}

						if (ref->onRemove == db::RemovePolicy::StrongReference
								&& targetIt != tables.end()) {
							TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before, name,
									toString(target, "_id"), target, "__oid");
							trigger.rootField = &fit.second;
							trigger.rootScheme = scheme;
							auto triggerName = trigger.makeName();
							extraTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
						}
					} while (0);
				}
				break;
			}
			case db::Type::Object: {
				auto ref = static_cast<const db::FieldObject *>(f.getSlot());
				auto targetIt = tables.find(ref->scheme->getName());
				if (targetIt != tables.end()) {
					TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before,
							ref->scheme->getName(), "__oid", scheme->getName(),
							fit.second.getName());
					trigger.rootField = &fit.second;
					trigger.rootScheme = scheme;
					trigger.onRemove = ref->onRemove;
					if (ref->onRemove == RemovePolicy::StrongReference) {
						trigger.onRemove =
								RemovePolicy::Reference; // make trigger to remove just reference
					}

					auto triggerName = trigger.makeName();
					targetIt->second.triggers.emplace(sp::move(triggerName), sp::move(trigger));

					if (ref->onRemove == RemovePolicy::StrongReference) {
						// make reverse-trigger to remove object with strong reference
						do {
							TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before,
									scheme->getName(), fit.second.getName(), ref->scheme->getName(),
									"__oid");
							trigger.rootField = &fit.second;
							trigger.rootScheme = scheme;
							trigger.onRemove = ref->onRemove;

							auto triggerName = trigger.makeName();
							schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
						} while (0);

						do {
							TriggerRec trigger(TriggerRec::Update, TriggerRec::Before,
									scheme->getName(), fit.second.getName(), ref->scheme->getName(),
									"__oid");
							trigger.rootField = &fit.second;
							trigger.rootScheme = scheme;
							trigger.onRemove = ref->onRemove;

							auto triggerName = trigger.makeName();
							schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
						} while (0);
					}
				}
				break;
			}
			case db::Type::Array: {
				auto slot = static_cast<const db::FieldArray *>(f.getSlot());
				if (slot->tfield && slot->tfield.isSimpleLayout()) {

					String name = toString(it.first, "_f_", fit.first);
					auto &source = it.first;

					auto sourceFieldName = toString(source, "_id");

					TableRec table;
					table.cols.emplace(sourceFieldName, ColRec(ColRec::Type::Int8));

					auto type = slot->tfield.getType();
					switch (type) {
					case db::Type::Float:
						table.cols.emplace("data", ColRec(ColRec::Type::Float8));
						break;
					case db::Type::Boolean:
						table.cols.emplace("data", ColRec(ColRec::Type::Bool));
						break;
					case db::Type::Text:
						table.cols.emplace("data", ColRec(ColRec::Type::Text));
						break;
					case db::Type::Integer:
						table.cols.emplace("data", ColRec(ColRec::Type::Int8));
						break;
					case db::Type::Data:
					case db::Type::Bytes:
					case db::Type::Extra:
						table.cols.emplace("data", ColRec(ColRec::Type::Bytes));
						break;
					default: break;
					}

					table.indexes.emplace(toString(name, "_idx_", source), toString(source, "_id"));
					if (f.hasFlag(db::Flags::Unique)) {
						table.indexes.emplace(toString(name, "_uidx_data"),
								IndexRec(toString("data"), true));
					}

					tables.emplace(StringView(name).pdup(), sp::move(table));

					TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before, scheme->getName(),
							f.getName(), name, sourceFieldName);
					trigger.rootField = &f;
					auto triggerName = trigger.makeName();

					schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
				}
				break;
			}
			case db::Type::View: {
				auto slot = static_cast<const db::FieldView *>(f.getSlot());

				String viewName = toString(it.first, "_f_", fit.first, "_view");
				auto &source = it.first;
				auto target = slot->scheme->getName();

				TableRec table;
				table.viewScheme = it.second;
				table.viewField = slot;
				table.cols.emplace("__vid", ColRec(ColRec::Type::Int8, ColRec::PrimaryKey));
				table.cols.emplace(toString(source, "_id"),
						ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
				table.cols.emplace(toString(target, "_id"),
						ColRec(ColRec::Type::Int8, ColRec::IsNotNull));

				do {
					TriggerRec trigger(TriggerRec::Delete, TriggerRec::Before, source, "__oid",
							viewName, toString(source, "_id"));
					trigger.rootField = &fit.second;
					auto triggerName = trigger.makeName();
					schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
				} while (0);

				do {
					auto targetIt = tables.find(target);
					if (targetIt != tables.end()) {
						TriggerRec trigger(TriggerRec::Delete, TriggerRec::After, target, "__oid",
								viewName, toString(target, "_id"));
						trigger.rootField = &fit.second;
						trigger.rootScheme = scheme;
						auto triggerName = trigger.makeName();
						targetIt->second.triggers.emplace(sp::move(triggerName), sp::move(trigger));
					}
				} while (0);

				table.indexes.emplace(toString(viewName, "_idx_", source), toString(source, "_id"));
				table.indexes.emplace(toString(viewName, "_idx_", target), toString(target, "_id"));

				auto tblIt = tables.emplace(StringView(viewName).pdup(), sp::move(table)).first;

				if (slot->delta) {
					String deltaName = toString(it.first, "_f_", fit.first, "_delta");
					table.cols.emplace("id", ColRec(ColRec::Type::Int8, ColRec::PrimaryKey));
					table.cols.emplace("tag", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
					table.cols.emplace("object", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
					table.cols.emplace("time", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
					table.cols.emplace("user", ColRec(ColRec::Type::Int8));

					table.indexes.emplace(deltaName + "_idx_tag", "tag");
					table.indexes.emplace(deltaName + "_idx_object", "object");
					table.indexes.emplace(deltaName + "_idx_time", "time");

					do {
						TriggerRec trigger(TriggerRec::Insert, TriggerRec::After, deltaName,
								"__delta", deltaName, toString(target, "_id"));
						trigger.tagField = toString(source, "_id");
						auto triggerName = trigger.makeName();
						tblIt->second.triggers.emplace(sp::move(triggerName), sp::move(trigger));
					} while (0);

					do {
						TriggerRec trigger(TriggerRec::Update, TriggerRec::After, deltaName,
								"__delta", deltaName, toString(target, "_id"));
						trigger.tagField = toString(source, "_id");
						auto triggerName = trigger.makeName();
						tblIt->second.triggers.emplace(sp::move(triggerName), sp::move(trigger));
					} while (0);

					do {
						TriggerRec trigger(TriggerRec::Delete, TriggerRec::After, deltaName,
								"__delta", deltaName, toString(target, "_id"));
						trigger.tagField = toString(source, "_id");
						auto triggerName = trigger.makeName();
						tblIt->second.triggers.emplace(sp::move(triggerName), sp::move(trigger));
					} while (0);

					tables.emplace(StringView(deltaName).pdup(), sp::move(table));
				}
				break;
			}
			case db::Type::FullTextView: {
				String name = toString(it.first, "_f_", fit.first);

				auto &source = it.first;
				auto sourceFieldName = toString(source, "_id");

				TableRec table;
				table.cols.emplace(sourceFieldName, ColRec(ColRec::Type::Int8));
				table.cols.emplace("word", ColRec(ColRec::Type::Int8));

				table.indexes.emplace(toString(name, "_idx_word"), "word");
				tables.emplace(StringView(name).pdup(), sp::move(table));

				do {
					TriggerRec trigger(TriggerRec::Insert, TriggerRec::After, it.first, fit.first,
							name, sourceFieldName, &fit.second);
					trigger.tagField = toString(source, "_id");
					auto triggerName = trigger.makeName();
					schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
				} while (0);

				do {
					TriggerRec trigger(TriggerRec::Update, TriggerRec::After, it.first, fit.first,
							name, sourceFieldName, &fit.second);
					trigger.tagField = toString(source, "_id");
					auto triggerName = trigger.makeName();
					schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
				} while (0);

				do {
					TriggerRec trigger(TriggerRec::Delete, TriggerRec::After, it.first, fit.first,
							name, sourceFieldName, &fit.second);
					trigger.tagField = toString(source, "_id");
					auto triggerName = trigger.makeName();
					schemeTable->triggers.emplace(sp::move(triggerName), sp::move(trigger));
				} while (0);

				break;
			}
			default: break;
			}

			if (scheme->hasDelta()) {
				auto name = Handle::getNameForDelta(*scheme);
				TableRec table;
				table.cols.emplace("object", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
				table.cols.emplace("time", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
				table.cols.emplace("action", ColRec(ColRec::Type::Int8, ColRec::IsNotNull));
				table.cols.emplace("user", ColRec(ColRec::Type::Int8));

				table.indexes.emplace(name + "_idx_object", "object");
				table.indexes.emplace(name + "_idx_time", "time");
				tables.emplace(StringView(name).pdup(), sp::move(table));
			}
		}
	}
	return tables;
}

Map<StringView, TableRec> TableRec::get(Handle &h, StringStream &stream) {
	Map<StringView, TableRec> ret;

	h.performSimpleSelect("SELECT name FROM sqlite_schema WHERE type='table';",
			[&](db::sql::Result &tables) {
		for (auto it : tables) {
			ret.emplace(it.at(0).pdup(), TableRec());
			stream << "TABLE " << it.at(0) << "\n";
		}
	});

	for (auto &it : ret) {
		auto &table = it.second;
		h.performSimpleSelect(toString("PRAGMA table_info('", it.first, "');"),
				[&](db::sql::Result &columns) {
			for (auto col : columns) {
				auto name = col.at(1);
				auto t = getStorageType(col.at(2));

				ColRec::Flags flags = ColRec::Flags::None;
				if (col.toBool(3)) {
					flags |= ColRec::Flags::IsNotNull;
				}
				if (col.toBool(5)) {
					flags |= ColRec::Flags::PrimaryKey;
				}

				if (t == BackendInterface::StorageType::Unknown) {
					table.cols.emplace(name.str<Interface>(),
							ColRec(col.at(2).str<Interface>(), flags));
				} else {
					table.cols.emplace(name.str<Interface>(), ColRec(t, flags));
				}
			}
		});
	}

	h.performSimpleSelect("SELECT tbl_name, name, sql FROM sqlite_schema WHERE type='index';",
			[&](db::sql::Result &indexes) {
		for (auto it : indexes) {
			auto tname = it.at(0).str<Interface>();
			auto f = ret.find(tname);
			if (f != ret.end()) {
				auto &table = f->second;
				auto name = it.at(1);
				auto sql = it.at(2);
				if (!name.starts_with("sqlite_autoindex_")) {
					bool unique = false;
					if (sql.starts_with("CREATE UNIQUE")) {
						unique = true;
					}

					auto stream = toString("\"", it.at(1), "\" ON \"", tname, "\" ");
					sql.readUntilString(stream);
					sql += stream.size();
					sql.skipChars<StringView::WhiteSpace>();
					if (sql.is("(")) {
						++sql;
						Vector<String> fields;
						while (!sql.empty() && !sql.is(')')) {
							sql.skipUntil<StringView::Chars<'"'>>();
							if (sql.is('"')) {
								++sql;
								auto field = sql.readUntil<StringView::Chars<'"'>>();
								if (sql.is('"')) {
									fields.emplace_back(field.str<Interface>());
									++sql;
								}
							}
						}
						table.indexes.emplace(it.at(1).str<Interface>(),
								IndexRec(sp::move(fields), unique));
					}
				}
			}
		}
	});

	h.performSimpleSelect("SELECT tbl_name, name, sql FROM sqlite_schema WHERE type='trigger';",
			[&](db::sql::Result &triggers) {
		for (auto it : triggers) {
			auto tableName = it.at(0);
			auto f = ret.find(tableName);
			if (f != ret.end()) {
				auto triggerName = it.at(1);
				if (!triggerName.starts_with("ST_TRIGGER:")) {
					continue;
				}

				triggerName += "ST_TRIGGER:"_len;

				TriggerRec trigger(triggerName);
				f->second.triggers.emplace(it.at(1).str<Interface>(), sp::move(trigger));
			}
		}
	});

	h.performSimpleSelect("SELECT name, version FROM __versions;", [&](db::sql::Result &versions) {
		for (auto it : versions) {
			auto tIt = ret.find(it.toString(0));
			if (tIt != ret.end()) {
				tIt->second.version = uint32_t(it.toInteger(1));
			}
		}
	});

	return ret;
}

TableRec::TableRec() { }
TableRec::TableRec(const Driver *driver, const BackendInterface::Config &cfg,
		const db::Scheme *scheme) {
	withOids = true;
	version = scheme->getVersion();
	if (scheme->isDetouched()) {
		detached = true;
	}

	auto name = scheme->getName();

	for (auto &it : scheme->getFields()) {
		bool emplaced = false;
		auto &f = it.second;
		auto type = it.second.getType();

		ColRec::Flags flags = ColRec::None;
		if (f.hasFlag(db::Flags::Required)) {
			flags |= ColRec::IsNotNull;
		}

		switch (type) {
		case db::Type::None:
		case db::Type::Array:
		case db::Type::View:
		case db::Type::Virtual: break;

		case db::Type::Float:
			cols.emplace(it.first, ColRec(ColRec::Type::Float8, flags));
			emplaced = true;
			break;

		case db::Type::Boolean:
			cols.emplace(it.first, ColRec(ColRec::Type::Bool, flags));
			emplaced = true;
			break;

		case db::Type::Text:
			cols.emplace(it.first, ColRec(ColRec::Type::Text, flags));
			emplaced = true;
			break;

		case db::Type::Data:
		case db::Type::Bytes:
		case db::Type::Extra:
			cols.emplace(it.first, ColRec(ColRec::Type::Bytes, flags));
			emplaced = true;
			break;

		case db::Type::Integer:
		case db::Type::File:
		case db::Type::Image:
			cols.emplace(it.first, ColRec(ColRec::Type::Int8, flags));
			emplaced = true;
			break;

		case db::Type::FullTextView:
			cols.emplace(it.first, ColRec(ColRec::Type::Bytes, flags));
			emplaced = true;
			break;

		case db::Type::Object:
			cols.emplace(it.first, ColRec(ColRec::Type::Int8, flags));
			emplaced = true;
			break;

		case db::Type::Set:
			if (f.isReference()) {
				// set is filled with references
			} else {
				//
			}
			break;

		case db::Type::Custom:
			if (auto objSlot = f.getSlot<db::FieldCustom>()) {
				if (auto info = driver->getCustomFieldInfo(objSlot->getDriverTypeName())) {
					cols.emplace(it.first, ColRec(StringView(info->typeName), flags));
					emplaced = true;
				}
			}
			break;
		}

		if (emplaced) {
			bool unique = f.hasFlag(db::Flags::Unique) || f.getTransform() == db::Transform::Alias;
			if (type == db::Type::Object) {
				auto ref = static_cast<const db::FieldObject *>(f.getSlot());
				auto target = ref->scheme->getName();
				StringStream cname;
				cname << name << "_ref_" << it.first << "_" << target;

				switch (ref->onRemove) {
				case RemovePolicy::Cascade: cname << "_csc"; break;
				case RemovePolicy::Restrict: cname << "_rst"; break;
				case RemovePolicy::Reference: cname << "_ref"; break;
				case RemovePolicy::StrongReference: cname << "_sref"; break;
				case RemovePolicy::Null: break;
				}

				indexes.emplace(toString(name, (unique ? "_uidx_" : "_idx_"), it.first),
						IndexRec(it.first, unique));
			} else if (type == db::Type::File || type == db::Type::Image) {
				TriggerRec updateTrigger(TriggerRec::Update, TriggerRec::After, name,
						it.second.getName(), cfg.fileScheme->getName(), "__oid", &it.second);
				auto updateTriggerName = updateTrigger.makeName();
				triggers.emplace(sp::move(updateTriggerName), sp::move(updateTrigger));

				TriggerRec removeTrigger(TriggerRec::Delete, TriggerRec::After, name,
						it.second.getName(), cfg.fileScheme->getName(), "__oid", &it.second);
				auto removeTriggerName = removeTrigger.makeName();
				triggers.emplace(sp::move(removeTriggerName), sp::move(removeTrigger));
			}

			if ((type == db::Type::Text && f.getTransform() == db::Transform::Alias)
					|| (f.hasFlag(db::Flags::Indexed))) {
				if (type == db::Type::Custom) {
					auto c = f.getSlot<db::FieldCustom>();
					if (auto info = driver->getCustomFieldInfo(c->getDriverTypeName())) {
						if (info->isIndexable) {
							indexes.emplace(toString(name, "_idx_", info->getIndexName(*c)),
									info->getIndexDefinition(*c));
						}
					}
				} else {
					indexes.emplace(toString(name, (unique ? "_uidx_" : "_idx_"), it.first),
							IndexRec(it.first, unique));
				}
			}

			if (type == db::Type::Text) {
				if (f.hasFlag(db::Flags::PatternIndexed)) {
					indexes.emplace(toString(name, "_idx_", it.first, "_pattern"),
							toString("( \"", it.first, "\" COLLATE NOCASE)"));
				}
				/*if (f.hasFlag(db::Flags::TrigramIndexed)) {
					indexes.emplace(toString(name, "_idx_", it.first, "_trgm"), toString("USING GIN ( \"", it.first, "\" gin_trgm_ops)"));
				}*/
			}

			if (type == db::Type::FullTextView) { }
		}
	}

	for (auto &it : scheme->getUnique()) {
		StringStream nameStream;
		nameStream << name << "_uidx";
		Vector<String> values;
		for (auto &f : it.fields) {
			values.emplace_back(f->getName().str<Interface>());
			nameStream << "_" << f->getName();
		}
		indexes.emplace(nameStream.str(), IndexRec(sp::move(values), true));
	}

	if (scheme->hasDelta()) {
		do {
			TriggerRec trigger(TriggerRec::Insert, TriggerRec::After, name, "__delta",
					Handle::getNameForDelta(*scheme), "object");
			trigger.rootScheme = scheme;
			auto triggerName = trigger.makeName();
			triggers.emplace(sp::move(triggerName), sp::move(trigger));
		} while (0);

		do {
			TriggerRec trigger(TriggerRec::Update, TriggerRec::After, name, "__delta",
					Handle::getNameForDelta(*scheme), "object");
			trigger.rootScheme = scheme;
			auto triggerName = trigger.makeName();
			triggers.emplace(sp::move(triggerName), sp::move(trigger));
		} while (0);

		do {
			TriggerRec trigger(TriggerRec::Delete, TriggerRec::After, name, "__delta",
					Handle::getNameForDelta(*scheme), "object");
			trigger.rootScheme = scheme;
			auto triggerName = trigger.makeName();
			triggers.emplace(sp::move(triggerName), sp::move(trigger));
		} while (0);
	}

	if (withOids && !detached) {
		indexes.emplace(toString(name, "_idx___oid"), IndexRec("__oid"));
	}
}

bool Handle::init(const BackendInterface::Config &cfg, const Map<StringView, const Scheme *> &s) {
	level = TransactionLevel::Exclusive;
	beginTransaction();

	if (!performSimpleQuery(StringView(DATABASE_DEFAULTS))) {
		endTransaction();
		return false;
	}

	StringStream tables;
	tables << "Server: " << cfg.name << "\n";
	auto existedTables = TableRec::get(*this, tables);
	auto requiredTables = TableRec::parse(driver, cfg, s);

	StringStream stream;
	TableRec::writeCompareResult(*this, stream, requiredTables, existedTables, s);

	if (!stream.empty()) {
		bool success = true;
		if (!performSimpleQuery(stream.weak(), [&](const Value &errInfo) {
			stream << "Server: " << cfg.name << "\n";
			stream << "\nErrorInfo: " << EncodeFormat::Pretty << errInfo << "\n";
		})) {
			endTransaction();
			success = false;
		}

		tables << "\n" << stream;
		if (_driver->getApplicationInterface()) {
			_driver->getApplicationInterface()->reportDbUpdate(tables.weak(), success);
		}
		if (!success) {
			return false;
		}
	}

	StringStream query;
	query << "DELETE FROM __login WHERE \"date\" < "
		  << Time::now().toSeconds() - config::STORAGE_DEFAULT_INTERNAL_INTERVAL.toSeconds() << ";";
	performSimpleQuery(query.weak());
	query.clear();

	auto iit = existedTables.find(StringView("__error"));
	if (iit != existedTables.end()) {
		query << "DELETE FROM __error WHERE \"time\" < "
			  << Time::now().toMicros() - config::STORAGE_DEFAULT_INTERNAL_INTERVAL.toMicros()
			  << ";";
		performSimpleQuery(query.weak());
		query.clear();
	}

	endTransaction();
	return true;
}

} // namespace stappler::db::sqlite
