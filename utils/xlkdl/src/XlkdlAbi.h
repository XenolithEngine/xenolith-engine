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

#ifndef UTILS_XLKDL_SRC_XLKDLABI_H_
#define UTILS_XLKDL_SRC_XLKDLABI_H_

#include "SPKdl.h"

// The ABI description model: a header map (the profile, every header and what it includes) and,
// for each generated header, the definitions of its description. C fragments - macro bodies,
// conditions, types - are kept as written and rendered with the profile's name rules on output.
namespace STAPPLER_VERSIONIZED stappler::xlkdl {

using String = mem_std::String;
template <typename T>
using Vector = mem_std::Vector<T>;

using KdlDocument = kdl::DocumentTemplate<memory::StandardInterface>;
using KdlNode = kdl::NodeTemplate<memory::StandardInterface>;
using KdlValue = kdl::ValueTemplate<memory::StandardInterface>;

struct Diagnostics {
	Vector<String> messages;

	void error(StringView file, kdl::Location loc, StringView message);
	bool empty() const { return messages.empty(); }
};

// How a flagged name is written: `prefix` puts text before it, `wrap` puts it in a macro call.
struct NameRule {
	String name;
	bool wrap = false;
	String text;

	// What a wrap call expands to in C, as a prefix (`__sprt_` for `__SPRT_ID`); for search only.
	String spelled;

	// A prefix rule may also cut a trailing `trim`, write the name in capitals and append `suffix`:
	// `rule n prefix=__SPRT_ suffix=_NAME case=upper trim=_t` writes mbstate_t as __SPRT_MBSTATE_NAME.
	String suffix;
	String trim;
	bool upper = false;

	String apply(StringView name) const;
};

struct Name {
	String name;
	const NameRule *rule = nullptr;

	String render() const { return rule ? rule->apply(name) : name; }
};

struct Header;

struct Symbol {
	String name;
	const NameRule *rule = nullptr;
	const Header *header = nullptr;
	kdl::Location location;
};

enum class ItemKind {
	Macro,
	Alias,
	Undef,
	Typedef,
	Record,
	Enum,
	Value,
	Field,
	Use,
	Decl,
	Error,
	Public,

	// A list of targets (`targets PLATFORM`), a group of its targets, one target.
	Targets,
	TargetGroup,
	Target,
};

// Where a block or an include comes from: a header of the map, or the variant of a platform
// fragment the preprocessor selects (`__SPRT_PLATFORM_NAME/F`, `__SPRT_PLATFORM_NAME/__SPRT_ARCH_NAME/F`).
enum class SourceKind : uint8_t {
	Path,
	Platform,
	Arch,
};

enum class ExternC : uint8_t {
	None,
	Macro,
	Raw,
};

struct Item {
	ItemKind kind = ItemKind::Macro;
	kdl::Location location;
	kdl::Location textLocation;
	kdl::Location conditionLocation;

	// A preprocessor expression the item is guarded by, as written; empty when unguarded.
	String condition;
	String ignoreWarning;

	// Macro, typedef, field and enumerator name; a record's tag.
	Name name;

	// A function-like macro or alias has a parameter list, possibly empty.
	bool hasParams = false;

	// A macro whose name is also defined without its rule, at the description's `public` node.
	bool isPublic = false;

	// An error message written as it is, not in quotes.
	bool raw = false;

	// A list of targets: the prefix of each target's flag macro, and what follows a target's name in
	// its directory name (`_sprt`).
	String flag;
	String suffix;
	Vector<String> params;

	// Macro body, typedef or field type, enumerator value, declaration, error message.
	String text;

	// The macro an alias names.
	String target;

	bool isUnion = false;

	// The member a nested record declares (`struct {...} u;`); empty for an anonymous member.
	String member;

	// The typedef a top-level record declares with it (`typedef struct X {...} Y;`), and further
	// declarators of the same declaration (`*PY, Z`).
	Name typedefName;
	String also;

	// Record and field details: a bit width, attributes before the tag (or between a field's type
	// and name) and after the closing brace, a C++ base, `#pragma pack` with its own guard.
	String bits;
	String attr;
	String attrAfter;
	String base;
	String pack;
	String packCondition;

	// A block: what it is and whether it is written in place of its include.
	SourceKind source = SourceKind::Path;
	String sourcePath;
	bool inlined = false;

	ExternC externC = ExternC::None;

	// Record members, enumerators, the overrides of a block.
	Vector<Item> children;
};

struct Include {
	SourceKind kind = SourceKind::Path;
	String path;
	String condition;
	kdl::Location location;
	kdl::Location conditionLocation;
};

struct Header {
	String path;

	// The part of the map the header is declared in.
	String mapFile;

	String guard;
	bool external = false;
	bool fragment = false;
	String abi;
	String abiFile;
	kdl::Location location;

	Vector<Include> includes;

	// Every header an include can resolve to: all variants of a platform include.
	Vector<const Header *> includeHeaders;

	// Blocks the description uses, and the headers that use this one as a block.
	Vector<const Header *> blocks;
	Vector<const Header *> users;

	Vector<Symbol> symbols;
	Vector<Item> items;
};

struct Platform {
	String name;
	Vector<String> arches;

	// An arch that takes every fragment it has none of from another arch of the platform.
	Vector<sprt::pair<String, String>> like;
};

struct GuardScheme {
	String root;
	String prefix;
};

struct Map;

// The rules names of a kind of node take when they are written without one, for the headers under
// `root`: `defaults root="sprt/gen/c/" macro=p typedef=id`.
struct Defaults {
	String root;
	Vector<sprt::pair<String, String>> rules;

	const NameRule *find(const Map &map, StringView kind) const;
};

// Where the description of a header is when the map does not name it: `abi-root header= dir=`.
struct AbiRoot {
	String header;
	String dir;
};

// A named condition, `@name` in an `if=`.
struct Condition {
	String name;
	String text;
};

struct Map {
	// The root of the map, and its directory: descriptions and the license are found from there.
	String file;
	String dir;

	// The parts it imports, root first.
	Vector<String> files;
	String license;

	Vector<NameRule> rules;
	Vector<Header> headers;

	Vector<Platform> platforms;
	String platformRoot;
	String platformInclude;
	String archInclude;
	String externCBegin;
	String externCEnd;
	Vector<GuardScheme> guardSchemes;
	Vector<Defaults> defaults;
	Vector<AbiRoot> abiRoots;
	Vector<Condition> conditions;

	const NameRule *findRule(StringView) const;
	const Defaults *findDefaults(StringView headerPath) const;
	const Header *findHeader(StringView) const;

	// The headers a source names: one for a path, one per platform or per platform and arch.
	Vector<String> variants(SourceKind kind, StringView path) const;
};

struct EmitOptions {
	// Write every block of a fragment in place of its include.
	bool inlineBlocks = false;
};

// Loads the map and the descriptions of its generated headers. False with diagnostics when any
// document is not what the model expects.
bool loadMap(StringView file, Map &map, Diagnostics &diag);

// The text of a generated header; false with diagnostics on an unresolved name.
bool emitHeader(const Map &map, const Header &header, const EmitOptions &opts, String &out,
		Diagnostics &diag);

// Every definition of each name, as written in a description or as spelled in C; returns how many
// names are not defined anywhere.
int findNames(const Map &map, SpanView<String> names, const Callback<void(StringView)> &out);

// Every definition of the map, as JSON.
void writeIndex(const Map &map, const Callback<void(StringView)> &out);

int runDump(int argc, const char *argv[]);
int runFormat(int argc, const char *argv[]);
int runAbi(int argc, const char *argv[]);

} // namespace stappler::xlkdl

#endif /* UTILS_XLKDL_SRC_XLKDLABI_H_ */
