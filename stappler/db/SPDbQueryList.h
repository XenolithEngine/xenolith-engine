/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_DB_SPDBQUERYLIST_H_
#define STAPPLER_DB_SPDBQUERYLIST_H_

#include "SPDbContinueToken.h"
#include "SPDbQuery.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class ApplicationInterface;

enum class Action {
	Get,
	Set,
	Append,
	Remove,
	Count,
};

enum class TransactionStatus {
	None,
	Commit,
	Rollback,
};

class SP_PUBLIC QueryFieldResolver : public AllocBase {
public:
	enum class Meta : uint32_t {
		None = 0,
		Time = 1,
		Action = 2,
		View = 4,
	};

	QueryFieldResolver();
	QueryFieldResolver(const ApplicationInterface *app, const Scheme &, const Query &,
			const Vector<StringView> &extraFields = Vector<StringView>());

	const Field *getField(const String &) const;
	const Scheme *getScheme() const;
	const Map<String, Field> *getFields() const;
	Meta getMeta() const;

	const Set<const Field *> &getResolves() const;
	const Set<StringView> &getResolvesData() const;

	const Query::FieldsVec *getIncludeVec() const;
	const Query::FieldsVec *getExcludeVec() const;

	QueryFieldResolver next(const StringView &) const;

	explicit operator bool() const;

protected:
	struct Data {
		const Scheme *scheme = nullptr;
		const Map<String, Field> *fields = nullptr;
		const Query::FieldsVec *include = nullptr;
		const Query::FieldsVec *exclude = nullptr;
		Set<const Field *> resolved;
		Set<StringView> resolvedData;
		Map<String, Data> next;
		Meta meta = Meta::None;
	};

	QueryFieldResolver(Data *);
	void doResolve(const ApplicationInterface *app, Data *, const Vector<StringView> &extraFields,
			uint16_t depth, uint16_t max);
	void doResolveData(Data *, uint16_t depth, uint16_t max);

	Data *root = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(QueryFieldResolver::Meta);

class SP_PUBLIC QueryList : public AllocBase {
public:
	using FieldCallback = stappler::Callback<void(const StringView &name, const Field *f)>;

	static constexpr int64_t DefaultSoftLimit = 25;
	static constexpr int64_t MinSoftLimit = 1;
	static constexpr int64_t MaxSoftLimit = 500;

	enum Flags : uint32_t {
		None,
		SimpleGet = 1 << 0,
	};

	struct Item {
		const Scheme *scheme = nullptr;
		const Field *ref = nullptr;
		const Field *field = nullptr;

		bool all = false;
		bool resolved = false;

		Query query;
		QueryFieldResolver fields;

		const Set<const Field *> &getQueryFields() const;
	};

	QueryList(const ApplicationInterface *app, const Scheme *);

	bool selectById(const Scheme *, uint64_t);
	bool selectByName(const Scheme *, const StringView &);
	bool selectByQuery(const Scheme *, Query::Select &&);

	bool order(const Scheme *, const StringView &f, db::Ordering o);
	bool first(const Scheme *, const StringView &f, size_t v);
	bool last(const Scheme *, const StringView &f, size_t v);
	bool limit(const Scheme *, size_t limit);
	bool offset(const Scheme *, size_t offset);

	bool setFullTextQuery(const Field *field, FullTextQuery &&);

	bool setAll();
	bool setField(const Scheme *, const Field *field);
	bool setProperty(const Field *field);

	StringView setQueryAsMtime();

	void clearFlags();
	void addFlag(Flags);
	bool hasFlag(Flags) const;

	bool isAll() const;
	bool isRefSet() const;
	bool isObject() const;
	bool isView() const;
	bool empty() const;

	bool isDeltaApplicable() const;

	bool apply(const Value &query);
	void resolve(const Vector<StringView> &);

	uint16_t getResolveDepth() const;
	void setResolveDepth(uint16_t);

	void setDelta(stappler::Time);
	stappler::Time getDelta() const;

	size_t size() const;

	const Scheme *getPrimaryScheme() const;
	const Scheme *getSourceScheme() const;
	const Scheme *getScheme() const;
	const Field *getField() const;
	const Query &getTopQuery() const;

	const Vector<Item> &getItems() const;

	const Query::FieldsVec &getIncludeFields() const;
	const Query::FieldsVec &getExcludeFields() const;

	QueryFieldResolver getFields() const;

	const Value &getExtraData() const;
	ContinueToken &getContinueToken() const;

	const ApplicationInterface *getApplicationInterface() const { return _application; }

protected:
	void decodeSelect(const Scheme &, Query &, const Value &);
	void decodeOrder(const Scheme &, Query &, const String &, const Value &);

	const ApplicationInterface *_application = nullptr;
	Flags _flags = Flags::None;
	Vector<Item> queries;
	Value extraData;
	mutable ContinueToken token;
	bool failed = false;
};

SP_DEFINE_ENUM_AS_MASK(QueryList::Flags);

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDBQUERYLIST_H_ */
