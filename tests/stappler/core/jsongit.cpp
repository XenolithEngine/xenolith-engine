/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

/* `EncodeFormat::JsonGit` - the JSON layout that goes into a version control system.

WHY THIS SECTION EXISTS AT ALL. The other two JSON layouts are pinned by almost nothing: two
incidental assertions in `datavalue` and `pug` cover the compact form, and `Pretty` has no check in
the engine whatsoever - which is exactly why it could not be "improved" into this job. A layout that
documents live in has the opposite requirement: the moment files are committed, the rules become
part of compatibility, and changing one is a "reformatted everything" commit that swallows the real
change. So the rules are asserted here, in the same subtask that introduces them, rather than left
to settle.

WHAT IS ACTUALLY BEING CLAIMED, and what would break if it were not:

  * the LAYOUT, as exact literals. An expectation spelled out as text fails by showing the two
    strings, so a regression names itself instead of reporting "the wrong number of lines";
  * the CIRCLE: write -> read -> write is byte-identical, and stays so on a third pass. Every
    format in the studio is built on this, and it is the one property a formatting change can break
    silently;
  * the DIFF: inserting a record touches exactly one line and REWRITES NONE. That is the whole
    reason for a leading comma, and a trailing one would fail it - which is why the price of the
    leading comma (an insertion at the front costs two lines) is asserted too, right next to it;
  * the four entry points - string, bytes, callback, file - produce the same bytes. The switches in
    EncodeTraits have no `default`, so a forgotten branch is not a build error but an empty result.

The number checks pin `sprt::dtoa` (dragonbox, shortest round-trip, a literal '.') rather than add
anything: the engine already prints numbers deterministically, and this format's promise of
byte-stability rests on it. The non-finite ones are the exception - they were a real hole, three
dialects deep, and the checks below are what closed it. */

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"
#include "SPFilesystem.h"

#include <sprt/c/__sprt_stdlib.h> // getenv

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using Value = mem_std::Value;
using Interface = mem_std::Interface;

constexpr auto Git = data::EncodeFormat::JsonGit;

static auto encode(const Value &v) -> mem_std::String { return data::toString<Interface>(v, Git); }

// A line-level diff, the way git counts one: drop the common prefix, drop the common suffix, and
// what is left in the middle is the change. For an insertion the old side's middle is EMPTY, and
// that - rather than any hand-counted line number - is the claim "the neighbouring lines were not
// touched".
struct LineDiff {
	size_t prefix = 0;
	size_t suffix = 0;
	size_t removed = 0;
	size_t added = 0;
};

static void splitLines(StringView s, mem_std::Vector<StringView> &out) {
	while (!s.empty()) {
		out.emplace_back(s.readUntil<StringView::Chars<'\n'>>());
		if (s.is('\n')) {
			++s;
		}
	}
}

static LineDiff lineDiff(StringView a, StringView b) {
	mem_std::Vector<StringView> la, lb;
	splitLines(a, la);
	splitLines(b, lb);

	LineDiff d;
	while (d.prefix < la.size() && d.prefix < lb.size() && la[d.prefix] == lb[d.prefix]) {
		++d.prefix;
	}
	while (d.suffix < la.size() - d.prefix && d.suffix < lb.size() - d.prefix
			&& la[la.size() - 1 - d.suffix] == lb[lb.size() - 1 - d.suffix]) {
		++d.suffix;
	}
	d.removed = la.size() - d.prefix - d.suffix;
	d.added = lb.size() - d.prefix - d.suffix;
	return d;
}

/* The layouts, spelled the way the file itself looks. Raw strings, and not escapes, for two
reasons: an expectation that is read as a picture catches a wrong indent at a glance, and a
formatter cannot reflow it into a column of fragments. */
static constexpr auto s_expectDoc = StringView(R"({
	"__meta": {"kind": "graph", "version": 1}
	, "nodes": [
		{"id": 1, "op": "add"}
		, {"id": 2, "op": "mul"}
	]
}
)");

static constexpr auto s_expectNesting = StringView(R"({
	"a": [
		{
			"b": {
				"c": [1, 2]
			}
		}
	]
}
)");

static constexpr auto s_expectKeyOrder = StringView(
		"{\"10\": 6, \"Alpha\": 5, \"__meta\": 4, \"_x\": 3, \"alpha\": 2, \"zeta\": 1}\n");

// The fixture the circle and the hygiene checks run on: every type at once, so that a claim about
// "a document" is not quietly a claim about a dictionary of integers.
static Value makeFixture() {
	Value meta;
	meta.setString("graph", "kind");
	meta.setInteger(1, "version");

	Value nodes(Value::Type::ARRAY);
	for (int i = 1; i <= 3; ++i) {
		Value node;
		node.setInteger(i, "id");
		node.setString(i == 2 ? "mul" : "add", "op");
		nodes.addValue(sp::move(node));
	}

	Value padding(Value::Type::ARRAY);
	padding.addInteger(4);
	padding.addInteger(8);

	Value ret;
	ret.setValue(sp::move(meta), "__meta");
	ret.setValue(Value(Value::Type::ARRAY), "edges");
	ret.setBool(true, "flag");
	ret.setString("узел графа", "имя");
	ret.setValue(Value(), "none");
	ret.setValue(sp::move(nodes), "nodes");
	ret.setValue(sp::move(padding), "padding");
	ret.setDouble(-0.5, "ratio");
	ret.setInteger(-17, "seed");
	return ret;
}

// A three-record array, on its own, so the diff checks operate on the shape the format was made
// for: a list of records that people append to.
static Value makeRecords(std::initializer_list<int> ids) {
	Value nodes(Value::Type::ARRAY);
	for (auto id : ids) {
		Value node;
		node.setInteger(id, "id");
		nodes.addValue(sp::move(node));
	}
	Value ret;
	ret.setValue(sp::move(nodes), "nodes");
	return ret;
}

} // namespace

void performJsonGitTests() {
	/* The cross-platform half of "the same bytes everywhere", and it has to be a FILE rather than
	a log: the Windows build under wine produces neither stdout nor an exit code - a section name
	that does not exist answers 0 just the same - and in fact never reaches this function at all,
	so there is nothing to compare. On a real Windows host the check is:

	    XL_JSONGIT_DUMP=./a stapplertest json-git
	    XL_JSONGIT_DUMP=./b stapplertest.exe json-git
	    cmp a b

	Until one is available, the claim rests on the msvc target BUILDING and on the construction:
	the format never emits a CR, the file layer has no text mode (_O_BINARY is 0), and the number
	printer takes its decimal point from a literal rather than a locale. That is weaker than a run
	and is said so rather than dressed up as one.

	Asked for by an environment variable, in the idiom this harness already uses for the
	destructive cases in `datavalue`. */
	if (auto env = __sprt_getenv("XL_JSONGIT_DUMP")) {
		auto text = encode(makeFixture());
		FileInfo out{StringView(env), FileCategory::Custom};
		filesystem::write(out, (const uint8_t *)text.data(), text.size());
		return;
	}

	sprt::cout << "\n== stappler data git-friendly JSON tests ==\n";

	// ---- the layout, as exact text --------------------------------------------------------------

	{
		Value nodes(Value::Type::ARRAY);
		Value a;
		a.setInteger(1, "id");
		a.setString("add", "op");
		Value b;
		b.setInteger(2, "id");
		b.setString("mul", "op");
		nodes.addValue(sp::move(a));
		nodes.addValue(sp::move(b));

		Value meta;
		meta.setString("graph", "kind");
		meta.setInteger(1, "version");

		Value doc;
		doc.setValue(sp::move(meta), "__meta");
		doc.setValue(sp::move(nodes), "nodes");

		checkEq(encode(doc), s_expectDoc, "json-git: the whole layout, byte for byte");
	}

	{
		// A container of scalars is one line - a vector reads as one value, not as four lines
		Value v;
		Value padding(Value::Type::ARRAY);
		padding.addInteger(4);
		padding.addInteger(8);
		v.setValue(sp::move(padding), "padding");
		v.setInteger(1, "z");
		checkEq(encode(v), StringView("{\n\t\"padding\": [4, 8]\n\t, \"z\": 1\n}\n"),
				"json-git: an array of scalars stays on one line");
	}

	{
		// ANY child a container, not ALL of them: under ALL (which is what Pretty's isObjectArray
		// asks) one stray scalar would collapse an array of five hundred records onto one line
		Value mixed(Value::Type::ARRAY);
		mixed.addInteger(1);
		Value d;
		d.setInteger(2, "a");
		mixed.addValue(sp::move(d));

		Value v;
		v.setValue(sp::move(mixed), "mixed");
		checkEq(encode(v), StringView("{\n\t\"mixed\": [\n\t\t1\n\t\t, {\"a\": 2}\n\t]\n}\n"),
				"json-git: one container among scalars makes the array a block");
	}

	{
		// An array of arrays: the outer is a block, each inner stays inline
		Value m(Value::Type::ARRAY);
		for (int i = 0; i < 2; ++i) {
			Value row(Value::Type::ARRAY);
			row.addInteger(i == 0 ? 1 : 0);
			row.addInteger(i == 0 ? 0 : 1);
			m.addValue(sp::move(row));
		}
		Value v;
		v.setValue(sp::move(m), "m");
		checkEq(encode(v), StringView("{\n\t\"m\": [\n\t\t[1, 0]\n\t\t, [0, 1]\n\t]\n}\n"),
				"json-git: an array of arrays is a block of inline rows");
	}

	{
		// Depth: an inline container inside a block must not move the indent, and this is the only
		// place where an off-by-one in the counter is visible directly in the text
		Value inner(Value::Type::ARRAY);
		inner.addInteger(1);
		inner.addInteger(2);
		Value c;
		c.setValue(sp::move(inner), "c");
		Value b;
		b.setValue(sp::move(c), "b");
		Value arr(Value::Type::ARRAY);
		arr.addValue(sp::move(b));
		Value v;
		v.setValue(sp::move(arr), "a");

		checkEq(encode(v), s_expectNesting, "json-git: nesting raises the indent once per block");
	}

	{
		// An empty container has no children, so it is inline: `{\n}` and `[\n]` are what a naive
		// "has a container child" reading would produce
		Value v;
		v.setValue(Value(Value::Type::DICTIONARY), "a");
		v.setValue(Value(Value::Type::ARRAY), "b");
		checkEq(encode(v), StringView("{\n\t\"a\": {}\n\t, \"b\": []\n}\n"),
				"json-git: empty containers stay on their line");
	}

	{
		// A scalar root: no prefix, no stack, and still a document (so, a trailing newline)
		checkEq(encode(Value()), StringView("null\n"), "json-git: a null root");
		checkEq(encode(Value(true)), StringView("true\n"), "json-git: a bool root");
		checkEq(encode(Value(int64_t(-7))), StringView("-7\n"), "json-git: an integer root");
		checkEq(encode(Value(Value::Type::DICTIONARY)), StringView("{}\n"),
				"json-git: an empty dictionary root");
		checkEq(encode(Value(Value::Type::ARRAY)), StringView("[]\n"),
				"json-git: an empty array root");

		Value arr(Value::Type::ARRAY);
		arr.addInteger(1);
		arr.addInteger(2);
		arr.addInteger(3);
		checkEq(encode(arr), StringView("[1, 2, 3]\n"),
				"json-git: a root array of scalars is not a special case");
	}

	{
		// The escaping rules are the format's, not a detail to tidy up: the circle's byte-identity
		// and the literal Cyrillic below both rest on them
		Value v;
		v.setString("a\"b\\c\nd\te\x01", "s");
		checkEq(encode(v), StringView("{\"s\": \"a\\\"b\\\\c\\nd\\te\\u0001\"}\n"),
				"json-git: strings are escaped exactly as the other JSON layouts escape them");
	}

	// ---- the circle -----------------------------------------------------------------------------

	{
		auto v = makeFixture();
		auto s1 = encode(v);
		auto v2 = data::read<Interface>(s1);
		auto s2 = encode(v2);
		checkEq(s2, s1, "json-git: write -> read -> write is byte-identical");

		auto s3 = encode(data::read<Interface>(s2));
		checkEq(s3, s2, "json-git: ...and a third pass changes nothing");

		// The text could converge while the tree did not - an integer that became a double and
		// back would still print the same. CBOR is the independent witness.
		auto c1 = data::write<Interface>(v, data::EncodeFormat::Cbor);
		auto c2 = data::write<Interface>(v2, data::EncodeFormat::Cbor);
		check(c1 == c2, "json-git: the circle preserves the value, not just the text");
	}

	{
		// The four entry points must agree. The switches in EncodeTraits have no `default`, so a
		// forgotten branch yields an empty result rather than a build error, and this is the only
		// check that would notice.
		auto v = makeFixture();
		auto viaString = encode(v);

		auto viaBytes = data::write<Interface>(v, Git);
		check(StringView((const char *)viaBytes.data(), viaBytes.size()) == viaString,
				"json-git: write to bytes gives the same bytes as toString");

		mem_std::String viaCallback;
		data::write<Interface>([&](StringView str) { viaCallback.append(str.data(), str.size()); },
				v, Git);
		checkEq(viaCallback, viaString, "json-git: write to a callback agrees too");

		FileInfo probe{"xljg_probe.json", FileCategory::Custom};
		filesystem::remove(probe);
		check(data::save<Interface>(v, probe, Git), "json-git: save writes the file");
		auto onDisk = filesystem::readIntoMemory<Interface>(probe);
		checkEq(StringView((const char *)onDisk.data(), onDisk.size()), viaString,
				"json-git: the file holds exactly those bytes");

		// The file half of the LF claim: this is the only layer that could translate a newline
		check(StringView((const char *)onDisk.data(), onDisk.size()).find('\r') == maxOf<size_t>(),
				"json-git: no CR survives the file layer");

		auto fromFile = data::readFile<Interface>(probe);
		checkEq(encode(fromFile), viaString, "json-git: the circle closes through a file as well");
		filesystem::remove(probe);
	}

	{
		// The defect this work fixed in passing: `save` accepted a timeMarkers flag and dropped it,
		// so PrettyTime saved as Pretty. Without an assertion it would quietly come back.
		Value v;
		v.setInteger(1'500'000'000'000'000, "time");

		FileInfo probe{"xljg_time.json", FileCategory::Custom};
		filesystem::remove(probe);
		check(data::save<Interface>(v, probe, data::EncodeFormat::PrettyTime),
				"json-git: save(PrettyTime) writes the file");
		auto onDisk = filesystem::readIntoMemory<Interface>(probe);
		checkEq(StringView((const char *)onDisk.data(), onDisk.size()),
				data::toString<Interface>(v, data::EncodeFormat::PrettyTime),
				"json-git: save(PrettyTime) no longer saves as Pretty");
		filesystem::remove(probe);
	}

	// ---- the diff -------------------------------------------------------------------------------

	{
		auto before = encode(makeRecords({1, 2, 4}));

		auto middle = encode(makeRecords({1, 2, 3, 4}));
		auto d = lineDiff(before, middle);
		check(d.added == 1 && d.removed == 0,
				"json-git: inserting into the middle adds one line and rewrites none");

		auto end = encode(makeRecords({1, 2, 4, 5}));
		d = lineDiff(before, end);
		check(d.added == 1 && d.removed == 0,
				"json-git: appending adds one line and rewrites none");

		// This is the price of the leading comma, asserted rather than discovered on review: the
		// old first element gains a `, `. Appending is the common case, and one end has to pay.
		auto front = encode(makeRecords({0, 1, 2, 4}));
		d = lineDiff(before, front);
		check(d.added == 2 && d.removed == 1,
				"json-git: inserting at the FRONT costs two lines - the stated price");

		// Editing a value in place moves nothing around it
		auto edited = makeRecords({1, 2, 4});
		edited.getValue("nodes").getValue(1).setInteger(9, "id");
		d = lineDiff(before, encode(edited));
		check(d.added == 1 && d.removed == 1,
				"json-git: editing a record touches only its own line");
	}

	{
		// A record that is itself a block: the inserted element must own whole lines, not share one
		Value before(Value::Type::ARRAY);
		Value after(Value::Type::ARRAY);
		auto makeBlockRecord = [](int id) {
			Value inner(Value::Type::ARRAY);
			inner.addInteger(id);
			Value pins;
			pins.setValue(sp::move(inner), "pins");
			Value rec;
			rec.setInteger(id, "id");
			rec.setValue(sp::move(pins), "meta");
			return rec;
		};
		before.addValue(makeBlockRecord(1));
		before.addValue(makeBlockRecord(3));
		after.addValue(makeBlockRecord(1));
		after.addValue(makeBlockRecord(2));
		after.addValue(makeBlockRecord(3));

		auto d = lineDiff(encode(before), encode(after));
		check(d.removed == 0, "json-git: inserting a block record rewrites no existing line");
		// six: `{`, `"id"`, `, "meta": {`, `"pins"`, `}`, `}` - the record owns whole lines
		check(d.added == 6, "json-git: ...and adds exactly its own six lines");
	}

	// ---- hygiene of the text --------------------------------------------------------------------

	{
		auto out = encode(makeFixture());

		check(!out.empty() && out.back() == '\n' && out[out.size() - 2] != '\n',
				"json-git: the document ends with exactly one newline");
		check(StringView(out).find('\r') == maxOf<size_t>(), "json-git: not one CR anywhere");

		mem_std::Vector<StringView> lines;
		splitLines(StringView(out), lines);

		StringView trailing, spaced;
		for (auto &line : lines) {
			if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
				if (trailing.empty()) {
					trailing = line;
				}
			}
			auto lead = line;
			lead.skipChars<StringView::Chars<'\t'>>();
			if (lead.is(' ') && spaced.empty()) {
				spaced = line;
			}
		}
		check(trailing.empty(), "json-git: no line ends in whitespace");
		check(spaced.empty(), "json-git: the indent is tabs only, nothing is aligned with spaces");
	}

	// ---- numbers --------------------------------------------------------------------------------

	{
		// The circle over numbers pins dragonbox and the absence of a locale: a separator that
		// followed one would break here and nowhere else
		const double doubles[] = {0.0, -0.0, 0.5, 0.1, 1.0, 3.14159265358979, 1e21, 1e-7,
			1.7976931348623157e+308};
		StringView badDouble;
		for (auto d : doubles) {
			auto t1 = encode(Value(d));
			auto t2 = encode(data::read<Interface>(t1));
			if (t1 != t2 && badDouble.empty()) {
				badDouble = StringView(t1).pdup();
			}
		}
		check(badDouble.empty(), "json-git: a double read back prints the same text");

		const int64_t ints[] = {0, -1, 1, maxOf<int64_t>(), minOf<int64_t>()};
		StringView badInt;
		for (auto i : ints) {
			auto t1 = encode(Value(i));
			auto t2 = encode(data::read<Interface>(t1));
			if (t1 != t2 && badInt.empty()) {
				badInt = StringView(t1).pdup();
			}
		}
		check(badInt.empty(), "json-git: an integer read back prints the same text");

		// Separately from the circle: both sides could have moved together
		checkEq(encode(Value(1.0)), StringView("1.0\n"), "json-git: a whole double keeps its .0");
		checkEq(encode(Value(0.1)), StringView("0.1\n"), "json-git: the shortest form is printed");
		checkEq(encode(Value(int64_t(1))), StringView("1\n"),
				"json-git: an integer is not printed as a double");
	}

	{
		/* The non-finite doubles, which used to be a hole: `sprt::dtoa` printed `inf` / `-inf` /
		`NaN`, this decoder knew only lowercase `nan`, and Python's json module - which every
		headless check reads engine output with - knows `NaN` / `Infinity` / `-Infinity` and
		refuses the rest. Three dialects, no two of them agreeing, so a NaN written here could not
		be read here and an infinity could not be read by anything. One spelling is written now,
		and it is the one that already had a reader on the other side of the socket. */
		const double values[] = {nan<double>(), sprt::Infinity<double>, -sprt::Infinity<double>};
		const StringView texts[] = {StringView("NaN\n"), StringView("Infinity\n"),
			StringView("-Infinity\n")};

		checkEq(encode(Value(values[0])), texts[0], "json-git: NaN is written as `NaN`");
		checkEq(encode(Value(values[1])), texts[1], "json-git: an infinity as `Infinity`");
		checkEq(encode(Value(values[2])), texts[2], "json-git: and a negative one as `-Infinity`");

		// The circle itself, which is the point: read it back and it is still that same number
		auto readBack = [](StringView text) { return data::read<Interface>(text); };

		check(readBack("NaN\n").isDouble() && sprt::isnan(readBack("NaN\n").getDouble()),
				"json-git: NaN survives the circle");
		check(readBack("Infinity\n").getDouble() == sprt::Infinity<double>,
				"json-git: an infinity survives the circle");
		check(readBack("-Infinity\n").getDouble() == -sprt::Infinity<double>,
				"json-git: a negative infinity survives it too - the sign is not lost");

		// ...and inside a document, where the parser reaches them through a key and through an
		// array element rather than as the whole text
		Value doc;
		Value arr(Value::Type::ARRAY);
		arr.addDouble(sprt::Infinity<double>);
		arr.addDouble(-sprt::Infinity<double>);
		arr.addDouble(1.5);
		doc.setValue(sp::move(arr), "edges");
		doc.setDouble(nan<double>(), "ratio");

		auto text = encode(doc);
		checkEq(text,
				StringView("{\n\t\"edges\": [Infinity, -Infinity, 1.5]\n\t, \"ratio\": NaN\n}\n"),
				"json-git: they are written in place, without disturbing the layout");

		auto back = data::read<Interface>(text);
		check(back.getValue("edges").getDouble(0) == sprt::Infinity<double>
						&& back.getValue("edges").getDouble(1) == -sprt::Infinity<double>
						&& sprt::isnan(back.getDouble("ratio")),
				"json-git: ...and read back in place");
		checkEq(encode(back), text, "json-git: a document holding them is byte-identical again");

		// The older spellings still open: `nan` is what this decoder used to accept, `inf` is what
		// the number printer used to emit, and files carrying either must not become unreadable
		check(sprt::isnan(data::read<Interface>(StringView("nan")).getDouble()),
				"json-git: the older lowercase `nan` still reads");
		check(data::read<Interface>(StringView("inf")).getDouble() == sprt::Infinity<double>,
				"json-git: ...and the older `inf`");
		check(data::read<Interface>(StringView("-inf")).getDouble() == -sprt::Infinity<double>,
				"json-git: ...and `-inf`");
	}

	// ---- key order, and the envelope it carries -------------------------------------------------

	{
		// The order is a property of the container (an ordered map over the key), not of the
		// encoder. Pinned here because the studio's probeMeta reads the FIRST key of a file.
		Value v;
		v.setInteger(1, "zeta");
		v.setInteger(2, "alpha");
		v.setInteger(3, "_x");
		v.setInteger(4, "__meta");
		v.setInteger(5, "Alpha");
		v.setInteger(6, "10");
		checkEq(encode(v), s_expectKeyOrder,
				"json-git: keys come out in byte order, and inline separators with them");
	}

	{
		auto out = encode(makeFixture());
		check(StringView(out).starts_with("{\n\t\"__meta\": {"),
				"json-git: the envelope is the first key, in the shape probeMeta reads");
	}

	// ---- non-ASCII ------------------------------------------------------------------------------

	{
		auto out = encode(makeFixture());
		check(StringView(out).find("узел графа") != maxOf<size_t>(),
				"json-git: Cyrillic passes through as literal UTF-8");
		check(StringView(out).find("имя") != maxOf<size_t>(),
				"json-git: ...in keys as well as values");
		check(StringView(out).find("\\u") == maxOf<size_t>(),
				"json-git: nothing non-ASCII is escaped - an escaped diff is unreadable");
	}

	// ---- the neighbours are untouched -----------------------------------------------------------

	{
		auto v = makeFixture();
		auto compact = data::toString<Interface>(v, false);
		check(StringView(compact).find('\n') == maxOf<size_t>(),
				"json-git: compact Json still has no whitespace at all");

		auto pretty = data::toString<Interface>(v, true);
		check(!pretty.empty() && pretty.back() != '\n',
				"json-git: Pretty still does not end in a newline");

		checkEq(data::toString<Interface>(Value::Null, false), StringView("null"),
				"json-git: the compact form of Null is unchanged");

		check(data::EncodeFormat(Git).isTextual(),
				"json-git: the format reports itself as textual (nothing else calls isTextual)");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
