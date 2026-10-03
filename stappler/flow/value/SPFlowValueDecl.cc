/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Enum families and type aliases: the named things that are not components.

#include "SPFlowValueDecl.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow::value {

// The registry's pool, as everywhere in this layer: a name outlives the description it came from.
static StringView internName(memory::pool_t *pool, StringView s) {
	if (s.empty()) {
		return StringView();
	}
	auto mem = reinterpret_cast<char *>(memory::pool::palloc(pool, s.size() + 1, 1));
	if (!mem) {
		return StringView();
	}
	__sprt_memcpy(mem, s.data(), s.size());
	mem[s.size()] = 0;
	return StringView(mem, s.size());
}

// Byte for byte the same on every target, like every other hash in this layer: the buffer is
// assembled explicitly and sprt::hash64 walks it, so neither endianness nor pointer width shows.
// The local lambdas are not an economy - a module that is one compile unit cannot have two
// functions of one name.
static uint64_t computeEnumHash(StringView name, SpanView<EnumMemberDef> members) {
	mem_std::Vector<uint8_t> buf;
	buf.reserve(32 + members.size() * 24);

	auto u32 = [&](uint32_t v) {
		for (uint32_t i = 0; i < 4; ++i) { buf.emplace_back(uint8_t(v >> (i * 8))); }
	};
	auto u64 = [&](uint64_t v) {
		for (uint32_t i = 0; i < 8; ++i) { buf.emplace_back(uint8_t(v >> (i * 8))); }
	};
	auto str = [&](StringView s) {
		u32(uint32_t(s.size()));
		for (auto c : s) { buf.emplace_back(uint8_t(c)); }
	};

	str(name);
	u32(uint32_t(members.size()));
	for (auto &m : members) {
		str(m.name);
		u64(uint64_t(m.value));
	}
	return sprt::hash64(reinterpret_cast<const char *>(buf.data()), buf.size());
}

Status EnumType::build(StringView name, SpanView<EnumMemberDef> members, memory::pool_t *pool,
		DiagSink *sink, const mem_std::Value *meta) {
	if (name.empty()) {
		report(sink, makeDiag(DiagCode::EnumNameEmpty, DiagLocus::Declaration));
		return Status::ErrorInvalidArguemnt;
	}

	_name = internName(pool, name);
	if (_name.empty()) {
		return Status::ErrorOutOfHostMemory;
	}
	_id = makeTypeId(_name);

	_members.clear();
	_members.reserve(members.size());
	for (auto &m : members) {
		if (m.name.empty()) {
			report(sink, makeDiag(DiagCode::EnumMemberNameEmpty, DiagLocus::Member, name));
			return Status::ErrorInvalidArguemnt;
		}
		for (auto &existing : _members) {
			if (existing.name == m.name) {
				report(sink, makeDiag(DiagCode::EnumMemberDuplicate, DiagLocus::Member, m.name));
				return Status::ErrorInvalidArguemnt;
			}
		}
		// A value may repeat - `Default` beside `North` is an ordinary thing to write - and only
		// the first name for it is what a display shows. A name may not: two members answering to
		// one name is a file that cannot be read back the way it was meant.
		EnumMemberDef out;
		out.name = internName(pool, m.name);
		if (out.name.empty()) {
			return Status::ErrorOutOfHostMemory;
		}
		out.value = m.value;
		if (m.meta.isDictionary() && m.meta.size() > 0) {
			out.meta = m.meta;
		}
		_members.emplace_back(sprt::move(out));
	}

	_meta = mem_std::Value();
	if (meta && meta->isDictionary() && meta->size() > 0) {
		_meta = *meta;
	}

	_hash = computeEnumHash(_name, SpanView<EnumMemberDef>(_members.data(), _members.size()));
	return Status::Ok;
}

bool EnumType::findValue(StringView name, int64_t &out) const {
	for (auto &m : _members) {
		if (m.name == name) {
			out = m.value;
			return true;
		}
	}
	return false;
}

bool EnumType::findName(int64_t value, StringView &out) const {
	for (auto &m : _members) {
		if (m.value == value) {
			out = m.name;
			return true;
		}
	}
	return false;
}

bool EnumType::hasValue(int64_t value) const {
	StringView unused;
	return findName(value, unused);
}

void EnumType::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	out.setString(_name, "name");
	out.setInteger(int64_t(_id), "id");
	out.setInteger(int64_t(_hash), "hash");
	if (_meta.isDictionary() && _meta.size() > 0) {
		out.setValue(_meta, "meta");
	}

	auto &values = out.newArray("values");
	for (auto &m : _members) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setString(m.name, "name");
		entry.setInteger(m.value, "value");
		if (m.meta.isDictionary() && m.meta.size() > 0) {
			entry.setValue(m.meta, "meta");
		}
		values.addValue(sprt::move(entry));
	}
}

Status TypeAlias::build(StringView name, StringView target, ElementChain element, TypeId subtype,
		StringView subtypeName, const mem_std::Value &meta, memory::pool_t *pool, DiagSink *sink) {
	if (name.empty() || target.empty()) {
		report(sink, makeDiag(DiagCode::AliasIncomplete, DiagLocus::Declaration, name));
		return Status::ErrorInvalidArguemnt;
	}
	if (name == target) {
		report(sink, makeDiag(DiagCode::AliasSelf, DiagLocus::Declaration, name));
		return Status::ErrorInvalidArguemnt;
	}

	_name = internName(pool, name);
	_targetName = internName(pool, target);
	if (_name.empty() || _targetName.empty()) {
		return Status::ErrorOutOfHostMemory;
	}
	_id = makeTypeId(_name);
	_target = makeTypeId(_targetName);

	// A builtin target ends the chain here, and only then is there a type to check. A target that
	// names another declaration is checked when the chain is walked - it may not be declared yet,
	// and requiring it to be would make the order of a file's own array load-bearing.
	_builtin = readVarType(_targetName, _type);
	_element = element;
	_subtype = subtype;
	if (!subtypeName.empty()) {
		_subtypeName = internName(pool, subtypeName);
	}
	_meta = meta;

	if (_builtin && validateFieldType(_type, _element, sink) != Status::Ok) {
		return Status::ErrorInvalidArguemnt;
	}
	return Status::Ok;
}

void TypeAlias::describe(mem_std::Value &out) const { writeAliasSpelling(*this, out); }

void writeEnumSpelling(const EnumType &type, mem_std::Value &out) { type.describe(out); }

void writeAliasSpelling(const TypeAlias &alias, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	out.setString(alias.getName(), "name");
	out.setString(alias.getTargetName(), "target");

	if (alias.getElement() != 0) {
		auto &chain = out.newArray("element");
		auto c = alias.getElement();
		while (chainHead(c) != VarType::Nil) {
			chain.addString(getVarTypeName(chainHead(c)));
			c = chainTail(c);
		}
	}
	if (!alias.getSubtypeName().empty()) {
		out.setString(alias.getSubtypeName(), "subtype");
	} else if (alias.getSubtype() != NullTypeId) {
		out.setInteger(int64_t(alias.getSubtype()), "subtype");
	}
	if (alias.getMeta().isDictionary() && alias.getMeta().size() > 0) {
		out.setValue(alias.getMeta(), "meta");
	}
}

} // namespace stappler::flow::value
