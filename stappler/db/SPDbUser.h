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

#ifndef STAPPLER_DB_SPDBUSER_H_
#define STAPPLER_DB_SPDBUSER_H_

#include "SPDbObject.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC User : public Object {
public:
	static User *create(const Transaction &, const StringView &name, const StringView &password);
	static User *setup(const Transaction &, const StringView &name, const StringView &password);
	static User *create(const Transaction &, Value &&);

	static User *get(const Adapter &, const StringView &name, const StringView &password);
	static User *get(const Adapter &, const Scheme &scheme, const StringView &name, const StringView &password);
	static User *get(const Adapter &, const Scheme &scheme, const BytesView &key);

	static User *get(const Adapter &, uint64_t oid);
	static User *get(const Adapter &, const Scheme &scheme, uint64_t oid);

	static User *get(const Transaction &, const StringView &name, const StringView &password);
	static User *get(const Transaction &, const Scheme &scheme, const StringView &name, const StringView &password);
	static User *get(const Transaction &, const Scheme &scheme, const BytesView &key);

	static User *get(const Transaction &, uint64_t oid);
	static User *get(const Transaction &, const Scheme &scheme, uint64_t oid);

	User(Value &&, const Scheme &);

	bool validatePassword(const StringView &passwd) const;
	void setPassword(const StringView &passwd);

	bool isAdmin() const;

	StringView getName() const;
};

}

#endif /* STAPPLER_DB_SPDBUSER_H_ */
