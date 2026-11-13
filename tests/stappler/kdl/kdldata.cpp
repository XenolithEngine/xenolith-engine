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

// data::Value as a KDL document (stappler/data/SPDataEncodeKdl.h, SPDataDecodeKdl.h): the
// canonical text as literals, the circle Value -> KDL -> Value, agreement with the emitter of
// stappler_kdl, the hand-written forms, and every refusal with its place in the text.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"
#include "SPKdl.h"
#include "SPFilesystem.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using Value = mem_std::Value;
using Interface = mem_std::Interface;

static Value fromJson(StringView json) { return data::read<Interface>(json); }

static mem_std::String toKdl(const Value &v) { return data::kdl::write<Interface>(v); }

static mem_std::String toGit(const Value &v) {
	return data::toString<Interface>(v, data::EncodeFormat::JsonGit);
}

struct WriteCase {
	StringView json;
	StringView kdl;
};

// What the writer produces, spelled out.
const WriteCase s_writeCases[] = {
	{R"({})", ""},
	{R"({"a": 1})", "a 1\n"},
	{R"({"__meta": {"generator": "xlstudio", "kind": "component", "version": 2}, "b": "x"})",
		"__meta generator=xlstudio kind=component version=2\nb x\n"},
	{R"({"e": []})", "(array)e\n"},
	{R"({"o": [5]})", "(array)o 5\n"},
	{R"({"t": [1, "two", true, null]})", "t 1 two #true #null\n"},
	{R"({"m": [1, {"x": 1}]})", "m {\n    - 1\n    - x=1\n}\n"},
	{R"({"n": [[1], [2, 3], []]})", "n {\n    (array)- 1\n    - 2 3\n    (array)-\n}\n"},
	{R"({"d": {}})", "(object)d\n"},
	{R"({"d": {"-": {"a": 1}}})", "(object)d {\n    - a=1\n}\n"},
	{R"({"d": {"-": {"a": 1}, "k": 2}})", "d k=2 {\n    - a=1\n}\n"},
	{R"({"c": {"name": "arena.Board", "fields": [{"name": "turn", "type": "int"}]}})",
		"c name=arena.Board {\n    fields {\n        - name=turn type=int\n    }\n}\n"},
	{R"({"s": "two words", "t": "true", "u": "1", "v": ""})",
		"s \"two words\"\nt \"true\"\nu \"1\"\nv \"\"\n"},
	{R"({"two words": 1})", "\"two words\" 1\n"},
	{R"({"f": 1.0, "g": 0.5, "h": -2.5e-7, "i": 1e21})", "f 1.0\ng 0.5\nh -2.5E-7\ni 1.0E+21\n"},
};

static void testWriter() {
	for (auto &c : s_writeCases) {
		auto v = fromJson(c.json);
		checkEq(toKdl(v), c.kdl, mem_std::toString("write ", c.json));
	}

	Value special(Value::Type::DICTIONARY);
	special.setDouble(sprt::NaN<double>, "nan");
	special.setDouble(sprt::Infinity<double>, "pinf");
	special.setDouble(-sprt::Infinity<double>, "ninf");
	special.setBytes(Value::BytesType{1, 2, 3, 250}, "bytes");
	special.setValue(Value(), "null");
	checkEq(toKdl(special),
			"bytes (base64)\"AQID+g==\"\n\"nan\" #nan\nninf #-inf\n\"null\" #null\npinf #inf\n",
			"write: bytes, non-finite doubles, null");
}

// Values the mapping has to keep apart, and the edges of every scalar.
static Value makeEdges() {
	auto v = fromJson(R"({
		"__meta": {"kind": "scene", "version": 2},
		"arrays": [[], [[]], [{}], [1], [[1]], [1, [2, [3, [4]]]], [{"-": [1]}]],
		"dicts": {"-": {"-": {"x": 1}}, "empty": {}, "only": {"-": 1}},
		"keys": {"": 1, "-": 2, "ключ": "значение", "a b": 3, "#x": 4, "1": 5, "true": 6},
		"mixed": [1, "s", {"a": [1, 2]}, [3, {"b": null}], false],
		"numbers": [0, -1, 9223372036854775807, -9223372036854775808, 0.1, -0.0, 1e-300, 1.5e300],
		"strings": ["", " ", "\"quoted\"", "line\nbreak\ttab", "\\back", "#hash", "-", "1.5", "null"]
	})");
	v.setBytes(Value::BytesType{}, "noBytes");
	v.setBytes(Value::BytesType{0, 255, 128}, "someBytes");
	v.getValue("numbers").addDouble(sprt::NaN<double>);
	v.getValue("numbers").addDouble(-sprt::Infinity<double>);
	v.getValue("strings").addString(StringView("nul\0inside", 10));
	return v;
}

static bool sameValue(const Value &a, const Value &b) {
	// NaN is not equal to itself, so values are compared through their CBOR bytes.
	return data::write<Interface>(a, data::EncodeFormat::Cbor)
			== data::write<Interface>(b, data::EncodeFormat::Cbor);
}

static void testCircle() {
	auto v = makeEdges();
	auto text = toKdl(v);
	check(!text.empty(), "circle: the edge value writes");

	data::kdl::ParseError err;
	Value back;
	auto st = data::kdl::read<Interface>(text, back, &err);
	check(st == Status::Ok, mem_std::toString("circle: reads back (", err.message, ")"));
	check(sameValue(v, back), "circle: Value -> KDL -> Value is the same value");
	checkEq(toKdl(back), text, "circle: the second write is the first one");

	// The writer's text is the canonical form of the KDL emitter over the same document.
	kdl::DocumentTemplate<memory::StandardInterface> doc;
	check(kdl::read(text, doc) == Status::Ok, "canonical: stappler_kdl reads the text");
	mem_std::String emitted;
	kdl::write([&](StringView s) { emitted.append(s.data(), s.size()); }, doc);
	checkEq(emitted, text, "canonical: kdl::write of the parsed text is the text");

	// A studio-like document goes JSON -> KDL -> JSON without a byte changed.
	auto json = StringView(R"({
	"__meta": {"generator": "xlstudio", "kind": "component", "version": 2}
	, "aliases": [{"meta": {"doc": "How much a piece has left.", "max": 20, "min": 0, "unit": "hp"}, "name": "arena.Hp", "target": "int"}]
	, "components": [
		{"fields": [{"name": "turn", "type": "int"}, {"name": "hp", "type": "arena.Hp", "default": 20}], "meta": {"doc": "The board."}, "name": "arena.Board"}
		, {"fields": [], "name": "arena.Selected"}
	]
	, "enums": [{"name": "arena.Side", "values": [{"name": "neutral", "value": 0}, {"name": "blue", "value": 1}]}]
})");
	auto git = toGit(fromJson(json));
	auto viaKdl = toGit(data::kdl::read<Interface>(toKdl(fromJson(json))));
	checkEq(viaKdl, git, "circle: JSON -> KDL -> JSON is the same JsonGit text");
}

struct ReadCase {
	StringView kdl;
	StringView json;
};

// Hand-written forms and the JiK spellings: each reads as the value of the JSON beside it.
const ReadCase s_readCases[] = {
	{"", "{}"},
	{"meta doc=\"a b\" max=20", R"({"meta": {"doc": "a b", "max": 20}})"},
	{"meta {\n  doc \"a b\"\n  max 20\n}", R"({"meta": {"doc": "a b", "max": 20}})"},
	{"meta doc=x { max 20 }", R"({"meta": {"doc": "x", "max": 20}})"},
	{"a 1 { - #true #false }", R"({"a": [1, [true, false]]})"},
	{"a 1 { - #true; - x=1 }", R"({"a": [1, true, {"x": 1}]})"},
	{"(array)a 1 2", R"({"a": [1, 2]})"},
	{"(object)a x=1", R"({"a": {"x": 1}})"},
	{"a x=1 x=2", R"({"a": {"x": 2}})"},
	{"// comment\n/- skipped 1\na 0x10 1_000", R"({"a": [16, 1000]})"},
	{"a 1e3", R"({"a": 1000.0})"},
	{"a \"\"\"\n    multi\n    line\n    \"\"\"", R"({"a": "multi\nline"})"},
	{"a #\"raw\\n\"#", R"({"a": "raw\\n"})"},
};

static void testReader() {
	for (auto &c : s_readCases) {
		data::kdl::ParseError err;
		Value v;
		auto st = data::kdl::read<Interface>(c.kdl, v, &err);
		check(st == Status::Ok, mem_std::toString("read ", c.kdl, " (", err.message, ")"));
		checkEq(toGit(v), toGit(fromJson(c.json)), mem_std::toString("read ", c.kdl));
	}
}

struct ErrorCase {
	StringView kdl;
	uint32_t line;
	uint32_t column;
	StringView name;
};

const ErrorCase s_errorCases[] = {
	{"a 1\na 2", 2, 1, "a key twice at the top"},
	{"d x=1 {\n    x 2\n}", 2, 5, "a key as a property and as a child"},
	{"a 1 x=2", 1, 1, "arguments with properties"},
	{"a 1 {\n    b 2\n}", 2, 5, "an array element not named -"},
	{"(map)a x=1", 1, 1, "an unknown node annotation"},
	{"a (u8)1", 1, 3, "an unknown value annotation"},
	{"a", 1, 1, "a node with no value"},
	{"a (base64)\"not base64!\"", 1, 3, "an invalid base64 string"},
	{"a 99999999999999999999", 1, 3, "an integer past int64"},
	{"(array)a x=1", 1, 10, "an array with properties"},
	{"(object)a 1", 1, 11, "an object with arguments"},
	{"a {\n  b 1\n", 3, 1, "a syntax error comes from the parser"},
};

static void testErrors() {
	for (auto &c : s_errorCases) {
		data::kdl::ParseError err;
		Value v;
		auto st = data::kdl::read<Interface>(c.kdl, v, &err);
		check(st != Status::Ok && !err.message.empty(), mem_std::toString("error: ", c.name));
		check(err.location.line == c.line && err.location.column == c.column,
				mem_std::toString("error: ", c.name, " at ", c.line, ":", c.column, ", reported ",
						err.location.line, ":", err.location.column, " (", err.message, ")"));
		check(data::kdl::read<Interface>(c.kdl).isNull(),
				mem_std::toString("error: ", c.name, " reads as an empty value"));
	}

	// What the writer refuses: a root that is not a dictionary, and text that is not UTF-8.
	check(toKdl(fromJson("[1, 2]")).empty(), "refuse: an array root");
	check(toKdl(Value(5)).empty(), "refuse: a scalar root");
	Value bad(Value::Type::DICTIONARY);
	bad.setString(StringView("\xff\xfe", 2), "s");
	mem_std::String out;
	check(!data::kdl::write<Interface>([&](StringView s) { out.append(s.data(), s.size()); }, bad)
					&& out.empty(),
			"refuse: a string that is not UTF-8, and nothing is written");
}

// The four entry points of stappler_data produce and accept the same text.
static void testFormat() {
	auto v = makeEdges();
	auto text = toKdl(v);

	checkEq(data::toString<Interface>(v, data::EncodeFormat::Kdl), text, "format: toString");
	auto bytes = data::write<Interface>(v, data::EncodeFormat::Kdl);
	checkEq(StringView((const char *)bytes.data(), bytes.size()), text, "format: write as bytes");
	mem_std::String streamed;
	check(data::write<Interface>([&](StringView s) { streamed.append(s.data(), s.size()); }, v,
				  data::EncodeFormat::Kdl),
			"format: write to a stream");
	checkEq(streamed, text, "format: the stream has the same text");
	check(data::EncodeFormat(data::EncodeFormat::Kdl).isTextual(), "format: Kdl is textual");

	check(sameValue(data::read<Interface>(bytes, data::DataFormat::Kdl), v),
			"format: read with DataFormat::Kdl");

	FileInfo probe{"xlkdl_probe.kdl", FileCategory::Custom};
	filesystem::remove(probe);
	check(data::save<Interface>(v, probe), "format: save by the .kdl extension");
	auto onDisk = filesystem::readIntoMemory<Interface>(probe);
	checkEq(StringView((const char *)onDisk.data(), onDisk.size()), text,
			"format: the file is the KDL text");
	check(sameValue(data::readFile<Interface>(probe), v), "format: readFile by the .kdl extension");
	check(!data::save<Interface>(fromJson("[1]"), probe, data::EncodeFormat::Kdl),
			"format: save refuses an array root");
	onDisk = filesystem::readIntoMemory<Interface>(probe);
	checkEq(StringView((const char *)onDisk.data(), onDisk.size()), text,
			"format: a refused save leaves the file as it was");
	filesystem::remove(probe);
}

} // namespace

void performKdlDataTests() {
	testWriter();
	testCircle();
	testReader();
	testErrors();
	testFormat();
}

} // namespace STAPPLER_VERSIONIZED stappler
