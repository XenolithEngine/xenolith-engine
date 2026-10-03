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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUEDIAG_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUEDIAG_H_

#include "SPFlowValue.h"

// What the kernel reports, as numbers. An entry is a domain, a code, the detail that says which
// sentence of the code it is, a severity, a locus and the arguments of the sentence; it carries no
// text of its own. Names - of a field, a node, a pin, a component - are borrowed from the caller's
// input for the duration of DiagSink::add, and everything else is a number. Turning an entry into a
// token, a sentence or a dictionary is the consumer's business, and a consumer without a string
// table can switch on the numbers.
namespace STAPPLER_VERSIONIZED stappler::flow::value {

// The same ordinals as format::DiagSeverity.
enum class DiagSeverity : uint8_t {
	Advice, // about cost, not correctness
	Warning, // it loads and is probably not what was meant
	Error, // it does not load or build
};

// Which vocabulary `code`, `detail` and `locus` belong to. A consumer that adds a layer of its own
// takes a domain from UserDomain up.
enum class DiagDomain : uint16_t {
	Value,
	Graph,
	Codegen,
	UserDomain = 0x100,
};

// One argument of the sentence an entry stands for, in the sentence's order.
enum class DiagArgKind : uint8_t {
	None,
	Name, // a name from the caller's input
	Type, // a VarType, by ordinal in `number`
	Number, // a signed number
	Phrase, // a word or a clause from the domain's own list, by number
	Detail, // another sentence of the same domain, by its detail
	Inner, // what the entry's `inner` said
};

struct DiagArg {
	DiagArgKind kind = DiagArgKind::None;
	StringView name;
	int64_t number = 0;
};

struct Diag {
	static constexpr uint32_t MaxArgs = 6;

	DiagDomain domain = DiagDomain::Value;
	uint16_t code = 0;
	uint16_t detail = 0;
	DiagSeverity severity = DiagSeverity::Error;

	// What the entry is about: a kind from the domain's own list, and what that kind carries.
	uint16_t locus = 0;
	StringView locusName[2];
	int64_t locusValue[4] = {0, 0, 0, 0};

	DiagArg args[MaxArgs];
	uint8_t argCount = 0;

	// The refusal this one carries up, when a layer re-reports what a callee said with its own
	// locus attached. Valid only inside DiagSink::add.
	const Diag *inner = nullptr;

	Diag &name(StringView v) { return push(DiagArg{DiagArgKind::Name, v, 0}); }
	Diag &type(uint8_t v) { return push(DiagArg{DiagArgKind::Type, StringView(), int64_t(v)}); }
	Diag &number(int64_t v) { return push(DiagArg{DiagArgKind::Number, StringView(), v}); }

	Diag &push(const DiagArg &a) {
		if (argCount < MaxArgs) {
			args[argCount++] = a;
		}
		return *this;
	}
};

class SP_PUBLIC DiagSink {
public:
	virtual ~DiagSink() = default;
	virtual void add(const Diag &) = 0;
};

inline void report(DiagSink *sink, const Diag &d) {
	if (sink) {
		sink->add(d);
	}
}

// Keeps the first entry it is given, and the entry that one carries up, for a caller that relays
// a nested refusal. The names the entries borrow stay valid only as long as whatever the callee
// borrowed them from.
class SP_PUBLIC DiagFirst final : public DiagSink {
public:
	DiagFirst() = default;
	DiagFirst(const DiagFirst &) = delete;
	DiagFirst &operator=(const DiagFirst &) = delete;

	void add(const Diag &d) override {
		if (_has) {
			return;
		}
		_first = d;
		_first.inner = nullptr;
		if (d.inner) {
			_inner = *d.inner;
			_inner.inner = nullptr;
			_first.inner = &_inner;
		}
		_has = true;
	}

	bool has() const { return _has; }
	const Diag &get() const { return _first; }

private:
	Diag _first;
	Diag _inner;
	bool _has = false;
};

// The value layer's codes.
enum class DiagCode : uint16_t {
	// A field's type and element chain, as validateFieldType judges them.
	FieldTypeNil,
	FieldTypeUnknown,
	FieldElementUnexpected, // DiagDetail::Scalar, or DiagDetail::Typed with the type as its argument
	FieldElementMissing, // DiagDetail::Array or DiagDetail::NestedArray
	FieldElementUnknown,
	FieldElementTooDeep,

	// Building a component out of field descriptions.
	ComponentNameEmpty,
	FieldNameEmpty,
	FieldNameDuplicate,
	FieldTypeInvalid, // carries what validateFieldType said as `inner`
	HostOffsetMismatch,
	HostSizeMismatch,

	// A default that does not fit its field; a warning, the record is initialized either way.
	DefaultUnfit,

	// The registry's namespace, which components, enum families and aliases share.
	ComponentDuplicate,
	DeclarationDuplicate,
	TypeNameUnknown,
	TypeNameIsReference,
	AliasChainTooDeep,
	AliasChainCycle,

	// Enum families and aliases.
	EnumNameEmpty,
	EnumMemberNameEmpty,
	EnumMemberDuplicate,
	AliasIncomplete,
	AliasSelf,
};

static constexpr uint32_t DiagCodeCount = uint32_t(DiagCode::AliasSelf) + 1;

// Which of a code's sentences an entry is.
enum class DiagDetail : uint16_t {
	Default,
	Scalar, // a scalar declaring an element
	Typed, // a String or a Bytes declaring an element
	Array, // an array without an element
	NestedArray, // a nested array without an element
};

// What a value entry is about; the name is `locusName[0]`.
enum class DiagLocus : uint16_t {
	None,
	Field, // a field of a record
	Declaration, // a component, an enum family or an alias
	Member, // a member of an enum family
};

inline Diag makeDiag(DiagCode code, DiagLocus locus = DiagLocus::None,
		StringView subject = StringView(), DiagSeverity severity = DiagSeverity::Error) {
	Diag d;
	d.domain = DiagDomain::Value;
	d.code = uint16_t(code);
	d.severity = severity;
	d.locus = uint16_t(locus);
	d.locusName[0] = subject;
	return d;
}

inline Diag makeDiag(DiagCode code, DiagDetail detail, DiagLocus locus,
		StringView subject = StringView()) {
	auto d = makeDiag(code, locus, subject);
	d.detail = uint16_t(detail);
	return d;
}

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUEDIAG_H_ */
