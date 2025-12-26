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

#ifndef STAPPLER_DB_SPDBAUTH_H_
#define STAPPLER_DB_SPDBAUTH_H_

#include "SPDbQueryList.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class SP_PUBLIC Auth : public AllocBase {
public:
	using NameFieldCallback = Function<stappler::Pair<const Field *, String>(const Scheme &, const StringView &)>;

	Auth(const ApplicationInterface *, const Scheme &);
	Auth(const ApplicationInterface *, const Scheme &, const StringView &name, const StringView &password = StringView());
	Auth(const ApplicationInterface *, const Scheme &, const Field *name, const Field *password = nullptr);

	Auth(const ApplicationInterface *, const Scheme &, const NameFieldCallback &, const Field *password);
	Auth(const ApplicationInterface *, const Scheme &, const NameFieldCallback &, const StringView &password = StringView());

	const Scheme &getScheme() const;

	stappler::Pair<const Field *, String> getNameField(const StringView &) const;
	const Field *getPasswordField() const;

	bool authorizeWithPassword(const StringView &input, const Bytes &database, size_t tryCount) const;

protected:
	const Field *detectPasswordField(const Scheme &);

	const ApplicationInterface *_application = nullptr;
	const Field *_password = nullptr;
	const Field *_name = nullptr;
	NameFieldCallback _nameFieldCallback = nullptr;
	const Scheme *_scheme = nullptr;
};

}

#endif /* STAPPLER_DB_SPDBAUTH_H_ */
