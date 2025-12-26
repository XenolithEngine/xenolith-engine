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
#include "SPDbFile.h"
#include "SPDbScheme.h"
#include "SPDbUser.h"

namespace STAPPLER_VERSIONIZED stappler::db::sql {

StringView SqlHandle::getKeyValueSchemeName() { return "__sessions"; }

String SqlHandle::getNameForDelta(const Scheme &scheme) {
	return toString("__delta_", scheme.getName());
}

Value SqlHandle::get(const stappler::CoderSource &key) {
	Value ret;
	makeQuery([&, this](SqlQuery &query) {
		query.select("data")
				.from(getKeyValueSchemeName())
				.where("name", Comparation::Equal, key)
				.finalize();
		selectQuery(query, [&](Result &res) {
			if (!res.empty()) {
				ret = data::read<Interface, BytesView>(res.current().toBytes(0));
			}
			return true;
		});
	}, nullptr);
	return ret;
}

SqlHandle::SqlHandle(const Driver *driver) : _driver(driver) { }

bool SqlHandle::set(const stappler::CoderSource &key, const Value &data,
		stappler::TimeInterval maxage) {
	bool ret = false;
	makeQuery([&, this](SqlQuery &query) {
		query.insert(getKeyValueSchemeName())
				.fields("name", "mtime", "maxage", "data")
				.values(key, stappler::Time::now().toSeconds(), maxage.toSeconds(),
						data::write<Interface>(data, stappler::data::EncodeFormat::Cbor))
				.onConflict("name")
				.doUpdate()
				.excluded("mtime")
				.excluded("maxage")
				.excluded("data")
				.finalize();
		ret = (performQuery(query) != stappler::maxOf<size_t>());
	}, nullptr);
	return ret;
}

bool SqlHandle::clear(const stappler::CoderSource &key) {
	bool ret = false;
	makeQuery([&, this](SqlQuery &query) {
		query.remove("__sessions").where("name", Comparation::Equal, key).finalize();
		ret = (performQuery(query) == 1); // one row should be affected
	}, nullptr);
	return ret;
}

db::User *SqlHandle::authorizeUser(const db::Auth &auth, const StringView &iname,
		const StringView &password) {
	auto namePair = auth.getNameField(iname);
	auto paswodField = auth.getPasswordField();

	if (!namePair.first || !paswodField) {
		_driver->getApplicationInterface()->error("Auth",
				"Invalid scheme: fields 'name', 'email' and 'password' is not defined");
		return nullptr;
	}

	auto minTime = stappler::Time::now() - config::AUTH_MAX_TIME;

	bool transactionStarted = false;
	if (transactionStatus == TransactionStatus::None) {
		transactionStarted = beginTransaction();
	}

	db::User *ret = nullptr;
	makeQuery([&, this](SqlQuery &query) {
		query.with("u",
					 [&](SqlQuery::GenericQuery &q) {
			q.select()
					.from(auth.getScheme().getName())
					.where(namePair.first->getName(), Comparation::Equal,
							sp::move(namePair.second));
		})
				.with("l",
						[&](SqlQuery::GenericQuery &q) {
			q.select()
					.count("failed_count")
					.from("__login")
					.innerJoinOn("u", [&](SqlQuery::WhereBegin &w) {
				w.where(SqlQuery::Field("__login", "user"), Comparation::Equal,
						 SqlQuery::Field("u", "__oid"))
						.where(Operator::And, SqlQuery::Field("__login", "success"),
								Comparation::Equal, Value(false))
						.where(Operator::And, SqlQuery::Field("__login", "date"),
								Comparation::GreatherThen, uint64_t((minTime).toSeconds()));
			});
		})
				.select()
				.from("l", "u")
				.finalize();

		size_t count = 0;
		Value ud;
		selectQuery(query, [&, this](Result &res) {
			count = res.current().toInteger(0);

			if (count >= size_t(config::AUTH_MAX_LOGIN_ATTEMPT)) {
				_driver->getApplicationInterface()->error("Auth", "Autorization blocked",
						Value{
							stappler::pair("cooldown",
									Value((int64_t)config::AUTH_MAX_TIME.toSeconds())),
							stappler::pair("failedAttempts", Value((int64_t)count)),
						});
				return false;
			}

			auto dv = res.decode(auth.getScheme(), Vector<const Field *>());
			if (dv.size() == 1) {
				ud = sp::move(dv.getValue(0));
			}
			return true;
		});

		if (!ud) {
			return;
		}

		bool success = false;
		auto req = _driver->getApplicationInterface()->getRequestData();
		auto passwd = ud.getBytes("password");

		auto userId = ud.getInteger("__oid");
		if (auth.authorizeWithPassword(password, passwd, count)) {
			ret = new (std::nothrow) db::User(sp::move(ud), auth.getScheme());
			success = true;
		}

		query.clear();

		query.with("u",
					 [&](SqlQuery::GenericQuery &q) {
			q.select()
					.from(auth.getScheme().getName())
					.where(namePair.first->getName(), Comparation::Equal,
							sp::move(namePair.second));
		})
				.with("l",
						[&](SqlQuery::GenericQuery &q) {
			q.select()
					.aggregate("MAX", SqlQuery::Field("id").as("maxId"))
					.from("__login")
					.innerJoinOn("u", [&](SqlQuery::WhereBegin &w) {
				w.where(SqlQuery::Field("__login", "user"), Comparation::Equal,
						 SqlQuery::Field("u", "__oid"))
						.where(Operator::And, SqlQuery::Field("__login", "success"),
								Comparation::Equal, Value(true))
						.where(Operator::And, SqlQuery::Field("__login", "date"),
								Comparation::GreatherThen, uint64_t((minTime).toSeconds()));
			});
		})
				.select()
				.from("l", "u")
				.finalize();

		selectQuery(query, [&, this](Result &res) {
			query.clear();
			int64_t id = 0;
			if (!res.empty() && (id = res.readId())) {
				query.update("__login")
						.set("date", stappler::Time::now().toSeconds())
						.where("id", Comparation::Equal, Value(id))
						.finalize();
				performQuery(query);
			} else {
				auto &f = query.insert("__login").fields("user", "name", "password", "date",
						"success", "addr", "host", "path");
				if (req) {
					f.values(userId, iname, passwd, stappler::Time::now().toSeconds(),
							 Value(success), SqlQuery::TypeString(req.address, "inet"),
							 req.hostname.str<Interface>(), req.uri.str<Interface>())
							.finalize();
				} else {
					f.values(userId, iname, passwd, stappler::Time::now().toSeconds(),
							 Value(success), SqlQuery::TypeString("NULL", "inet"), String("NULL"),
							 String("NULL"))
							.finalize();
				}
				performQuery(query);
			}
			return true;
		});
	}, nullptr);

	if (transactionStarted) {
		endTransaction();
	}

	return ret;
}

void SqlHandle::makeSessionsCleanup() {
	bool transactionStarted = false;
	if (transactionStatus == TransactionStatus::None) {
		beginTransaction();
		transactionStarted = true;
	}

	StringStream query;
	query << "DELETE FROM __sessions WHERE (mtime + maxage + 10) < "
		  << stappler::Time::now().toSeconds() << ";";
	performSimpleQuery(query.weak());

	query.clear();
	query << "DELETE FROM __removed RETURNING __oid;";
	performSimpleSelect(query.weak(), [&, this](Result &res) {
		if (res.empty()) {
			return;
		}

		query.clear();
		query << "SELECT obj.__oid AS id FROM __files obj WHERE obj.__oid IN (";

		bool first = true;
		for (auto it : res) {
			if (first) {
				first = false;
			} else {
				query << ",";
			}
			query << it.at(0);
		}
		query << ");";
		performSimpleSelect(query.weak(), [&, this](Result &res) {
			if (!res.empty()) {
				query.clear();
				query << "DELETE FROM __files WHERE __oid IN (";
				first = true;
				// check for files to remove
				for (auto it : res) {
					if (auto fileId = it.toInteger(0)) {
						db::File::removeFile(_driver->getApplicationInterface(), fileId);
						if (first) {
							first = false;
						} else {
							query << ",";
						}
						query << fileId;
					}
				}
				query << ");";
				performSimpleQuery(query.weak());
			}
		});
	});

	query.clear();
	query << "DELETE FROM __broadcasts WHERE date < "
		  << (stappler::Time::now() - stappler::TimeInterval::seconds(10)).toMicroseconds() << ";";
	performSimpleQuery(query.weak());

	if (transactionStarted) {
		endTransaction();
	}
}

void SqlHandle::finalizeBroadcast() {
	if (!_bcasts.empty()) {
		makeQuery([&, this](SqlQuery &query) {
			auto vals = query.insert("__broadcasts").fields("date", "msg").values();
			for (auto &it : _bcasts) { vals.values(it.first, sp::move(it.second)); }
			query.finalize();
			performQuery(query);
			_bcasts.clear();
		}, nullptr);
	}
}

int64_t SqlHandle::processBroadcasts(const stappler::Callback<void(BytesView)> &cb, int64_t value) {
	int64_t maxId = value;
	makeQuery([&, this](SqlQuery &query) {
		if (value <= 0) {
			query.select("last_value").from("__broadcasts_id_seq").finalize();
			maxId = selectQueryId(query);
		} else {
			query.select("id", "date", "msg")
					.from("__broadcasts")
					.where("id", Comparation::GreatherThen, value)
					.finalize();
			selectQuery(query, [&](Result &res) {
				for (auto it : res) {
					if (it.size() >= 3) {
						auto msgId = it.toInteger(0);
						auto msgData = it.toBytes(2);
						if (!msgData.empty()) {
							if (msgId > maxId) {
								maxId = msgId;
							}
							cb(msgData);
						}
					}
				}
				return true;
			});
		}
	}, nullptr);
	return maxId;
}

void SqlHandle::broadcast(const Bytes &bytes) {
	if (getTransactionStatus() == db::TransactionStatus::None) {
		makeQuery([&, this](SqlQuery &query) {
			query.insert("__broadcasts")
					.fields("date", "msg")
					.values(stappler::Time::now(), Bytes(bytes))
					.finalize();
			performQuery(query);
		}, nullptr);
		if (isNotificationsSupported()) {
			makeQuery([&, this](SqlQuery &query) {
				query.getStream() << "NOTIFY " << config::BROADCAST_CHANNEL_NAME << ";";
				performQuery(query);
			}, nullptr);
		}
	} else {
		_bcasts.emplace_back(stappler::Time::now(), bytes);
	}
}

static bool Handle_convertViewDelta(Value &it) {
	auto vid_it = it.asDict().find("__vid");
	auto d_it = it.asDict().find("__delta");
	if (vid_it != it.asDict().end() && d_it != it.asDict().end()) {
		if (vid_it->second.getInteger()) {
			d_it->second.setString("update", "action");
			it.asDict().erase(vid_it);
		} else {
			d_it->second.setString("delete", "action");
			auto dict_it = it.asDict().begin();
			while (dict_it != it.asDict().end()) {
				if (dict_it->first != "__oid" && dict_it->first != "__delta") {
					dict_it = it.asDict().erase(dict_it);
				} else {
					++dict_it;
				}
			}
			return false;
		}
	}
	return true;
}

static void Handle_mergeViews(Value &objs, Value &vals) {
	for (auto &it : objs.asArray()) {
		if (!Handle_convertViewDelta(it)) {
			continue;
		}

		if (auto oid = it.getInteger("__oid")) {
			auto v_it = std::lower_bound(vals.asArray().begin(), vals.asArray().end(), oid,
					[&](const Value &l, int64_t r) -> bool {
				return (l.isInteger() ? l.getInteger() : l.getInteger("__oid")) < r;
			});
			if (v_it != vals.asArray().end()) {
				auto objId = v_it->getInteger("__oid");
				if (objId == oid) {
					v_it->erase("__oid");
					if (it.hasValue("__views")) {
						it.getValue("__views").addValue(sp::move(*v_it));
					} else {
						it.emplace("__views").addValue(sp::move(*v_it));
					}
					v_it->setInteger(oid);
				}
			}
		}
	}
}

int64_t SqlHandle::getDeltaValue(const Scheme &scheme) {
	if (scheme.hasDelta()) {
		int64_t ret = 0;
		makeQuery([&, this](SqlQuery &q) {
			q.select()
					.aggregate("max", SqlQuery::Field("d", "time"))
					.from(SqlQuery::Field(getNameForDelta(scheme)).as("d"))
					.finalize();
			selectQuery(q, [&](Result &res) {
				if (res) {
					ret = res.current().toInteger(0);
				}
				return true;
			});
		}, nullptr);
		return ret;
	}
	return 0;
}

int64_t SqlHandle::getDeltaValue(const Scheme &scheme, const db::FieldView &view, uint64_t tag) {
	if (view.delta) {
		int64_t ret = 0;
		makeQuery([&, this](SqlQuery &q) {
			String deltaName = toString(scheme.getName(), "_f_", view.name, "_delta");
			q.select()
					.aggregate("max", SqlQuery::Field("d", "time"))
					.from(SqlQuery::Field(deltaName).as("d"))
					.where("tag", Comparation::Equal, tag)
					.finalize();
			selectQuery(q, [&](Result &res) {
				if (res) {
					ret = res.current().toInteger(0);
				}
				return true;
			});
		}, nullptr);
		return ret;
	}
	return 0;
}

Value SqlHandle::getHistory(const Scheme &scheme, const stappler::Time &time, bool resolveUsers) {
	Value ret;
	if (!scheme.hasDelta()) {
		return ret;
	}

	makeQuery([&, this](SqlQuery &q) {
		q.select()
				.from(getNameForDelta(scheme))
				.where("time", Comparation::GreatherThen, time.toMicroseconds())
				.order(db::Ordering::Descending, "time")
				.finalize();

		selectQuery(q, [&, this](Result &res) {
			for (auto it : res) {
				auto &d = ret.emplace();
				for (size_t i = 0; i < it.size(); ++i) {
					auto name = res.name(i);
					if (name == "action") {
						switch (DeltaAction(it.toInteger(i))) {
						case DeltaAction::Create: d.setString("create", "action"); break;
						case DeltaAction::Update: d.setString("update", "action"); break;
						case DeltaAction::Delete: d.setString("delete", "action"); break;
						case DeltaAction::Append: d.setString("append", "action"); break;
						case DeltaAction::Erase: d.setString("erase", "action"); break;
						default: break;
						}
					} else if (name == "time") {
						d.setString(Time::microseconds(it.toInteger(i)).toHttp<Interface>(),
								"http-date");
						d.setInteger(it.toInteger(i), "time");
					} else if (name == "user" && resolveUsers) {
						if (auto u = db::User::get(
									db::Adapter(this, _driver->getApplicationInterface()),
									it.toInteger(i))) {
							auto &ud = d.emplace("user");
							ud.setInteger(u->getObjectId(), "id");
							ud.setString(u->getName(), "name");
						} else {
							d.setInteger(it.toInteger(i), name.str<Interface>());
						}
					} else if (name != "id") {
						d.setInteger(it.toInteger(i), name.str<Interface>());
					}
				}
			}
			return true;
		});
	}, nullptr);
	return ret;
}

Value SqlHandle::getHistory(const db::FieldView &view, const Scheme *scheme, uint64_t tag,
		const stappler::Time &time, bool resolveUsers) {
	Value ret;
	if (!view.delta) {
		return ret;
	}

	makeQuery([&, this](SqlQuery &q) {
		String name = toString(scheme->getName(), "_f_", view.name, "_delta");
		q.select()
				.from(name)
				.where("time", Comparation::GreatherThen, time.toMicroseconds())
				.where(Operator::And, "tag", Comparation::Equal, tag)
				.order(db::Ordering::Descending, "time")
				.finalize();

		selectQuery(q, [&, this](Result &res) {
			for (auto it : res) {
				auto &d = ret.emplace();
				for (size_t i = 0; i < it.size(); ++i) {
					auto name = res.name(i);
					if (name == "tag") {
						d.setInteger(it.toInteger(i), "tag");
					} else if (name == "time") {
						d.setString(Time::microseconds(it.toInteger(i)).toHttp<Interface>(),
								"http-date");
						d.setInteger(it.toInteger(i), "time");
					} else if (name == "user" && resolveUsers) {
						if (auto u = db::User::get(
									db::Adapter(this, _driver->getApplicationInterface()),
									it.toInteger(i))) {
							auto &ud = d.emplace("user");
							ud.setInteger(u->getObjectId(), "id");
							ud.setString(u->getName(), "name");
						} else {
							d.setInteger(it.toInteger(i), name.str<Interface>());
						}
					} else if (name != "id") {
						d.setInteger(it.toInteger(i), name.str<Interface>());
					}
				}
			}
			return true;
		});
	}, nullptr);

	return ret;
}

Value SqlHandle::getDeltaData(const Scheme &scheme, const stappler::Time &time) {
	Value ret;
	if (scheme.hasDelta()) {
		makeQuery([&, this](SqlQuery &q) {
			FieldResolver resv(scheme);
			q.writeQueryDelta(scheme, time, Set<const Field *>(), false);
			q.finalize();
			ret = selectValueQuery(scheme, q, resv.getVirtuals());
		}, nullptr);
	}
	return ret;
}

SPUNUSED static void Handle_writeSelectViewDataQuery(SqlQuery &q, const db::Scheme &s, uint64_t oid,
		const db::FieldView &f, const Value &data) {
	auto fs = f.scheme;

	auto fieldName = toString(fs->getName(), "_id");
	auto sel = q.select(SqlQuery::Field(fieldName).as("__oid")).field("__vid");

	sel.from(toString(s.getName(), "_f_", f.getName(), "_view"))
			.where(toString(s.getName(), "_id"), db::Comparation::Equal, oid)
			.parenthesis(db::Operator::And,
					[&](SqlQuery::WhereBegin &w) {
		auto subw = w.where();
		for (auto &it : data.asArray()) {
			subw.where(db::Operator::Or, fieldName, db::Comparation::Equal, it.getInteger("__oid"));
		}
	})
			.order(db::Ordering::Ascending, SqlQuery::Field(fieldName))
			.finalize();
}

Value SqlHandle::getDeltaData(const Scheme &scheme, const db::FieldView &view,
		const stappler::Time &time, uint64_t tag) {
	Value ret;
	if (view.delta) {
		makeQuery([&, this](SqlQuery &q) {
			Field field(&view);
			QueryList list(_driver->getApplicationInterface(), &scheme);
			list.selectById(&scheme, tag);
			list.setField(view.scheme, &field);

			FieldResolver resv(scheme);

			q.writeQueryViewDelta(list, time, Set<const Field *>(), false);
			auto r = selectValueQuery(*view.scheme, q, resv.getVirtuals());
			if (r.isArray() && r.size() > 0) {
				q.clear();
				Field f(&view);
				Handle_writeSelectViewDataQuery(q, scheme, tag, view, r);
				selectValueQuery(r, view, q);
				ret = sp::move(r);
			}
		}, nullptr);
	}
	return ret;
}

int64_t SqlHandle::selectQueryId(const SqlQuery &query) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return 0;
	}
	int64_t id = 0;
	selectQuery(query, [&](Result &res) {
		if (!res.empty()) {
			id = res.readId();
			return true;
		}
		return false;
	});
	return id;
}

size_t SqlHandle::performQuery(const SqlQuery &query) {
	if (getTransactionStatus() == db::TransactionStatus::Rollback) {
		return stappler::maxOf<size_t>();
	}
	size_t ret = stappler::maxOf<size_t>();
	selectQuery(query, [&](Result &res) {
		if (res.success()) {
			ret = res.getAffectedRows();
			return true;
		}
		return false;
	});
	return ret;
}

Value SqlHandle::selectValueQuery(const Scheme &scheme, const SqlQuery &query,
		const Vector<const Field *> &virtuals) {
	Value ret;
	selectQuery(query, [&](Result &result) {
		if (result) {
			ret = result.decode(scheme, virtuals);
			return true;
		}
		return false;
	});
	return ret;
}

Value SqlHandle::selectValueQuery(const Field &field, const SqlQuery &query,
		const Vector<const Field *> &virtuals) {
	Value ret;
	selectQuery(query, [&](Result &result) {
		if (result) {
			ret = result.decode(field, virtuals);
			return true;
		}
		return false;
	});
	return ret;
}

void SqlHandle::selectValueQuery(Value &objs, const FieldView &field, const SqlQuery &query) {
	selectQuery(query, [&](Result &result) {
		if (result) {
			auto vals = result.decode(field);
			if (!vals.isArray()) {
				for (auto &it : objs.asArray()) { Handle_convertViewDelta(it); }
			} else if (vals.isArray() && objs.isArray()) {
				Handle_mergeViews(objs, vals);
			}
			return true;
		}
		return false;
	});
}

} // namespace stappler::db::sql
