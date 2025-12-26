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

#include "SPDbUser.h"

#include "SPDbAdapter.h"
#include "SPDbScheme.h"
#include "SPDbWorker.h"
#include "SPValid.h"

namespace STAPPLER_VERSIONIZED stappler::db {

User *User::create(const Transaction &a, const StringView &name, const StringView &password) {
	return create(a,
			Value{
				std::make_pair("name", Value(name)),
				std::make_pair("password", Value(password)),
			});
}

User *User::setup(const Transaction &a, const StringView &name, const StringView &password) {
	auto s = a.getAdapter().getApplicationInterface()->getUserScheme();
	if (Worker(*s, a).asSystem().count() == 0) {
		return create(a,
				Value{
					std::make_pair("name", Value(name)),
					std::make_pair("password", Value(password)),
					std::make_pair("isAdmin", Value(true)),
				});
	}
	return nullptr;
}
User *User::create(const Transaction &a, Value &&val) {
	auto s = a.getAdapter().getApplicationInterface()->getUserScheme();

	auto d = Worker(*s, a).asSystem().create(val);
	return new (std::nothrow) User(sp::move(d), *s);
}

User *User::get(const Adapter &a, const StringView &name, const StringView &password) {
	auto s = a.getApplicationInterface()->getUserScheme();
	return get(a, *s, name, password);
}

User *User::get(const Adapter &a, const Scheme &scheme, const StringView &name,
		const StringView &password) {
	return a.authorizeUser(Auth(a.getApplicationInterface(), scheme), name, password);
}

User *User::get(const Adapter &a, const Scheme &scheme, const BytesView &key) {
	for (auto &it : scheme.getFields()) {
		if (it.second.getType() == db::Type::Bytes
				&& it.second.getTransform() == db::Transform::PublicKey && it.second.isIndexed()) {
			auto d = Worker(scheme, a).asSystem().select(
					db::Query().select(it.second.getName(), Value(key)));
			if (d.isArray() && d.size() == 1) {
				return new (std::nothrow) User(sp::move(d.getValue(0)), scheme);
			}
			break;
		}
	}
	return nullptr;
}

User *User::get(const Adapter &a, uint64_t oid) {
	auto s = a.getApplicationInterface()->getUserScheme();
	return get(a, *s, oid);
}

User *User::get(const Adapter &a, const Scheme &s, uint64_t oid) {
	auto d = Worker(s, a).asSystem().get(oid);
	if (d.isDictionary()) {
		return new (std::nothrow) User(sp::move(d), s);
	}
	return nullptr;
}

User *User::get(const Transaction &a, const StringView &name, const StringView &password) {
	auto s = a.getAdapter().getApplicationInterface()->getUserScheme();
	return get(a, *s, name, password);
}

User *User::get(const Transaction &a, const Scheme &scheme, const StringView &name,
		const StringView &password) {
	return a.getAdapter().authorizeUser(Auth(a.getAdapter().getApplicationInterface(), scheme),
			name, password);
}

User *User::get(const Transaction &a, const Scheme &scheme, const BytesView &key) {
	for (auto &it : scheme.getFields()) {
		if (it.second.getType() == db::Type::Bytes
				&& it.second.getTransform() == db::Transform::PublicKey && it.second.isIndexed()) {
			auto d = Worker(scheme, a).asSystem().select(
					db::Query().select(it.second.getName(), Value(key)));
			if (d.isArray() && d.size() == 1) {
				return new (std::nothrow) User(sp::move(d.getValue(0)), scheme);
			}
			break;
		}
	}
	return nullptr;
}

User *User::get(const Transaction &a, uint64_t oid) {
	auto s = a.getAdapter().getApplicationInterface()->getUserScheme();
	return get(a, *s, oid);
}

User *User::get(const Transaction &a, const Scheme &s, uint64_t oid) {
	auto d = Worker(s, a).asSystem().get(oid);
	if (d.isDictionary()) {
		return new (std::nothrow) User(sp::move(d), s);
	}
	return nullptr;
}

User::User(Value &&d, const Scheme &s) : Object(sp::move(d), s) { }

bool User::validatePassword(const StringView &passwd) const {
	auto &fields = _scheme.getFields();
	auto it = _scheme.getFields().find("password");
	if (it != fields.end() && it->second.getTransform() == Transform::Password) {
		auto f = static_cast<const FieldPassword *>(it->second.getSlot());
		return stappler::valid::validatePassord(passwd, getBytes("password"), f->salt);
	}
	return false;
}

void User::setPassword(const StringView &passwd) {
	auto &fields = _scheme.getFields();
	auto it = _scheme.getFields().find("password");
	if (it != fields.end() && it->second.getTransform() == Transform::Password) {
		auto f = static_cast<const FieldPassword *>(it->second.getSlot());
		setBytes(stappler::valid::makePassword<Interface>(passwd, f->salt), "password");
	}
}

bool User::isAdmin() const { return getBool("isAdmin"); }

StringView User::getName() const { return getString("name"); }

} // namespace stappler::db
