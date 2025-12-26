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

#include "SPPqHandle.h"

namespace STAPPLER_VERSIONIZED stappler::db::pq {

struct ExecParamData {
	std::array<const char *, 64> values;
	std::array<int, 64> sizes;
	std::array<int, 64> formats;

	Vector<const char *> valuesVec;
	Vector<int> sizesVec;
	Vector<int> formatsVec;

	const char *const *paramValues = nullptr;
	const int *paramLengths = nullptr;
	const int *paramFormats = nullptr;

	ExecParamData(const db::sql::SqlQuery &query) {
		auto queryInterface = static_cast<PgQueryInterface *>(query.getInterface());

		auto size = queryInterface->params.size();

		if (size > 64) {
			valuesVec.reserve(size);
			sizesVec.reserve(size);
			formatsVec.reserve(size);

			for (size_t i = 0; i < size; ++i) {
				const Bytes &d = queryInterface->params.at(i);
				bool bin = queryInterface->binary.at(i);
				valuesVec.emplace_back((const char *)d.data());
				sizesVec.emplace_back(int(d.size()));
				formatsVec.emplace_back(bin);
			}

			paramValues = valuesVec.data();
			paramLengths = sizesVec.data();
			paramFormats = formatsVec.data();
		} else {
			for (size_t i = 0; i < size; ++i) {
				const Bytes &d = queryInterface->params.at(i);
				bool bin = queryInterface->binary.at(i);
				values[i] = (const char *)d.data();
				sizes[i] = int(d.size());
				formats[i] = bin;
			}

			paramValues = values.data();
			paramLengths = sizes.data();
			paramFormats = formats.data();
		}
	}
};

SPUNUSED static String pg_numeric_to_string(BytesViewNetwork r) {
	using NumericDigit = int16_t;
	static constexpr auto DEC_DIGITS = 4;
	static constexpr auto NUMERIC_NEG = 0x4000;

	int d;
	NumericDigit dig;
	NumericDigit d1;

	uint16_t ndigits = uint16_t(r.readUnsigned16());
	int16_t weight = int16_t(r.readUnsigned16());
	int16_t sign = int16_t(r.readUnsigned16());
	int16_t dscale = int16_t(r.readUnsigned16());

	Vector<int16_t> digits;
	for (uint16_t i = 0; i < ndigits && !r.empty(); i++) {
		digits.emplace_back(r.readUnsigned16());
	}

	int i = (weight + 1) * DEC_DIGITS;
	if (i <= 0) {
		i = 1;
	}

	String str;
	str.reserve(i + dscale + DEC_DIGITS + 2);

	if (sign == NUMERIC_NEG) {
		str.push_back('-');
	}
	if (weight < 0) {
		d = weight + 1;
		str.push_back('0');
	} else {
		for (d = 0; d <= weight; d++) {
			dig = (d < ndigits) ? digits[d] : 0;

			bool putit = (d > 0);
			d1 = dig / 1'000;
			dig -= d1 * 1'000;
			putit |= (d1 > 0);
			if (putit) {
				str.push_back(d1 + '0');
			}
			d1 = dig / 100;
			dig -= d1 * 100;
			putit |= (d1 > 0);
			if (putit) {
				str.push_back(d1 + '0');
			}
			d1 = dig / 10;
			dig -= d1 * 10;
			putit |= (d1 > 0);
			if (putit) {
				str.push_back(d1 + '0');
			}
			str.push_back(dig + '0');
		}
	}

	if (dscale > 0) {
		str.push_back('.');
		for (i = 0; i < dscale; d++, i += DEC_DIGITS) {
			dig = (d >= 0 && d < ndigits) ? digits[d] : 0;

			d1 = dig / 1'000;
			dig -= d1 * 1'000;
			str.push_back(d1 + '0');
			d1 = dig / 100;
			dig -= d1 * 100;
			str.push_back(d1 + '0');
			d1 = dig / 10;
			dig -= d1 * 10;
			str.push_back(d1 + '0');
			str.push_back(dig + '0');
		}
	}

	return str;
}
PgQueryInterface::PgQueryInterface(const sql::Driver *d, const sql::QueryStorageHandle *s)
: driver(d), storage(s) { }

size_t PgQueryInterface::push(String &&val) {
	params.emplace_back(Bytes());
	params.back().assign_strong((uint8_t *)val.data(), val.size() + 1);
	binary.emplace_back(false);
	return params.size();
}

size_t PgQueryInterface::push(const StringView &val) {
	params.emplace_back(Bytes());
	params.back().assign_strong((uint8_t *)val.data(), val.size() + 1);
	binary.emplace_back(false);
	return params.size();
}

size_t PgQueryInterface::push(Bytes &&val) {
	params.emplace_back(sp::move(val));
	binary.emplace_back(true);
	return params.size();
}
size_t PgQueryInterface::push(StringStream &query, const Value &val, bool force, bool compress) {
	if (!force || val.getType() == Value::Type::EMPTY) {
		switch (val.getType()) {
		case Value::Type::EMPTY: query << "NULL"; break;
		case Value::Type::BOOLEAN:
			if (val.asBool()) {
				query << "TRUE";
			} else {
				query << "FALSE";
			}
			break;
		case Value::Type::INTEGER: query << val.asInteger(); break;
		case Value::Type::DOUBLE:
			if (std::isnan(val.asDouble())) {
				query << "NaN";
			} else if (val.asDouble() == std::numeric_limits<double>::infinity()) {
				query << "-Infinity";
			} else if (-val.asDouble() == std::numeric_limits<double>::infinity()) {
				query << "Infinity";
			} else {
				query << std::setprecision(std::numeric_limits<double>::max_digits10 + 1)
					  << val.asDouble();
			}
			break;
		case Value::Type::CHARSTRING:
			params.emplace_back(Bytes());
			params.back().assign_strong((uint8_t *)val.getString().data(), val.size() + 1);
			binary.emplace_back(false);
			query << "$" << params.size() << "::text";
			break;
		case Value::Type::BYTESTRING:
			params.emplace_back(val.asBytes());
			binary.emplace_back(true);
			query << "$" << params.size() << "::bytea";
			break;
		case Value::Type::ARRAY:
		case Value::Type::DICTIONARY:
			params.emplace_back(data::write<Interface>(val,
					EncodeFormat(EncodeFormat::Cbor,
							compress ? EncodeFormat::LZ4HCCompression
									 : EncodeFormat::DefaultCompress)));
			binary.emplace_back(true);
			query << "$" << params.size() << "::bytea";
			break;
		default: break;
		}
	} else {
		params.emplace_back(data::write<Interface>(val,
				EncodeFormat(EncodeFormat::Cbor,
						compress ? EncodeFormat::LZ4HCCompression
								 : EncodeFormat::DefaultCompress)));
		binary.emplace_back(true);
		query << "$" << params.size() << "::bytea";
	}
	return params.size();
}

void PgQueryInterface::bindInt(db::Binder &, StringStream &query, int64_t val) { query << val; }

void PgQueryInterface::bindUInt(db::Binder &, StringStream &query, uint64_t val) { query << val; }

void PgQueryInterface::bindDouble(db::Binder &, StringStream &query, double val) {
	query << std::setprecision(std::numeric_limits<double>::max_digits10 + 1) << val;
}

void PgQueryInterface::bindString(db::Binder &, StringStream &query, const String &val) {
	if (auto num = push(String(val))) {
		query << "$" << num << "::text";
	}
}

void PgQueryInterface::bindMoveString(db::Binder &, StringStream &query, String &&val) {
	if (auto num = push(sp::move(val))) {
		query << "$" << num << "::text";
	}
}

void PgQueryInterface::bindStringView(db::Binder &, StringStream &query, const StringView &val) {
	if (auto num = push(val)) {
		query << "$" << num << "::text";
	}
}

void PgQueryInterface::bindBytes(db::Binder &, StringStream &query, const Bytes &val) {
	if (auto num = push(Bytes(val))) {
		query << "$" << num << "::bytea";
	}
}

void PgQueryInterface::bindMoveBytes(db::Binder &, StringStream &query, Bytes &&val) {
	if (auto num = push(sp::move(val))) {
		query << "$" << num << "::bytea";
	}
}

void PgQueryInterface::bindCoderSource(db::Binder &, StringStream &query,
		const stappler::CoderSource &val) {
	if (auto num = push(Bytes(val.data(), val.data() + val.size()))) {
		query << "$" << num << "::bytea";
	}
}

void PgQueryInterface::bindValue(db::Binder &, StringStream &query, const Value &val) {
	push(query, val, false);
}

void PgQueryInterface::bindDataField(db::Binder &, StringStream &query,
		const db::Binder::DataField &f) {
	if (f.field && f.field->getType() == db::Type::Custom) {
		auto custom = f.field->getSlot<db::FieldCustom>();
		if (auto info = driver->getCustomFieldInfo(custom->getDriverTypeName())) {
			if (!info->writeToStorage(*custom, *this, query, f.data)) {
				query << "NULL";
			}
		} else {
			query << "NULL";
		}
	} else {
		push(query, f.data, f.force, f.compress);
	}
}

void PgQueryInterface::bindTypeString(db::Binder &, StringStream &query,
		const db::Binder::TypeString &type) {
	if (auto num = push(type.str)) {
		query << "$" << num << "::" << type.type;
	}
}

void PgQueryInterface::bindFullText(db::Binder &, StringStream &query,
		const db::Binder::FullTextField &d) {
	if (d.data.empty()) {
		query << "NULL";
	} else {
		auto slot = d.field->getSlot<FieldFullTextView>();
		auto str = slot->searchConfiguration->encodeSearchVectorPostgres(d.data);
		auto dataIdx = push(str);
		query << " $" << dataIdx << "::tsvector";
	}
}

void PgQueryInterface::bindFullTextFrom(db::Binder &, StringStream &query,
		const db::Binder::FullTextFrom &) { }

void PgQueryInterface::bindFullTextRank(db::Binder &, StringStream &query,
		const db::Binder::FullTextRank &d) {
	auto slot = d.field->getSlot<FieldFullTextView>();
	query << " ts_rank(" << d.scheme << ".\"" << d.field->getName() << "\", " << d.query << ", "
		  << toInt(slot->normalization) << ")";
}

void PgQueryInterface::bindFullTextQuery(db::Binder &, StringStream &query,
		const db::Binder::FullTextQueryRef &d) {
	StringStream tmp;
	d.query.encode([&](StringView str) { tmp << str; }, FullTextQuery::Postgresql);
	auto idx = push(tmp.str());
	query << " $" << idx << "::tsquery ";
}

void PgQueryInterface::bindIntVector(Binder &, StringStream &query, const Vector<int64_t> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		query << it;
	}
	query << ")";
}

void PgQueryInterface::bindDoubleVector(Binder &b, StringStream &query, const Vector<double> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		bindDouble(b, query, it);
	}
	query << ")";
}

void PgQueryInterface::bindStringVector(Binder &b, StringStream &query,
		const Vector<StringView> &vec) {
	query << "(";
	bool start = true;
	for (auto &it : vec) {
		if (start) {
			start = false;
		} else {
			query << ",";
		}
		bindStringView(b, query, it);
	}
	query << ")";
}

void PgQueryInterface::clear() {
	params.clear();
	binary.clear();
}

Handle::Handle(const Driver *d, Driver::Handle h) : SqlHandle(d), driver(d), handle(h) {
	if (h.get()) {
		auto c = d->getConnection(h);
		if (c.get()) {
			conn = c;

			performSimpleSelect("SELECT current_database();", [&, this](db::Result &qResult) {
				if (!qResult.empty()) {
					dbName = qResult.current().toString(0).pdup();
				}
			});
		}
	}
}

Handle::operator bool() const { return conn.get() != nullptr; }

Driver::Handle Handle::getHandle() const { return handle; }

Driver::Connection Handle::getConnection() const { return conn; }

void Handle::close() { conn = Driver::Connection(nullptr); }

void Handle::makeQuery(const stappler::Callback<void(sql::SqlQuery &)> &cb,
		const sql::QueryStorageHandle *s) {
	PgQueryInterface interface(_driver, s);
	db::sql::SqlQuery query(&interface, _driver);
	cb(query);
}

bool Handle::selectQuery(const sql::SqlQuery &query,
		const stappler::Callback<bool(sql::Result &)> &cb,
		const Callback<void(const Value &)> &errCb) {
	if (!conn.get() || getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	auto queryInterface = static_cast<PgQueryInterface *>(query.getInterface());

	ExecParamData data(query);
	ResultCursor res(driver,
			driver->exec(conn, query.getQuery().weak().data(), int(queryInterface->params.size()),
					data.paramValues, data.paramLengths, data.paramFormats, 1));
	if (!res.isSuccess()) {
		auto info = res.getInfo();
		info.setString(query.getQuery().str(), "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		cancelTransaction_pg();
	}

	lastError = res.getError();

	db::sql::Result ret(&res);
	return cb(ret);
}

bool Handle::performSimpleQuery(const StringView &query,
		const Callback<void(const Value &)> &errCb) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	ResultCursor res(driver, driver->exec(conn, query.data()));
	lastError = res.getError();
	if (!res.isSuccess()) {
		auto info = res.getInfo();
		info.setString(query, "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		cancelTransaction_pg();
	}
	return res.isSuccess();
}

bool Handle::performSimpleSelect(const StringView &query,
		const stappler::Callback<void(sql::Result &)> &cb,
		const Callback<void(const Value &)> &errCb) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return false;
	}

	ResultCursor res(driver, driver->exec(conn, query.data(), 0, nullptr, nullptr, nullptr, 1));
	lastError = res.getError();

	if (res.isSuccess()) {
		db::sql::Result ret(&res);
		cb(ret);
		return true;
	} else {
		auto info = res.getInfo();
		info.setString(query, "query");
#if DEBUG
		log::source().debug("pq::Handle", EncodeFormat::Pretty, info);
#endif
		if (errCb) {
			errCb(info);
		}
		driver->getApplicationInterface()->debug("Database", "Fail to perform query",
				sp::move(info));
		driver->getApplicationInterface()->error("Database", "Fail to perform query");
		cancelTransaction_pg();
	}

	return false;
}

bool Handle::isSuccess() const { return ResultCursor::pgsql_is_success(lastError); }

bool Handle::beginTransaction_pg(TransactionLevel l) {
	int64_t userId = _driver->getApplicationInterface()->getUserIdFromContext();
	int64_t now = stappler::Time::now().toMicros();

	if (transactionStatus != db::TransactionStatus::None) {
		log::source().error("pq::Handle", "Transaction already started");
		return false;
	}

	auto setVariables = [&, this] {
		performSimpleQuery(toString("SET LOCAL serenity.\"user\" = ", userId,
				";SET LOCAL serenity.\"now\" = ", now, ";"));
	};

	switch (l) {
	case TransactionLevel::ReadCommited:
		if (performSimpleQuery("BEGIN ISOLATION LEVEL READ COMMITTED"_weak)) {
			setVariables();
			level = TransactionLevel::ReadCommited;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	case TransactionLevel::RepeatableRead:
		if (performSimpleQuery("BEGIN ISOLATION LEVEL REPEATABLE READ"_weak)) {
			setVariables();
			level = TransactionLevel::RepeatableRead;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	case TransactionLevel::Serialized:
		if (performSimpleQuery("BEGIN ISOLATION LEVEL SERIALIZABLE"_weak)) {
			setVariables();
			level = TransactionLevel::Serialized;
			transactionStatus = db::TransactionStatus::Commit;
			return true;
		}
		break;
	default: break;
	}
	return false;
}

void Handle::cancelTransaction_pg() { transactionStatus = db::TransactionStatus::Rollback; }

bool Handle::endTransaction_pg() {
	switch (transactionStatus) {
	case db::TransactionStatus::Commit:
		transactionStatus = db::TransactionStatus::None;
		if (performSimpleQuery("COMMIT"_weak)) {
			finalizeBroadcast();
			return true;
		}
		break;
	case db::TransactionStatus::Rollback:
		transactionStatus = db::TransactionStatus::None;
		if (performSimpleQuery("ROLLBACK"_weak)) {
			finalizeBroadcast();
			return false;
		}
		break;
	default: break;
	}
	return false;
}

} // namespace stappler::db::pq
