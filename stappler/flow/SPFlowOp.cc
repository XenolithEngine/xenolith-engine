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

// The operation registry: signatures, their hash, and the one place a literal is checked against a
// pin's type.

#include "SPFlowOp.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow {

// Little-endian by explicit shifts, exactly as the schema layer hashes a layout, and for the same
// reason: the number has to be identical on every target, and a memcpy of a native integer is the
// only way it could fail to be.
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

static void appendPin(mem_std::Vector<uint8_t> &buf, const PinDesc &pin) {
	appendName(buf, pin.name);
	appendU32(buf, uint32_t(pin.type));
	appendU32(buf, pin.element);
	appendU64(buf, pin.subtypeId);
	// Required changes whether a graph is well-formed, so it is part of the wiring; nothing else
	// about a pin is.
	appendU32(buf, uint32_t(pin.flags & PinFlags::Required));

	// A pin that used to carry an ordinary string and now names a component type is checked against
	// the scene, so an asset written against the other meaning should be told. Appended only when
	// there is a role, so that every signature written before roles existed hashes to the number it
	// always did and only the ones that name something in the scene drift.
	if (pin.role != PinRole::None) {
		appendU32(buf, uint32_t(pin.role));
	}
}

// The parts of a signature appended only when they differ from the default, so every signature
// written before they existed keeps its hash. Each is tagged, so two of them can never spell the
// same bytes.
struct SignatureTail {
	OpParallel parallel = OpParallel::Serial;
	ScopeKind scopeKind = ScopeKind::Loop;
	bool joinsScope = false;
	bool execExclusive = false;
	SpanView<SettingDesc> settings;
};

static uint64_t computeSignatureHash(StringView name, SpanView<PinDesc> dataIn,
		SpanView<PinDesc> dataOut, bool hasExecIn, SpanView<StringView> execOut,
		uint32_t scopeExecOut, const SignatureTail &tail) {
	mem_std::Vector<uint8_t> buf;
	buf.reserve(64 + (dataIn.size() + dataOut.size()) * 32);

	appendName(buf, name);
	appendU32(buf, uint32_t(dataIn.size()));
	for (auto &p : dataIn) { appendPin(buf, p); }
	appendU32(buf, uint32_t(dataOut.size()));
	for (auto &p : dataOut) { appendPin(buf, p); }
	appendU32(buf, hasExecIn ? 1 : 0);
	appendU32(buf, uint32_t(execOut.size()));
	for (auto &e : execOut) { appendName(buf, e); }

	// Which outputs open a scope is wiring: an author connects a body to one and a continuation to
	// the other, and a graph built when the answer was different means something else entirely.
	appendU32(buf, scopeExecOut);

	// Appended only for a classified operation, so unclassified signatures keep their hashes.
	if (tail.parallel != OpParallel::Serial) {
		appendU32(buf, uint32_t(tail.parallel));
	}

	// Which outputs may fire together, what a scope is and where a block closes change how a graph
	// is checked and run, exactly as scopeExecOut does.
	if (tail.execExclusive) {
		appendName(buf, StringView("exec-exclusive"));
	}
	if (tail.scopeKind != ScopeKind::Loop) {
		appendName(buf, StringView("scope-kind"));
		appendU32(buf, uint32_t(tail.scopeKind));
	}
	if (tail.joinsScope) {
		appendName(buf, StringView("joins-scope"));
	}
	if (!tail.settings.empty()) {
		appendName(buf, StringView("settings"));
		appendU32(buf, uint32_t(tail.settings.size()));
		for (auto &it : tail.settings) {
			appendName(buf, it.name);
			appendU32(buf, uint32_t(it.type));
			appendU32(buf, it.element);
			appendU32(buf, uint32_t(it.role));
			appendU32(buf, uint32_t(it.choices.size()));
			for (auto &c : it.choices) { appendName(buf, c); }
		}
	}

	return sprt::hash64(reinterpret_cast<const char *>(buf.data()), buf.size());
}

// The type a literal is written as. Ambiguous for an array or a dictionary - [0, 0] is a Vec2 and
// also an Array<Int> - so those are resolved against the pin instead of guessed at.
static bool naturalScalarType(const mem_std::Value &value, VarType &out) {
	switch (value.getType()) {
	case mem_std::Value::Type::BOOLEAN: out = VarType::Bool; return true;
	case mem_std::Value::Type::INTEGER: out = VarType::Int; return true;
	case mem_std::Value::Type::DOUBLE: out = VarType::Float; return true;
	case mem_std::Value::Type::CHARSTRING: out = VarType::String; return true;
	case mem_std::Value::Type::BYTESTRING: out = VarType::Bytes; return true;
	default: return false;
	}
}

static Status reportConstant(DiagReport &report, DiagCode failure, NodeId node, const PinDesc &pin,
		const DiagText &reason) {
	report.reportPin(DiagSeverity::Error, failure, node, pin.name, reason);
	return Status::ErrorInvalidArguemnt;
}

Status resolveConstant(const mem_std::Value &value, const PinDesc &pin, NodeId node,
		DiagCode failure, DiagReport &report, mem_std::Value &out) {
	out = mem_std::Value();

	// Nothing written means the type's zero, which is a legal answer for every type. Whether that
	// is acceptable - a Required input with no value anywhere - is the caller's question, not this
	// one's.
	if (value.isNull()) {
		return Status::Ok;
	}

	// A container is checked by shape and copied verbatim: turning it into a value needs an arena,
	// and neither registration nor the build has one. Its elements are checked when the interpreter
	// materializes it through blob::decode, and deliberately not re-checked here: a second
	// implementation of "may this element become that" drifts a cell at a time from the first.
	if (value::isContainerType(pin.type)) {
		switch (pin.type) {
		case VarType::String:
			if (!value.isString()) {
				return reportConstant(report, failure, node, pin,
						DiagText(DiagDetail::ConstExpectedString));
			}
			break;
		case VarType::Bytes:
			// Bytes, or the "BASE64:" form the engine's JSON writer emits for them - the same pair
			// the field decoder accepts, so an asset survives a trip through JSON.
			if (!value.isBytes()
					&& !(value.isString()
							&& StringView(value.getString()).starts_with("BASE64:"))) {
				return reportConstant(report, failure, node, pin,
						DiagText(DiagDetail::ConstExpectedBytes));
			}
			break;
		case VarType::Array:
			if (!value.isArray()) {
				return reportConstant(report, failure, node, pin,
						DiagText(DiagDetail::ConstExpectedArray));
			}
			break;
		default:
			if (!value.isDictionary()) {
				return reportConstant(report, failure, node, pin,
						DiagText(DiagDetail::ConstExpectedDict));
			}
			break;
		}
		out = value;
		return Status::Ok;
	}

	// A vector, a colour, an enum or an entity reference has a structural spelling and no other:
	// decodeVar is the one reader of it, and letting it decide keeps this from becoming a second.
	const bool scalar = pin.type == VarType::Bool || pin.type == VarType::Int
			|| pin.type == VarType::Float || pin.type == VarType::Int32 || pin.type == VarType::UInt32
			|| pin.type == VarType::Float32;
	if (!scalar) {
		Var decoded;
		if (value::decodeVar(value, pin.type, decoded) != Status::Ok) {
			return reportConstant(report, failure, node, pin,
					DiagText(DiagDetail::ConstNotValidLiteral).type(pin.type));
		}
		// The descriptor is authoritative about identity. An Enum carries the family it belongs to
		// and an EntityRef the schema it points at, both inside the Var, and decodeVar takes them
		// from the literal - it is handed a VarType and never sees the pin. So without this a
		// literal of one enum family is accepted on a pin of another, which is the same mistake
		// ComponentType::setField refuses one layer down and the same one an edge refuses as
		// SubtypeMismatch. Written but silent, deliberately: a literal that names no family is
		// accepted and takes the pin's, because the pin's is the only answer there could be. Only
		// the identity - the element chain of a container is not checked here, for the reason
		// written above the container branch.
		if (pin.subtypeId != value::NullTypeId
				&& (pin.type == VarType::Enum || pin.type == VarType::EntityRef)) {
			auto &carried = (pin.type == VarType::Enum) ? decoded.e.type : decoded.ent.schema;
			if (carried == value::NullTypeId) {
				carried = pin.subtypeId;
			} else if (carried != pin.subtypeId) {
				// Which of the two words the sentence takes is a fact about the pin, so it is a
				// key of its own rather than a ternary inside a template.
				return reportConstant(report, failure, node, pin,
						DiagText(DiagDetail::ConstUndeclaredFamily)
								.detailArg(pin.type == VarType::Enum ? DiagDetail::ConstEnumFamily
																	 : DiagDetail::ConstSchema));
			}
		}

		if (!value::encodeVar(decoded, out)) {
			return reportConstant(report, failure, node, pin,
					DiagText(DiagDetail::ConstNotProjectable));
		}
		return Status::Ok;
	}

	VarType natural = VarType::Nil;
	if (!naturalScalarType(value, natural)) {
		return reportConstant(report, failure, node, pin,
				DiagText(DiagDetail::ConstExpectedLiteral).type(pin.type));
	}

	Var raw;
	if (value::decodeVar(value, natural, raw) != Status::Ok) {
		return reportConstant(report, failure, node, pin, DiagText(DiagDetail::ConstOutOfRange));
	}

	if (natural != pin.type) {
		// A string literal on a numeric pin would be Parse, which reads bytes out of an arena. Say
		// so rather than reporting the generic refusal: the fix is a conversion node, and the
		// author should not have to work that out from "cannot convert".
		if (value::castNeedsArena(natural, pin.type)) {
			return reportConstant(report, failure, node, pin,
					DiagText(DiagDetail::ConstNeedsConversion).type(natural).type(pin.type));
		}
		Var converted;
		// Lossless: the literal is accepted only if it survives the inverse conversion unchanged. 3
		// reaches a Float input; 3.5 does not reach an Int one. A Float32 pin takes the nearest
		// float, since almost no decimal literal survives the round trip; the range is still
		// enforced.
		const auto policy = pin.type == VarType::Float32 ? value::CastPolicy::Lossy
														 : value::CastPolicy::Lossless;
		if (value::castVar(raw, pin.type, policy, converted) != Status::Ok) {
			return reportConstant(report, failure, node, pin,
					DiagText(DiagDetail::ConstLossy).type(pin.type));
		}
		raw = converted;
	}

	if (!value::encodeVar(raw, out)) {
		return reportConstant(report, failure, node, pin,
				DiagText(DiagDetail::ConstNotProjectable));
	}
	return Status::Ok;
}

bool resolveSetting(const SettingDesc &setting, const mem_std::Value &value, mem_std::Value &out,
		DiagPhrase &reason) {
	out = mem_std::Value();
	auto isChoice = [&](StringView name) {
		for (auto &c : setting.choices) {
			if (c == name) {
				return true;
			}
		}
		return false;
	};

	if (value.isNull()) {
		if (!setting.def.isNull()) {
			out = setting.def;
			return true;
		}
		switch (setting.role) {
		case SettingRole::None: return true;
		case SettingRole::Choice:
			out = mem_std::Value(setting.choices.empty() ? StringView() : setting.choices[0]);
			return true;
		case SettingRole::ChoiceSet:
		case SettingRole::ComponentNames:
			out = mem_std::Value(mem_std::Value::Type::ARRAY);
			return true;
		}
		return true;
	}

	switch (setting.role) {
	case SettingRole::None: {
		PinDesc pin;
		pin.name = setting.name;
		pin.type = setting.type;
		pin.element = setting.element;
		DiagReport silent(nullptr);
		if (resolveConstant(value, pin, NullNodeId, DiagCode::SettingInvalid, silent, out)
				!= Status::Ok) {
			reason = DiagPhrase::SettingValueType;
			return false;
		}
		return true;
	}
	case SettingRole::Choice:
		if (!value.isString() || !isChoice(StringView(value.getString()))) {
			reason = DiagPhrase::SettingValueNotChoice;
			return false;
		}
		out = value;
		return true;
	case SettingRole::ChoiceSet:
	case SettingRole::ComponentNames:
		if (!value.isArray()) {
			reason = DiagPhrase::SettingValueNotNames;
			return false;
		}
		for (size_t i = 0; i < value.size(); ++i) {
			auto &item = value.getValue(i);
			if (!item.isString() || item.getString().empty()) {
				reason = DiagPhrase::SettingValueNotNames;
				return false;
			}
			auto name = StringView(item.getString());
			if (setting.role == SettingRole::ChoiceSet && !isChoice(name)) {
				reason = DiagPhrase::SettingValueNotChoice;
				return false;
			}
			for (size_t j = 0; j < i; ++j) {
				if (StringView(value.getValue(j).getString()) == name) {
					reason = DiagPhrase::SettingValueTwice;
					return false;
				}
			}
		}
		out = value;
		return true;
	}
	reason = DiagPhrase::SettingRoleUnknown;
	return false;
}

StringView readNodeFamilyName(const OpDesc &op, const mem_std::Value &params) {
	auto pin = op.getFamilyPin();
	if (pin == NullPin) {
		return StringView();
	}
	auto &desc = op.getDataIn()[pin];
	auto &param = params.getValue(desc.name);
	if (param.isString()) {
		return StringView(param.getString());
	}
	if (param.isNull() && desc.def.isString()) {
		return StringView(desc.def.getString());
	}
	return StringView();
}

GpuForm classifyGpuOp(const OpDesc &op) {
	auto name = op.getShaderName();
	if (name.empty()) {
		return GpuForm::None;
	}
	if (name[0] != '@') {
		return (op.getFlags() & OpFlags::ShaderForm) != OpFlags::None ? GpuForm::Body : GpuForm::None;
	}
	if (name == StringView("@fireAll")) {
		return GpuForm::FireAll;
	} else if (name == StringView("@branch")) {
		return GpuForm::Branch;
	} else if (name == StringView("@sceneGet")) {
		return GpuForm::SceneGet;
	} else if (name == StringView("@sceneSet")) {
		return GpuForm::SceneSet;
	} else if (name == StringView("@sceneHas")) {
		return GpuForm::SceneHas;
	} else if (name == StringView("@narrow")) {
		return GpuForm::Narrow;
	} else if (name == StringView("@widen")) {
		return GpuForm::Widen;
	} else if (name == StringView("@passthrough")) {
		return GpuForm::Passthrough;
	}
	return GpuForm::None;
}

StringView getGpuFormName(GpuForm form) {
	switch (form) {
	case GpuForm::None: return StringView("none");
	case GpuForm::Body: return StringView("body");
	case GpuForm::FireAll: return StringView("fire-all");
	case GpuForm::Branch: return StringView("branch");
	case GpuForm::SceneGet: return StringView("scene-get");
	case GpuForm::SceneSet: return StringView("scene-set");
	case GpuForm::SceneHas: return StringView("scene-has");
	case GpuForm::Narrow: return StringView("narrow");
	case GpuForm::Widen: return StringView("widen");
	case GpuForm::Passthrough: return StringView("passthrough");
	}
	return StringView("?");
}

PinDesc nodeDataOut(const OpDesc &op, uint32_t pin, TypeId family) {
	auto out = op.getDataOut()[pin];
	if (carriesNodeFamily(op, out)) {
		out.subtypeId = family;
	}
	return out;
}

// The rule is value::valueTypesMeet; what is left here is the graph's name for each of the three
// refusals. A pin is a ValueShape plus the things this question never reads - a name, flags, a role
// and a default - so the unpacking is the whole of the adapter.
bool edgeTypesMeet(const PinDesc &from, const PinDesc &to, value::CastRule &outRule,
		DiagCode &outFailure) {
	switch (value::valueTypesMeet(value::ValueShape{from.type, from.element, from.subtypeId},
			value::ValueShape{to.type, to.element, to.subtypeId}, outRule)) {
	case value::TypeMeet::Ok: return true;
	case value::TypeMeet::Tag: outFailure = DiagCode::TypeMismatch; break;
	case value::TypeMeet::Element: outFailure = DiagCode::ElementMismatch; break;
	case value::TypeMeet::Subtype: outFailure = DiagCode::SubtypeMismatch; break;
	}
	return false;
}

static bool findPin(const mem_std::Vector<PinDesc> &pins, StringView name, uint32_t &out) {
	for (uint32_t i = 0; i < uint32_t(pins.size()); ++i) {
		if (pins[i].name == name) {
			out = i;
			return true;
		}
	}
	return false;
}

bool OpDesc::findDataIn(StringView name, uint32_t &out) const {
	return findPin(_dataIn, name, out);
}

bool OpDesc::findDataOut(StringView name, uint32_t &out) const {
	return findPin(_dataOut, name, out);
}

bool OpDesc::findExecOut(StringView name, uint32_t &out) const {
	for (uint32_t i = 0; i < uint32_t(_execOut.size()); ++i) {
		if (_execOut[i] == name) {
			out = i;
			return true;
		}
	}
	return false;
}

Status OpDesc::build(const OpDef &def, memory::pool_t *pool, DiagReport &report) {
	if (def.name.empty()) {
		report.report(DiagSeverity::Error, DiagCode::OpNameEmpty,
				DiagText(DiagDetail::OpNameEmpty));
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

	_name = intern(def.name);
	if (_name.empty()) {
		return Status::ErrorOutOfHostMemory;
	}
	_id = makeTypeId(_name);
	_flags = def.flags;
	_invoke = def.invoke;
	_parallel = def.parallel;
	// Interned like the name and checked like nothing: what a generator writes into a source file
	// is the generator's problem, and a name that does not resolve is a compile error in the unit,
	// which is where it belongs. Empty is the ordinary answer.
	_inlineName = intern(def.inlineName);
	_shaderName = intern(def.shaderName);
	_shaderSource = intern(def.shaderSource);
	_function = intern(def.function);
	_functionRole = def.functionRole;
	_functionExit = def.functionExit;
	_hasExecIn = def.hasExecIn;

	// Every problem is reported before returning, so an author fixing a signature sees the whole
	// list. `ok` decides the verdict; it is not an early exit.
	bool ok = true;

	auto takePins = [&](SpanView<PinDesc> src, mem_std::Vector<PinDesc> &dst, bool isInput) {
		if (src.size() > MaxDataPins) {
			ok = false;
			report.report(DiagSeverity::Error, DiagCode::OpPinLimit,
					DiagText(DiagDetail::OpPinLimit).name(_name).number(int64_t(MaxDataPins)));
			return;
		}
		for (auto &p : src) {
			PinDesc pin;
			pin.name = intern(p.name);
			pin.type = p.type;
			pin.element = p.element;
			pin.subtypeId = p.subtypeId;
			pin.flags = p.flags;
			pin.role = p.role;

			if (pin.name.empty()) {
				ok = false;
				report.report(DiagSeverity::Error, DiagCode::OpPinInvalid,
						DiagText(DiagDetail::OpPinName).name(_name));
				continue;
			}

			uint32_t dup = 0;
			if (findPin(dst, pin.name, dup)) {
				ok = false;
				report.report(DiagSeverity::Error, DiagCode::OpPinDuplicate,
						DiagText(DiagDetail::OpPinDuplicate).name(_name).name(pin.name));
				continue;
			}

			// The same rule a component field obeys, applied by the same function: a pin's type and
			// element chain are a field's type and element chain, and there is no reason for this
			// layer to have its own opinion about which pairs are legal.
			value::DiagFirst typeDiag;
			if (value::validateFieldType(pin.type, pin.element, &typeDiag) != Status::Ok) {
				ok = false;
				report.report(DiagSeverity::Error, DiagCode::OpPinInvalid,
						DiagText(DiagDetail::OpPinInvalid).name(_name).name(pin.name).innerArg(),
						typeDiag.has() ? &typeDiag.get() : nullptr);
				continue;
			}

			if (isInput && !p.def.isNull()) {
				DiagReport defaults(nullptr);
				mem_std::Value canonical;
				if (resolveConstant(p.def, pin, NullNodeId, DiagCode::OpDefaultInvalid, defaults,
							canonical)
						!= Status::Ok) {
					ok = false;
					report.report(DiagSeverity::Error, DiagCode::OpDefaultInvalid,
							DiagText(DiagDetail::OpDefaultInvalid).name(_name).name(pin.name));
					continue;
				}
				// Stored converted, so the build never repeats the conversion and a dump shows what
				// the operation will actually see.
				pin.def = sprt::move(canonical);
			}

			dst.emplace_back(sprt::move(pin));
		}
	};

	takePins(def.dataIn, _dataIn, true);
	takePins(def.dataOut, _dataOut, false);

	if (def.execOut.size() > MaxExecOut) {
		ok = false;
		report.report(DiagSeverity::Error, DiagCode::OpPinLimit,
				DiagText(DiagDetail::OpExecLimit).name(_name).number(int64_t(MaxExecOut)));
	} else {
		for (auto &e : def.execOut) {
			auto name = intern(e);
			if (name.empty()) {
				ok = false;
				report.report(DiagSeverity::Error, DiagCode::OpPinInvalid,
						DiagText(DiagDetail::OpExecName).name(_name));
				continue;
			}
			uint32_t dup = 0;
			if (findExecOut(name, dup)) {
				ok = false;
				report.report(DiagSeverity::Error, DiagCode::OpPinDuplicate,
						DiagText(DiagDetail::OpExecDuplicate).name(_name).name(name));
				continue;
			}
			_execOut.emplace_back(name);
		}
	}

	// Search words. Interned like every other string here, and refused for nothing: an empty one is
	// dropped rather than reported, because a word an author might have typed is not a property of
	// the graph and a registration must not fail over one.
	for (auto &s : def.synonyms) {
		auto word = intern(s);
		if (!word.empty()) {
			_synonyms.emplace_back(word);
		}
	}

	if (!ok) {
		return Status::ErrorInvalidArguemnt;
	}

	// A scope bit for an exec output that does not exist would be a promise nothing can keep.
	if (_execOut.size() < MaxExecOut && (def.scopeExecOut >> _execOut.size()) != 0) {
		report.report(DiagSeverity::Error, DiagCode::OpPinInvalid,
				DiagText(DiagDetail::OpScopeUnknown).name(_name));
		return Status::ErrorInvalidArguemnt;
	}
	// At most one, and the reason is the outputs rather than the exec pins. A node that opens a
	// loop hands its body a value per iteration, so anything computed from that value belongs to
	// the body too - and with two bodies there would be no answer to which one.
	if ((def.scopeExecOut & (def.scopeExecOut - 1)) != 0) {
		report.report(DiagSeverity::Error, DiagCode::OpPinInvalid,
				DiagText(DiagDetail::OpScopeMany).name(_name));
		return Status::ErrorInvalidArguemnt;
	}
	_scopeExecOut = def.scopeExecOut;
	_scopeKind = def.scopeKind;
	_joinsScope = def.joinsScope;
	_execExclusive = def.execExclusive;

	// The scene groups. The grouping is the pin order: a component name opens a group, and the
	// field name and the value after it belong to that group. Worked out here, once per signature,
	// so that neither the build nor the interpreter ever parses roles per node. Deliberately after
	// the `ok` gate above: a pin that was refused is not in _dataIn, so grouping a partial list
	// would report the same mistake a second time in a shape that names the wrong pin.
	bool roleOk = true;
	auto roleFault = [&](StringView pin, DiagPhrase reason) {
		roleOk = false;
		report.report(DiagSeverity::Error, DiagCode::OpPinRoleInvalid,
				DiagText(DiagDetail::OpPinRole).name(_name).name(pin).phrase(reason));
	};

	for (uint32_t i = 0; i < uint32_t(_dataIn.size()); ++i) {
		auto &pin = _dataIn[i];
		switch (pin.role) {
		case PinRole::None: break;
		case PinRole::ComponentName:
		case PinRole::ComponentNameOptional:
			if (pin.type != VarType::String || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleComponentNameType);
				break;
			}
			_sceneGroups.emplace_back(SceneGroup{.componentPin = i,
				.optional = pin.role == PinRole::ComponentNameOptional});
			break;
		case PinRole::FieldName:
			if (pin.type != VarType::String || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleFieldNameType);
			} else if (_sceneGroups.empty()) {
				roleFault(pin.name, DiagPhrase::RoleFieldNameOrphan);
			} else if (_sceneGroups.back().fieldPin != NullPin) {
				roleFault(pin.name, DiagPhrase::RoleFieldNameSecond);
			} else {
				_sceneGroups.back().fieldPin = i;
			}
			break;
		case PinRole::FieldValue:
			if (_sceneGroups.empty()) {
				roleFault(pin.name, DiagPhrase::RoleFieldValueOrphan);
			} else if (_sceneGroups.back().valuePin != NullPin) {
				roleFault(pin.name, DiagPhrase::RoleFieldValueSecond);
			} else {
				_sceneGroups.back().valuePin = i;
			}
			break;
		case PinRole::EnumFamily:
			if (pin.type != VarType::String || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleFamilyNameType);
			} else if (_familyPin != NullPin) {
				roleFault(pin.name, DiagPhrase::RoleFamilyNameSecond);
			} else {
				_familyPin = i;
			}
			break;
		case PinRole::EntityTarget:
			if (pin.type != VarType::EntityRef || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleTargetType);
			} else if (_targetPin != NullPin) {
				roleFault(pin.name, DiagPhrase::RoleTargetSecond);
			} else {
				_targetPin = i;
			}
			break;
		case PinRole::BranchValue:
			if (_hasExecIn) {
				roleFault(pin.name, DiagPhrase::RoleBranchNoExec);
			} else if (_branchPin != NullPin) {
				roleFault(pin.name, DiagPhrase::RoleBranchSecond);
			} else {
				_branchPin = i;
			}
			break;
		case PinRole::EntityName:
			if (pin.type != VarType::String || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleEntityNameType);
			}
			break;
		case PinRole::ExtensionName:
			// A group of its own, so it neither opens nor joins a scene group: an extension's
			// vocabulary is the extension's, and there is nothing here a field name could belong
			// to.
			if (pin.type != VarType::String || pin.element != 0) {
				roleFault(pin.name, DiagPhrase::RoleExtensionNameType);
			} else {
				_extensionGroups.emplace_back(ExtensionGroup{.namePin = i});
			}
			break;
		}
	}

	for (uint32_t i = 0; i < uint32_t(_dataOut.size()); ++i) {
		auto &pin = _dataOut[i];
		if (pin.role == PinRole::None) {
			continue;
		}
		if (pin.role == PinRole::EnumFamily) {
			if (pin.type != VarType::Enum || pin.element != 0
					|| pin.subtypeId != value::NullTypeId) {
				roleFault(pin.name, DiagPhrase::RoleFamilyOutputType);
			} else if (_familyPin == NullPin) {
				roleFault(pin.name, DiagPhrase::RoleFamilyOutputOrphan);
			}
			continue;
		}
		// An output can only ever be the value of a group: naming a component, a field or an
		// extension is telling the operation what to reach for, and an output is what it hands
		// back.
		if (pin.role != PinRole::FieldValue) {
			roleFault(pin.name, DiagPhrase::RoleOutputValueOnly);
		} else if (_sceneGroups.empty()) {
			roleFault(pin.name, DiagPhrase::RoleFieldValueOrphan);
		} else if (_sceneGroups.back().valuePin != NullPin) {
			roleFault(pin.name, DiagPhrase::RoleFieldValueSecond);
		} else {
			// The last group the inputs opened: `scene.getInt` names the component and the field on
			// its inputs and hands the value back on an output, and a value has nowhere else to be.
			_sceneGroups.back().valuePin = i;
			_sceneGroups.back().valueIsOutput = true;
		}
	}

	for (auto &g : _sceneGroups) {
		if (g.valuePin != NullPin && g.fieldPin == NullPin) {
			roleFault(_dataIn[g.componentPin].name, DiagPhrase::RoleFieldValueNoField);
		}
		// An output value is a read; otherwise the operation's own flag decides, so a key such as
		// the value of `scene.findByField` stays a read.
		if (!g.valueIsOutput && (_flags & OpFlags::WritesScene) != OpFlags::None) {
			g.access = SceneAccess::Write;
		}
	}

	// What the operation names itself, appended after the pin-derived groups. Such a group has no
	// pins at all, and `componentPin == NullPin` is what tells the two apart wherever it matters.
	for (auto &r : def.sceneRefs) {
		SceneRef ref;
		ref.component = intern(r.component);
		ref.field = intern(r.field);
		ref.type = r.type;
		ref.optional = r.optional;
		ref.access = r.access;
		ref.targeted = r.targeted;

		if (ref.component.empty()) {
			roleFault(StringView("<scene>"), DiagPhrase::RefNoComponent);
			continue;
		}
		if (ref.type != VarType::Nil && ref.field.empty()) {
			roleFault(ref.component, DiagPhrase::RefTypeNoField);
			continue;
		}

		if (ref.targeted && _targetPin == NullPin) {
			roleFault(ref.component, DiagPhrase::RefTargetNoPin);
			continue;
		}

		_sceneRefs.emplace_back(sprt::move(ref));
		_sceneGroups.emplace_back(SceneGroup{.optional = r.optional, .access = r.access});
	}

	if (!roleOk) {
		return Status::ErrorInvalidArguemnt;
	}

	if (takeSettings(def, pool, report) != Status::Ok) {
		return Status::ErrorInvalidArguemnt;
	}

	if (checkParallel(def, report) != Status::Ok) {
		return Status::ErrorInvalidArguemnt;
	}

	SignatureTail tail;
	tail.parallel = _parallel;
	tail.scopeKind = _scopeKind;
	tail.joinsScope = _joinsScope;
	tail.execExclusive = _execExclusive;
	tail.settings = SpanView<SettingDesc>(_settings.data(), _settings.size());
	_hash = computeSignatureHash(_name, SpanView<PinDesc>(_dataIn.data(), _dataIn.size()),
			SpanView<PinDesc>(_dataOut.data(), _dataOut.size()), _hasExecIn,
			SpanView<StringView>(_execOut.data(), _execOut.size()), _scopeExecOut, tail);
	return Status::Ok;
}

bool OpDesc::findSetting(StringView name, uint32_t &out) const {
	for (uint32_t i = 0; i < uint32_t(_settings.size()); ++i) {
		if (_settings[i].name == name) {
			out = i;
			return true;
		}
	}
	return false;
}

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

Status OpDesc::takeSettings(const OpDef &def, memory::pool_t *pool, DiagReport &report) {
	bool ok = true;
	auto fault = [&](StringView setting, DiagPhrase reason) {
		ok = false;
		report.report(DiagSeverity::Error, DiagCode::OpSettingInvalid,
				DiagText(DiagDetail::OpSetting).name(_name).name(setting).phrase(reason));
	};

	size_t choiceCount = 0;
	for (auto &it : def.settings) { choiceCount += it.choices.size(); }
	// Reserved up front: each setting's choices is a slice of this vector, and a reallocation would
	// leave the earlier slices pointing at freed storage.
	_settingChoices.reserve(choiceCount);
	_settings.reserve(def.settings.size());

	const auto stringArray = value::makeChain(VarType::String);
	for (auto &src : def.settings) {
		SettingDesc it;
		it.name = internName(pool, src.name);
		it.type = src.type;
		it.element = src.element;
		it.role = src.role;

		if (it.name.empty()) {
			fault(StringView("<setting>"), DiagPhrase::SettingNoName);
			continue;
		}
		uint32_t dup = 0;
		if (findSetting(it.name, dup)) {
			fault(it.name, DiagPhrase::SettingSecond);
			continue;
		}

		if (value::validateFieldType(it.type, it.element, nullptr) != Status::Ok) {
			fault(it.name, DiagPhrase::SettingTypeIllegal);
			continue;
		}
		const bool isStringArray = it.type == VarType::Array && it.element == stringArray;
		switch (it.role) {
		case SettingRole::None:
			if (!src.choices.empty()) {
				fault(it.name, DiagPhrase::SettingChoicesNotChoice);
				continue;
			}
			break;
		case SettingRole::Choice:
			if (it.type != VarType::String || it.element != 0 || src.choices.empty()) {
				fault(it.name, DiagPhrase::SettingChoiceShape);
				continue;
			}
			break;
		case SettingRole::ChoiceSet:
			if (!isStringArray || src.choices.empty()) {
				fault(it.name, DiagPhrase::SettingChoiceSetShape);
				continue;
			}
			break;
		case SettingRole::ComponentNames:
			if (!isStringArray || !src.choices.empty()) {
				fault(it.name, DiagPhrase::SettingComponentNamesShape);
				continue;
			}
			break;
		default: fault(it.name, DiagPhrase::SettingRoleUnknown); continue;
		}

		auto first = _settingChoices.size();
		for (auto &c : src.choices) { _settingChoices.emplace_back(internName(pool, c)); }
		it.choices = SpanView<StringView>(_settingChoices.data() + first, src.choices.size());

		if (!src.def.isNull()) {
			DiagPhrase reason = DiagPhrase::None;
			mem_std::Value canonical;
			if (!resolveSetting(it, src.def, canonical, reason)) {
				fault(it.name, reason);
				continue;
			}
			it.def = sprt::move(canonical);
		}

		_settings.emplace_back(sprt::move(it));
	}

	return ok ? Status::Ok : Status::ErrorInvalidArguemnt;
}

Status OpDesc::checkParallel(const OpDef &def, DiagReport &report) const {
	bool ok = true;
	auto fault = [&](DiagPhrase reason) {
		ok = false;
		report.report(DiagSeverity::Error, DiagCode::OpParallelInvalid,
				DiagText(DiagDetail::OpParallel).name(_name).phrase(reason));
	};

	if (uint32_t(_parallel) >= OpParallelCount) {
		fault(DiagPhrase::ParallelClassUnknown);
		return Status::ErrorInvalidArguemnt;
	}
	if (uint32_t(_scopeKind) > uint32_t(ScopeKind::Parallel)) {
		fault(DiagPhrase::ScopeKindUnknown);
	}
	if (_scopeKind == ScopeKind::Parallel && _scopeExecOut == 0) {
		fault(DiagPhrase::ParallelScopeNoScope);
	}
	if (_joinsScope && (!_hasExecIn || _scopeExecOut != 0)) {
		fault(DiagPhrase::ParallelBarrierShape);
	}
	if (!ok) {
		return Status::ErrorInvalidArguemnt;
	}
	if (_parallel == OpParallel::Serial) {
		if (_scopeKind != ScopeKind::Loop || _joinsScope) {
			fault(DiagPhrase::ParallelBlockIsFlow);
			return Status::ErrorInvalidArguemnt;
		}
		return Status::Ok;
	}

	const bool reads = (_flags & OpFlags::ReadsScene) != OpFlags::None;
	const bool writes = (_flags & OpFlags::WritesScene) != OpFlags::None;
	const bool hostCall = (_flags & OpFlags::HostCall) != OpFlags::None;
	const bool scene = !_sceneGroups.empty();

	if (hostCall) {
		fault(DiagPhrase::ParallelHostSerial);
	}
	if (!_extensionGroups.empty()) {
		fault(DiagPhrase::ParallelExtensionSerial);
	}
	if (_scopeExecOut != 0 && _parallel != OpParallel::Flow) {
		fault(DiagPhrase::ParallelScopeFlowOnly);
	}
	if (_joinsScope && _parallel != OpParallel::Flow) {
		fault(DiagPhrase::ParallelBarrierFlowOnly);
	}

	const auto pinGroups = _sceneGroups.size() - _sceneRefs.size();

	switch (_parallel) {
	case OpParallel::Serial: break;
	case OpParallel::Pure:
		if (_hasExecIn || !_execOut.empty()) {
			fault(DiagPhrase::ParallelPureNoExec);
		}
		if (!def.locals.empty()) {
			fault(DiagPhrase::ParallelPureNoLocals);
		}
		[[fallthrough]];
	case OpParallel::Local:
	case OpParallel::Flow: {
		// A fan-out's query reads the scene before any branch runs, on the machine's thread.
		bool query = false;
		if (_parallel == OpParallel::Flow && _scopeKind == ScopeKind::Parallel) {
			for (auto &it : _settings) {
				query = query || it.role == SettingRole::ComponentNames;
			}
		}
		if ((reads && !query) || writes || scene) {
			fault(DiagPhrase::ParallelClassNoScene);
		}
		break;
	}
	case OpParallel::SceneRead:
		if (!reads || writes) {
			fault(DiagPhrase::ParallelSceneReadOnly);
		}
		break;
	case OpParallel::SceneWrite:
		if (!writes) {
			fault(DiagPhrase::ParallelSceneWrite);
		}
		for (uint32_t i = 0; i < uint32_t(_sceneGroups.size()); ++i) {
			auto &g = _sceneGroups[i];
			if (g.access != SceneAccess::Write) {
				continue;
			}
			const bool isRef = g.componentPin == NullPin;
			const SceneRef *ref = isRef ? &_sceneRefs[i - pinGroups] : nullptr;
			if (isRef ? ref->field.empty() : g.fieldPin == NullPin) {
				fault(DiagPhrase::ParallelAddRemoveSerial);
				continue;
			}
			if (isRef ? !ref->targeted : _targetPin == NullPin) {
				fault(DiagPhrase::ParallelWriteTarget);
			}
			auto type = isRef ? ref->type
							  : (g.valuePin != NullPin ? _dataIn[g.valuePin].type : VarType::Nil);
			if (type == VarType::Nil || value::isContainerType(type)) {
				fault(DiagPhrase::ParallelWriteFixed);
			}
		}
		break;
	}

	return ok ? Status::Ok : Status::ErrorInvalidArguemnt;
}

// The local schema. Nothing here computes a layout: the fields are handed to the same
// ComponentType::build the native and data-driven paths use, so a derived schema is a schema in
// every respect - same offsets, same hash, same accessors - and the golden offsets stay identical
// on every ABI for the same reason everyone else's do. Inputs are not fields: a node reads an input
// from the record of whoever produced it, and copying it into the consumer's record too would store
// every value twice and give the interpreter two places to disagree about. Nor is the readiness of
// the outputs here - that is the interpreter's bookkeeping, it has the same shape for every node,
// and it belongs in one uniform component of its own.
Status OpDesc::deriveLocalSchema(const OpDef &def, value::TypeRegistry &types, DiagReport &report) {
	_localSchema = nullptr;
	if (_dataOut.empty() && def.locals.empty()) {
		return Status::Ok;
	}

	mem_std::Vector<value::FieldDef> fields;
	fields.reserve(_dataOut.size() + def.locals.size());

	for (auto &p : _dataOut) {
		value::FieldDef f;
		f.name = p.name;
		f.type = p.type;
		f.element = p.element;
		f.subtypeId = p.subtypeId;
		fields.emplace_back(sprt::move(f));
	}

	// Collisions are found before the type is created: a half-built type left in the registry would
	// make the next attempt to register a corrected signature fail on a name that is already taken.
	bool ok = true;
	for (auto &l : def.locals) {
		bool duplicate = false;
		for (auto &f : fields) {
			if (f.name == l.name) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			ok = false;
			report.report(DiagSeverity::Error, DiagCode::OpLocalInvalid,
					DiagText(DiagDetail::OpLocalCollides).name(_name).name(l.name));
			continue;
		}
		fields.emplace_back(l);
	}

	if (!ok) {
		return Status::ErrorInvalidArguemnt;
	}

	auto name = mem_std::toString("op.", _name, ".locals");
	value::DiagFirst diag;
	auto type = static_cast<value::TypeRegistry &>(types).createDerived(StringView(name),
			[&](mem_std::Vector<value::FieldDef> &out) { out = sprt::move(fields); }, &diag);
	if (!type) {
		report.report(DiagSeverity::Error, DiagCode::OpLocalInvalid,
				DiagText(DiagDetail::OpLocalRefused).name(_name).innerArg(),
				diag.has() ? &diag.get() : nullptr);
		return Status::ErrorInvalidArguemnt;
	}

	_localSchema = type;
	return Status::Ok;
}

StringView getPinRoleName(PinRole r) {
	switch (r) {
	case PinRole::None: return StringView("none");
	case PinRole::ComponentName: return StringView("component");
	case PinRole::ComponentNameOptional: return StringView("component?");
	case PinRole::FieldName: return StringView("field");
	case PinRole::FieldValue: return StringView("value");
	case PinRole::ExtensionName: return StringView("extension");
	case PinRole::EntityTarget: return StringView("target");
	case PinRole::EnumFamily: return StringView("family");
	case PinRole::BranchValue: return StringView("branch");
	case PinRole::EntityName: return StringView("entity");
	}
	return StringView("?");
}

StringView getScopeKindName(ScopeKind k) {
	switch (k) {
	case ScopeKind::Loop: return StringView("loop");
	case ScopeKind::Parallel: return StringView("parallel");
	case ScopeKind::Function: return StringView("function");
	}
	return StringView("?");
}

StringView getSettingRoleName(SettingRole r) {
	switch (r) {
	case SettingRole::None: return StringView("none");
	case SettingRole::Choice: return StringView("choice");
	case SettingRole::ChoiceSet: return StringView("choice-set");
	case SettingRole::ComponentNames: return StringView("components");
	}
	return StringView("?");
}

StringView getOpParallelName(OpParallel p) {
	switch (p) {
	case OpParallel::Serial: return StringView("serial");
	case OpParallel::Pure: return StringView("pure");
	case OpParallel::Local: return StringView("local");
	case OpParallel::Flow: return StringView("flow");
	case OpParallel::SceneRead: return StringView("scene-read");
	case OpParallel::SceneWrite: return StringView("scene-write");
	}
	return StringView("?");
}

bool readOpParallel(StringView name, OpParallel &out) {
	for (uint32_t i = 0; i < OpParallelCount; ++i) {
		if (getOpParallelName(OpParallel(i)) == name) {
			out = OpParallel(i);
			return true;
		}
	}
	return false;
}

static void describePins(mem_std::Value &out, StringView key,
		const mem_std::Vector<PinDesc> &pins) {
	auto &arr = out.newArray(key);
	for (auto &p : pins) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setString(p.name, "name");
		entry.setString(value::getVarTypeName(p.type), "type");
		if (p.element != 0) {
			auto &chain = entry.newArray("element");
			auto c = p.element;
			while (value::chainHead(c) != VarType::Nil) {
				chain.addString(value::getVarTypeName(value::chainHead(c)));
				c = value::chainTail(c);
			}
		}
		if (p.subtypeId != value::NullTypeId) {
			entry.setInteger(int64_t(p.subtypeId), "subtype");
		}
		if ((p.flags & PinFlags::Required) != PinFlags::None) {
			entry.setBool(true, "required");
		}
		if (p.role != PinRole::None) {
			entry.setString(getPinRoleName(p.role), "role");
		}
		if (!p.def.isNull()) {
			entry.setValue(p.def, "default");
		}
		arr.addValue(sprt::move(entry));
	}
}

void OpDesc::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	out.setString(_name, "name");
	// A TypeId does not fit data::Value's signed integer, so it goes in bit for bit and may print
	// negative. It round-trips exactly, which is what a golden dump needs.
	out.setInteger(int64_t(_id), "id");
	out.setInteger(int64_t(_hash), "hash");

	if (_flags != OpFlags::None) {
		auto &flags = out.newArray("flags");
		if ((_flags & OpFlags::Pure) != OpFlags::None) {
			flags.addString(StringView("pure"));
		}
		if ((_flags & OpFlags::ReadsScene) != OpFlags::None) {
			flags.addString(StringView("reads-scene"));
		}
		if ((_flags & OpFlags::WritesScene) != OpFlags::None) {
			flags.addString(StringView("writes-scene"));
		}
		if ((_flags & OpFlags::HostCall) != OpFlags::None) {
			flags.addString(StringView("host-call"));
		}
		if ((_flags & OpFlags::ShaderForm) != OpFlags::None) {
			flags.addString(StringView("shader-form"));
		}
	}

	if (_parallel != OpParallel::Serial) {
		out.setString(getOpParallelName(_parallel), "parallel");
	}

	describePins(out, StringView("dataIn"), _dataIn);
	describePins(out, StringView("dataOut"), _dataOut);

	out.setBool(_hasExecIn, "execIn");
	auto &execOut = out.newArray("execOut");
	for (auto &e : _execOut) { execOut.addString(e); }

	// By name and only when there is one, so that a dump of an operation without loops reads as it
	// always did.
	if (_scopeExecOut != 0) {
		auto &scopes = out.newArray("scopeExecOut");
		for (uint32_t i = 0; i < uint32_t(_execOut.size()); ++i) {
			if (opensScope(i)) {
				scopes.addString(_execOut[i]);
			}
		}
	}

	if (_scopeKind != ScopeKind::Loop) {
		out.setString(getScopeKindName(_scopeKind), "scopeKind");
	}
	if (_joinsScope) {
		out.setBool(true, "joinsScope");
	}
	if (_execExclusive) {
		out.setBool(true, "execExclusive");
	}

	if (!_settings.empty()) {
		auto &arr = out.newArray("settings");
		for (auto &it : _settings) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setString(it.name, "name");
			entry.setString(value::getVarTypeName(it.type), "type");
			if (it.element != 0) {
				entry.setString(value::getVarTypeName(value::chainHead(it.element)), "element");
			}
			if (it.role != SettingRole::None) {
				entry.setString(getSettingRoleName(it.role), "role");
			}
			if (!it.choices.empty()) {
				auto &choices = entry.newArray("choices");
				for (auto &c : it.choices) { choices.addString(c); }
			}
			if (!it.def.isNull()) {
				entry.setValue(it.def, "default");
			}
			arr.addValue(sprt::move(entry));
		}
	}

	// The name only: the schema itself is dumped by the type registry, and repeating it here would
	// give a golden dump two places to disagree with itself.
	if (_localSchema) {
		out.setString(_localSchema->getName(), "locals");
	}

	// What the operation names in the scene on its own account. The pin-derived groups are not
	// repeated here - they are already visible as the roles on the pins above.
	if (!_sceneRefs.empty()) {
		auto &refs = out.newArray("scene");
		for (auto &r : _sceneRefs) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setString(r.component, "component");
			if (!r.field.empty()) {
				entry.setString(r.field, "field");
			}
			if (r.type != VarType::Nil) {
				entry.setString(value::getVarTypeName(r.type), "type");
			}
			if (r.optional) {
				entry.setBool(true, "optional");
			}
			if (r.access == SceneAccess::Write) {
				entry.setString(StringView("write"), "access");
			}
			if (r.targeted) {
				entry.setBool(true, "targeted");
			}
			refs.addValue(sprt::move(entry));
		}
	}
}

OpRegistry::~OpRegistry() {
	// An OpDesc holds mem_std containers, so it is new'd rather than pool-allocated: a pool never
	// runs a destructor. Same arrangement as ComponentRegistry.
	for (auto it : _order) { delete it; }
	_order.clear();

	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool OpRegistry::init(memory::pool_t *parent) {
	if (_pool) {
		return false;
	}
	_pool = parent;
	_ownsPool = false;
	if (!_pool) {
		_pool = memory::pool::create();
		_ownsPool = true;
	}
	if (!_pool) {
		return false;
	}
	// Shares this registry's pool, so the derived names live exactly as long as the descriptors
	// that point at them.
	return _localTypes.init(_pool);
}

bool OpRegistry::init(const OpRegistry *base, memory::pool_t *parent) {
	if (!init(parent)) {
		return false;
	}
	_base = base;
	return true;
}

const OpDesc *OpRegistry::createNative(const OpDef &def, DiagSink *diagnostic) {
	if (!_pool) {
		return nullptr;
	}

	DiagReport report(diagnostic);
	auto desc = new OpDesc();
	if (desc->build(def, _pool, report) != Status::Ok) {
		delete desc;
		return nullptr;
	}

	if (get(desc->getId()) != nullptr) {
		report.report(DiagSeverity::Error, DiagCode::OpDuplicate,
				DiagText(DiagDetail::OpDuplicate).name(desc->getName()));
		delete desc;
		return nullptr;
	}

	// After the duplicate check, so a refused registration never leaves a derived type behind under
	// a name the corrected registration would need.
	if (desc->deriveLocalSchema(def, _localTypes, report) != Status::Ok) {
		delete desc;
		return nullptr;
	}

	_order.emplace_back(desc);
	return desc;
}

const OpDesc *OpRegistry::get(OpId id) const {
	for (auto it : _order) {
		if (it->getId() == id) {
			return it;
		}
	}
	return _base ? _base->get(id) : nullptr;
}

const OpDesc *OpRegistry::get(StringView name) const { return get(makeTypeId(name)); }

void OpRegistry::describe(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (auto it : _order) {
		mem_std::Value entry;
		it->describe(entry);
		out.addValue(sprt::move(entry));
	}
}

} // namespace stappler::flow
