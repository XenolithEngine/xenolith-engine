/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPSqliteDriver.h"
#include "sqlite3.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

static void sp_ts_update_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();
	DriverHandle *data = (DriverHandle *)sym->_user_data(ctx);

	auto id = sym->_value_int64(args[0]);
	auto blob = BytesView((const uint8_t *)sym->_value_blob(args[1]), sym->_value_bytes(args[1]));
	auto scheme = StringView((const char *)sym->_value_text(args[2]), sym->_value_bytes(args[2]));
	auto field = StringView((const char *)sym->_value_text(args[3]), sym->_value_bytes(args[3]));
	auto target = StringView((const char *)sym->_value_text(args[4]), sym->_value_bytes(args[4]));
	auto action = sym->_value_int(args[5]);

	if (action == 1 || action == 2) {
		auto q = toString("DELETE FROM \"", target, "\" WHERE \"", scheme, "_id\"=", id);
		Driver_exec(sym, nullptr, data->conn, q);
	}

	if (action == 2) {
		return;
	}

	bool first = true;
	StringStream queryStream;
	queryStream << "INSERT INTO \"" << target << "\"(\"" << scheme << "_id\",\"word\") VALUES ";

	auto wordsWritten = false;
	if (auto storage = data->driver->getQueryStorage(scheme)) {
		auto it = storage->find(field);
		if (it != storage->end()) {
			db::FullTextVector *vec = (db::FullTextVector *)it->second;
			for (auto &word : vec->words) {
				auto wordId = data->driver->insertWord(Driver::Handle(data), word.first);
				if (first) { first = false; } else { queryStream << ","; }
				queryStream << "(" << id << "," << wordId << ")";
			}
			wordsWritten = true;
		}
	}

	if (!wordsWritten) {
		auto block = data::read<Interface>(blob).getValue(1);
		if (block.getInteger(0) == 1) {
			auto &d = data::read<Interface>(blob).getValue(1);
			for (auto &it : d.asDict()) {
				auto wordId = data->driver->insertWord(Driver::Handle(data), it.first);
				if (first) { first = false; } else { queryStream << ","; }
				queryStream << "(" << id << "," << wordId << ")";
				wordsWritten = true;
			}
		}
	}

	if (wordsWritten) {
		StringView str(queryStream.weak());
		Driver_exec(sym, nullptr, data->conn, str);
	}
}

static void sp_ts_rank_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();
	DriverHandle *data = (DriverHandle *)sym->_user_data(ctx);
	auto storageData = data->driver->getCurrentQueryStorage();
	if (!storageData) {
		sym->_result_double(ctx, 0.0f);
		return;
	}

	auto blob = BytesView((const uint8_t *)sym->_value_blob(args[0]), sym->_value_bytes(args[0]));
	auto query = StringView((const char *)sym->_value_text(args[1]), sym->_value_bytes(args[1]));
	auto norm = sym->_value_int(args[2]);

	auto it = storageData->find(query);
	if (it == storageData->end()) {
		sym->_result_double(ctx, 0.0f);
		return;
	}

	auto q = (TextQueryData *)it->second;
	sym->_result_double(ctx, q->query->rankQuery(blob, search::Normalization(norm)));
}

static void sp_ts_query_valid_xFunc(sqlite3_context *ctx, int nargs, sqlite3_value **args) {
	auto sym = DriverSym::getCurrent();
	DriverHandle *data = (DriverHandle *)sym->_user_data(ctx);
	auto storageData = data->driver->getCurrentQueryStorage();
	if (!storageData) {
		sym->_result_int(ctx, 0);
		return;
	}

	auto blob = BytesView((const uint8_t *)sym->_value_blob(args[0]), sym->_value_bytes(args[0]));
	auto query = StringView((const char *)sym->_value_text(args[1]), sym->_value_bytes(args[1]));

	auto it = storageData->find(query);
	if (it == storageData->end()) {
		sym->_result_int(ctx, 0);
		return;
	}

	auto q = (TextQueryData *)it->second;
	sym->_result_int(ctx, q->query->isMatch(blob) ? 1 : 0);
}

}
