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

// Enum families: the members behind a name that used to be a hash and nothing else.
//
// Two properties carry the section, and neither is about the members themselves.
//
// THE FAMILY IS OUTSIDE EVERY LAYOUT. A family that gains a member changes no component's schema
// hash, because a member is not a byte - and if it did, adding a name to a list would migrate every
// record in a project. The family carries a hash of its OWN, for reading a drift, and it goes
// nowhere near a component's.
//
// THE NAMESPACE IS ONE. A component, a family and an alias share the TypeId space, so a name any of
// them has is refused to the other two - otherwise `get()` would answer differently depending on
// which table it happened to look in first.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValueSchema.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace flow::value;

mem_std::String membersOf(const EnumType &type) {
	mem_std::String out;
	for (auto &m : type.getMembers()) {
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(mem_std::toString(m.name, "=", m.value));
	}
	return out;
}

} // namespace

void performSchemaEnumsTests() {
	sprt::cout << "\n== flow value: enum families ==\n";

	// 1. A family, natively, and both directions of the lookup it exists for.
	{
		TypeRegistry reg;
		check(reg.init(), "schema-enums: registry init");

		auto facing = SP_FLOW_VALUE_ENUM(reg, "game.Facing", {StringView("North"), 0},
				{StringView("East"), 1}, {StringView("South"), 2}, {StringView("West"), 3});
		check(facing != nullptr, "schema-enums: a family registers");

		if (facing) {
			checkEq(membersOf(*facing), StringView("North=0 East=1 South=2 West=3"),
					"schema-enums: with its members in the order they were written");
			check(facing->getId() == makeTypeId("game.Facing"),
					"schema-enums: its id is the hash of its name, like every other declaration");

			int64_t value = -1;
			StringView name;
			check(facing->findValue(StringView("South"), value) && value == 2,
					"schema-enums: a name gives a value");
			check(facing->findName(1, name) && name == "East",
					"schema-enums: and a value gives a name");
			check(!facing->findValue(StringView("Up"), value) && !facing->findName(9, name),
					"schema-enums: what is not a member is not found");
			check(facing->hasValue(3) && !facing->hasValue(4),
					"schema-enums: and membership is answerable on its own");

			check(reg.getEnum(StringView("game.Facing")) == facing
							&& reg.getEnum(makeTypeId("game.Facing")) == facing,
					"schema-enums: the registry finds it by name and by id");
			check(reg.getEnum(StringView("game.Nonesuch")) == nullptr,
					"schema-enums: and an unknown family is null");
		}
	}

	// 2. What a family refuses. A duplicate NAME is refused because a file with one cannot be read
	//    back the way it was meant; a duplicate VALUE is allowed, because `Default` beside `North`
	//    is an ordinary thing to write and only the display has to pick one.
	{
		TypeRegistry reg;
		check(reg.init(), "schema-enums: registry init");

		sprt::cout << "       (the report below is expected)\n";
		mem_std::Value diag;
		check(SP_FLOW_VALUE_ENUM(reg, "bad.Dup", {StringView("A"), 0}, {StringView("A"), 1}) == nullptr,
				"schema-enums: two members of one name are refused");

		auto aliased = SP_FLOW_VALUE_ENUM(reg, "ok.Aliased", {StringView("Default"), 0},
				{StringView("North"), 0});
		check(aliased != nullptr, "schema-enums: two members of one value are not");

		StringView name;
		check(aliased && aliased->findName(0, name) && name == "Default",
				"schema-enums: and the first of them is the one a display gets");

		check(reg.createEnum(StringView("x.T"), SpanView<EnumMemberDef>(),
					  test::NumberSink(&diag).get())
						!= nullptr,
				"schema-enums: a family with no members is legal - it is a family nobody filled "
				"yet");
	}

	// 3. THE FAMILY IS OUTSIDE THE LAYOUT. Its own hash moves when its members do; no component's
	//    does, because a member is not a byte.
	{
		TypeRegistry a;
		TypeRegistry b;
		check(a.init() && b.init(), "schema-enums: hash registries init");

		auto one = SP_FLOW_VALUE_ENUM(a, "game.Facing", {StringView("North"), 0});
		auto two = SP_FLOW_VALUE_ENUM(b, "game.Facing", {StringView("North"), 0},
				{StringView("South"), 1});
		check(one && two && one->getHash() != two->getHash(),
				"schema-enums: a family that gained a member has another hash");

		auto same = SP_FLOW_VALUE_ENUM(a, "game.Facing2", {StringView("North"), 0});
		check(one && same && one->getHash() != same->getHash(),
				"schema-enums: and the name is in the hash too");

		mem_std::Vector<FieldDef> fields;
		fields.emplace_back(FieldDef{.name = "facing",
			.type = VarType::Enum,
			.subtypeName = StringView("game.Facing")});
		auto x = a.createNative("game.Body", SpanView<FieldDef>(fields.data(), fields.size()));
		auto y = b.createNative("game.Body", SpanView<FieldDef>(fields.data(), fields.size()));
		check(x && y && x->getSchemaHash() == y->getSchemaHash(),
				"schema-enums: and the component referencing it has the SAME schema hash either "
				"way");
	}

	// 4. One namespace over the three tables.
	{
		TypeRegistry reg;
		check(reg.init(), "schema-enums: namespace registry init");

		mem_std::Vector<FieldDef> fields;
		fields.emplace_back(FieldDef{.name = "x", .type = VarType::Int});
		check(reg.createNative("one.Name", SpanView<FieldDef>(fields.data(), fields.size()))
						!= nullptr,
				"schema-enums: a component takes the name");

		sprt::cout << "       (the two reports below are expected)\n";
		check(SP_FLOW_VALUE_ENUM(reg, "one.Name", {StringView("A"), 0}) == nullptr,
				"schema-enums: a family cannot have a component's name");

		check(SP_FLOW_VALUE_ENUM(reg, "other.Name", {StringView("A"), 0}) != nullptr,
				"schema-enums: a family takes a free one");
		check(reg.createNative("other.Name", SpanView<FieldDef>(fields.data(), fields.size()))
						== nullptr,
				"schema-enums: and a component cannot have a family's");
	}

}

} // namespace STAPPLER_VERSIONIZED stappler