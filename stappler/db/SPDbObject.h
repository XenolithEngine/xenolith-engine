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

#ifndef STAPPLER_DB_SPDBOBJECT_H_
#define STAPPLER_DB_SPDBOBJECT_H_

#include "SPDataWrapper.h"
#include "SPDb.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC Object : public stappler::data::WrapperTemplate<Interface> {
public:
	Object(Value &&, const Scheme &);

	const Scheme &getScheme() const;
	uint64_t getObjectId() const;

	void lockProperty(const StringView &prop);
	void unlockProperty(const StringView &prop);
	bool isPropertyLocked(const StringView &prop) const;

	bool isFieldProtected(const StringView &) const;

	auto begin() { return WrapperTemplate::begin<Object>(this); }
	auto end() { return WrapperTemplate::end<Object>(this); }

	auto begin() const { return WrapperTemplate::begin<Object>(this); }
	auto end() const { return WrapperTemplate::end<Object>(this); }

	bool save(const Adapter &, bool force = false);

protected:
	friend class Scheme;

	uint64_t _oid;
	Set<String> _locked;
	const Scheme &_scheme;
};

}

#endif /* STAPPLER_DB_SPDBOBJECT_H_ */
