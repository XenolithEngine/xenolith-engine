/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_DB_SPDB_H_
#define STAPPLER_DB_SPDB_H_

#include "SPFilepath.h"
#include "SPMemory.h"
#include "SPData.h"
#include "SPSql.h"
#include "SPDbConfig.h"
#include "SPSearchConfiguration.h"

namespace STAPPLER_VERSIONIZED stappler::db {

using namespace mem_pool;
using namespace stappler::sql;

struct InputFile;

class Adapter;
class Transaction;
class Worker;

class Query;
class BackendInterface;
class Binder;
class QueryInterface;
class ResultCursor;

class Scheme;
class Field;
class Object;
class User;

struct FieldText;
struct FieldPassword;
struct FieldExtra;
struct FieldFile;
struct FieldImage;
struct FieldObject;
struct FieldArray;
struct FieldView;
struct FieldFullTextView;
struct FieldCustom;

using FullTextRank = search::SearchRank;
using FullTextData = search::SearchData;
using FullTextVector = search::SearchVector;
using FullTextQuery = search::SearchQuery;

struct RequestData {
	bool exists = false;
	StringView address;
	StringView hostname;
	StringView uri;

	explicit operator bool() { return exists; }
};

struct SP_PUBLIC InputConfig {
	enum class Require : uint32_t {
		None = 0,
		Data = 1,
		Files = 2,
		Body = 4,
		FilesAsData = 8,
	};

	static bool isFileAsDataSupportedForType(StringView);

	void updateLimits(const Map<String, Field> &vec);

	Require required = Require::None;
	size_t maxRequestSize = config::INPUT_MAX_REQUEST_SIZE;
	size_t maxVarSize = config::INPUT_MAX_VAR_SIZE;
	size_t maxFileSize = config::INPUT_MAX_FILE_SIZE;

	TimeInterval updateTime = config::INPUT_UPDATE_TIME;
	float updateFrequency = config::INPUT_UPDATE_FREQUENCY;
};

SP_DEFINE_ENUM_AS_MASK(InputConfig::Require);

struct SP_PUBLIC InputFile : public AllocBase {
	String path;
	String name;
	String type;
	String encoding;
	String original;
	filesystem::File file;

	bool isBinary = false;
	size_t writeSize;
	size_t headerSize;
	int64_t id;

	InputFile(String &&name, String &&type, String &&enc, String &&orig, size_t s, int64_t id);
	~InputFile();

	bool isOpen() const;

	size_t write(const char *, size_t);
	void close();

	bool save(const FileInfo &) const;
	Bytes readBytes();
	String readText();

	int64_t negativeId() const { return -id - 1; }

	InputFile(const InputFile &) = delete;
	InputFile(InputFile &&) = default;

	InputFile &operator=(const InputFile &) = delete;
	InputFile &operator=(InputFile &&) = default;
};

struct SP_PUBLIC InputValue {
	enum class Type {
		None,
		Value,
		File,
		TSV
	};

	Type type;
	union {
		Value value;
		InputFile *file;
		FullTextVector tsv;
	};

	bool hasValue() const { return type == Type::Value && !value.empty(); }

	InputValue() : type(Type::None) { }
	InputValue(Value &&val) : type(Type::Value), value(move(val)) { }
	InputValue(FullTextVector &&val) : type(Type::TSV), tsv(move(val)) { }

	InputValue(InputValue &&);
	InputValue &operator=(InputValue &&);

	InputValue(const InputValue &);
	InputValue &operator=(const InputValue &);

	void clear();

	~InputValue();
};

struct SP_PUBLIC InputField {
	const Field *field = nullptr;

	bool operator==(const InputField &other) const { return field == other.field; }
	bool operator!=(const InputField &other) const { return field != other.field; }
	bool operator<(const InputField &other) const { return field < other.field; }
};

struct SP_PUBLIC InputRow {
	Vector<InputValue> values;

	InputRow() = default;
};

} // namespace stappler::db

#endif /* STAPPLER_DB_SPDB_H_ */
