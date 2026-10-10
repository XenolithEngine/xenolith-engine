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

#include "SPCommon.h"
#include "SPKdl.h"
#include "SPFilesystem.h"

#include "../tests.h"

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using Document = kdl::DocumentTemplate<memory::StandardInterface>;

struct Case {
	StringView input;
	StringView expected;
};

// Inputs and their canonical form; an empty expectation is a document of no nodes.
const Case s_cases[] = {
	{"node", "node\n"},
	{"node 1 2 3", "node 1 2 3\n"},
	{"node a=1 a=2 b=3", "node a=2 b=3\n"},
	{"node z=1 y=2 \"x\" 3", "node x 3 y=2 z=1\n"},
	{"(t)node (u8)12 key=(f64)1.5", "(t)node (u8)12 key=(f64)1.5\n"},
	{"node 0xFF 0o17 0b101 1_000", "node 255 15 5 1000\n"},
	{"node 0xABCDEF0123456789abcdef", "node 207698809136909011942886895\n"},
	{"node 1e10 1.0e-1_0 -2.5E+3", "node 1E+10 1.0E-10 -2.5E+3\n"},
	{"node #true #false #null #inf #-inf #nan", "node #true #false #null #inf #-inf #nan\n"},
	{"node \"a b\" \"plain\" #\"raw\\n\"#", "node \"a b\" plain \"raw\\\\n\"\n"},
	{"node \"\\u{48}\\s\\t\"", "node \"H \\t\"\n"},
	{"node \"Hello \\    World\"", "node \"Hello World\"\n"},
	{"node \"\"\"\n    foo\n      bar\n    \"\"\"", "node \"foo\\n  bar\"\n"},
	{"node \"\"\"\n  a\n\n  b\n  \"\"\"", "node \"a\\n\\nb\"\n"},
	{"node #\"\"\"\n  \\n \"\"\"\n  \"\"\"#", "node \"\\\\n \\\"\\\"\\\"\"\n"},
	{"node ##\"a\"#b\"##", "node \"a\\\"#b\"\n"},
	{"parent { child1; child2 }", "parent {\n    child1\n    child2\n}\n"},
	{"a {\n  b {\n    c 1\n  }\n}", "a {\n    b {\n        c 1\n    }\n}\n"},
	{"node {}", "node\n"},
	{"/- node 1\nother", "other\n"},
	{"node /- 1 2 /- k=v /- { child }", "node 2\n"},
	{"node 1 \\ // comment\n  2", "node 1 2\n"},
	{"node /* inline /* nested */ */ 1", "node 1\n"},
	{"// only a comment\n", ""},
	{"\xEF\xBB\xBFnode", "node\n"},
	{"/- kdl-version 2\nnode", "node\n"},
	{"node -.x +foo --bar .md", "node -.x +foo --bar .md\n"},
	{"node \"true\" \"1x\"", "node \"true\" \"1x\"\n"},
	{"node x=\"\\u{0}\"", "node x=\"\\u{0}\"\n"},
	{"\"quoted name\" \"\"=1", "\"quoted name\" \"\"=1\n"},
	{"node a=b;node2", "node a=b\nnode2\n"},
	{"node\r\nnode2\r\n", "node\nnode2\n"},
};

// Documents that must not parse.
const StringView s_failures[] = {
	"node true",
	"node inf",
	"node 1x",
	"node .1",
	"node -.5",
	"node \"unclosed",
	"node \"a\nb\"",
	"node \"\\q\"",
	"node \"\\u{D800}\"",
	"node \"\\u{110000}\"",
	"node \"\"\"foo\"\"\"",
	"node \"\"\"\n  a\n b\n  \"\"\"",
	"node \"\"\"\n  a\n  b\"\"\"",
	"node #\"raw\nline\"#",
	"node {} 1",
	"node { a } { b }",
	"node \"a\"\"b\"",
	"node (t)",
	"node (t)key=1",
	"node 1=2",
	"node {",
	"}",
	"node /* unclosed",
	"node \\ x",
	"node #maybe",
	"node [1]",
	"node \x01",
	"node \xC0\x80",
	"node \xEF\xBB\xBF",
	"/- kdl-version 1\nnode",
	"/-",
};

mem_std::String canonical(const Document &doc) {
	mem_std::StringStream out;
	kdl::write(out, doc);
	return out.str();
}

void checkCases() {
	uint32_t failed = 0;
	for (auto &it : s_cases) {
		Document doc;
		kdl::ParseError err;
		auto st = kdl::read(it.input, doc, &err);
		if (st != Status::Ok) {
			++failed;
			sprt::cout << "  parse failed: " << it.input << ": " << err.location.line << ":"
					   << err.location.column << ": " << err.message << "\n";
			continue;
		}
		auto out = canonical(doc);
		if (StringView(out) != it.expected) {
			++failed;
			sprt::cout << "  mismatch: " << it.input << "\n    got:      " << out
					   << "\n    expected: " << it.expected << "\n";
		}
	}
	check(failed == 0, "kdl: canonical form of the parsed cases");

	failed = 0;
	for (auto &it : s_failures) {
		Document doc;
		kdl::ParseError err;
		if (kdl::read(it, doc, &err) == Status::Ok) {
			++failed;
			sprt::cout << "  parsed but must fail: " << it << "\n";
		} else if (err.message.empty() || err.location.line == 0) {
			++failed;
			sprt::cout << "  failed without a located message: " << it << "\n";
		}
	}
	check(failed == 0, "kdl: invalid documents are refused with a located message");
}

void checkDom() {
	Document doc;
	auto st = kdl::read(
			StringView("first\nsecond 1 key=\"v\" {\n    (ty)child 0xFFFF_FFFF_FFFF_FFFF\n}\n"),
			doc);
	check(st == Status::Ok && doc.nodes.size() == 2, "kdl: dom has the top-level nodes");
	if (doc.nodes.size() != 2) {
		return;
	}
	auto &second = doc.nodes[1];
	check(second.location.line == 2 && second.location.column == 1, "kdl: node location");
	check(second.arguments.size() == 1 && second.arguments[0].getInteger().get(0) == 1,
			"kdl: integer argument");
	auto key = second.getPropertyValue("key");
	check(key && key->getString() == "v", "kdl: property lookup");
	check(second.children.size() == 1 && second.children[0].getAnnotation() == "ty",
			"kdl: child annotation");
	if (second.children.size() == 1) {
		auto &child = second.children[0];
		auto &big = child.arguments[0];
		check(child.location.line == 3 && child.location.column == 5, "kdl: child location");
		check(!big.getInteger().valid(), "kdl: u64 max does not fit int64");
		check(big.getUnsigned().get(0) == sprt::Max<uint64_t>, "kdl: u64 max fits uint64");
		checkEq(big.literal, "0xFFFF_FFFF_FFFF_FFFF", "kdl: literal is kept as written");
	}

	memory::perform_temporary([&] {
		kdl::DocumentTemplate<memory::PoolInterface> pooled;
		auto st = kdl::read(StringView("node (t)\"x\" k=0x10 {\n child\n}"), pooled);
		mem_std::StringStream out;
		kdl::write(out, pooled);
		check(st == Status::Ok && StringView(out.str()) == "node (t)x k=16 {\n    child\n}\n",
				"kdl: a document in a pool");
	});

	kdl::ParseError err;
	Document bad;
	kdl::read(StringView("ok\nnode \"x\" (t)\n"), bad, &err);
	check(err.location.line == 2 && err.location.column == 13, "kdl: error location");
}

// The KDL test suite (kdl-org/kdl, tests/test_cases) is not carried in the repository: point
// XL_KDL_TEST_SUITE at a directory with `input` and `expected_kdl` to run it.
void checkSuite() {
	auto root = ::getenv("XL_KDL_TEST_SUITE");
	if (!root) {
		sprt::cout << "[SKIP] kdl: test suite (set XL_KDL_TEST_SUITE)\n";
		return;
	}

	auto inputDir = mem_std::toString(root, "/input");
	mem_std::Vector<mem_std::String> inputs;
	filesystem::ftw(FileInfo{inputDir}, [&](const FileInfo &info, FileType type) {
		if (type == FileType::File && info.path.ends_with(".kdl")) {
			inputs.emplace_back(info.path.str<memory::StandardInterface>());
		}
		return true;
	});
	sprt::sort(inputs.begin(), inputs.end());

	uint32_t failed = 0;
	for (auto &path : inputs) {
		auto name = filepath::lastComponent(path);
		auto text = filesystem::readTextFile<memory::StandardInterface>(FileInfo{path});
		Document doc;
		kdl::ParseError err;
		auto st = kdl::read(text, doc, &err);
		if (name.ends_with("_fail.kdl")) {
			if (st == Status::Ok) {
				++failed;
				sprt::cout << "  parsed but must fail: " << name << "\n";
			}
			continue;
		}
		if (st != Status::Ok) {
			++failed;
			sprt::cout << "  parse failed: " << name << ": " << err.location.line << ":"
					   << err.location.column << ": " << err.message << "\n";
			continue;
		}
		auto expected = filesystem::readTextFile<memory::StandardInterface>(
				FileInfo{mem_std::toString(root, "/expected_kdl/", name)});
		auto out = canonical(doc);
		if (out.empty()) {
			out = "\n";
		}
		if (StringView(out) != StringView(expected)) {
			++failed;
			sprt::cout << "  mismatch: " << name << "\n    got:      " << out
					   << "    expected: " << expected;
		}
	}
	sprt::cout << "  " << inputs.size() << " documents, " << failed << " failed\n";
	check(!inputs.empty() && failed == 0, "kdl: test suite");
}

} // namespace

void performKdlTests() {
	checkCases();
	checkDom();
	checkSuite();
}

} // namespace STAPPLER_VERSIONIZED stappler
