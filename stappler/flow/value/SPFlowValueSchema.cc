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

// Component schemas: layout, hash, accessors, migration. The rule that governs everything below is
// that layout is computed by this layer, never mirrored from the host compiler. Golden offsets must
// be identical on linux, win32, android and wasm, and the only way to guarantee that is to not
// consult sizeof/alignof at all. The native registration macro passes the host struct's
// offsetof/sizeof as a witness and build() checks the computed layout against it, so the compiler
// is a second opinion that fails the build when it disagrees, never the source of truth.

#include "SPFlowValueSchema.h"

#include "SPLog.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow::value {

uint32_t getTypeSize(VarType t) {
	switch (t) {
	case VarType::Nil: return 0;
	case VarType::Bool: return 1;
	case VarType::Int:
	case VarType::Float:
	case VarType::EntityRef:
	case VarType::Enum: return 8;
	case VarType::Vec2: return 8;
	case VarType::Vec3: return 12;
	case VarType::Vec4:
	case VarType::Color: return 16;
	case VarType::Int32:
	case VarType::UInt32:
	case VarType::Float32: return 4;
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map: return uint32_t(sizeof(BlobHandle));
	}
	return 0;
}

uint32_t getTypeAlign(VarType t) {
	switch (t) {
	case VarType::Nil: return 0;
	case VarType::Bool: return 1;
	case VarType::Int:
	case VarType::Float:
	case VarType::EntityRef:
	case VarType::Enum: return 8;
	// Vectors are float[N], so four - not the 16 that sprt::geom::Vec4's alignas would impose.
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color:
	case VarType::Int32:
	case VarType::UInt32:
	case VarType::Float32: return 4;
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map: return 4;
	}
	return 0;
}

uint32_t getElementStride(VarType t) { return getTypeSize(t); }

Status validateFieldType(VarType type, ElementChain element, DiagSink *sink) {
	if (type == VarType::Nil) {
		report(sink, makeDiag(DiagCode::FieldTypeNil, DiagLocus::Field));
		return Status::ErrorInvalidArguemnt;
	}
	if (uint32_t(type) >= VarTypeCount) {
		report(sink, makeDiag(DiagCode::FieldTypeUnknown, DiagLocus::Field));
		return Status::ErrorInvalidArguemnt;
	}

	if (!isContainerType(type)) {
		if (element != 0) {
			report(sink, makeDiag(DiagCode::FieldElementUnexpected, DiagDetail::Scalar, DiagLocus::Field));
			return Status::ErrorInvalidArguemnt;
		}
		return Status::Ok;
	}

	// A String or a Bytes is a container of bytes: its element type is implicit and must not be
	// spelled.
	if (type == VarType::String || type == VarType::Bytes) {
		if (element != 0) {
			report(sink,
					makeDiag(DiagCode::FieldElementUnexpected, DiagDetail::Typed, DiagLocus::Field)
							.type(uint8_t(type)));
			return Status::ErrorInvalidArguemnt;
		}
		return Status::Ok;
	}

	// An Array declares its element; a Map declares its value type (its key is always a string).
	// Zero would mean "heterogeneous", which only a Map may be - an Array is compact by design and
	// has to know its stride.
	if (element == 0) {
		if (type == VarType::Array) {
			report(sink, makeDiag(DiagCode::FieldElementMissing, DiagDetail::Array, DiagLocus::Field));
			return Status::ErrorInvalidArguemnt;
		}
		return Status::Ok; // a heterogeneous map
	}

	// Walk the chain: every container byte must be followed by its own element, and the whole thing
	// must terminate inside the four bytes available.
	auto chain = element;
	for (uint32_t depth = 0; depth < MaxChainDepth; ++depth) {
		auto head = chainHead(chain);
		if (head == VarType::Nil) {
			return depth > 0 ? Status::Ok : Status::ErrorInvalidArguemnt;
		}
		if (uint32_t(head) >= VarTypeCount) {
			report(sink, makeDiag(DiagCode::FieldElementUnknown, DiagLocus::Field));
			return Status::ErrorInvalidArguemnt;
		}
		auto tail = chainTail(chain);
		if (head == VarType::Array && chainHead(tail) == VarType::Nil) {
			report(sink, makeDiag(DiagCode::FieldElementMissing, DiagDetail::NestedArray, DiagLocus::Field));
			return Status::ErrorInvalidArguemnt;
		}
		chain = tail;
	}

	// Four levels used and the chain has not terminated: it needed a fifth.
	if (chainHead(chain) != VarType::Nil) {
		report(sink, makeDiag(DiagCode::FieldElementTooDeep, DiagLocus::Field));
		return Status::ErrorInvalidArguemnt;
	}
	return Status::Ok;
}

// Little-endian by explicit shifts, never a memcpy of the native integer: the hash has to be the
// same number on every target, and this is the only place that could make it not be.
static void appendU16(mem_std::Vector<uint8_t> &buf, uint16_t v) {
	buf.emplace_back(uint8_t(v));
	buf.emplace_back(uint8_t(v >> 8));
}

static void appendU32(mem_std::Vector<uint8_t> &buf, uint32_t v) {
	for (uint32_t i = 0; i < 4; ++i) { buf.emplace_back(uint8_t(v >> (i * 8))); }
}

static void appendU64(mem_std::Vector<uint8_t> &buf, uint64_t v) {
	for (uint32_t i = 0; i < 8; ++i) { buf.emplace_back(uint8_t(v >> (i * 8))); }
}

static void appendName(mem_std::Vector<uint8_t> &buf, StringView name) {
	appendU32(buf, uint32_t(name.size()));
	for (auto c : name) { buf.emplace_back(uint8_t(c)); }
}

// Only the flags that change how stored bytes are interpreted. ReadOnly and Hidden are editor
// policy with no effect on the record, so hashing them would force a migration for nothing.
static constexpr FieldFlags HashedFlags = FieldFlags::Transient;

// The question this hash answers, and the only one: "can I take these bytes as they are, or do I
// have to move them field by field?" - which is "is the layout the same and does each byte still
// mean the same thing?". The computed offset/size/align go in, not just the type and the order they
// derive from: deriving them again inside the hash would be a second implementation of the layout
// rules, while hashing what computeLayout actually produced means a bug in it changes the hash and
// schema-layout's golden literal catches it.
static uint64_t computeSchemaHash(StringView name, SpanView<FieldDesc> fields, uint32_t size,
		uint32_t align) {
	mem_std::Vector<uint8_t> buf;
	buf.reserve(64 + fields.size() * 48);

	appendName(buf, name);
	appendU32(buf, size);
	appendU32(buf, align);
	appendU32(buf, uint32_t(fields.size()));

	for (auto &f : fields) {
		// The name is hashed because migration is by name: a field renamed in place, same type at
		// the same offset, is a different field and must not silently adopt the old bytes.
		appendName(buf, f.name);
		appendU16(buf, uint16_t(f.type));
		appendU32(buf, f.element);
		appendU64(buf, f.subtypeId);
		appendU32(buf, f.offset);
		appendU32(buf, f.size);
		appendU32(buf, f.align);
		appendU32(buf, uint32_t(f.flags & HashedFlags));
	}

	// sprt::hash64 and nothing else: it assembles its words byte by byte, so it is stable across
	// endianness and pointer width. StringView::hash() and sprt::hash<T>{} are not.
	return sprt::hash64(reinterpret_cast<const char *>(buf.data()), buf.size());
}

const FieldDesc *ComponentType::getField(StringView name) const {
	SP_FLOW_VALUE_COUNT(fieldByName);
	for (auto &f : _fields) {
		if (f.name == name) {
			return &f;
		}
	}
	return nullptr;
}

Status ComponentType::build(StringView name, SpanView<FieldDef> defs, memory::pool_t *pool,
		DiagSink *sink, const mem_std::Value *meta) {
	if (name.empty()) {
		report(sink, makeDiag(DiagCode::ComponentNameEmpty, DiagLocus::Declaration));
		return Status::ErrorInvalidArguemnt;
	}

	auto intern = [&](StringView s) -> StringView {
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
	};

	_name = intern(name);
	if (_name.empty()) {
		return Status::ErrorOutOfHostMemory;
	}
	_id = makeTypeId(_name);

	_fields.clear();
	_fields.reserve(defs.size());

	uint32_t cursor = 0;
	uint32_t maxAlign = 1;

	for (auto &def : defs) {
		if (def.name.empty()) {
			report(sink, makeDiag(DiagCode::FieldNameEmpty, DiagLocus::Field));
			return Status::ErrorInvalidArguemnt;
		}
		for (auto &existing : _fields) {
			if (existing.name == def.name) {
				report(sink, makeDiag(DiagCode::FieldNameDuplicate, DiagLocus::Field, def.name));
				return Status::ErrorInvalidArguemnt;
			}
		}

		DiagFirst fieldDiag;
		if (validateFieldType(def.type, def.element, &fieldDiag) != Status::Ok) {
			// Re-reported with the field's name attached: the validator does not know it. What the
			// validator said travels as `inner`, so its own code goes up with its own words.
			auto d = makeDiag(DiagCode::FieldTypeInvalid, DiagLocus::Field, def.name);
			d.inner = fieldDiag.has() ? &fieldDiag.get() : nullptr;
			report(sink, d);
			return Status::ErrorInvalidArguemnt;
		}

		FieldDesc desc;
		desc.name = intern(def.name);
		if (desc.name.empty()) {
			return Status::ErrorOutOfHostMemory;
		}
		desc.type = def.type;
		desc.element = def.element;
		desc.subtypeId = def.subtypeId;

		// A name with no id gives the id. A name with an id is left exactly as it stands, and the
		// two are allowed to differ: that is what a reference through an alias looks like - the
		// author wrote `game.Enemy`, the resolver answered with what that name now stands for, and
		// the file keeps saying what was written while the descriptor holds what it means. build()
		// has no registry and cannot tell an alias from a mistake, so that check belongs to whoever
		// resolved the name.
		if (!def.subtypeName.empty()) {
			if (desc.subtypeId == NullTypeId) {
				desc.subtypeId = makeTypeId(def.subtypeName);
			}
			desc.subtypeName = intern(def.subtypeName);
			if (desc.subtypeName.empty()) {
				return Status::ErrorOutOfHostMemory;
			}
		}

		// Presentational and outside the hash, both of them: the name a type was written as, and
		// the hints declared on it.
		if (!def.aliasName.empty()) {
			desc.aliasName = intern(def.aliasName);
			if (desc.aliasName.empty()) {
				return Status::ErrorOutOfHostMemory;
			}
		}
		desc.meta = def.meta;

		desc.flags = def.flags;
		desc.def = def.def;
		desc.size = getTypeSize(def.type);
		desc.align = getTypeAlign(def.type);
		desc.offset = alignUp(cursor, desc.align);

		// The witness. Only ever a cross-check: when the layer and the host compiler disagree, the
		// build fails on the target where they disagree, which is the whole point.
		if (def.hostOffset != FieldDef::NoWitness && def.hostOffset != desc.offset) {
			slog().error("vstore::ComponentType", name, ".", def.name, ": computed offset ",
					desc.offset, " but the host struct has ", def.hostOffset);
			report(sink, makeDiag(DiagCode::HostOffsetMismatch, DiagLocus::Field, def.name));
			return Status::ErrorInvalidArguemnt;
		}
		if (def.hostSize != FieldDef::NoWitness && def.hostSize != desc.size) {
			slog().error("vstore::ComponentType", name, ".", def.name, ": computed size ",
					desc.size, " but the host struct has ", def.hostSize);
			report(sink, makeDiag(DiagCode::HostSizeMismatch, DiagLocus::Field, def.name));
			return Status::ErrorInvalidArguemnt;
		}

		cursor = desc.offset + desc.size;
		if (desc.align > maxAlign) {
			maxAlign = desc.align;
		}
		_fields.emplace_back(sprt::move(desc));
	}

	_align = maxAlign;
	_size = alignUp(cursor, _align);
	if (_size == 0) {
		// An empty component is legal as a tag, but it still needs a byte to have an address.
		_size = _align;
	}
	_hash = computeSchemaHash(_name, SpanView<FieldDesc>(_fields.data(), _fields.size()), _size,
			_align);

	// After the hash, deliberately and visibly: the declaration's own keys are presentation, and a
	// component that gained a line of documentation must describe the same type it described
	// before.
	if (meta && meta->isDictionary()) {
		_meta = *meta;
	}
	return Status::Ok;
}

// Scalar field access, under two rules. Every mutated byte comes from Arena::write() over a
// range covering the whole mutation, and getting the pointer is the announcement, so the range has
// to be the field's exact extent - marking less is silent corruption of a later rollback, marking
// more only costs journal bytes. And nothing here holds a pointer across a call that could
// allocate, which is the easy half since these accessors do not allocate at all; the blob layer is
// where it bites.

template <typename A>
Status ComponentType::getField(const A &arena, Addr instance, const FieldDesc &field,
		Var &out) const {
	SP_FLOW_VALUE_COUNT(getField);
	if (instance == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto src = arena.read(instance + field.offset, field.size);
	if (!src) {
		return Status::ErrorInvalidArguemnt;
	}
	return decodeField(src, field.type, field.size, field.element, field.subtypeId, out);
}

template <typename A>
Status ComponentType::setField(A &arena, Addr instance, const FieldDesc &field,
		const Var &value) const {
	SP_FLOW_VALUE_COUNT(setField);
	if (instance == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = checkFieldWrite(field.type, field.subtypeId, value);
	if (st != Status::Ok) {
		return st;
	}

	auto dst = arena.write(instance + field.offset, field.size);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	return encodeField(dst, field.type, field.size, value);
}

template <typename A>
Status ComponentType::getField(const A &arena, Addr instance, StringView name, Var &out) const {
	auto field = getField(name);
	return field ? getField(arena, instance, *field, out) : Status::ErrorNotFound;
}

template <typename A>
Status ComponentType::setField(A &arena, Addr instance, StringView name, const Var &value) const {
	auto field = getField(name);
	return field ? setField(arena, instance, *field, value) : Status::ErrorNotFound;
}

template <typename A>
Status ComponentType::setFieldFromValue(A &arena, Addr instance, const FieldDesc &field,
		const mem_std::Value &value, CastPolicy policy) const {
	Var decoded;
	auto st = decodeVar(value, field.type, decoded);
	if (st == Status::Ok) {
		// decodeVar produces a bare value; a declared field's identity comes from its descriptor.
		if (field.type == VarType::Enum) {
			decoded = makeEnum(decoded.e.value, field.subtypeId);
		} else if (field.type == VarType::EntityRef) {
			decoded = makeEntityRef(EntityId::unpack(decoded.ent.id), field.subtypeId);
		}
		return setField(arena, instance, field, decoded);
	}

	// The value did not decode as the field's type. Try the matrix: an asset written against an
	// older schema, or an author who typed 1 where a float was wanted.
	static const VarType probes[] = {VarType::Bool, VarType::Int, VarType::Float, VarType::String};
	for (auto probe : probes) {
		Var raw;
		if (decodeVar(value, probe, raw) != Status::Ok) {
			continue;
		}
		if (getCastRule(probe, field.type) == CastRule::Reject
				|| castNeedsArena(probe, field.type)) {
			continue;
		}
		Var converted;
		if (castVar(raw, field.type, policy, converted) == Status::Ok) {
			if (field.type == VarType::Enum) {
				converted = makeEnum(converted.e.value, field.subtypeId);
			}
			return setField(arena, instance, field, converted);
		}
	}
	return st;
}

template <typename A>
Status ComponentType::initInstance(A &arena, Addr instance, DiagSink *sink) const {
	if (instance == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}

	// The whole record in one barrier call, before anything else. Two things fall out of doing it
	// this way rather than field by field: alloc() hands back dirty payload bytes, so a record that
	// was not zeroed would carry whatever the last owner left; and the padding between fields gets
	// covered without a separate list of gaps to keep in step with the layout.
	auto dst = arena.write(instance, _size);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	__sprt_memset(dst, 0, _size);

	for (auto &field : _fields) {
		if (field.def.isNull()) {
			continue; // the type's zero, which the memset already wrote
		}

		// A default that does not fit leaves the field at its zero and is reported rather than
		// returned: the record is initialized either way, and refusing to make an instance because
		// its description carries a bad default would make a schema mistake into a runtime one.
		auto st = isContainerType(field.type)
				? blob::decode(arena, instance + field.offset, field.type, field.element, field.def)
				: setFieldFromValue(arena, instance, field, field.def, CastPolicy::Lossy);
		if (st != Status::Ok) {
			// A warning rather than a refusal: the record is initialized, and only the default is
			// dropped.
			report(sink, makeDiag(DiagCode::DefaultUnfit, DiagLocus::Field, field.name,
					DiagSeverity::Warning));
		}
	}
	return Status::Ok;
}

template <typename A>
void ComponentType::destroyInstance(A &arena, Addr instance) const {
	if (instance == NullAddr) {
		return;
	}
	// Ownership is strictly by the component - blocks are never shared between records - so
	// destroying one frees everything it owns and there is no garbage collection to do anywhere.
	for (auto &field : _fields) {
		if (!isContainerType(field.type)) {
			continue;
		}
		Var value;
		if (getField(arena, instance, field, value) != Status::Ok) {
			continue;
		}
		blob::destroy(arena, value.c.blob, field.type, field.element);

		// Clear the handle so a double destroy is a no-op rather than a double free.
		if (auto dst = arena.write(instance + field.offset, field.size)) {
			__sprt_memset(dst, 0, field.size);
		}
	}
}

template <typename A>
Addr ComponentType::createInstance(A &arena) const {
	auto addr = arena.alloc(_size);
	if (addr == NullAddr) {
		return NullAddr;
	}
	if (initInstance(arena, addr) != Status::Ok) {
		arena.free(addr);
		return NullAddr;
	}
	return addr;
}

template <typename A>
void ComponentType::freeInstance(A &arena, Addr instance) const {
	destroyInstance(arena, instance);
	arena.free(instance);
}

template <typename D, typename S>
Status ComponentType::copyInstance(D &dst, Addr dstInstance, const S &src, Addr srcInstance) const {
	auto st = initInstance(dst, dstInstance);
	if (st != Status::Ok) {
		return st;
	}
	for (auto &field : _fields) {
		if (isContainerType(field.type)) {
			// Deep: the destination gets its own blocks. Sharing one would break the rule that a
			// block has exactly one owner, and then destroying either record would free it twice.
			st = blob::copy(dst, dstInstance + field.offset, src, srcInstance + field.offset,
					field.type, field.element);
		} else {
			Var value;
			st = getField(src, srcInstance, field, value);
			if (st == Status::Ok) {
				st = setField(dst, dstInstance, field, value);
			}
		}
		if (st != Status::Ok) {
			return st;
		}
	}
	return Status::Ok;
}

template <typename A>
void ComponentType::encodeInstance(const A &arena, Addr instance, mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	for (auto &field : _fields) {
		mem_std::Value encoded;
		if (isContainerType(field.type)) {
			blob::encode(arena, instance + field.offset, field.type, field.element, encoded);
			out.setValue(sprt::move(encoded), field.name);
			continue;
		}
		Var value;
		if (getField(arena, instance, field, value) != Status::Ok) {
			continue;
		}
		if (encodeVar(value, encoded)) {
			out.setValue(sprt::move(encoded), field.name);
		}
	}
}

template <typename A>
Status ComponentType::decodeInstance(A &arena, Addr instance, const mem_std::Value &value,
		CastPolicy policy) const {
	if (!value.isDictionary()) {
		return Status::ErrorInvalidArguemnt;
	}
	for (auto &field : _fields) {
		auto &entry = value.getValue(field.name);
		if (entry.isNull()) {
			continue; // absent: whatever initInstance put there stands
		}
		if (isContainerType(field.type)) {
			auto st =
					blob::decode(arena, instance + field.offset, field.type, field.element, entry);
			if (st != Status::Ok) {
				return st;
			}
			continue;
		}
		auto st = setFieldFromValue(arena, instance, field, entry, policy);
		if (st != Status::Ok) {
			return st;
		}
	}
	return Status::Ok;
}

static void reportMigration(mem_std::Value *report, StringView field, StringView action,
		StringView from = StringView(), StringView to = StringView()) {
	if (!report) {
		return;
	}
	if (!report->isArray()) {
		*report = mem_std::Value(mem_std::Value::Type::ARRAY);
	}
	mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
	entry.setString(field, "field");
	entry.setString(action, "action");
	if (!from.empty()) {
		entry.setString(from, "from");
	}
	if (!to.empty()) {
		entry.setString(to, "to");
	}
	report->addValue(sprt::move(entry));
}

// Carries a container across when the shapes are not identical. Only the case that actually comes
// up is handled - an array of scalars whose element type changed - because anything more general
// would be inventing a conversion the author never asked for.
template <typename D, typename S>
static bool migrateContainer(D &dst, Addr dstAddr, const FieldDesc &to, const S &src, Addr srcAddr,
		const FieldDesc &from, CastPolicy policy) {
	if (to.type != from.type) {
		return false;
	}
	if (to.element == from.element) {
		return blob::copy(dst, dstAddr, src, srcAddr, to.type, to.element) == Status::Ok;
	}
	if (to.type != VarType::Array) {
		return false;
	}

	auto srcHead = chainHead(from.element);
	auto dstHead = chainHead(to.element);
	if (isContainerType(srcHead) || isContainerType(dstHead)) {
		return false; // a nested reshape is not a conversion, it is a rewrite
	}
	if (getCastRule(srcHead, dstHead) == CastRule::Reject || castNeedsArena(srcHead, dstHead)) {
		return false;
	}

	auto count = blob::arrayCount(src, srcAddr, from.element);
	if (blob::arrayResize(dst, dstAddr, to.element, count) != Status::Ok) {
		return false;
	}
	for (uint32_t i = 0; i < count; ++i) {
		Var value;
		if (blob::arrayGet(src, srcAddr, from.element, i, value) != Status::Ok) {
			return false;
		}
		Var converted;
		if (castVar(value, dstHead, policy, converted) != Status::Ok) {
			return false;
		}
		if (blob::arraySet(dst, dstAddr, to.element, i, converted) != Status::Ok) {
			return false;
		}
	}
	return true;
}

template <typename D, typename S>
Status ComponentType::migrateInstance(D &dst, Addr dstInstance, const ComponentType &from,
		const S &src, Addr srcInstance, mem_std::Value *report) const {
	// Everything starts at this type's defaults, so an unmatched field is never a zero it did not
	// ask for. Also covers the padding, which is part of the record's byte image.
	auto st = initInstance(dst, dstInstance);
	if (st != Status::Ok) {
		return st;
	}

	for (auto &field : _fields) {
		auto old = from.getField(field.name);
		if (!old) {
			reportMigration(report, field.name, StringView("added"));
			continue;
		}

		if (old->type == field.type && old->element == field.element
				&& old->subtypeId == field.subtypeId) {
			if (isContainerType(field.type)) {
				if (!migrateContainer(dst, dstInstance + field.offset, field, src,
							srcInstance + old->offset, *old, CastPolicy::Lossy)) {
					reportMigration(report, field.name, StringView("failed"));
					return Status::ErrorNotRecoverable;
				}
			} else {
				Var value;
				if (getField(src, srcInstance, *old, value) != Status::Ok
						|| setField(dst, dstInstance, field, value) != Status::Ok) {
					reportMigration(report, field.name, StringView("failed"));
					return Status::ErrorNotRecoverable;
				}
			}
			reportMigration(report, field.name, StringView("carried"));
			continue;
		}

		// Same name, different type. The value-conversion matrix decides - reused rather than
		// reimplemented, because a migration-specific policy would be a second place that could
		// disagree with the interpreter's own conversions.
		auto fromName = getVarTypeName(old->type);
		auto toName = getVarTypeName(field.type);

		if (isContainerType(field.type) || isContainerType(old->type)) {
			if (migrateContainer(dst, dstInstance + field.offset, field, src,
						srcInstance + old->offset, *old, CastPolicy::Lossy)) {
				reportMigration(report, field.name, StringView("converted"), fromName, toName);
			} else {
				// Falls back to the default, not to a zero: a scale that defaults to 1 must not
				// silently become 0, which would load fine and be wrong.
				reportMigration(report, field.name, StringView("rejected"), fromName, toName);
			}
			continue;
		}

		Var value;
		Var converted;
		if (getField(src, srcInstance, *old, value) == Status::Ok
				&& castVar(value, field.type, CastPolicy::Lossy, converted) == Status::Ok) {
			// A converted Enum arrives with no family; the destination descriptor stamps its own.
			if (field.type == VarType::Enum) {
				converted = makeEnum(converted.e.value, field.subtypeId);
			} else if (field.type == VarType::EntityRef) {
				converted = makeEntityRef(EntityId::unpack(converted.ent.id), field.subtypeId);
			}
			if (setField(dst, dstInstance, field, converted) == Status::Ok) {
				reportMigration(report, field.name, StringView("converted"), fromName, toName);
				continue;
			}
		}
		reportMigration(report, field.name, StringView("rejected"), fromName, toName);
	}

	// Fields the old type had and this one does not are never read; naming them is what makes an
	// unintended removal visible to whoever changed the schema.
	for (auto &old : from.getFields()) {
		if (!getField(old.name)) {
			reportMigration(report, old.name, StringView("dropped"));
		}
	}
	return Status::Ok;
}

const EnumType *TypeRegistry::createEnum(StringView name, SpanView<EnumMemberDef> members,
		DiagSink *sink, const mem_std::Value *meta) {
	if (!_pool) {
		return nullptr;
	}
	if (isNameTaken(makeTypeId(name))) {
		report(sink, makeDiag(DiagCode::DeclarationDuplicate, DiagLocus::Declaration, name));
		return nullptr;
	}

	auto type = new EnumType();
	if (type->build(name, members, _pool, sink, meta) != Status::Ok) {
		delete type;
		return nullptr;
	}
	_enums.emplace_back(type);
	return type;
}

const EnumType *TypeRegistry::getEnum(TypeId id) const {
	auto resolved = resolveTypeId(id);
	for (auto it : _enums) {
		if (it->getId() == resolved) {
			return it;
		}
	}
	return nullptr;
}

const EnumType *TypeRegistry::getEnum(StringView name) const {
	return getEnum(makeTypeId(name));
}

const TypeAlias *TypeRegistry::createAlias(const AliasSpelling &spelling,
		DiagSink *sink) {
	if (!_pool) {
		return nullptr;
	}
	if (isNameTaken(makeTypeId(spelling.name))) {
		report(sink, makeDiag(DiagCode::DeclarationDuplicate, DiagLocus::Declaration, spelling.name));
		return nullptr;
	}

	auto alias = new TypeAlias();
	if (alias->build(spelling.name, spelling.target, spelling.element, spelling.subtypeId,
				spelling.subtypeName, spelling.meta, _pool, sink)
			!= Status::Ok) {
		delete alias;
		return nullptr;
	}
	_aliases.emplace_back(alias);
	return alias;
}

const TypeAlias *TypeRegistry::getAlias(TypeId id) const {
	for (auto it : _aliases) {
		if (it->getId() == id) {
			return it;
		}
	}
	return nullptr;
}

const TypeAlias *TypeRegistry::getAlias(StringView name) const {
	return getAlias(makeTypeId(name));
}

TypeId TypeRegistry::resolveTypeId(TypeId id) const {
	auto current = id;
	for (uint32_t depth = 0; depth < MaxAliasDepth; ++depth) {
		auto alias = getAlias(current);
		if (!alias) {
			return current;
		}
		current = alias->getTarget();
	}
	// Past the bound: a chain this long is a mistake, and the honest answer is the one the caller
	// can act on - the name resolves to nothing rather than to whatever the cycle happened to end
	// on this time.
	return current;
}

bool TypeRegistry::isNameTaken(TypeId id) const {
	for (auto it : _order) {
		if (it->getId() == id) {
			return true;
		}
	}
	for (auto it : _enums) {
		if (it->getId() == id) {
			return true;
		}
	}
	return getAlias(id) != nullptr;
}

bool TypeRegistry::resolveTypeName(StringView name, ResolvedType &out,
		DiagSink *sink) const {
	// A builtin ends it at once, which is the common case and costs a table walk of fourteen.
	if (readVarType(name, out.type)) {
		return true;
	}

	// An alias contributes what it declares and then asks its target the same question. The keys an
	// outer alias declares win, because it is the more specific description: `Ratio` based on
	// `Fraction` with its own range means that range. Applied on the way out of the recursion, so
	// the outermost is applied last.
	mem_std::Vector<const TypeAlias *> chain;
	auto current = name;
	for (uint32_t depth = 0; depth <= MaxAliasDepth; ++depth) {
		auto alias = getAlias(current);
		if (!alias) {
			break;
		}
		if (depth == MaxAliasDepth) {
			report(sink, makeDiag(DiagCode::AliasChainTooDeep, DiagLocus::Declaration, name));
			return false;
		}
		for (auto seen : chain) {
			if (seen == alias) {
				report(sink, makeDiag(DiagCode::AliasChainCycle, DiagLocus::Declaration, name));
				return false;
			}
		}
		chain.emplace_back(alias);

		if (alias->isBuiltin()) {
			out.type = alias->getType();
			break;
		}
		current = alias->getTargetName();
	}

	if (chain.empty()) {
		// Nothing is declared under this name, which is the same answer an unknown builtin gets and
		// is reported in the same words: from where a field is being read, the two are one mistake.
		report(sink, makeDiag(DiagCode::TypeNameUnknown, DiagLocus::Declaration, name));
		return false;
	}
	if (!chain.back()->isBuiltin()) {
		// The chain ended on a component or an enum family, which is a thing and not a type: it can
		// be referred to (a subtype) and it cannot be a field's type.
		report(sink, makeDiag(DiagCode::TypeNameIsReference, DiagLocus::Declaration, name));
		return false;
	}

	for (size_t i = chain.size(); i > 0; --i) {
		auto alias = chain[i - 1];
		if (alias->getElement() != 0) {
			out.element = alias->getElement();
		}
		if (alias->getSubtype() != NullTypeId) {
			out.subtype = alias->getSubtype();
			out.subtypeName = alias->getSubtypeName();
		}
		if (alias->getMeta().isDictionary()) {
			if (!out.meta.isDictionary()) {
				out.meta = mem_std::Value(mem_std::Value::Type::DICTIONARY);
			}
			for (auto &it : alias->getMeta().asDict()) { out.meta.setValue(it.second, it.first); }
		}
	}
	return true;
}

void TypeRegistry::resolveFieldMeta(const FieldDesc &field, mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);

	if (!field.aliasName.empty()) {
		ResolvedType resolved;
		if (resolveTypeName(field.aliasName, resolved, nullptr) && resolved.meta.isDictionary()) {
			for (auto &it : resolved.meta.asDict()) { out.setValue(it.second, it.first); }
		}
	}
	if (field.meta.isDictionary()) {
		for (auto &it : field.meta.asDict()) { out.setValue(it.second, it.first); }
	}
}

TypeRegistry::~TypeRegistry() {
	// The descriptors hold mem_std containers, so they are new'd rather than pool-allocated and
	// have to be deleted: a pool never runs a destructor.
	for (auto it : _order) { delete it; }
	_order.clear();
	for (auto it : _enums) { delete it; }
	_enums.clear();
	for (auto it : _aliases) { delete it; }
	_aliases.clear();

	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool TypeRegistry::init(memory::pool_t *parent) {
	if (_pool) {
		return false;
	}
	_pool = parent;
	_ownsPool = false;
	if (!_pool) {
		_pool = memory::pool::create();
		_ownsPool = true;
	}
	return _pool != nullptr;
}

const ComponentType *TypeRegistry::add(ComponentType *type, DiagSink *sink) {
	if (get(type->getId()) != nullptr) {
		report(sink, makeDiag(DiagCode::ComponentDuplicate, DiagLocus::Declaration, type->getName()));
		delete type;
		return nullptr;
	}
	// One namespace over the three tables: a name an alias or a family already answers to would
	// make get() depend on which table it looked in first.
	if (isNameTaken(type->getId())) {
		report(sink, makeDiag(DiagCode::DeclarationDuplicate, DiagLocus::Declaration, type->getName()));
		delete type;
		return nullptr;
	}
	_order.emplace_back(type);
	return type;
}

const ComponentType *TypeRegistry::create(StringView name, SpanView<FieldDef> fields,
		const mem_std::Value *meta, DiagSink *sink) {
	if (!_pool) {
		return nullptr;
	}
	auto type = new ComponentType();
	if (type->build(name, fields, _pool, sink, meta) != Status::Ok) {
		delete type;
		return nullptr;
	}
	return add(type, sink);
}

const ComponentType *TypeRegistry::createNative(StringView name, SpanView<FieldDef> fields,
		DiagSink *sink) {
	return create(name, fields, nullptr, sink);
}

const ComponentType *TypeRegistry::createDerived(StringView name,
		const Callback<void(mem_std::Vector<FieldDef> &)> &fill, DiagSink *sink) {
	mem_std::Vector<FieldDef> fields;
	fill(fields);
	return createNative(name, SpanView<FieldDef>(fields.data(), fields.size()), sink);
}

// The writer of the same keys. Two overloads over the two records that carry them - the input and
// the output of a build - rather than one over a common base: FieldDef and FieldDesc are aggregates
// on purpose, and giving them a base class to share a writer would be the tail wagging the dog.
template <typename F>
static void writeFieldSpellingImpl(const F &f, mem_std::Value &out) {
	out.setString(f.name, "name");

	// The name the author wrote, when they wrote a declared one. A reader without that declaration
	// refuses the field rather than reading a different type - which is right: the file means what
	// it says, and what it says is `Hp`.
	out.setString(f.aliasName.empty() ? getVarTypeName(f.type) : f.aliasName, "type");

	// What the alias gave is the alias's to say, and saying it again here would freeze it into this
	// file: an alias edited afterwards would stop reaching a field that had been written out once.
	if (f.aliasName.empty() && f.element != 0) {
		auto &chain = out.newArray("element");
		auto c = f.element;
		while (chainHead(c) != VarType::Nil) {
			chain.addString(getVarTypeName(chainHead(c)));
			c = chainTail(c);
		}
	}

	// The name when the author gave one, the number otherwise. readFieldSpelling takes both, so
	// either spelling feeds straight back in - and the one that survives a round trip is the one a
	// person can read.
	if (!f.aliasName.empty()) {
		// Nothing: the subtype came from the declaration this field names.
	} else if (!f.subtypeName.empty()) {
		out.setString(f.subtypeName, "subtype");
	} else if (f.subtypeId != NullTypeId) {
		out.setInteger(int64_t(f.subtypeId), "subtype");
	}

	if (f.flags != FieldFlags::None) {
		auto &flags = out.newArray("flags");
		if ((f.flags & FieldFlags::ReadOnly) != FieldFlags::None) {
			flags.addString(StringView("readonly"));
		}
		if ((f.flags & FieldFlags::Transient) != FieldFlags::None) {
			flags.addString(StringView("transient"));
		}
		if ((f.flags & FieldFlags::Hidden) != FieldFlags::None) {
			flags.addString(StringView("hidden"));
		}
	}

	if (!f.def.isNull()) {
		out.setValue(f.def, "default");
	}
	if (f.meta.isDictionary() && f.meta.size() > 0) {
		out.setValue(f.meta, "meta");
	}
}

void writeFieldSpelling(const FieldDef &f, mem_std::Value &out) { writeFieldSpellingImpl(f, out); }

void writeFieldSpelling(const FieldDesc &f, mem_std::Value &out) { writeFieldSpellingImpl(f, out); }

const ComponentType *TypeRegistry::get(TypeId id) const {
	SP_FLOW_VALUE_COUNT(registryGet);
	for (auto it : _order) {
		if (it->getId() == id) {
			return it;
		}
	}

	// Only then the aliases, so the common answer costs what it always did and the indirection is
	// paid for by whoever is actually using one.
	auto resolved = resolveTypeId(id);
	if (resolved == id) {
		return nullptr;
	}
	for (auto it : _order) {
		if (it->getId() == resolved) {
			return it;
		}
	}
	return nullptr;
}

const ComponentType *TypeRegistry::get(StringView name) const { return get(makeTypeId(name)); }

void TypeRegistry::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (auto it : _order) {
		mem_std::Value entry;
		it->describe(entry);
		out.addValue(sprt::move(entry));
	}
}

void computeRecordSpans(const ComponentType &type, mem_std::Vector<RecordSpan> &out) {
	out.clear();
	auto fields = type.getFields();
	out.reserve(fields.size() + 1);

	uint32_t cursor = 0;
	int32_t index = 0;
	for (auto &f : fields) {
		if (f.offset > cursor) {
			out.emplace_back(RecordSpan{cursor, f.offset - cursor, -1});
		}
		out.emplace_back(RecordSpan{f.offset, f.size, index});
		cursor = f.offset + f.size;
		++index;
	}

	// The tail: alignUp of the last field's end to the record's alignment. Reported for the same
	// reason the inner gaps are - it is the cost of the field order, and it is paid per instance.
	if (type.getSize() > cursor) {
		out.emplace_back(RecordSpan{cursor, type.getSize() - cursor, -1});
	}
}

void ComponentType::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	out.setString(_name, "name");
	out.setInteger(int64_t(_id), "id");
	out.setInteger(int64_t(_hash), "hash");
	out.setInteger(_size, "size");
	out.setInteger(_align, "align");

	// Only when there is something in it, by the rule a field's `meta` follows five lines further
	// down: a component that documents nothing writes no key at all.
	if (_meta.isDictionary() && _meta.size() > 0) {
		out.setValue(_meta, "meta");
	}

	auto &fields = out.newArray("fields");

	// The gaps come from computeRecordSpans rather than from a second walk here: a test asserts the
	// padding is where the layout rules say it is, and an editor draws the same picture, so the two
	// have to be reading one answer.
	mem_std::Vector<RecordSpan> spans;
	computeRecordSpans(*this, spans);

	for (auto &span : spans) {
		if (span.field < 0) {
			mem_std::Value gap(mem_std::Value::Type::DICTIONARY);
			gap.setString(StringView("<padding>"), "name");
			gap.setInteger(span.offset, "offset");
			gap.setInteger(span.size, "size");
			fields.addValue(sprt::move(gap));
			continue;
		}

		auto &f = _fields[uint32_t(span.field)];
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);

		// The authored keys through the one writer there is; the derived ones are this function's
		// own, and are what a person diffs two dumps for.
		writeFieldSpelling(f, entry);
		entry.setInteger(f.offset, "offset");
		entry.setInteger(f.size, "size");
		entry.setInteger(f.align, "align");

		fields.addValue(sprt::move(entry));
	}
}

} // namespace stappler::flow::value
