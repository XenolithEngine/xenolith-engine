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

#include "SPCommon.h"
#include "SPDbAdapter.cc"
#include "SPDbContinueToken.cc"

#include "SPDbFieldExtensions.cc"
#include "SPFilepath.h"
#include "SPPqDriver.cc"
#include "SPPqHandle.cc"
#include "SPPqHandleInit.cc"
#include "SPSqlDriver.cc"
#include "SPSqlHandle.cc"
#include "SPSqlHandleObject.cc"
#include "SPSqlHandleProp.cc"
#include "SPSqlQuery.cc"
#include "SPSqliteDriverHandle.cc"
#include "SPSqliteModuleTextSearch.cc"
#include "SPSqliteModuleUnwrap.cc"
#include "SPSqliteDriver.cc"
#include "SPSqliteHandle.cc"
#include "SPSqliteHandleInit.cc"
#include "SPDbAuth.cc"
#include "SPDbField.cc"
#include "SPDbFile.cc"
#include "SPDbObject.cc"
#include "SPDbQuery.cc"
#include "SPDbQueryList.cc"
#include "SPDbScheme.cc"
#include "SPDbTransaction.cc"
#include "SPDbUser.cc"
#include "SPDbWorker.cc"
#include "SPDbSimpleServer.cc"

namespace STAPPLER_VERSIONIZED stappler::db {

InputFile::InputFile(String &&name, String &&type, String &&enc, String &&orig, size_t s,
		int64_t id)
: name(sp::move(name))
, type(sp::move(type))
, encoding(sp::move(enc))
, original(sp::move(orig))
, writeSize(0)
, headerSize(s)
, id(id) {
	file = filesystem::File::open_tmp(config::UPLOAD_TMP_FILE_PREFIX,
			filesystem::OpenFlags::Read | filesystem::OpenFlags::Write);
	path = file.path();
}

InputFile::~InputFile() { close(); }

bool InputFile::isOpen() const { return file.is_open(); }

size_t InputFile::write(const char *s, size_t n) {
	auto tmp = s;
	for (size_t i = 0; i < n; ++i) {
		if (*tmp == 0) {
			isBinary = true;
		}
		++tmp;
	}
	writeSize += n;
	return file.xsputn(s, n);
}

void InputFile::close() { file.close_remove(); }

bool InputFile::save(const FileInfo &ipath) const {
	return const_cast<filesystem::File &>(file).close_rename(ipath);
}

Bytes InputFile::readBytes() {
	Bytes ret;
	ret.resize(writeSize);
	file.seek(0, stappler::io::Seek::Set);
	file.xsgetn((char *)ret.data(), ret.size());
	return ret;
}

String InputFile::readText() {
	if (!isBinary) {
		String ret;
		ret.resize(writeSize);
		file.seek(0, stappler::io::Seek::Set);
		file.xsgetn((char *)ret.data(), ret.size());
		return ret;
	}
	return String();
}


InputValue::InputValue(InputValue &&other) : type(other.type) {
	switch (type) {
	case Type::Value: new (&value) db::Value(move(other.value)); break;
	case Type::File: file = other.file; break;
	case Type::TSV: new (&tsv) FullTextVector(move(other.tsv)); break;
	case Type::None: break;
	}
	other.clear();
}

InputValue &InputValue::operator=(InputValue &&other) {
	clear();
	type = other.type;
	switch (type) {
	case Type::Value: new (&value) db::Value(move(other.value)); break;
	case Type::File: file = other.file; break;
	case Type::TSV: new (&tsv) FullTextVector(move(other.tsv)); break;
	case Type::None: break;
	}
	other.clear();
	return *this;
}

InputValue::InputValue(const InputValue &other) : type(other.type) {
	switch (type) {
	case Type::Value: new (&value) db::Value(other.value); break;
	case Type::File: file = other.file; break;
	case Type::TSV: new (&tsv) FullTextVector(other.tsv); break;
	case Type::None: break;
	}
}

InputValue &InputValue::operator=(const InputValue &other) {
	clear();
	type = other.type;
	switch (type) {
	case Type::Value: new (&value) db::Value(other.value); break;
	case Type::File: file = other.file; break;
	case Type::TSV: new (&tsv) FullTextVector(other.tsv); break;
	case Type::None: break;
	}
	return *this;
}

void InputValue::clear() {
	switch (type) {
	case Type::Value: value.~Value(); break;
	case Type::File: file = nullptr; break;
	case Type::TSV: tsv.~SearchVector(); break;
	case Type::None: break;
	}
	type = Type::None;
}

InputValue::~InputValue() { clear(); }

static size_t processExtraVarSize(const FieldExtra *s) {
	size_t ret = 256;
	for (auto it : s->fields) {
		auto t = it.second.getType();
		if (t == Type::Text || t == Type::Bytes) {
			auto f = static_cast<const FieldText *>(it.second.getSlot());
			ret = std::max(f->maxLength, ret);
		} else if (t == Type::Extra) {
			auto f = static_cast<const FieldExtra *>(it.second.getSlot());
			ret = std::max(processExtraVarSize(f), ret);
		}
	}
	return ret;
}

static size_t updateFieldLimits(const Map<String, Field> &vec) {
	size_t ret = 256 * vec.size();
	for (auto &it : vec) {
		auto t = it.second.getType();
		if (t == Type::Text || t == Type::Bytes) {
			auto f = static_cast<const FieldText *>(it.second.getSlot());
			ret += std::max(f->maxLength, f->inputSizeHint);
		} else if (t == Type::Data || t == Type::Array) {
			ret += std::max(config::FIELD_EXTRA_DEFAULT_HINT_SIZE,
					it.second.getSlot()->inputSizeHint);
		} else if (t == Type::Extra) {
			auto f = static_cast<const FieldExtra *>(it.second.getSlot());
			ret += updateFieldLimits(f->fields) + f->fields.size() * 8;
		} else {
			ret += 256;
		}
	}
	return ret;
}

bool InputConfig::isFileAsDataSupportedForType(StringView type) {
	return type.starts_with(data::MIME_CBOR) || type.starts_with(data::MIME_JSON);
}

void InputConfig::updateLimits(const Map<String, Field> &fields) {
	maxRequestSize = 256 * fields.size();
	for (auto &it : fields) {
		auto t = it.second.getType();
		if (t == Type::File) {
			auto f = static_cast<const FieldFile *>(it.second.getSlot());
			maxFileSize = std::max(std::max(f->maxSize, f->inputSizeHint), maxFileSize);
			maxRequestSize += std::max(f->maxSize, f->inputSizeHint) + 256;
		} else if (t == Type::Image) {
			auto f = static_cast<const FieldImage *>(it.second.getSlot());
			maxFileSize = std::max(std::max(f->maxSize, f->inputSizeHint), maxFileSize);
			maxRequestSize += std::max(f->maxSize, f->inputSizeHint) + 256;
		} else if (t == Type::Text || t == Type::Bytes) {
			auto f = static_cast<const FieldText *>(it.second.getSlot());
			maxVarSize = std::max(std::max(f->maxLength, f->inputSizeHint), maxVarSize);
			maxRequestSize += std::max(f->maxLength, f->inputSizeHint);
		} else if (t == Type::Data || t == Type::Array) {
			maxRequestSize += std::max(config::FIELD_EXTRA_DEFAULT_HINT_SIZE,
					it.second.getSlot()->inputSizeHint);
		} else if (t == Type::Extra) {
			auto f = static_cast<const FieldExtra *>(it.second.getSlot());
			maxRequestSize += updateFieldLimits(f->fields) + f->fields.size() * 8;
			maxVarSize = std::max(processExtraVarSize(f), maxVarSize);
		}
	}
}

} // namespace stappler::db
