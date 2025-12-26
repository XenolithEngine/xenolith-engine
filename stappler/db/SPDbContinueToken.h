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

#ifndef STAPPLER_DB_SPDBCONTINUETOKEN_H_
#define STAPPLER_DB_SPDBCONTINUETOKEN_H_

#include "SPDbQuery.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC ContinueToken {
public:
	enum Flags : uint32_t {
		None = 0,
		Initial = 1,
		Reverse = 2,
		Inverted = 4,
	};

	ContinueToken() = default;
	ContinueToken(const StringView &f, size_t count, bool reverse);
	ContinueToken(const StringView &);

	ContinueToken(const ContinueToken &) = default;
	ContinueToken(ContinueToken &&) = default;

	ContinueToken &operator=(const ContinueToken &) = default;
	ContinueToken &operator=(ContinueToken &&) = default;

	explicit operator bool() const { return !field.empty() && count > 0; }

	bool hasPrev() const;
	bool hasNext() const;
	bool isInit() const;

	String encode() const;
	Value perform(const Scheme &, const Transaction &, Query &);
	Value perform(const Scheme &, const Transaction &, Query &, Ordering ord);

	Value performOrdered(const Scheme &, const Transaction &, Query &);

	void refresh(const Scheme &, const Transaction &, Query &);

	String encodeNext() const;
	String encodePrev() const;

	size_t getStart() const;
	size_t getEnd() const;
	size_t getTotal() const;
	size_t getCount() const;
	size_t getFetched() const;
	StringView getField() const;

	size_t getNumResults() const;

	bool hasFlag(Flags) const;
	void setFlag(Flags);
	void unsetFlag(Flags);

	const Value &getFirstVec() const;
	const Value &getLastVec() const;

protected:
	bool hasPrevImpl() const;
	bool hasNextImpl() const;

	String encodeNextImpl() const;
	String encodePrevImpl() const;

	bool _init = false;
	size_t _numResults = 0;
	String field;

	Value initVec;

	Value firstVec;
	Value lastVec;

	size_t count = 0;
	size_t fetched = 0;
	size_t total = 0;

	Flags flags = None;
};

SP_DEFINE_ENUM_AS_MASK(ContinueToken::Flags)

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDBCONTINUETOKEN_H_ */
