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

// The record as a run of bytes: what computeRecordSpans answers, and why it is the same answer
// describe() gives.
//
// A record's layout has two audiences - a dump a test compares and a picture a person reads - and
// the gap between two fields is the interesting part of it for both, because it is the price of the
// field ORDER and it is paid on every instance. describe() used to walk the fields and find the gaps
// itself; now it calls computeRecordSpans and so does everything else. What this section asserts is
// therefore not "the spans are right" but "there is one walk": the spans and the `<padding>`
// pseudo-entries of a dump are compared entry for entry, and a second implementation would have to
// agree with the first on every fixture here to survive.

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

FieldDef scalar(StringView name, VarType type) { return FieldDef{.name = name, .type = type}; }

// Every span, in order, as `offset:size:field` - `pad` where no field owns the run. One line per
// record, so a disagreement reads as a diff of two records rather than of two numbers.
mem_std::String spansToString(const ComponentType &type) {
	mem_std::Vector<RecordSpan> spans;
	computeRecordSpans(type, spans);

	mem_std::String out;
	for (auto &span : spans) {
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(mem_std::toString(span.offset, ":", span.size, ":"));
		if (span.field < 0) {
			out.append("pad");
		} else {
			out.append(type.getFields()[uint32_t(span.field)].name.str<mem_std::Interface>());
		}
	}
	return out;
}

// The same line, read out of a describe() dump instead. The dump names a gap `<padding>` and gives
// it an offset and a size and nothing else, which is exactly a span.
mem_std::String dumpToString(const ComponentType &type) {
	mem_std::Value dump;
	type.describe(dump);

	mem_std::String out;
	for (auto &field : dump.getValue("fields").asArray()) {
		if (!out.empty()) {
			out.append(" ");
		}
		StringView name(field.getString("name"));
		out.append(mem_std::toString(field.getInteger("offset"), ":", field.getInteger("size"), ":",
				name == "<padding>" ? StringView("pad") : name));
	}
	return out;
}

// Contiguous, starting at zero, ending at the record's size. Anything else means an editor drawing
// the spans would leave a hole in the picture that the record does not have.
bool spansCoverRecord(const ComponentType &type) {
	mem_std::Vector<RecordSpan> spans;
	computeRecordSpans(type, spans);

	uint32_t cursor = 0;
	uint32_t fields = 0;
	for (auto &span : spans) {
		if (span.offset != cursor || span.size == 0) {
			return false;
		}
		if (span.field >= 0) {
			if (uint32_t(span.field) != fields) {
				return false; // field spans come in field order, one each
			}
			auto &desc = type.getFields()[uint32_t(span.field)];
			if (desc.offset != span.offset || desc.size != span.size) {
				return false;
			}
			++fields;
		}
		cursor += span.size;
	}
	return cursor == type.getSize() && fields == type.getFields().size();
}

const ComponentType *build(TypeRegistry &reg, StringView name,
		mem_std::Vector<FieldDef> fields) {
	mem_std::Value diag;
	auto type = reg.createNative(name, SpanView<FieldDef>(fields.data(), fields.size()),
			test::NumberSink(&diag).get());
	check(type != nullptr,
			mem_std::toString("schema-spans: ", name, " builds (", data::toString(diag, false),
					")"));
	return type;
}

} // namespace

void performSchemaSpansTests() {
	sprt::cout << "\n== flow value: the record as a run of bytes ==\n";

	TypeRegistry reg;
	check(reg.init(), "schema-spans: registry init");

	// 1. GOLDEN. A one-byte field ahead of an eight-byte one buys seven bytes of nothing, and the
	//    record is rounded up to its own alignment at the end - so this record spends 11 of its 24
	//    bytes on the order it was written in. That is the whole reason the spans exist.
	if (auto type = build(reg, "spans.Mixed",
				{scalar("flag", VarType::Bool), scalar("id", VarType::Int),
					scalar("tail", VarType::Bool)})) {
		checkEq(spansToString(*type), StringView("0:1:flag 1:7:pad 8:8:id 16:1:tail 17:7:pad"),
				"schema-spans: an inner gap and a trailing one are both reported");
		check(type->getSize() == 24 && spansCoverRecord(*type),
				"schema-spans: and together with the fields they cover all 24 bytes");
	}

	// 2. A record with no gap at all reports no gap - the walk must not invent a zero-length run
	//    between two fields that touch.
	if (auto type = build(reg, "spans.Packed",
				{scalar("a", VarType::Int), scalar("b", VarType::Float)})) {
		checkEq(spansToString(*type), StringView("0:8:a 8:8:b"),
				"schema-spans: fields that touch produce no padding span");
		check(spansCoverRecord(*type), "schema-spans: Packed is covered exactly");
	}

	// 3. Trailing padding ALONE: a Vec3 leaves the cursor at 12 and the record's alignment is 4, so
	//    there is nothing to round - while a Bool after an Int is rounded by seven. The two cases
	//    are separated here because a walk that only looked between fields would pass the first.
	if (auto type = build(reg, "spans.NoTail", {scalar("pos", VarType::Vec3)})) {
		checkEq(spansToString(*type), StringView("0:12:pos"),
				"schema-spans: a record that needs no rounding gets no tail span");
	}
	if (auto type = build(reg, "spans.Tail",
				{scalar("id", VarType::Int), scalar("flag", VarType::Bool)})) {
		checkEq(spansToString(*type), StringView("0:8:id 8:1:flag 9:7:pad"),
				"schema-spans: a record that does gets one");
	}

	// 4. An empty component is a tag, and build() still gives it a byte to have an address. The span
	//    over that byte belongs to no field, which is the honest answer rather than an empty list.
	if (auto type = build(reg, "spans.Tag", {})) {
		checkEq(spansToString(*type), StringView("0:1:pad"),
				"schema-spans: an empty component is one span of padding");
		check(spansCoverRecord(*type), "schema-spans: and it is covered exactly");
	}

	// 5. THE POINT OF THE SECTION: the spans and the dump are one walk. Asserted over every fixture
	//    above plus a wide one, because a second implementation would agree on the easy records.
	if (auto type = build(reg, "spans.Wide",
				{scalar("flag", VarType::Bool), scalar("uv", VarType::Vec2),
					scalar("name", VarType::String), scalar("count", VarType::Int),
					scalar("tint", VarType::Color), scalar("enabled", VarType::Bool),
					scalar("weight", VarType::Float)})) {
		check(spansCoverRecord(*type), "schema-spans: Wide is covered exactly");
		for (uint32_t i = 0; i < reg.getCount(); ++i) {
			auto it = reg.getAt(i);
			checkEq(spansToString(*it), dumpToString(*it),
					mem_std::toString("schema-spans: describe() reports the same runs for ",
							it->getName()));
		}
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
