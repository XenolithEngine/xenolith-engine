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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUEDECL_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUEDECL_H_

#include "SPFlowValueVar.h"
#include "SPFlowValueDiag.h"

// The named things that are not components. Beside a component, two more kinds of declaration share
// the one namespace: a name already taken by a component, a family or an alias is refused for the
// other two, so `get()` never has to say which table it looked in. An enum family is a name and the
// members behind it - nothing in the runtime needs them, a value being an int64 in a Var, and what
// needs them is everything a person reads: a dropdown, a diagnostic that can say a stored value is
// not a member, a dump that says `North` instead of 0. Members do not enter any layout, so a
// family's members can change without moving a component's schema hash, which is why the family's
// own hash is here for drift detection rather than for migration. A type alias is one name standing
// for another, and one key says which:
//
//     {"name": "Hp",         "target": "int",   "meta": {"min": 0, "unit": "hp"}}
//     {"name": "Ints",       "target": "array", "element": ["int"]}
//     {"name": "game.Actor", "target": "game.Enemy"}
//
// A target is a builtin type name, another alias, a component or an enum family - the resolver does
// not care which, and neither does the spelling. That is what makes one table answer two problems
// at once: the first two lines are typedefs, a spelling a field may use in its `type` position
// carrying the metadata that would otherwise be repeated on every field meaning the same thing; the
// third is indirection, which is what lets a rename leave the old name working - for the file being
// edited, and, the table being keyed by TypeId, for a hash sitting inside a record written months
// ago. An alias is not a new type: a field that reaches a type through one hashes as if it had
// named the target, because the alternative would make the same layout described two ways produce
// two schema hashes. Reading either declaration out of a file is the host's; what is here is the
// declaration once it has been read.

namespace STAPPLER_VERSIONIZED stappler::flow::value {

// How many aliases one name may hop through before the chain is called a mistake. Four, for the
// same reason an element chain gets four: it is more than anything real needs, and it is a bound.
static constexpr uint32_t MaxAliasDepth = 4;

// A field's type and element chain, judged. Declared here rather than beside the schema because a
// typedef needs it too and is read before any component is.
SP_PUBLIC Status validateFieldType(VarType, ElementChain, DiagSink *);

struct EnumMemberDef {
	StringView name;
	int64_t value = 0;

	// Declared and not derived, outside the family hash: what this member means, for whoever picks
	// it. Empty unless the file said something.
	mem_std::Value meta;
};

class SP_PUBLIC EnumType final {
public:
	StringView getName() const { return _name; }
	TypeId getId() const { return _id; }

	// Over the name and the members, and nothing else. Not part of any component's schema hash: a
	// family that gained a member did not change the layout of anything, and a migration triggered
	// by that would be a migration of identical bytes.
	uint64_t getHash() const { return _hash; }

	SpanView<EnumMemberDef> getMembers() const { return _members; }

	bool findValue(StringView name, int64_t &out) const;

	// The first name carrying this value. Two members may share one - `Default` beside `North` is
	// an ordinary thing to write - and the first is what a display shows.
	bool findName(int64_t value, StringView &out) const;

	bool hasValue(int64_t) const;

	// The family's own documentation, on the terms a component's `meta` is on: outside the hash,
	// written only when non-empty.
	const mem_std::Value &getMeta() const { return _meta; }

	void describe(mem_std::Value &) const;

	// Public because the registry owns the storage; use the registry's factories.
	Status build(StringView name, SpanView<EnumMemberDef>, memory::pool_t *, DiagSink *,
			const mem_std::Value *meta = nullptr);

private:
	StringView _name;
	TypeId _id = NullTypeId;
	uint64_t _hash = 0;
	mem_std::Vector<EnumMemberDef> _members;
	mem_std::Value _meta;
};

// What an alias declaration says, before anything is resolved.
struct AliasSpelling {
	StringView name;
	StringView target;
	ElementChain element = 0;
	TypeId subtypeId = NullTypeId;
	StringView subtypeName;
	mem_std::Value meta;
};

class SP_PUBLIC TypeAlias final {
public:
	StringView getName() const { return _name; }
	TypeId getId() const { return _id; }

	// What this name stands for, as it was written. Always present: an alias with no target is not
	// an alias.
	StringView getTargetName() const { return _targetName; }

	// The same name hashed, for the subtype position - where a reference is an id and the chain is
	// followed by id rather than by string.
	TypeId getTarget() const { return _target; }

	// The target names a VarType, so the chain ends here.
	bool isBuiltin() const { return _builtin; }
	VarType getType() const { return _type; }

	// What this alias adds to whatever its target resolves to. An element chain and a subtype are
	// its own when it declares them; `meta` is merged over the target's, key by key, so a typedef
	// of a typedef refines rather than replaces.
	ElementChain getElement() const { return _element; }
	TypeId getSubtype() const { return _subtype; }
	StringView getSubtypeName() const { return _subtypeName; }
	const mem_std::Value &getMeta() const { return _meta; }

	void describe(mem_std::Value &) const;

	Status build(StringView name, StringView target, ElementChain, TypeId subtype,
			StringView subtypeName, const mem_std::Value &meta, memory::pool_t *, DiagSink *);

private:
	StringView _name;
	TypeId _id = NullTypeId;

	StringView _targetName;
	TypeId _target = NullTypeId;

	bool _builtin = false;
	VarType _type = VarType::Nil;

	ElementChain _element = 0;
	TypeId _subtype = NullTypeId;
	StringView _subtypeName;
	mem_std::Value _meta;
};

// The written form of a declaration. The readers, which have something to refuse, are
// the host's; the writers are here, beside describe(), because describe() is one of them.
SP_PUBLIC void writeEnumSpelling(const EnumType &, mem_std::Value &out);
SP_PUBLIC void writeAliasSpelling(const TypeAlias &, mem_std::Value &out);

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUEDECL_H_ */
