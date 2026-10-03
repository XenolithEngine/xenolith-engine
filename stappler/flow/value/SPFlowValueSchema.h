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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUESCHEMA_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUESCHEMA_H_

#include "SPFlowValue.h"
#include "SPVStoreArena.h"
#include "SPFlowValueBlob.h"
#include "SPFlowValueDecl.h"
#include "SPFlowValueVar.h"

// The schema layer sits on the arena and nothing else - not on the journal. A component type is
// exercisable with a bare Arena, and only the ECS tests pair one with a Journal.
namespace STAPPLER_VERSIONIZED stappler::flow::value {

enum class FieldFlags : uint32_t {
	None = 0,
	ReadOnly = 1 << 0, // the editor may not write it - NOT hashed, it is policy
	Transient = 1 << 1, // not serialized - HASHED, it changes what a loaded record contains
	Hidden = 1 << 2, // editor-only - NOT hashed
};

SP_DEFINE_ENUM_AS_MASK(FieldFlags)

// The input of a schema. An aggregate on purpose: all three construction paths - data-driven,
// native C++ and programmatic - produce a SpanView<FieldDef> and hand it to the one build() there
// is, so "the same schema described two ways gives the same descriptor" holds by construction
// rather than by two implementations agreeing.
struct FieldDef {
	StringView name;
	VarType type = VarType::Nil;
	ElementChain element = 0; // container element chain; 0 for a scalar
	TypeId subtypeId = NullTypeId; // enum family or referenced component type

	// The name that id was made of, when the author wrote one, and nothing else depends on it: a
	// TypeId is a hash64 (makeTypeId) and cannot be turned back into a name, so without this a
	// subtype spelled as a name comes back from describe() as a number and everything downstream
	// has a hash to show and no word. Outside the schema hash, for the same reason OpFlags is
	// outside the signature hash. Empty leaves the id standing alone; given without an id, the id
	// is derived from it; given with an id that disagrees, build() refuses rather than picking one
	// of the two.
	StringView subtypeName;

	// The declared name written in the `type` position, when it was not a builtin one. Kept for the
	// same reason `subtypeName` is: the resolution is one-way, and a file that said `Hp` must not
	// come back saying `int`. Outside the schema hash - an alias is not a type.
	StringView aliasName;

	FieldFlags flags = FieldFlags::None;
	mem_std::Value def; // default; EMPTY means the type's zero

	// Presentation, declared rather than derived: a unit, a range, a step, a label. The control a
	// field gets is inferred from its type by the editor and cannot be declared; what cannot
	// be inferred is declared here, and nothing else may be. Outside the schema hash, like every
	// other presentational key, and merged over the meta of the alias the type came from, this one
	// winning key by key. The one forward-compatible island in a strict payload: an unknown key
	// inside here is carried verbatim and ignored, because an editor that does not know a hint does
	// not show it.
	mem_std::Value meta;

	// Filled in by the native macro from the host struct. Unchecked when left at NoWitness.
	// This is what makes "layout is defined by this layer, never mirrored from the compiler"
	// verifiable rather than merely asserted: the layer computes, and the compiler is consulted
	// only as a witness - on every target, at build time.
	static constexpr uint32_t NoWitness = 0xffff'ffffu;
	uint32_t hostOffset = NoWitness;
	uint32_t hostSize = NoWitness;
};

// The output: layout resolved. No behaviour, no vtable, no owner pointers - unlike db::Field::Slot,
// because a field here has no filters, no transforms and no foreign schemes, and it has to be both
// buildable from data::Value and hashable byte for byte.
struct FieldDesc {
	StringView name; // interned in the registry's pool; stable for the registry's lifetime
	VarType type = VarType::Nil;
	ElementChain element = 0;
	TypeId subtypeId = NullTypeId;
	StringView subtypeName; // interned beside `name`; empty when the author gave only an id
	StringView aliasName; // the declared name the type was written as, when it was not a builtin
	FieldFlags flags = FieldFlags::None;

	uint32_t offset = 0;
	uint32_t size = 0; // layer-defined, never sizeof()
	uint32_t align = 0; // layer-defined

	mem_std::Value def;

	// The field's own hints, as authored. What the alias it names adds is merged in on demand by
	// TypeRegistry::resolveFieldMeta, never stored here: a field that froze its alias's hints
	// would stop hearing about the alias being edited. Outside the hash.
	mem_std::Value meta;
};

// The three things `valueTypesMeet` asks about, read off a field. Here rather than in SPFlowValueVar.h
// because that file knows nothing about a FieldDesc, and it should not have to.
inline ValueShape shapeOf(const FieldDesc &f) { return ValueShape{f.type, f.element, f.subtypeId}; }

// The bytes of one field, decoded and encoded, with nothing of the arena in them. Split out of
// ComponentType::getField/setField so that a caller which already knows a field's shape reaches the
// same rules instead of a second copy of them - a generated unit, where the offset, the type
// and the size of a node's own record field are constants the compiler has. What is here is exactly
// what those two members do once the arena has handed over the bytes: no barrier, no address
// arithmetic, no counter. The split follows setField's own body and is load-bearing:
// `checkFieldWrite` runs before the write barrier is taken, because a refused write that had
// already marked the page would put bytes in the journal that never changed. Inline in the header
// on purpose: called with a runtime FieldDesc the switch is a switch, called with constants it
// folds to the one case.

inline Status decodeField(const uint8_t *src, VarType type, uint32_t size, ElementChain element,
		TypeId subtypeId, Var &out) {
	switch (type) {
	case VarType::Bool:
		// Anything other than 0 or 1 means the record is corrupt: nothing in this layer can write
		// such a byte, so reporting it beats silently normalising it to true.
		if (src[0] > 1) {
			return Status::ErrorNotRecoverable;
		}
		out = makeBool(src[0] != 0);
		return Status::Ok;
	case VarType::Int: {
		int64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeInt(v);
		return Status::Ok;
	}
	case VarType::Float: {
		double v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeFloat(v);
		return Status::Ok;
	}
	case VarType::Int32: {
		int32_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeInt32(v);
		return Status::Ok;
	}
	case VarType::UInt32: {
		uint32_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeUInt32(v);
		return Status::Ok;
	}
	case VarType::Float32: {
		float v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeFloat32(v);
		return Status::Ok;
	}
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		// Zeroed first, then `size` bytes over it: a Vec2 must not hand back whatever was in the
		// two lanes it does not store.
		float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		__sprt_memcpy(v, src, size);
		switch (type) {
		case VarType::Vec2: out = makeVec2(v[0], v[1]); break;
		case VarType::Vec3: out = makeVec3(v[0], v[1], v[2]); break;
		case VarType::Vec4: out = makeVec4(v[0], v[1], v[2], v[3]); break;
		default: out = makeColor(v[0], v[1], v[2], v[3]); break;
		}
		return Status::Ok;
	}
	case VarType::EntityRef: {
		uint64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		// The descriptor is authoritative for a declared field, so the reference comes back
		// carrying the schema it is declared to point at.
		out = makeEntityRef(EntityId::unpack(v), subtypeId);
		return Status::Ok;
	}
	case VarType::Enum: {
		int64_t v = 0;
		__sprt_memcpy(&v, src, sizeof(v));
		out = makeEnum(v, subtypeId);
		return Status::Ok;
	}
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map: {
		BlobHandle h;
		__sprt_memcpy(&h, src, sizeof(h));
		out = makeBlob(type, element, h);
		return Status::Ok;
	}
	case VarType::Nil: break;
	}
	return Status::ErrorInvalidArguemnt;
}

// Whether a value may be written to a field of this shape at all. Everything setField decides
// before it touches the arena, and in the same order.
inline Status checkFieldWrite(VarType type, TypeId subtypeId, const Var &value) {
	if (value.type != type) {
		return Status::ErrorInvalidArguemnt;
	}

	// The descriptor is authoritative: a declared field's family or schema is the one the schema
	// says, and a Var carrying a different one is a mistake rather than an override. Refusing it
	// here is what keeps the two copies of the identity from drifting apart.
	if (subtypeId != NullTypeId) {
		if (type == VarType::Enum && value.e.type != NullTypeId && value.e.type != subtypeId) {
			return Status::ErrorInvalidArguemnt;
		}
		if (type == VarType::EntityRef && value.ent.schema != NullTypeId
				&& value.ent.schema != subtypeId) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	// A container field is written through the blob accessors, which own the handle's lifetime.
	// Letting a field write overwrite one would orphan the block it points at.
	if (isContainerType(type)) {
		return Status::ErrorInvalidArguemnt;
	}
	return Status::Ok;
}

// The store itself, into bytes the caller has already had the arena announce.
inline Status encodeField(uint8_t *dst, VarType type, uint32_t size, const Var &value) {
	switch (type) {
	case VarType::Bool: dst[0] = value.i != 0 ? 1 : 0; return Status::Ok;
	case VarType::Int: __sprt_memcpy(dst, &value.i, sizeof(int64_t)); return Status::Ok;
	case VarType::Float: __sprt_memcpy(dst, &value.f, sizeof(double)); return Status::Ok;
	case VarType::Int32: {
		auto v = int32_t(value.i);
		__sprt_memcpy(dst, &v, sizeof(v));
		return Status::Ok;
	}
	case VarType::UInt32: {
		auto v = uint32_t(value.i);
		__sprt_memcpy(dst, &v, sizeof(v));
		return Status::Ok;
	}
	case VarType::Float32: __sprt_memcpy(dst, &value.v[0], sizeof(float)); return Status::Ok;
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: __sprt_memcpy(dst, value.v, size); return Status::Ok;
	case VarType::EntityRef: __sprt_memcpy(dst, &value.ent.id, sizeof(uint64_t)); return Status::Ok;
	case VarType::Enum: __sprt_memcpy(dst, &value.e.value, sizeof(int64_t)); return Status::Ok;
	default: break;
	}
	return Status::ErrorInvalidArguemnt;
}

// Layer-defined storage of one value, never the host compiler's. Golden offsets have to be
// identical on linux, win32, android and wasm, so nothing here may consult sizeof/alignof. Vectors
// are stored as plain float[N]: sprt::geom::Vec4 is alignas(16), so using it as the storage type
// would give a field alignment of 16 and contradict the table below.
SP_PUBLIC uint32_t getTypeSize(VarType);
SP_PUBLIC uint32_t getTypeAlign(VarType);

// Same rules for an element inside a container: a nested container is a BlobHandle there.
SP_PUBLIC uint32_t getElementStride(VarType);

// validateFieldType is SPFlowValueDecl.h's: a typedef needs it and is read before any component is.

// The file spelling of one field, written. The authored keys - name, type, element, subtype,
// flags, default - and only those: the derived ones (offset/size/align) are describe()'s own. The
// reader, which has something to refuse, is the host's, and the two are one spelling.
SP_PUBLIC void writeFieldSpelling(const FieldDef &, mem_std::Value &out);
SP_PUBLIC void writeFieldSpelling(const FieldDesc &, mem_std::Value &out);

class SP_PUBLIC ComponentType final {
public:
	StringView getName() const { return _name; }
	TypeId getId() const { return _id; }
	uint64_t getSchemaHash() const { return _hash; }

	uint32_t getSize() const { return _size; }
	uint32_t getAlign() const { return _align; }

	SpanView<FieldDesc> getFields() const { return _fields; }
	const FieldDesc *getField(StringView) const;

	// The declaration's own presentation keys, on the same terms a field's `meta` is on: outside
	// the schema hash, forward-compatible, and declared rather than derived. What a field's carries
	// is a unit and a range; what a type's carries is the one thing nothing can be inferred from -
	// what this component is for, under the key `doc`. The file of a declaration is the only place
	// that question has an answer: an author choosing a component for a slot has the name and
	// nothing else. A type that documents nothing carries an empty dictionary and describe() writes
	// no key at all.
	const mem_std::Value &getMeta() const { return _meta; }

	// Field-complete dump: the golden oracle for schema-layout and schema-native. Reports the
	// padding gaps too, so a test can assert they are where the rules say they are.
	void describe(mem_std::Value &) const;

	// Scalar field access. Every mutation goes through Arena::write() over the field's exact
	// extent, so the journal sees it, and no pointer ever escapes these calls: a pointer
	// from read()/write() dies at the next allocator call, and a blob resize is one. Member
	// templates of a class that is not a template: a ComponentType holds no arena, it is a
	// description that any store can be read through, so the kind belongs to the call and not to
	// the type, and one registry serves a debugger's store and a release one at once.
	template <typename A>
	Status getField(const A &, Addr instance, const FieldDesc &, Var &out) const;
	template <typename A>
	Status setField(A &, Addr instance, const FieldDesc &, const Var &) const;

	template <typename A>
	Status getField(const A &, Addr instance, StringView name, Var &out) const;
	template <typename A>
	Status setField(A &, Addr instance, StringView name, const Var &) const;

	// Converts through the value-conversion matrix when the value's type does not match the
	// field's.
	template <typename A>
	Status setFieldFromValue(A &, Addr instance, const FieldDesc &, const mem_std::Value &,
			CastPolicy = CastPolicy::Lossy) const;

	// Instance lifecycle: called when a component is added to or removed from an entity.
	template <typename A>
	Addr createInstance(A &) const; // alloc + initInstance
	// Whole record zeroed, then the defaults. `report` receives a `{field, reason}` entry for every
	// default that did not fit its field, and is the only way to learn that: the record is still
	// initialized, the status is still Ok, and a field whose default was refused keeps its zero.
	// That is right for a hot path - a default that does not fit is a question about the schema,
	// and the place to ask it is an editor or a check. It is also why the two share this walk
	// rather than each having one: a second implementation would apply defaults slightly
	// differently from the one that runs.
	template <typename A>
	Status initInstance(A &, Addr instance, DiagSink * = nullptr) const;
	template <typename A>
	void destroyInstance(A &, Addr instance) const; // frees every blob the record owns
	template <typename A>
	void freeInstance(A &, Addr instance) const; // destroyInstance + free

	// Two parameters: the source and the destination need not be the same kind of store, which is
	// what makes carrying a record out of a debugger's arena into a release one an ordinary copy.
	template <typename D, typename S>
	Status copyInstance(D &dst, Addr dstInstance, const S &src, Addr srcInstance) const;

	template <typename A>
	void encodeInstance(const A &, Addr instance, mem_std::Value &out) const;
	template <typename A>
	Status decodeInstance(A &, Addr instance, const mem_std::Value &,
			CastPolicy = CastPolicy::Lossy) const;

	// Migration by field name: carries a record written against `from` into a record of this type.
	// Fields are matched by name - which is why the name is part of the schema hash - and
	// everything unmatched takes this type's default rather than a zero, so a native component's
	// invariants survive. The source is never mutated: blobs are copied, not moved, so the
	// migration is re-runnable and a failure part-way leaves the source whole, the caller
	// destroying the source when it is done. `report` receives one entry per field describing what
	// happened, because "everything took its default" is otherwise indistinguishable from a
	// successful migration.
	template <typename D, typename S>
	Status migrateInstance(D &dst, Addr dstInstance, const ComponentType &from, const S &src,
			Addr srcInstance, mem_std::Value *report = nullptr) const;

	// Builds in place. Public because the registry owns the storage; use the registry's factories.
	// `meta` is taken by pointer and defaults to none because the layout half of this call has no
	// use for it: it is copied and never read here, and the hash is computed before it is even
	// looked at.
	Status build(StringView name, SpanView<FieldDef>, memory::pool_t *, DiagSink *,
			const mem_std::Value *meta = nullptr);

private:
	StringView _name;
	TypeId _id = NullTypeId;
	uint64_t _hash = 0;
	uint32_t _size = 0;
	uint32_t _align = 1;
	mem_std::Vector<FieldDesc> _fields;

	// Never in `_hash`. A component that gained a line of documentation did not change the layout
	// of anything, and a migration for that would move identical bytes.
	mem_std::Value _meta;
};

// One run of bytes in a record: a field, or the padding the field before it left behind. The same
// walk describe() needs to report its `<padding>` pseudo-entries, and it is one walk rather than
// two - describe() calls this - so a dump for a test and a picture for a person can never disagree
// about where a gap is.
struct RecordSpan {
	uint32_t offset = 0;
	uint32_t size = 0;
	int32_t field = -1; // index into getFields(); NEGATIVE is a gap, which no field owns
};

// Covers the record exactly: spans are contiguous, start at 0 and end at getSize(), tail padding
// included. One span per field, in field order, and a gap span only where there is a gap.
SP_PUBLIC void computeRecordSpans(const ComponentType &, mem_std::Vector<RecordSpan> &out);

// Descriptors are host-side and their addresses are stable: the graph layer's OpDesc holds a raw
// `const ComponentType *localSchema`. Only component instances live in an arena. The registry owns
// a pool for the interned field and component names, and owns the descriptors themselves with
// new/delete - a ComponentType holds a mem_std::Vector, so pool-allocating it would never run its
// destructor. The order vector holds pointers, so it may grow without moving anything. Reading
// declarations out of a description is the host's, on top of this one.
class SP_PUBLIC TypeRegistry {
public:
	~TypeRegistry();

	TypeRegistry() = default;
	TypeRegistry(const TypeRegistry &) = delete;
	TypeRegistry &operator=(const TypeRegistry &) = delete;

	bool init(memory::pool_t *parent = nullptr);

	// The construction paths. All of them end in the same ComponentType::build over the same
	// SpanView<FieldDef>, which is what makes "the same schema described two ways gives the same
	// descriptor" true by construction rather than by two implementations agreeing. Native C++: the
	// macros below spell a FieldDef list with C++ types and pass the host struct's offsetof/sizeof
	// as a witness. A data-driven path is the host's, on top of this one.
	const ComponentType *createNative(StringView name, SpanView<FieldDef>, DiagSink * = nullptr);

	// Programmatic: the interpreter derives a node's local-variable schema from its signature this
	// way.
	const ComponentType *createDerived(StringView name,
			const Callback<void(mem_std::Vector<FieldDef> &)> &, DiagSink * = nullptr);

	// Both follow aliases. That is the whole point of having them: `get("game.Actor")` answers with
	// game.Enemy's descriptor after the rename, and so does `get(makeTypeId("game.Actor"))` - which
	// is the id sitting inside every record and every scene projection written before it.
	const ComponentType *get(TypeId) const;
	const ComponentType *get(StringView) const;

	uint32_t getCount() const { return uint32_t(_order.size()); }
	const ComponentType *getAt(uint32_t i) const { return _order[i]; }

	// The list of members behind a `subtype` an Enum field names. Nothing in the runtime reads them
	// - a value is an int64 and is stored as one - and everything a person reads does.
	const EnumType *createEnum(StringView, SpanView<EnumMemberDef>, DiagSink * = nullptr,
			const mem_std::Value *meta = nullptr);

	const EnumType *getEnum(TypeId) const;
	const EnumType *getEnum(StringView) const;

	uint32_t getEnumCount() const { return uint32_t(_enums.size()); }
	const EnumType *getEnumAt(uint32_t i) const { return _enums[i]; }

	const TypeAlias *createAlias(const AliasSpelling &, DiagSink * = nullptr);

	// Not alias-following, either of them: this is the table itself.
	const TypeAlias *getAlias(TypeId) const;
	const TypeAlias *getAlias(StringView) const;

	uint32_t getAliasCount() const { return uint32_t(_aliases.size()); }
	const TypeAlias *getAliasAt(uint32_t i) const { return _aliases[i]; }

	// Follows the chain by id and answers with the id it finally names - the input itself when
	// nothing aliases it. Stops after MaxAliasDepth hops, and a cycle therefore ends rather than
	// hangs.
	TypeId resolveTypeId(TypeId) const;

	// What a name in the type position resolves to. An alias contributes its element chain, its
	// subtype and its meta, and an outer alias's keys win over the target's.
	struct ResolvedType {
		VarType type = VarType::Nil;
		ElementChain element = 0;
		TypeId subtype = NullTypeId;
		StringView subtypeName;
		mem_std::Value meta;
	};

	bool resolveTypeName(StringView, ResolvedType &, DiagSink *) const;

	// The hints in force for a field: what the declaration it names carries, with the field's own
	// over them, key by key. A field stores only its own - so that an alias edited later reaches
	// every field declared through it - and this is where the two are put together.
	void resolveFieldMeta(const FieldDesc &, mem_std::Value &out) const;

	// Whether anything at all is declared under this name. One namespace over the three tables:
	// `get()` must never have to say which one it looked in.
	bool isNameTaken(TypeId) const;

	memory::pool_t *getPool() const { return _pool; }

	void describe(mem_std::Value &) const;

protected:
	// The one construction path every factory ends in. `meta` is the declaration's presentation
	// keys and reaches only the data-driven factory: a native macro spells C++ types and has
	// nowhere to write a line of documentation, so passing it nothing is the honest answer rather
	// than an omission.
	const ComponentType *create(StringView name, SpanView<FieldDef>, const mem_std::Value *meta,
			DiagSink *);

	const ComponentType *add(ComponentType *, DiagSink *);

	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;
	mem_std::Vector<ComponentType *> _order;
	mem_std::Vector<EnumType *> _enums;
	mem_std::Vector<TypeAlias *> _aliases;
};

// A field of a native C++ component. The macro computes nothing: layout comes from the same
// computeLayout the data-driven path uses. What it adds is the witness - the host struct's own
// offset and size - which turns "the layout is identical on every target" from an assertion into
// something the build checks, on the target where it would differ.
#define SP_FLOW_VALUE_FIELD(Struct, member, nameLit, varType) \
	::stappler::flow::value::FieldDef{.name = ::stappler::StringView(nameLit), \
		.type = (varType), .hostOffset = uint32_t(offsetof(Struct, member)), \
		.hostSize = uint32_t(sizeof(((Struct *)nullptr)->member))}

// An enum family in C++, beside the components that reference it. The macro computes nothing and
// hides nothing - the members are what they say - and it exists so that a native declaration and a
// data-driven one are written the same way round.
#define SP_FLOW_VALUE_ENUM(reg, nameLit, ...) \
	([&]() { \
		const ::stappler::flow::value::EnumMemberDef _xsMembers[] = {__VA_ARGS__}; \
		return (reg).createEnum(::stappler::StringView(nameLit), \
				::stappler::SpanView<::stappler::flow::value::EnumMemberDef>(_xsMembers, \
						sizeof(_xsMembers) / sizeof(_xsMembers[0]))); \
	}())

// A lambda rather than a compound literal: `(const FieldDef[]){...}` is C99 and only a clang
// extension in C++, and a `static` array would give FieldDef's mem_std::Value member a static
// initializer for no reason.
#define SP_FLOW_VALUE_COMPONENT(reg, nameLit, ...) \
	([&]() { \
		const ::stappler::flow::value::FieldDef _xsFields[] = {__VA_ARGS__}; \
		return (reg).createNative(::stappler::StringView(nameLit), \
				::stappler::SpanView<::stappler::flow::value::FieldDef>(_xsFields, \
						sizeof(_xsFields) / sizeof(_xsFields[0]))); \
	}())

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUESCHEMA_H_ */
