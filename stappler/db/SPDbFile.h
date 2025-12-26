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

#ifndef STAPPLER_DB_SPDBFILE_H_
#define STAPPLER_DB_SPDBFILE_H_

#include "SPDbObject.h"

namespace STAPPLER_VERSIONIZED stappler::db {

class ApplicationInterface;

class SP_PUBLIC File : public Object {
public:
	static String getFilesystemPath(const ApplicationInterface *app, uint64_t oid);

	static bool validateFileField(const ApplicationInterface *app, const Field &, const InputFile &);
	static bool validateFileField(const ApplicationInterface *app, const Field &, const StringView &type, const BytesView &data);

	static Value createFile(const Transaction &, const Field &, InputFile &);
	static Value createFile(const Transaction &, const StringView &type, const StringView &path, int64_t = 0);
	static Value createFile(const Transaction &, const StringView &type, const BytesView &data, int64_t = 0);

	static Value createImage(const Transaction &, const Field &, InputFile &);
	static Value createImage(const Transaction &, const Field &, const StringView &type, const BytesView &data, int64_t = 0);

	static Value getData(const Transaction &, uint64_t id);
	static void setData(const Transaction &, uint64_t id, const Value &);

	// remove file from filesystem
	static bool removeFile(const ApplicationInterface *app, const Value &);
	static bool removeFile(const ApplicationInterface *app, int64_t);

	// remove file from storage and filesystem
	static bool purgeFile(const Transaction &, const Value &);
	static bool purgeFile(const Transaction &, int64_t);
};

}

#endif /* STAPPLER_DB_SPDBFILE_H_ */
