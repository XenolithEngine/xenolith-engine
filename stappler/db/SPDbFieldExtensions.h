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

#ifndef STAPPLER_DB_SPDBFIELDEXTENSIONS_H_
#define STAPPLER_DB_SPDBFIELDEXTENSIONS_H_

#include "SPDbField.h"
#include "SPSqlDriver.h"

namespace STAPPLER_VERSIONIZED stappler::db {

struct SP_PUBLIC FieldIntArray : db::FieldCustom {
	static auto constexpr FIELD_NAME = StringView("INT[]");
	static bool registerForPostgres(CustomFieldInfo &);
	static bool registerForSqlite(CustomFieldInfo &);

	template <typename ... Args>
	FieldIntArray(String && n, Args && ... args) : FieldCustom(sp::move(n), std::forward<Args>(args)...) { }

	virtual StringView getDriverTypeName() const override { return FIELD_NAME; }

	virtual bool transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const override;
	virtual bool isSimpleLayout() const override;
};

struct SP_PUBLIC FieldBigIntArray : db::FieldCustom {
	static auto constexpr FIELD_NAME = StringView("BIGINT[]");
	static bool registerForPostgres(CustomFieldInfo &);
	static bool registerForSqlite(CustomFieldInfo &);

	template <typename ... Args>
	FieldBigIntArray(String && n, Args && ... args) : FieldCustom(sp::move(n), std::forward<Args>(args)...) { }

	virtual StringView getDriverTypeName() const override { return FIELD_NAME; }

	virtual bool transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const override;
	virtual bool isSimpleLayout() const override;
};

struct SP_PUBLIC FieldPoint : db::FieldCustom {
	static auto constexpr FIELD_NAME = StringView("POINT");
	static bool registerForPostgres(CustomFieldInfo &);
	static bool registerForSqlite(CustomFieldInfo &);

	template <typename ... Args>
	FieldPoint(String && n, Args && ... args) : FieldCustom(sp::move(n), std::forward<Args>(args)...) { }

	virtual StringView getDriverTypeName() const override { return FIELD_NAME; }

	virtual bool transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const override;
	virtual bool isSimpleLayout() const override;
};

struct SP_PUBLIC FieldTextArray : db::FieldCustom {
	static auto constexpr FIELD_NAME = StringView("TEXT[]");
	static bool registerForPostgres(CustomFieldInfo &);
	static bool registerForSqlite(CustomFieldInfo &);

	template <typename ... Args>
	FieldTextArray(String && n, Args && ... args) : FieldCustom(sp::move(n), std::forward<Args>(args)...) { }

	virtual StringView getDriverTypeName() const override { return FIELD_NAME; }

	virtual bool transformValue(const db::Scheme &, const Value &obj, Value &val, bool isCreate) const override;
	virtual bool isSimpleLayout() const override;
};

}

#endif /* STAPPLER_DB_SPDBFIELDEXTENSIONS_H_ */
