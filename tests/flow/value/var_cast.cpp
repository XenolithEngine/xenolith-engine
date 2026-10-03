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

// Subtask C1: the interpreter's value and the table of conversions between value types.
//
// Two properties carry this section.
//
// The FIRST is that a Var's byte image is fully determined by its logical value. `operator==` asks
// "are these the same value", `varBytesEqual` asks "are these the same 24 bytes", and the whole
// corpus below asserts that the two answers agree. They can only agree if every constructor zeroes
// the bytes its variant does not use - which is the property subtask F1 will need when it demands
// byte-identical local stores, long after anyone remembers why the zeroing is there.
//
// The SECOND is that the conversion matrix in the source is the matrix that was intended. The table
// is written out again here, as characters, and compared cell by cell - every one of them. A matrix
// that only existed in a switch statement could drift a cell at a time with nothing to notice.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValue.h"
#include "SPFlowValueVar.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow::value;

// The union's layout is part of the image format, so it is pinned here rather than left to be
// whatever the compiler happened to pick.
static_assert(sizeof(Var) == 24);
static_assert(alignof(Var) == 8);
static_assert(sizeof(BlobHandle) == 12);
static_assert(offsetof(Var, i) == 8);
static_assert(offsetof(Var, c.blob) == 8);
static_assert(offsetof(Var, c.elem) == 20); // the four bytes a BlobHandle leaves over
static_assert(offsetof(Var, e.value) == 8);
static_assert(offsetof(Var, e.type) == 16); // Enum's family fills the union exactly
static_assert(offsetof(Var, ent.id) == 8);
static_assert(offsetof(Var, ent.schema) == 16);

// A value of every type, including the awkward ones. Reused by the byte-discipline check and by the
// projection round trip.
mem_std::Vector<Var> makeCorpus() {
	mem_std::Vector<Var> out;
	out.emplace_back(makeNil());
	out.emplace_back(makeBool(false));
	out.emplace_back(makeBool(true));
	out.emplace_back(makeInt(0));
	out.emplace_back(makeInt(-1));
	out.emplace_back(makeInt(int64_t(0x7fff'ffff'ffff'ffffll)));
	out.emplace_back(makeFloat(0.0));
	out.emplace_back(makeFloat(-0.5));
	out.emplace_back(makeFloat(1e300));
	out.emplace_back(makeVec2(1.0f, 2.0f));
	out.emplace_back(makeVec3(1.0f, 2.0f, 3.0f));
	out.emplace_back(makeVec4(1.0f, 2.0f, 3.0f, 4.0f));
	out.emplace_back(makeColor(0.25f, 0.5f, 0.75f, 1.0f));
	out.emplace_back(makeEntityRef(EntityId{7, 3}));
	out.emplace_back(makeEntityRef(EntityId{7, 3}, makeTypeId("Transform")));
	out.emplace_back(makeEnum(2));
	out.emplace_back(makeEnum(2, makeTypeId("BlendMode")));
	out.emplace_back(makeInt32(0));
	out.emplace_back(makeInt32(-2'147'483'647 - 1));
	out.emplace_back(makeUInt32(4'294'967'295u));
	out.emplace_back(makeFloat32(0.0f));
	out.emplace_back(makeFloat32(-0.5f));
	return out;
}

} // namespace

void performVarCastTests() {
	sprt::cout << "\n== flow value: Var and the conversion matrix ==\n";

	// 1. Names round-trip, which is what makes a data-driven schema description readable and a
	//    diagnostic message useful.
	{
		bool ok = true;
		for (uint32_t i = 0; i < VarTypeCount && ok; ++i) {
			VarType back = VarType::Nil;
			ok = readVarType(getVarTypeName(VarType(i)), back) && back == VarType(i);
		}
		check(ok, "var-cast: every VarType name round-trips");

		VarType ignored = VarType::Nil;
		check(!readVarType(StringView("nonesuch"), ignored),
				"var-cast: an unknown type name is refused");
	}

	// 2. THE byte-discipline check. If any constructor forgot to zero the bytes its variant does not
	//    use, two logically equal values would differ on those bytes and the two comparisons would
	//    disagree here.
	{
		auto corpus = makeCorpus();
		bool agree = true;
		for (size_t a = 0; a < corpus.size() && agree; ++a) {
			for (size_t b = 0; b < corpus.size(); ++b) {
				if ((corpus[a] == corpus[b]) != varBytesEqual(corpus[a], corpus[b])) {
					agree = false;
					sprt::cout << "       disagreement at corpus[" << a << "] vs [" << b << "]\n";
					break;
				}
			}
		}
		check(agree, "var-cast: logical equality and byte equality agree on the whole corpus");

		// Built twice, independently: same value, same bytes. This is the property that makes a
		// store's image a function of its content rather than of its history.
		check(varBytesEqual(makeVec2(1.0f, 2.0f), makeVec2(1.0f, 2.0f)),
				"var-cast: the same value built twice has the same bytes");
		check(!varBytesEqual(makeFloat(0.0), makeFloat(-0.0)),
				"var-cast: and -0.0 is a different value from +0.0, by bytes");
	}

	// 3. The identity every variant carries inline. This is what the union's spare bytes bought.
	{
		auto blend = makeTypeId("BlendMode");
		auto cull = makeTypeId("CullMode");
		check(blend != cull && blend != NullTypeId, "var-cast: TypeId is derived from the name");
		check(makeTypeId("BlendMode") == blend, "var-cast: and is stable for the same name");

		auto a = makeEnum(2, blend);
		auto b = makeEnum(2, cull);
		check(a != b, "var-cast: two enum families with the same ordinal are different values");

		auto e = makeEntityRef(EntityId{7, 3}, makeTypeId("Transform"));
		check(EntityId::unpack(e.ent.id) == EntityId({7, 3}),
				"var-cast: an EntityRef round-trips index and generation");
		check(makeEntityRef(EntityId{7, 3}) != makeEntityRef(EntityId{7, 4}),
				"var-cast: a stale generation is a different reference");
	}

	// 4. The element chain: the reason the container variant is worth its four spare bytes.
	{
		ElementChain c = 0;
		check(chainPush(VarType::Int, 0, c) && chainHead(c) == VarType::Int && chainDepth(c) == 1,
				"var-cast: a one-level chain is Array<Int>");

		ElementChain c2 = 0;
		check(chainPush(VarType::Array, c, c2) && chainHead(c2) == VarType::Array
						&& chainHead(chainTail(c2)) == VarType::Int && chainDepth(c2) == 2,
				"var-cast: chainTail peels one level, so Array<Array<Int>> is expressible");

		// Build the full four levels, then check the fifth is refused rather than truncated.
		ElementChain deep = makeChain(VarType::Int);
		bool built = true;
		for (uint32_t i = 1; i < MaxChainDepth && built; ++i) {
			ElementChain next = 0;
			built = chainPush(VarType::Array, deep, next);
			deep = next;
		}
		check(built && chainDepth(deep) == MaxChainDepth,
				mem_std::toString("var-cast: a chain builds to the full depth of ", MaxChainDepth));

		ElementChain overflow = 0;
		check(!chainPush(VarType::Array, deep, overflow),
				"var-cast: and the level past it is refused, not silently truncated");
		check(!chainPush(VarType::Nil, 0, overflow), "var-cast: Nil is not an element type");
	}

	// 5. The projection. Bare values, with the type supplied on the way back - a declared field
	//    takes it from its descriptor, a map value from its own tag.
	{
		auto corpus = makeCorpus();
		bool exact = true;
		for (auto &var : corpus) {
			mem_std::Value encoded;
			if (!encodeVar(var, encoded)) {
				exact = false;
				break;
			}
			Var back;
			if (decodeVar(encoded, var.type, back) != Status::Ok || !varBytesEqual(var, back)) {
				exact = false;
				sprt::cout << "       " << getVarTypeName(var.type) << " did not round-trip\n";
				break;
			}
		}
		check(exact,
				"var-cast: every inline variant round-trips through data::Value, byte for byte");

		// A container has no inline content, so there is nothing here to project.
		Var blob = makeBlob(VarType::String, 0, BlobHandle{});
		mem_std::Value unused;
		check(!encodeVar(blob, unused),
				"var-cast: a container is refused - only the blob layer can project one");

		// NaN and Inf are refused rather than canonicalized: their bit patterns are not stable
		// across platforms, and quietly rewriting the author's number is worse than saying no.
		mem_std::Value nan;
		nan.setDouble(__builtin_nan(""));
		Var out;
		check(decodeVar(nan, VarType::Float, out) != Status::Ok,
				"var-cast: NaN is refused rather than canonicalized");
		mem_std::Value inf;
		inf.setDouble(__builtin_huge_val());
		check(decodeVar(inf, VarType::Float, out) != Status::Ok, "var-cast: and so is infinity");

		mem_std::Value shortArray(mem_std::Value::Type::ARRAY);
		shortArray.addDouble(1.0);
		check(decodeVar(shortArray, VarType::Vec3, out) != Status::Ok,
				"var-cast: a Vec3 needs exactly three components");
	}

	// 6. THE matrix check. A second copy of the table, drawn as characters, compared against
	//    getCastRule cell by cell. The implementation's table and this one have to be
	//    edited together or the section fails, which is the whole point of writing it twice.
	//
	//    Columns, in VarType order:
	//    Nil Bool Int Float Vec2 Vec3 Vec4 Color Entity String Array Map Enum Bytes Int32 UInt32 Float32
	{
		static const StringView golden[VarTypeCount] = {
			StringView("=................"), // Nil       - "absent" converts to nothing
			StringView(".=>>.....f....>>>"), // Bool
			StringView(".<=<.....f..<.<<<"), // Int
			StringView(".<<=.....f....<<<"), // Float
			StringView("....=>>.........."), // Vec2      - widening zero-fills, never one-fills
			StringView("....<=>.........."), // Vec3
			StringView("....<<==........."), // Vec4      - Vec4 <-> Color is the same bytes
			StringView("....<<==........."), // Color
			StringView(
					"..<.....=........"), // EntityRef - readable as an int, never forgeable from one
			StringView(
					".ppp.....=..p=ppp"), // String    - String -> Bytes drops a claim, so it is Same
			StringView("..........=......"), // Array
			StringView("...........=....."), // Map
			StringView("..>......f..=.<<."), // Enum
			StringView(".............=..."), // Bytes     - never back to String: that CLAIMS text
			StringView(".<>>.....f..<.=<<"), // Int32     - exact into Int and Float
			StringView(".<>>.....f..<.<=<"), // UInt32
			StringView(".<<>.....f....<<="), // Float32   - exact into Float
		};

		auto ruleChar = [](CastRule r) -> char {
			switch (r) {
			case CastRule::Reject: return '.';
			case CastRule::Same: return '=';
			case CastRule::Widen: return '>';
			case CastRule::Narrow: return '<';
			case CastRule::Parse: return 'p';
			case CastRule::Format: return 'f';
			}
			return '?';
		};

		uint32_t mismatches = 0;
		uint32_t cells = 0;
		for (uint32_t f = 0; f < VarTypeCount; ++f) {
			if (golden[f].size() != VarTypeCount) {
				++mismatches;
				sprt::cout << "       golden row " << getVarTypeName(VarType(f)) << " has "
						   << golden[f].size() << " cells\n";
				continue;
			}
			for (uint32_t t = 0; t < VarTypeCount; ++t) {
				++cells;
				auto got = ruleChar(getCastRule(VarType(f), VarType(t)));
				if (got != golden[f][t]) {
					++mismatches;
					sprt::cout << "       " << getVarTypeName(VarType(f)) << " -> "
							   << getVarTypeName(VarType(t)) << ": got '" << got << "', golden '"
							   << golden[f][t] << "'\n";
				}
			}
		}
		check(cells == VarTypeCount * VarTypeCount,
				mem_std::toString("var-cast: the matrix has all ", VarTypeCount * VarTypeCount,
						" cells (", cells, ")"));
		check(mismatches == 0, "var-cast: every cell matches the golden matrix");

		// The diagonal is reflexive everywhere, including for the container types.
		bool diagonal = true;
		for (uint32_t i = 0; i < VarTypeCount; ++i) {
			diagonal = diagonal && getCastRule(VarType(i), VarType(i)) == CastRule::Same;
		}
		check(diagonal, "var-cast: every type converts to itself");
	}

	// 7. The executor. Every Reject cell must fail, every reachable cell must produce the value the
	//    rule promises, and the policy has to be the only thing that decides a Narrow.
	{
		uint32_t rejected = 0;
		uint32_t deferred = 0;
		bool rejectsFail = true;
		for (uint32_t f = 0; f < VarTypeCount && rejectsFail; ++f) {
			for (uint32_t t = 0; t < VarTypeCount; ++t) {
				if (getCastRule(VarType(f), VarType(t)) != CastRule::Reject) {
					if (castNeedsArena(VarType(f), VarType(t))) {
						++deferred;
					}
					continue;
				}
				++rejected;
				// A Nil source is the cheapest witness that does not need an arena to build.
				Var out;
				Var src = makeNil();
				src.type = VarType(f);
				if (castVar(src, VarType(t), CastPolicy::Lossy, out) == Status::Ok) {
					rejectsFail = false;
					sprt::cout << "       " << getVarTypeName(VarType(f)) << " -> "
							   << getVarTypeName(VarType(t)) << " should have been refused\n";
					break;
				}
			}
		}
		check(rejectsFail,
				mem_std::toString("var-cast: all ", rejected,
						" Reject cells refuse the conversion"));
		check(deferred > 0,
				mem_std::toString("var-cast: ", deferred,
						" cells need an arena and are deferred to castVarInArena"));

		Var out;
		check(castVar(makeInt(7), VarType::String, CastPolicy::Lossy, out) == Status::Declined,
				"var-cast: a cell needing an arena is Declined, not an error");

		// Widening is exact, so the policy cannot change the answer.
		check(castVar(makeBool(true), VarType::Int, CastPolicy::Lossless, out) == Status::Ok
						&& out == makeInt(1),
				"var-cast: Bool widens to Int under either policy");
		check(castVar(makeEnum(5, makeTypeId("BlendMode")), VarType::Int, CastPolicy::Lossless, out)
								== Status::Ok
						&& out == makeInt(5),
				"var-cast: an Enum widens to its ordinal");

		// Narrowing: Lossless demands that THIS value survives the round trip.
		check(castVar(makeInt(1), VarType::Bool, CastPolicy::Lossless, out) == Status::Ok
						&& out == makeBool(true),
				"var-cast: Int 1 narrows to Bool losslessly");
		check(castVar(makeInt(7), VarType::Bool, CastPolicy::Lossless, out) != Status::Ok,
				"var-cast: but Int 7 does not - the value would not survive");
		check(castVar(makeInt(7), VarType::Bool, CastPolicy::Lossy, out) == Status::Ok
						&& out == makeBool(true),
				"var-cast: and Lossy accepts it");

		check(castVar(makeInt(1 << 20), VarType::Float, CastPolicy::Lossless, out) == Status::Ok,
				"var-cast: a small Int converts to Float losslessly");
		// Note the +1: 2^60 on its own is a power of two and therefore exactly representable, so it
		// would pass the round trip. What a double cannot hold is the odd neighbour.
		check(castVar(makeInt((int64_t(1) << 60) + 1), VarType::Float, CastPolicy::Lossless, out)
						!= Status::Ok,
				"var-cast: an Int past 2^53 that is not a power of two does not");
		check(castVar(makeInt((int64_t(1) << 60) + 1), VarType::Float, CastPolicy::Lossy, out)
						== Status::Ok,
				"var-cast: and Lossy accepts that too");
		check(castVar(makeInt(int64_t(1) << 60), VarType::Float, CastPolicy::Lossless, out)
						== Status::Ok,
				"var-cast: while a large power of two survives exactly, so Lossless allows it");

		// A cell whose inverse is Reject can never demonstrate exactness, so Lossless refuses it.
		check(castVar(makeEntityRef(EntityId{7, 3}), VarType::Int, CastPolicy::Lossy, out)
						== Status::Ok,
				"var-cast: an EntityRef reads out as an Int under Lossy");
		check(castVar(makeEntityRef(EntityId{7, 3}), VarType::Int, CastPolicy::Lossless, out)
						!= Status::Ok,
				"var-cast: but not under Lossless - Int -> EntityRef is Reject, so nothing could "
				"prove the value survived");

		// Vectors: widening fills with zero, and narrowing is lossless exactly when the dropped
		// components were zero.
		check(castVar(makeVec2(1.0f, 2.0f), VarType::Vec4, CastPolicy::Lossless, out) == Status::Ok
						&& out == makeVec4(1.0f, 2.0f, 0.0f, 0.0f),
				"var-cast: Vec2 widens to Vec4 by zero-filling, not by guessing w = 1");
		check(castVar(makeVec4(1.0f, 2.0f, 0.0f, 0.0f), VarType::Vec2, CastPolicy::Lossless, out)
						== Status::Ok,
				"var-cast: and narrows back when the dropped components were zero");
		check(castVar(makeVec4(1.0f, 2.0f, 3.0f, 0.0f), VarType::Vec2, CastPolicy::Lossless, out)
						!= Status::Ok,
				"var-cast: but not when they were not");
		check(castVar(makeVec4(1.0f, 2.0f, 3.0f, 4.0f), VarType::Color, CastPolicy::Lossless, out)
								== Status::Ok
						&& out == makeColor(1.0f, 2.0f, 3.0f, 4.0f),
				"var-cast: Vec4 and Color are the same four floats under a different tag");
		check(castVar(makeVec3(1.0f, 2.0f, 3.0f), VarType::Color, CastPolicy::Lossy, out)
						!= Status::Ok,
				"var-cast: Vec3 -> Color is refused - alpha 0 would silently make it invisible");

		// The family of an enum built from a bare integer is unknown; a declared field's setField
		// stamps the descriptor's TypeId over it.
		check(castVar(makeInt(3), VarType::Enum, CastPolicy::Lossy, out) == Status::Ok
						&& out.e.value == 3 && out.e.type == NullTypeId,
				"var-cast: an Int becomes an Enum with no family until a descriptor stamps one");
	}

	// 8. The whole question over the matrix: tag, element chain and subtype together.
	//
	//    It came down here in U2 because a second consumer appeared - the screen binds a control to
	//    a component's field and must not link the graph to ask what a wire asks. The two callers
	//    that name it are `graph::edgeTypesMeet` and the screen's binding check, and neither of them
	//    is exercised here on purpose: this is the section that owns the matrix, so this is where
	//    the rule ITSELF is stated, and `edit-connect` and `graph-validate` remain the sections that
	//    state what the graph makes of it.
	{
		CastRule rule = CastRule::Reject;
		auto meet = [&](const ValueShape &from, const ValueShape &to) {
			return valueTypesMeet(from, to, rule);
		};

		check(meet({VarType::Float}, {VarType::Float}) == TypeMeet::Ok && rule == CastRule::Same,
				"var-cast: a type meets itself, and the verdict carries the cell");
		check(meet({VarType::Bool}, {VarType::Int}) == TypeMeet::Ok && rule == CastRule::Widen,
				"var-cast: Bool reaches Int, because nothing is lost");
		check(meet({VarType::Int}, {VarType::Float}) == TypeMeet::Tag,
				"var-cast: Int does not reach Float - Narrow is not Same and not Widen");
		check(meet({VarType::String}, {VarType::Int}) == TypeMeet::Tag,
				"var-cast: ... and neither does a Parse cell, which may still fail on the value");

		// The element chain: the matrix knows nothing about it, so a table check alone would join
		// these two.
		ElementChain arrayOfInt = 0;
		ElementChain arrayOfFloat = 0;
		ElementChain arrayOfArrayOfInt = 0;
		chainPush(VarType::Int, 0, arrayOfInt);
		chainPush(VarType::Float, 0, arrayOfFloat);
		chainPush(VarType::Array, arrayOfInt, arrayOfArrayOfInt);

		check(meet({VarType::Array, arrayOfInt}, {VarType::Array, arrayOfInt}) == TypeMeet::Ok,
				"var-cast: Array<Int> meets Array<Int>");
		check(meet({VarType::Array, arrayOfInt}, {VarType::Array, arrayOfFloat})
						== TypeMeet::Element,
				"var-cast: ... and does not meet Array<Float>, which the matrix cannot tell apart");
		check(meet({VarType::Array, arrayOfArrayOfInt}, {VarType::Array, arrayOfInt})
						== TypeMeet::Element,
				"var-cast: ... nor does a chain of two meet a chain of one");

		// The subtype, and the asymmetry that is easy to get backwards.
		auto famA = makeTypeId(StringView("x.A"));
		auto famB = makeTypeId(StringView("x.B"));

		check(meet({VarType::Enum, 0, famA}, {VarType::Enum, 0, famA}) == TypeMeet::Ok,
				"var-cast: an enum family meets itself");
		check(meet({VarType::Enum, 0, famA}, {VarType::Enum, 0, famB}) == TypeMeet::Subtype,
				"var-cast: two families do not meet");
		check(meet({VarType::Enum, 0, famA}, {VarType::Enum, 0, NullTypeId}) == TypeMeet::Ok,
				"var-cast: an unconstrained destination takes a family");
		check(meet({VarType::Enum, 0, NullTypeId}, {VarType::Enum, 0, famA}) == TypeMeet::Subtype,
				"var-cast: ... and a constrained one refuses a source that names none");

		// The order the halves are asked in is observable: a pair that fails two of them reports the
		// tag, because that is the one that makes the other two unanswerable.
		check(meet({VarType::Array, arrayOfInt}, {VarType::Map, arrayOfFloat}) == TypeMeet::Tag,
				"var-cast: the tag is asked first, and its answer is the one reported");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
