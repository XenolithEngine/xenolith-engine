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

#include "XlkdlAbi.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

void Diagnostics::error(StringView file, kdl::Location loc, StringView message) {
	messages.emplace_back(mem_std::toString(file, ":", loc.line, ":", loc.column, ": ", message));
}

String NameRule::apply(StringView n) const {
	if (wrap) {
		return mem_std::toString(text, "(", n, ")");
	}
	if (!trim.empty() && n.ends_with(StringView(trim)) && n.size() > trim.size()) {
		n = n.sub(0, n.size() - trim.size());
	}
	String out = text;
	for (auto c : n) { out.push_back((upper && c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c); }
	out.append(suffix);
	return out;
}

const NameRule *Defaults::find(const Map &map, StringView kind) const {
	for (auto &it : rules) {
		if (StringView(it.first) == kind) {
			return map.findRule(it.second);
		}
	}
	return nullptr;
}

// The defaults of the longest root the path is under: an API map adds roots inside the ABI's.
const Defaults *Map::findDefaults(StringView path) const {
	const Defaults *found = nullptr;
	for (auto &it : defaults) {
		if (path.starts_with(StringView(it.root)) && (!found || it.root.size() > found->root.size())) {
			found = &it;
		}
	}
	return found;
}

const NameRule *Map::findRule(StringView name) const {
	for (auto &it : rules) {
		if (StringView(it.name) == name) {
			return &it;
		}
	}
	return nullptr;
}

const Linkage *Map::findLinkage(StringView name) const {
	for (auto &it : linkages) {
		if (StringView(it.name) == name) {
			return &it;
		}
	}
	return nullptr;
}

const Charset *Map::findCharset(StringView name) const {
	for (auto &it : charsets) {
		if (StringView(it.name) == name) {
			return &it;
		}
	}
	return nullptr;
}

const Dll *Map::findDll(StringView name) const {
	for (auto &it : dlls) {
		if (StringView(it.name) == name) {
			return &it;
		}
	}
	return nullptr;
}

const Header *Map::findHeader(StringView path) const {
	for (auto &it : headers) {
		if (StringView(it.path) == path) {
			return &it;
		}
	}
	return nullptr;
}

Vector<String> Map::variants(SourceKind kind, StringView path) const {
	Vector<String> out;
	switch (kind) {
	case SourceKind::Path: out.emplace_back(path.str<memory::StandardInterface>()); break;
	case SourceKind::Platform:
		for (auto &p : platforms) {
			out.emplace_back(mem_std::toString(platformRoot, "/", p.name, "/", path));
		}
		break;
	case SourceKind::Arch:
		for (auto &p : platforms) {
			for (auto &a : p.arches) {
				out.emplace_back(mem_std::toString(platformRoot, "/", p.name, "/", a, "/", path));
			}
		}
		break;
	}
	return out;
}

namespace {

// What a node may carry: how many arguments, which properties, whether children.
struct NodeShape {
	size_t minArgs = 0;
	size_t maxArgs = 0;
	SpanView<StringView> props;
	bool children = false;
};

// Where an item stands, which decides what it may be.
enum class Context : uint8_t {
	Top,
	Record,
	Enum,
	Overrides,
};

String upperName(StringView name);
Name targetMacro(const Item &list, StringView part, StringView target = StringView());
void listTargets(const Item &list, Vector<const Item *> &out);
Vector<String> targetAliases(const Item &target);
String targetDir(const Item &list, const Item &target);

class Loader {
public:
	Loader(Map &map, Diagnostics &diag) : _map(&map), _diag(&diag) { }

	bool load(StringView file);

private:
	bool readDocument(StringView file, KdlDocument &doc);
	bool checkShape(const KdlNode &node, const NodeShape &shape);
	bool readString(const KdlValue &value, String &out, StringView what);
	bool readName(const KdlValue &value, Name &out, StringView what, StringView kind = StringView());
	bool readCondition(const KdlValue &value, String &out, kdl::Location &loc);
	bool expand(StringView in, kdl::Location loc, String &out);
	bool loadCondition(const KdlNode &node, Vector<Condition> &list);
	bool readBody(const KdlValue &value, String &out, StringView what);
	bool readParams(const KdlValue &value, Item &item);
	bool readOptional(const KdlNode &node, StringView prop, String &out);
	bool readSource(const KdlNode &node, SourceKind &kind, String &path, kdl::Location &loc);
	bool readCommon(const KdlNode &node, Item &item);

	void error(kdl::Location loc, StringView message) { _diag->error(_file, loc, message); }

	bool loadPart(StringView file, bool root);
	bool loadProfile(const KdlNode &node);
	bool loadPlatforms(const KdlNode &node);
	bool loadHeader(const KdlNode &node);
	void addLikeHeaders();
	bool resolveIncludes();
	bool loadDescription(Header &header);
	bool loadItem(const KdlNode &node, Item &item, Context ctx);
	bool loadRecord(const KdlNode &node, Item &item, Context ctx);
	bool loadEnum(const KdlNode &node, Item &item, Context ctx);
	bool loadUse(const KdlNode &node, Item &item, Context ctx);
	bool loadTargets(const KdlNode &node, Item &item, Context ctx);
	bool loadTarget(const KdlNode &node, Item &item);
	bool checkTargets();
	bool collectSymbols(Header &header, const Item &item);
	bool resolveBlocks(Header &header, const Vector<Item> &items);
	bool checkCycles();

	// An API map: the ABI map it loads, the nodes of its profile, its functions and DLLs.
	bool loadAbiMap(const KdlNode &node);
	bool loadApiProfile(const KdlNode &child, bool &handled);
	bool readFuncProps(const KdlNode &node, Item &item, bool defaults, bool &stubGiven);
	bool loadFuncDefaults(const KdlNode &node);
	bool loadFunc(const KdlNode &node, Item &item, Context ctx);
	bool loadDll(const KdlNode &node, const Header &header);
	bool resolveUmbrellas();
	bool checkDlls();

	Map *_map = nullptr;
	Diagnostics *_diag = nullptr;
	String _file;

	// A fragment's description may be a list of record members, for a block inside a record.
	bool _fragment = false;

	// The rules of names written without one, and the named conditions of the description loaded.
	const Defaults *_defaults = nullptr;
	Vector<Condition> _conditions;

	// The defaults a description was loaded with: a shared one has to get the same everywhere.
	mem_std::Map<String, const Defaults *> _descriptionDefaults;

	// What the functions of the description loaded take when they do not say, and whether it says
	// stub=: without it a function with dll= has a generated wrapper.
	Item _funcDefaults;
	bool _funcStubGiven = false;
};

const StringView s_ruleProps[] = {"prefix", "wrap", "spelled", "suffix", "case", "trim"};
const StringView s_defaultKinds[] = {"macro", "undef", "typedef", "tag", "value", "func"};
const StringView s_abiRootProps[] = {"header", "dir"};
const StringView s_archProps[] = {"like"};
const StringView s_externProps[] = {"begin", "end"};
const StringView s_schemeProps[] = {"root", "prefix"};
const StringView s_headerProps[] = {"abi", "api", "external", "fragment", "guard", "umbrella"};
const StringView s_includeProps[] = {"platform", "arch", "if"};
const StringView s_macroProps[] = {"params", "if", "public"};
const StringView s_aliasProps[] = {"to", "params", "if"};
const StringView s_undefProps[] = {"if"};
const StringView s_typedefProps[] = {"if"};
const StringView s_recordProps[] = {"typedef", "also", "if", "ignore-warning", "attr", "attr-after",
	"pack", "pack-if", "base"};
const StringView s_nestedRecordProps[] = {"tag", "if", "attr", "attr-after"};
const StringView s_enumProps[] = {"typedef", "also", "if", "attr", "macro", "base"};
const StringView s_valueProps[] = {"if"};
const StringView s_fieldProps[] = {"if", "bits", "attr"};
const StringView s_useProps[] = {"platform", "arch", "inline", "if"};
const StringView s_declProps[] = {"extern-c", "if"};
const StringView s_errorProps[] = {"if", "raw"};
const StringView s_publicProps[] = {"if"};
const StringView s_targetsProps[] = {"flag", "suffix", "error"};
const StringView s_groupProps[] = {"if"};
const StringView s_targetProps[] = {"if", "also", "dir"};

bool Loader::readDocument(StringView file, KdlDocument &doc) {
	_file = file.str<memory::StandardInterface>();
	if (!filesystem::exists(FileInfo{file})) {
		_diag->error(file, kdl::Location{}, "no such file");
		return false;
	}
	auto text = filesystem::readTextFile<memory::StandardInterface>(FileInfo{file});
	kdl::ParseError err;
	if (kdl::read(text, doc, &err) != Status::Ok) {
		_diag->error(file, err.location, err.message);
		return false;
	}
	return true;
}

bool Loader::checkShape(const KdlNode &node, const NodeShape &shape) {
	bool ok = true;
	if (node.arguments.size() < shape.minArgs || node.arguments.size() > shape.maxArgs) {
		error(node.location,
				mem_std::toString("'", node.name, "' takes ",
						shape.minArgs == shape.maxArgs
								? mem_std::toString(shape.minArgs)
								: mem_std::toString(shape.minArgs, " to ", shape.maxArgs),
						" arguments, not ", node.arguments.size()));
		ok = false;
	}
	for (auto &prop : node.properties) {
		bool known = false;
		for (auto &it : shape.props) {
			if (it == StringView(prop.name)) {
				known = true;
			}
		}
		if (!known) {
			error(prop.location,
					mem_std::toString("unknown property '", prop.name, "' of '", node.name, "'"));
			ok = false;
		}
	}
	if (!shape.children && !node.children.empty()) {
		error(node.location, mem_std::toString("'", node.name, "' has no children"));
		ok = false;
	}
	return ok;
}

bool Loader::readString(const KdlValue &value, String &out, StringView what) {
	if (!value.isString()) {
		error(value.location, mem_std::toString(what, " must be a string"));
		return false;
	}
	out = value.getString().str<memory::StandardInterface>();
	return true;
}

// `(p)NAME` takes the rule p, `` `NAME` `` none, and a plain NAME the default of its kind of node.
bool Loader::readName(const KdlValue &value, Name &out, StringView what, StringView kind) {
	if (!readString(value, out.name, what)) {
		return false;
	}
	out.rule = nullptr;
	if (value.hasAnnotation) {
		out.rule = _map->findRule(value.getAnnotation());
		if (!out.rule) {
			error(value.location,
					mem_std::toString("no name rule '", value.getAnnotation(), "' in the profile"));
			return false;
		}
	} else if (out.name.size() > 2 && out.name.front() == '`' && out.name.back() == '`') {
		out.name = StringView(out.name).sub(1, out.name.size() - 2).str<memory::StandardInterface>();
	} else if (!kind.empty() && _defaults) {
		out.rule = _defaults->find(*_map, kind);
	}
	return true;
}

// A macro body or an enumerator value: a string, or a number written as it is in C.
bool Loader::readBody(const KdlValue &value, String &out, StringView what) {
	if (value.isNumber()) {
		StringView lit(value.literal);
		if (lit.find('_') != sprt::Max<size_t> || lit.starts_with("0o") || lit.starts_with("-0o")) {
			error(value.location,
					mem_std::toString("the number ", lit, " is not spelled as in C; write it as a string"));
			return false;
		}
		out = lit.str<memory::StandardInterface>();
		return true;
	}
	return readString(value, out, what);
}

static bool isConditionName(StringView name) {
	if (name.empty() || !((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')
								 || name[0] == '_')) {
		return false;
	}
	for (auto c : name) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
			return false;
		}
	}
	return true;
}

// An expression that needs no parentheses wherever it is put: a name, a number, a call, or a group.
static bool isAtomic(StringView expr) {
	expr.trimChars<StringView::WhiteSpace>();
	if (expr.empty()) {
		return false;
	}
	auto closes = [&](size_t open) {
		size_t depth = 0;
		for (size_t i = open; i < expr.size(); ++i) {
			if (expr[i] == '(') {
				++depth;
			} else if (expr[i] == ')' && --depth == 0) {
				return i == expr.size() - 1;
			}
		}
		return false;
	};
	size_t i = 0;
	while (i < expr.size()
			&& ((expr[i] >= 'a' && expr[i] <= 'z') || (expr[i] >= 'A' && expr[i] <= 'Z')
					|| (expr[i] >= '0' && expr[i] <= '9') || expr[i] == '_')) {
		++i;
	}
	if (i == expr.size()) {
		return true;
	}
	return expr[i] == '(' && closes(i);
}

// Whether an expression has `||` or `?` outside parentheses: between `&&`s it needs them.
static bool hasLowOperator(StringView expr) {
	size_t depth = 0;
	for (size_t i = 0; i < expr.size(); ++i) {
		auto c = expr[i];
		if (c == '(') {
			++depth;
		} else if (c == ')') {
			--depth;
		} else if (depth == 0 && (c == '?' || (c == '|' && i + 1 < expr.size() && expr[i + 1] == '|'))) {
			return true;
		}
	}
	return false;
}

// `@name` is the named condition: of the description, or of the profile. It is put in parentheses
// when it would not be read as one operand otherwise: never when it is the whole expression or
// already stands in them, between `&&` and `||` only with `||` or `?` of its own, and next to any
// other operator unless it is atomic.
bool Loader::expand(StringView in, kdl::Location loc, String &out) {
	out.clear();
	auto trimmed = in;
	trimmed.trimChars<StringView::WhiteSpace>();
	size_t i = 0;
	while (i < in.size()) {
		if (in[i] != '@') {
			out.push_back(in[i++]);
			continue;
		}
		size_t start = i + 1;
		size_t end = start;
		while (end < in.size()
				&& ((in[end] >= 'a' && in[end] <= 'z') || (in[end] >= 'A' && in[end] <= 'Z')
						|| (in[end] >= '0' && in[end] <= '9') || in[end] == '_')) {
			++end;
		}
		auto name = in.sub(start, end - start);
		const Condition *found = nullptr;
		for (auto &it : _conditions) {
			if (StringView(it.name) == name) {
				found = &it;
			}
		}
		if (!found) {
			for (auto &it : _map->conditions) {
				if (StringView(it.name) == name) {
					found = &it;
				}
			}
		}
		if (!found) {
			error(loc, mem_std::toString("no condition '@", name, "'"));
			return false;
		}
		size_t before = i;
		while (before > 0 && (in[before - 1] == ' ' || in[before - 1] == '\t')) { --before; }
		size_t after = end;
		while (after < in.size() && (in[after] == ' ' || in[after] == '\t')) { ++after; }
		bool whole = trimmed.size() == end - i && trimmed.data() == in.data() + i;
		bool grouped = before > 0 && in[before - 1] == '(' && after < in.size() && in[after] == ')';
		auto logical = [&](StringView side, bool left) {
			if (side.empty()) {
				return true;
			}
			auto c = left ? side.back() : side.front();
			if (c == '(' || c == ')' || c == ',') {
				return (left && c != ')') || (!left && c != '(');
			}
			return left ? (side.ends_with("&&") || side.ends_with("||"))
						: (side.starts_with("&&") || side.starts_with("||"));
		};
		auto leftSide = in.sub(0, before);
		auto rightSide = in.sub(after);
		bool parens = false;
		if (whole || grouped) {
			parens = false;
		} else if (logical(leftSide, true) && logical(rightSide, false)) {
			parens = hasLowOperator(found->text);
		} else {
			parens = !isAtomic(found->text);
		}
		out.append(parens ? mem_std::toString("(", found->text, ")") : found->text);
		i = end;
	}
	return true;
}

bool Loader::readCondition(const KdlValue &value, String &out, kdl::Location &loc) {
	String raw;
	if (!readString(value, raw, "a condition")) {
		return false;
	}
	loc = value.location;
	return expand(raw, value.location, out);
}

// `cond NAME "expression"`: a condition `if=` refers to as `@NAME`.
bool Loader::loadCondition(const KdlNode &node, Vector<Condition> &list) {
	if (!checkShape(node, NodeShape{2, 2, SpanView<StringView>(), false})) {
		return false;
	}
	Condition cond;
	if (!readString(node.arguments[0], cond.name, "a condition name")) {
		return false;
	}
	if (!isConditionName(cond.name)) {
		error(node.arguments[0].location,
				mem_std::toString("'", cond.name, "' is not a condition name (letters, digits, _)"));
		return false;
	}
	for (auto &it : list) {
		if (it.name == cond.name) {
			error(node.location, mem_std::toString("a second condition '", cond.name, "'"));
			return false;
		}
	}
	kdl::Location loc;
	if (!readCondition(node.arguments[1], cond.text, loc)) {
		return false;
	}
	list.emplace_back(sprt::move(cond));
	return true;
}

bool Loader::readParams(const KdlValue &value, Item &item) {
	String text;
	if (!readString(value, text, "params")) {
		return false;
	}
	item.hasParams = true;
	StringView r(text);
	while (!r.empty()) {
		r.skipChars<StringView::Chars<' ', '\t', ','>>();
		auto param = r.readUntil<StringView::Chars<' ', '\t', ','>>();
		if (!param.empty()) {
			item.params.emplace_back(param.str<memory::StandardInterface>());
		}
	}
	return true;
}

bool Loader::readOptional(const KdlNode &node, StringView prop, String &out) {
	auto value = node.getPropertyValue(prop);
	if (!value) {
		return true;
	}
	if (value->isNumber()) {
		out = value->literal;
		return true;
	}
	return readString(*value, out, prop);
}

// `use PATH`, `use platform=F`, `use arch=F`, and the same for an include of the map.
bool Loader::readSource(const KdlNode &node, SourceKind &kind, String &path, kdl::Location &loc) {
	auto platform = node.getPropertyValue("platform");
	auto arch = node.getPropertyValue("arch");
	size_t count = node.arguments.size() + (platform ? 1 : 0) + (arch ? 1 : 0);
	if (count != 1) {
		error(node.location,
				mem_std::toString("'", node.name, "' names one of a path, platform= or arch="));
		return false;
	}
	if (platform || arch) {
		kind = platform ? SourceKind::Platform : SourceKind::Arch;
		loc = (platform ? platform : arch)->location;
		return readString(platform ? *platform : *arch, path, "a fragment name");
	}
	kind = SourceKind::Path;
	loc = node.arguments[0].location;
	return readString(node.arguments[0], path, "a path");
}

bool Loader::readCommon(const KdlNode &node, Item &item) {
	item.location = node.location;
	if (auto cond = node.getPropertyValue("if")) {
		if (!readCondition(*cond, item.condition, item.conditionLocation)) {
			return false;
		}
	}
	return readOptional(node, "ignore-warning", item.ignoreWarning);
}

bool Loader::load(StringView file) {
	_map->file = file.str<memory::StandardInterface>();
	_map->dir = filepath::root(file).str<memory::StandardInterface>();
	if (!loadPart(file, true)) {
		return false;
	}
	addLikeHeaders();
	if (!resolveIncludes()) {
		return false;
	}

	bool ok = resolveUmbrellas();
	for (auto &header : _map->headers) {
		if (!header.abi.empty()) {
			ok = loadDescription(header) && ok;
		}
	}
	_file = _map->file;
	return ok && checkCycles() && checkTargets() && checkDlls();
}

// The list whose K_NAME the platform include names lists the platforms, the one only the arch
// include names lists the arches: every directory of the map's platforms has to be a target of it.
bool Loader::checkTargets() {
	bool ok = true;
	for (auto &header : _map->headers) {
		for (auto &item : header.items) {
			if (item.kind != ItemKind::Targets) {
				continue;
			}
			auto selector = targetMacro(item, "NAME").render();
			auto inPlatform = StringView(_map->platformInclude).find(StringView(selector))
					!= sprt::Max<size_t>;
			auto inArch = StringView(_map->archInclude).find(StringView(selector)) != sprt::Max<size_t>;
			if (!inPlatform && !inArch) {
				continue;
			}
			Vector<const Item *> targets;
			listTargets(item, targets);
			auto known = [&](StringView dir) {
				for (auto t : targets) {
					if (StringView(targetDir(item, *t)) == dir) {
						return true;
					}
				}
				return false;
			};
			for (auto &platform : _map->platforms) {
				if (inPlatform && !known(platform.name)) {
					_diag->error(header.abiFile, item.location,
							mem_std::toString("the map has the platform ", platform.name,
									", which the list ", item.name.name, " does not"));
					ok = false;
				}
				if (!inPlatform && inArch) {
					for (auto &arch : platform.arches) {
						if (!known(arch)) {
							_diag->error(header.abiFile, item.location,
									mem_std::toString("the map has the arch ", arch, " of ",
											platform.name, ", which the list ", item.name.name,
											" does not"));
							ok = false;
						}
					}
				}
			}
		}
	}
	return ok;
}

// One file of the map: the root holds the profile, every file may hold platforms, headers and
// imports of further parts, found from the file that imports them.
bool Loader::loadPart(StringView file, bool root) {
	for (auto &it : _map->files) {
		if (StringView(it) == file) {
			return true;
		}
	}
	_map->files.emplace_back(file.str<memory::StandardInterface>());

	KdlDocument doc;
	if (!readDocument(file, doc)) {
		return false;
	}
	auto self = file.str<memory::StandardInterface>();

	bool ok = true;
	for (auto &node : doc.nodes) {
		_file = self;
		if (node.name == "profile") {
			if (!root) {
				error(node.location, "the profile belongs to the root of the map");
				ok = false;
				continue;
			}
			ok = loadProfile(node) && ok;
		} else if (node.name == "platforms") {
			ok = loadPlatforms(node) && ok;
		}
	}
	for (auto &node : doc.nodes) {
		_file = self;
		if (node.name == "profile" || node.name == "platforms") {
			continue;
		} else if (node.name == "header") {
			ok = loadHeader(node) && ok;
		} else if (node.name == "import") {
			String path;
			if (!checkShape(node, NodeShape{1, 1, SpanView<StringView>(), false})
					|| !readString(node.arguments[0], path, "an import")) {
				ok = false;
				continue;
			}
			auto full = filepath::merge<memory::StandardInterface>(filepath::root(StringView(self)),
					path);
			if (!filesystem::exists(FileInfo{full})) {
				error(node.arguments[0].location, mem_std::toString("no map part ", full));
				ok = false;
				continue;
			}
			ok = loadPart(full, false) && ok;
		} else {
			error(node.location, mem_std::toString("unknown map node '", node.name, "'"));
			ok = false;
		}
	}
	return ok;
}

bool Loader::loadProfile(const KdlNode &node) {
	if (!checkShape(node, NodeShape{0, 1, SpanView<StringView>(), true})) {
		return false;
	}
	bool ok = true;
	// The ABI map comes first: its rules are what the rest of the profile names.
	for (auto &child : node.children) {
		if (child.name == "abi") {
			ok = loadAbiMap(child) && ok;
		}
	}
	for (auto &child : node.children) {
		auto name = StringView(child.name);
		bool handled = false;
		if (!loadApiProfile(child, handled)) {
			ok = false;
			continue;
		}
		if (handled) {
			continue;
		}
		if (name == "rule") {
			if (!checkShape(child, NodeShape{1, 1, makeSpanView(s_ruleProps), false})) {
				ok = false;
				continue;
			}
			NameRule rule;
			readString(child.arguments[0], rule.name, "a rule name");
			auto prefix = child.getPropertyValue("prefix");
			auto wrap = child.getPropertyValue("wrap");
			if ((prefix != nullptr) == (wrap != nullptr)) {
				error(child.location, "a rule is either prefix= or wrap=");
				ok = false;
				continue;
			}
			rule.wrap = wrap != nullptr;
			String letterCase;
			if (!readString(wrap ? *wrap : *prefix, rule.text, "a rule text")
					|| !readOptional(child, "spelled", rule.spelled)
					|| !readOptional(child, "suffix", rule.suffix)
					|| !readOptional(child, "trim", rule.trim)
					|| !readOptional(child, "case", letterCase)) {
				ok = false;
				continue;
			}
			if (!letterCase.empty() && letterCase != "upper") {
				error(child.location, "case= is \"upper\" or absent");
				ok = false;
				continue;
			}
			rule.upper = !letterCase.empty();
			if (rule.wrap && (!rule.suffix.empty() || !rule.trim.empty() || rule.upper)) {
				error(child.location, "suffix=, trim= and case= belong to a prefix= rule");
				ok = false;
				continue;
			}
			if (!rule.wrap && !rule.spelled.empty()) {
				error(child.location, "spelled= belongs to a wrap= rule");
				ok = false;
				continue;
			}
			if (_map->findRule(rule.name)) {
				error(child.location, mem_std::toString("a second rule '", rule.name, "'"));
				ok = false;
				continue;
			}
			_map->rules.emplace_back(sprt::move(rule));
		} else if (name == "license") {
			if (!checkShape(child, NodeShape{1, 1, SpanView<StringView>(), false})) {
				ok = false;
				continue;
			}
			String path;
			if (!readString(child.arguments[0], path, "a license file")) {
				ok = false;
				continue;
			}
			auto full = filepath::merge<memory::StandardInterface>(_map->dir, path);
			if (!filesystem::exists(FileInfo{full})) {
				error(child.arguments[0].location, mem_std::toString("no license file ", full));
				ok = false;
				continue;
			}
			_map->license = filesystem::readTextFile<memory::StandardInterface>(FileInfo{full});
		} else if (name == "platform-root" || name == "platform-include"
				|| name == "arch-include") {
			if (!checkShape(child, NodeShape{1, 1, SpanView<StringView>(), false})) {
				ok = false;
				continue;
			}
			auto &target = name == "platform-root"
					? _map->platformRoot
					: (name == "platform-include" ? _map->platformInclude : _map->archInclude);
			ok = readString(child.arguments[0], target, name) && ok;
		} else if (name == "extern-c") {
			if (!checkShape(child, NodeShape{0, 0, makeSpanView(s_externProps), false})) {
				ok = false;
				continue;
			}
			ok = readOptional(child, "begin", _map->externCBegin)
					&& readOptional(child, "end", _map->externCEnd) && ok;
		} else if (name == "guard-scheme") {
			if (!checkShape(child, NodeShape{0, 0, makeSpanView(s_schemeProps), false})) {
				ok = false;
				continue;
			}
			GuardScheme scheme;
			ok = readOptional(child, "root", scheme.root)
					&& readOptional(child, "prefix", scheme.prefix) && ok;
			_map->guardSchemes.emplace_back(sprt::move(scheme));
		} else if (name == "defaults") {
			auto props = makeSpanView(s_defaultKinds);
			Vector<StringView> allowed(props.begin(), props.end());
			allowed.emplace_back("root");
			if (!checkShape(child, NodeShape{0, 0, makeSpanView(allowed), false})) {
				ok = false;
				continue;
			}
			Defaults defaults;
			ok = readOptional(child, "root", defaults.root) && ok;
			for (auto &prop : child.properties) {
				if (StringView(prop.name) == "root") {
					continue;
				}
				String rule;
				if (!readString(prop.value, rule, prop.name)) {
					ok = false;
					continue;
				}
				if (!_map->findRule(rule)) {
					error(prop.location, mem_std::toString("no name rule '", rule, "' in the profile"));
					ok = false;
					continue;
				}
				defaults.rules.emplace_back(String(prop.name), sprt::move(rule));
			}
			_map->defaults.emplace_back(sprt::move(defaults));
		} else if (name == "abi-root") {
			if (!checkShape(child, NodeShape{0, 0, makeSpanView(s_abiRootProps), false})) {
				ok = false;
				continue;
			}
			AbiRoot root;
			ok = readOptional(child, "header", root.header) && readOptional(child, "dir", root.dir)
					&& ok;
			_map->abiRoots.emplace_back(sprt::move(root));
		} else if (name == "cond") {
			ok = loadCondition(child, _map->conditions) && ok;
		} else {
			error(child.location, mem_std::toString("unknown profile node '", child.name, "'"));
			ok = false;
		}
	}
	return ok;
}

bool Loader::loadPlatforms(const KdlNode &node) {
	if (!checkShape(node, NodeShape{0, 0, SpanView<StringView>(), true})) {
		return false;
	}
	bool ok = true;
	for (auto &child : node.children) {
		if (child.name != "platform"
				|| !checkShape(child, NodeShape{1, 1, SpanView<StringView>(), true})) {
			error(child.location, "a platforms node holds platform NAME { arch NAMES... }");
			ok = false;
			continue;
		}
		Platform platform;
		readString(child.arguments[0], platform.name, "a platform name");
		for (auto &arch : child.children) {
			if (arch.name != "arch"
					|| !checkShape(arch, NodeShape{1, 100, makeSpanView(s_archProps), false})) {
				error(arch.location, "a platform holds arch NAMES... [like=ARCH]");
				ok = false;
				continue;
			}
			String like;
			ok = readOptional(arch, "like", like) && ok;
			for (auto &a : arch.arguments) {
				String name;
				if (readString(a, name, "an arch name")) {
					if (!like.empty()) {
						platform.like.emplace_back(name, like);
					}
					platform.arches.emplace_back(sprt::move(name));
				}
			}
		}
		_map->platforms.emplace_back(sprt::move(platform));
	}
	return ok;
}

// A path as the include guard of the original headers spells it: SPRT_WRAPPERS_..._H_, or with the
// prefix of the first guard scheme whose root the path is under.
String guardOf(const Map &map, StringView path) {
	StringView rest = path;
	String out;
	for (auto &scheme : map.guardSchemes) {
		if (path.starts_with(StringView(scheme.root))) {
			rest = path.sub(scheme.root.size());
			out = scheme.prefix;
			break;
		}
	}
	for (auto c : rest) {
		if ((c >= 'a' && c <= 'z')) {
			out.push_back(c - 'a' + 'A');
		} else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			out.push_back(c);
		} else {
			out.push_back('_');
		}
	}
	out.push_back('_');
	return out;
}

bool Loader::loadHeader(const KdlNode &node) {
	if (!checkShape(node, NodeShape{1, 1, makeSpanView(s_headerProps), true})) {
		return false;
	}
	Header header;
	header.location = node.location;
	header.mapFile = _file;
	if (!readString(node.arguments[0], header.path, "a header path")) {
		return false;
	}
	if (_map->findHeader(header.path)) {
		error(node.location, mem_std::toString("header ", header.path, " is already in the map"));
		return false;
	}

	auto flag = [&](StringView name) {
		auto v = node.getPropertyValue(name);
		return v && v->isBool() && v->boolean;
	};
	header.external = flag("external");
	// Everything under the platform root is a fragment unless the map says otherwise.
	if (node.getPropertyValue("fragment")) {
		header.fragment = flag("fragment");
	} else {
		StringView root(_map->platformRoot);
		StringView path(header.path);
		header.fragment = !header.external && !root.empty() && path.starts_with(root)
				&& path.size() > root.size() && path[root.size()] == '/'
				&& path.sub(root.size() + 1).find('/') != sprt::Max<size_t>;
	}
	// An API map names a description api=; an umbrella header has none of its own.
	auto abi = node.getPropertyValue("abi");
	if (auto api = node.getPropertyValue("api")) {
		if (abi) {
			error(node.location, "a header has abi= or api=, not both");
			return false;
		}
		abi = api;
	}
	if (header.external && abi) {
		error(node.location, "an external header has no description");
		return false;
	}
	if (auto umbrella = node.getPropertyValue("umbrella")) {
		if (abi || header.external) {
			error(node.location, "an umbrella header writes the functions of another, and has no description");
			return false;
		}
		if (!readString(*umbrella, header.umbrellaOf, "umbrella")) {
			return false;
		}
	}
	if (abi) {
		if (!readString(*abi, header.abi, "abi")) {
			return false;
		}
		header.abiFile = filepath::merge<memory::StandardInterface>(_map->dir, header.abi);
	} else if (!header.external) {
		// The description at the mirror of the path, when there is one, by the longest root.
		StringView path(header.path);
		const AbiRoot *found = nullptr;
		for (auto &root : _map->abiRoots) {
			if (path.starts_with(StringView(root.header)) && path.ends_with(".h")
					&& (!found || root.header.size() > found->header.size())) {
				found = &root;
			}
		}
		if (found && header.umbrellaOf.empty()) {
			auto &root = *found;
			auto rest = path.sub(root.header.size(), path.size() - root.header.size() - 2);
			auto rel = mem_std::toString(root.dir, rest, ".kdl");
			auto full = filepath::merge<memory::StandardInterface>(_map->dir, rel);
			if (filesystem::exists(FileInfo{full})) {
				header.abi = sprt::move(rel);
				header.abiFile = sprt::move(full);
			}
		}
	}
	if (auto guard = node.getPropertyValue("guard")) {
		if (!readString(*guard, header.guard, "guard")) {
			return false;
		}
	} else if (!header.fragment) {
		header.guard = guardOf(*_map, header.path);
	}

	bool ok = true;
	for (auto &child : node.children) {
		if (child.name == "include") {
			if (!checkShape(child, NodeShape{0, 1, makeSpanView(s_includeProps), false})) {
				ok = false;
				continue;
			}
			Include inc;
			if (!readSource(child, inc.kind, inc.path, inc.location)) {
				ok = false;
				continue;
			}
			if (auto cond = child.getPropertyValue("if")) {
				ok = readCondition(*cond, inc.condition, inc.conditionLocation) && ok;
			}
			header.includes.emplace_back(sprt::move(inc));
		} else if (child.name == "macro" || child.name == "type") {
			if (!header.external) {
				error(child.location, "a generated header takes its symbols from its description");
				ok = false;
				continue;
			}
			if (!checkShape(child, NodeShape{1, 100'000, SpanView<StringView>(), false})) {
				ok = false;
				continue;
			}
			for (auto &arg : child.arguments) {
				Name name;
				if (!readName(arg, name, "a symbol")) {
					ok = false;
					continue;
				}
				header.symbols.emplace_back(Symbol{name.name, name.rule, nullptr, arg.location});
			}
		} else {
			error(child.location, mem_std::toString("unknown header node '", child.name, "'"));
			ok = false;
		}
	}
	_map->headers.emplace_back(sprt::move(header));
	return ok;
}

// `arch A like=B`: every fragment of B that A has none of is, for A, an include of B's.
void Loader::addLikeHeaders() {
	Vector<Header> added;
	for (auto &platform : _map->platforms) {
		for (auto &like : platform.like) {
			auto from = mem_std::toString(_map->platformRoot, "/", platform.name, "/", like.second, "/");
			auto to = mem_std::toString(_map->platformRoot, "/", platform.name, "/", like.first, "/");
			for (auto &header : _map->headers) {
				StringView path(header.path);
				if (!path.starts_with(StringView(from))) {
					continue;
				}
				auto file = path.sub(from.size());
				if (file.find('/') != sprt::Max<size_t>) {
					continue;
				}
				auto target = mem_std::toString(to, file);
				if (_map->findHeader(target)) {
					continue;
				}
				Header h;
				h.path = sprt::move(target);
				h.mapFile = header.mapFile;
				h.location = header.location;
				h.fragment = true;
				h.imported = header.imported;
				Include inc;
				inc.path = header.path;
				inc.location = header.location;
				h.includes.emplace_back(sprt::move(inc));
				added.emplace_back(sprt::move(h));
			}
		}
	}
	for (auto &it : added) { _map->headers.emplace_back(sprt::move(it)); }
}

bool Loader::resolveIncludes() {
	bool ok = true;
	for (auto &header : _map->headers) {
		for (auto &sym : header.symbols) { sym.header = &header; }
		for (auto &inc : header.includes) {
			auto variants = _map->variants(inc.kind, inc.path);
			if (variants.empty()) {
				_diag->error(header.mapFile, inc.location,
						"platform= and arch= need platforms in the map");
				ok = false;
			}
			for (auto &path : variants) {
				auto target = _map->findHeader(path);
				if (!target) {
					_diag->error(header.mapFile, inc.location,
							mem_std::toString(header.path, " includes ", path,
									", which is not in the map"));
					ok = false;
					continue;
				}
				header.includeHeaders.emplace_back(target);
				const_cast<Header *>(target)->users.emplace_back(&header);
			}
		}
	}
	return ok;
}

bool Loader::checkCycles() {
	// Blocks are written into each other, so a cycle of blocks has no end; include cycles are the C
	// kind, broken by include guards, and name visibility walks them with a seen set.
	enum class Mark : uint8_t {
		None,
		Active,
		Done
	};
	Vector<Mark> marks(_map->headers.size(), Mark::None);
	auto indexOf = [&](const Header *h) { return size_t(h - _map->headers.data()); };
	auto visit = [&](auto &&self, const Header *h) -> bool {
		auto &mark = marks[indexOf(h)];
		if (mark == Mark::Done) {
			return true;
		}
		if (mark == Mark::Active) {
			_diag->error(h->mapFile, h->location,
					mem_std::toString("a cycle of blocks through ", h->path));
			return false;
		}
		mark = Mark::Active;
		for (auto inc : h->blocks) {
			if (!self(self, inc)) {
				return false;
			}
		}
		marks[indexOf(h)] = Mark::Done;
		return true;
	};
	for (auto &header : _map->headers) {
		if (!visit(visit, &header)) {
			return false;
		}
	}
	return true;
}

bool Loader::loadDescription(Header &header) {
	KdlDocument doc;
	if (!readDocument(header.abiFile, doc)) {
		return false;
	}
	bool ok = true;
	_fragment = header.fragment;
	_defaults = _map->findDefaults(header.path);
	_conditions.clear();
	auto seen = _descriptionDefaults.find(header.abiFile);
	if (seen == _descriptionDefaults.end()) {
		_descriptionDefaults.emplace(header.abiFile, _defaults);
	} else if (seen->second != _defaults) {
		_diag->error(header.mapFile, header.location,
				mem_std::toString(header.path, " shares ", header.abi,
						" with a header of other defaults"));
		ok = false;
	}
	_funcDefaults = Item();
	_funcStubGiven = false;
	for (auto &node : doc.nodes) {
		if (node.name == "cond") {
			ok = loadCondition(node, _conditions) && ok;
		} else if (node.name == "defaults") {
			ok = loadFuncDefaults(node) && ok;
		} else if (node.name == "dll") {
			ok = loadDll(node, header) && ok;
		}
	}
	size_t publics = 0;
	size_t marked = 0;
	for (auto &node : doc.nodes) {
		if (node.name == "cond" || node.name == "defaults" || node.name == "dll") {
			continue;
		}
		Item item;
		auto ctx = (_fragment && node.name == "field") ? Context::Record : Context::Top;
		if (!loadItem(node, item, ctx)) {
			ok = false;
			continue;
		}
		if (item.kind == ItemKind::Public && ++publics > 1) {
			error(item.location, "a second public node");
			ok = false;
		}
		marked += item.isPublic ? 1 : 0;
		ok = collectSymbols(header, item) && ok;
		header.items.emplace_back(sprt::move(item));
	}
	if (publics && !marked) {
		for (auto &it : header.items) {
			if (it.kind == ItemKind::Public) {
				error(it.location, "a public node, and no macro is public=#true");
			}
		}
		ok = false;
	}
	ok = resolveBlocks(header, header.items) && ok;
	_defaults = nullptr;
	_conditions.clear();
	_file = _map->file;
	return ok;
}

bool Loader::resolveBlocks(Header &header, const Vector<Item> &items) {
	bool ok = true;
	for (auto &item : items) {
		if (item.kind == ItemKind::Use) {
			auto variants = _map->variants(item.source, item.sourcePath);
			if (variants.empty()) {
				error(item.location, "platform= and arch= need platforms in the map");
				ok = false;
			}
			for (auto &path : variants) {
				auto target = _map->findHeader(path);
				if (!target) {
					error(item.location,
							mem_std::toString("a block from ", path, ", which is not in the map"));
					ok = false;
					continue;
				}
				if (item.inlined
						&& (!target->fragment || !target->guard.empty()
								|| item.source != SourceKind::Path)) {
					error(item.location,
							mem_std::toString(
									"only an unguarded fragment can be written inline, and ", path,
									" is not one"));
					ok = false;
				}
				header.blocks.emplace_back(target);
				const_cast<Header *>(target)->users.emplace_back(&header);
			}
		} else if (item.kind == ItemKind::Record) {
			ok = resolveBlocks(header, item.children) && ok;
		}
	}
	return ok;
}

bool Loader::loadItem(const KdlNode &node, Item &item, Context ctx) {
	auto name = StringView(node.name);
	auto allowed = [&](bool top, bool record, bool enumeration, bool overrides) {
		bool ok = (ctx == Context::Top && top) || (ctx == Context::Record && record)
				|| (ctx == Context::Enum && enumeration)
				|| (ctx == Context::Overrides && overrides);
		if (!ok) {
			error(node.location,
					mem_std::toString("'", name, "' cannot stand ",
							ctx == Context::Top				 ? "at the top of a description"
									: ctx == Context::Record ? "in a record"
									: ctx == Context::Enum	 ? "in an enum"
															 : "among the overrides of a block"));
		}
		return ok;
	};

	if (name == "macro") {
		if (!allowed(true, true, false, true)
				|| !checkShape(node, NodeShape{1, 2, makeSpanView(s_macroProps), false})) {
			return false;
		}
		item.kind = ItemKind::Macro;
		if (!readName(node.arguments[0], item.name, "a macro name", "macro")) {
			return false;
		}
		if (node.arguments.size() > 1) {
			if (!readBody(node.arguments[1], item.text, "a macro body")) {
				return false;
			}
			item.textLocation = node.arguments[1].location;
		}
		if (auto pub = node.getPropertyValue("public")) {
			item.isPublic = pub->isBool() && pub->boolean;
			if (item.isPublic && (ctx != Context::Top || !item.name.rule)) {
				error(pub->location, "a public macro stands at the top and has a name rule");
				return false;
			}
		}
		if (auto params = node.getPropertyValue("params")) {
			if (!readParams(*params, item)) {
				return false;
			}
		}
		return readCommon(node, item);
	} else if (name == "alias") {
		if (!allowed(true, true, false, false)
				|| !checkShape(node, NodeShape{1, 1, makeSpanView(s_aliasProps), false})) {
			return false;
		}
		item.kind = ItemKind::Alias;
		if (!readString(node.arguments[0], item.name.name, "an alias name")) {
			return false;
		}
		item.target = item.name.name;
		if (!readOptional(node, "to", item.target)) {
			return false;
		}
		if (auto params = node.getPropertyValue("params")) {
			if (!readParams(*params, item)) {
				return false;
			}
		}
		return readCommon(node, item);
	} else if (name == "undef") {
		if (!allowed(true, true, false, false)
				|| !checkShape(node, NodeShape{1, 1, makeSpanView(s_undefProps), false})) {
			return false;
		}
		item.kind = ItemKind::Undef;
		return readName(node.arguments[0], item.name, "a macro name", "undef")
				&& readCommon(node, item);
	} else if (name == "typedef") {
		if (!allowed(true, false, false, false)
				|| !checkShape(node, NodeShape{2, 2, makeSpanView(s_typedefProps), false})) {
			return false;
		}
		item.kind = ItemKind::Typedef;
		item.textLocation = node.arguments[1].location;
		return readName(node.arguments[0], item.name, "a typedef name", "typedef")
				&& readString(node.arguments[1], item.text, "a type") && readCommon(node, item);
	} else if (name == "decl") {
		if (!allowed(true, false, false, false)
				|| !checkShape(node, NodeShape{1, 1, makeSpanView(s_declProps), false})) {
			return false;
		}
		item.kind = ItemKind::Decl;
		item.textLocation = node.arguments[0].location;
		if (auto ext = node.getPropertyValue("extern-c")) {
			if (ext->isBool()) {
				item.externC = ext->boolean ? ExternC::Macro : ExternC::None;
			} else if (ext->getString() == "raw") {
				item.externC = ExternC::Raw;
			} else {
				error(ext->location, "extern-c is #true, #false or \"raw\"");
				return false;
			}
		}
		return readString(node.arguments[0], item.text, "a declaration") && readCommon(node, item);
	} else if (name == "error") {
		if (!allowed(true, true, false, false)
				|| !checkShape(node, NodeShape{1, 1, makeSpanView(s_errorProps), false})) {
			return false;
		}
		item.kind = ItemKind::Error;
		if (auto raw = node.getPropertyValue("raw")) {
			item.raw = raw->isBool() && raw->boolean;
		}
		return readString(node.arguments[0], item.text, "an error message")
				&& readCommon(node, item);
	} else if (name == "public") {
		if (!allowed(true, false, false, false)
				|| !checkShape(node, NodeShape{0, 0, makeSpanView(s_publicProps), false})) {
			return false;
		}
		item.kind = ItemKind::Public;
		return readCommon(node, item);
	} else if (name == "field") {
		if (!allowed(false, true, false, false)
				|| !checkShape(node, NodeShape{2, 2, makeSpanView(s_fieldProps), false})) {
			return false;
		}
		item.kind = ItemKind::Field;
		item.textLocation = node.arguments[1].location;
		return readString(node.arguments[0], item.name.name, "a field name")
				&& readString(node.arguments[1], item.text, "a field type")
				&& readOptional(node, "bits", item.bits) && readOptional(node, "attr", item.attr)
				&& readCommon(node, item);
	} else if (name == "value") {
		if (!allowed(false, false, true, false)
				|| !checkShape(node, NodeShape{1, 2, makeSpanView(s_valueProps), false})) {
			return false;
		}
		item.kind = ItemKind::Value;
		if (!readName(node.arguments[0], item.name, "an enumerator", "value")) {
			return false;
		}
		if (node.arguments.size() > 1) {
			auto &v = node.arguments[1];
			item.textLocation = v.location;
			if (!readBody(v, item.text, "an enumerator value")) {
				return false;
			}
		}
		return readCommon(node, item);
	} else if (name == "struct" || name == "union") {
		return loadRecord(node, item, ctx);
	} else if (name == "enum") {
		return loadEnum(node, item, ctx);
	} else if (name == "use") {
		return loadUse(node, item, ctx);
	} else if (name == "targets") {
		return loadTargets(node, item, ctx);
	} else if (name == "func") {
		return loadFunc(node, item, ctx);
	}
	error(node.location, mem_std::toString("unknown node '", node.name, "'"));
	return false;
}

bool Loader::loadRecord(const KdlNode &node, Item &item, Context ctx) {
	if (ctx != Context::Top && ctx != Context::Record) {
		error(node.location, "a record stands at the top of a description or in a record");
		return false;
	}
	item.kind = ItemKind::Record;
	item.isUnion = node.name == "union";
	if (ctx == Context::Record) {
		if (!checkShape(node, NodeShape{0, 1, makeSpanView(s_nestedRecordProps), true})) {
			return false;
		}
		if (!node.arguments.empty()
				&& !readString(node.arguments[0], item.member, "a member name")) {
			return false;
		}
		if (auto tag = node.getPropertyValue("tag")) {
			if (!readName(*tag, item.name, "a tag", "tag")) {
				return false;
			}
		}
	} else {
		if (!checkShape(node, NodeShape{0, 1, makeSpanView(s_recordProps), true})) {
			return false;
		}
		if (!node.arguments.empty() && !readName(node.arguments[0], item.name, "a tag", "tag")) {
			return false;
		}
		if (auto td = node.getPropertyValue("typedef")) {
			if (!readName(*td, item.typedefName, "typedef", "typedef")) {
				return false;
			}
		}
		if (auto packIf = node.getPropertyValue("pack-if")) {
			kdl::Location loc;
			if (!readCondition(*packIf, item.packCondition, loc)) {
				return false;
			}
		}
		if (!readOptional(node, "also", item.also) || !readOptional(node, "pack", item.pack)
				|| !readOptional(node, "base", item.base)) {
			return false;
		}
		if (item.name.name.empty() && item.typedefName.name.empty()) {
			error(node.location, "a top-level record needs a tag or a typedef");
			return false;
		}
		if (!item.also.empty() && item.typedefName.name.empty()) {
			error(node.location, "also= continues a typedef=");
			return false;
		}
	}
	if (!readOptional(node, "attr", item.attr) || !readOptional(node, "attr-after", item.attrAfter)
			|| !readCommon(node, item)) {
		return false;
	}
	bool ok = true;
	for (auto &child : node.children) {
		Item member;
		if (loadItem(child, member, Context::Record)) {
			item.children.emplace_back(sprt::move(member));
		} else {
			ok = false;
		}
	}
	return ok;
}

bool Loader::loadEnum(const KdlNode &node, Item &item, Context ctx) {
	if (ctx != Context::Top && ctx != Context::Record) {
		error(node.location, "an enum stands at the top of a description or in a record");
		return false;
	}
	if (!checkShape(node, NodeShape{0, 1, makeSpanView(s_enumProps), true})) {
		return false;
	}
	item.kind = ItemKind::Enum;
	if (!node.arguments.empty() && !readName(node.arguments[0], item.name, "a tag", "tag")) {
		return false;
	}
	if (auto td = node.getPropertyValue("typedef")) {
		if (!readName(*td, item.typedefName, "typedef", "typedef")) {
			return false;
		}
	}
	if (!readOptional(node, "also", item.also) || !readOptional(node, "attr", item.attr)
			|| !readOptional(node, "macro", item.target) || !readOptional(node, "base", item.base)
			|| !readCommon(node, item)) {
		return false;
	}
	// `typedef NS_ENUM(base, NAME) { … };`: the macro declares the type with its name.
	if (!item.target.empty()
			&& (item.name.name.empty() || item.base.empty() || !item.typedefName.name.empty()
					|| !item.also.empty() || !item.attr.empty())) {
		error(node.location, "an enum with macro= has a name and base=, and no typedef=, also= or attr=");
		return false;
	}
	bool ok = true;
	for (auto &child : node.children) {
		Item value;
		if (loadItem(child, value, Context::Enum)) {
			item.children.emplace_back(sprt::move(value));
		} else {
			ok = false;
		}
	}
	return ok;
}

// `targets KIND flag= suffix= error= { group NAME if= { target… }; target NAME ID if= also= }`
bool Loader::loadTargets(const KdlNode &node, Item &item, Context ctx) {
	if (ctx != Context::Top) {
		error(node.location, "a list of targets stands at the top of a description");
		return false;
	}
	if (!checkShape(node, NodeShape{1, 1, makeSpanView(s_targetsProps), true})) {
		return false;
	}
	item.kind = ItemKind::Targets;
	item.location = node.location;
	if (!readName(node.arguments[0], item.name, "a list name", "macro")
			|| !readOptional(node, "flag", item.flag) || !readOptional(node, "suffix", item.suffix)
			|| !readOptional(node, "error", item.text)) {
		return false;
	}
	if (item.flag.size() > 2 && item.flag.front() == '`' && item.flag.back() == '`') {
		item.flag = StringView(item.flag).sub(1, item.flag.size() - 2).str<memory::StandardInterface>();
	}
	bool ok = true;
	mem_std::Map<String, String> ids;
	auto add = [&](const KdlNode &child, Vector<Item> &into, bool inGroup) {
		Item t;
		if (child.name == "target") {
			if (!loadTarget(child, t)) {
				return false;
			}
			for (auto &it : ids) {
				if (it.second == t.text || it.first == t.name.name) {
					error(child.location,
							mem_std::toString("target ", t.name.name, " ", t.text, " repeats ",
									it.first, " ", it.second));
					return false;
				}
			}
			ids.emplace(t.name.name, t.text);
		} else if (child.name == "group" && !inGroup) {
			if (!checkShape(child, NodeShape{1, 1, makeSpanView(s_groupProps), true})
					|| !readString(child.arguments[0], t.name.name, "a group name")
					|| !readCommon(child, t)) {
				return false;
			}
			t.kind = ItemKind::TargetGroup;
			if (t.condition.empty()) {
				error(child.location, "a group is selected by its if=");
				return false;
			}
		} else {
			error(child.location, inGroup ? "a group holds targets"
										  : "a list of targets holds groups and targets");
			return false;
		}
		into.emplace_back(sprt::move(t));
		return true;
	};
	auto checkElse = [&](const Vector<Item> &list, kdl::Location loc) {
		for (size_t i = 0; i + 1 < list.size(); ++i) {
			if (list[i].condition.empty()) {
				error(list[i].location, "only the last target may have no if= (the #else)");
				return false;
			}
		}
		return true;
	};
	for (auto &child : node.children) {
		if (!add(child, item.children, false)) {
			ok = false;
			continue;
		}
		if (item.children.back().kind == ItemKind::TargetGroup) {
			auto &group = item.children.back();
			for (auto &g : child.children) {
				if (!add(g, group.children, true)) {
					ok = false;
				}
			}
			ok = checkElse(group.children, group.location) && ok;
		}
	}
	return checkElse(item.children, item.location) && ok;
}

bool Loader::loadTarget(const KdlNode &node, Item &item) {
	if (!checkShape(node, NodeShape{2, 2, makeSpanView(s_targetProps), true})) {
		return false;
	}
	item.kind = ItemKind::Target;
	if (!readString(node.arguments[0], item.name.name, "a target name")
			|| !readBody(node.arguments[1], item.text, "a target number")
			|| !readOptional(node, "also", item.also) || !readOptional(node, "dir", item.member)
			|| !readCommon(node, item)) {
		return false;
	}
	bool ok = true;
	for (auto &child : node.children) {
		Item extra;
		if (child.name != "macro") {
			error(child.location, "a target holds the macros it defines when it is selected");
			ok = false;
			continue;
		}
		if (loadItem(child, extra, Context::Top)) {
			item.children.emplace_back(sprt::move(extra));
		} else {
			ok = false;
		}
	}
	return ok;
}

bool Loader::loadUse(const KdlNode &node, Item &item, Context ctx) {
	if (ctx != Context::Top && ctx != Context::Record) {
		error(node.location, "a block stands at the top of a description or in a record");
		return false;
	}
	if (!checkShape(node, NodeShape{0, 1, makeSpanView(s_useProps), true})) {
		return false;
	}
	item.kind = ItemKind::Use;
	kdl::Location loc;
	if (!readSource(node, item.source, item.sourcePath, loc)) {
		return false;
	}
	if (auto inl = node.getPropertyValue("inline")) {
		item.inlined = inl->isBool() && inl->boolean;
	}
	if (!readCommon(node, item)) {
		return false;
	}
	bool ok = true;
	for (auto &child : node.children) {
		Item over;
		if (loadItem(child, over, Context::Overrides)) {
			item.children.emplace_back(sprt::move(over));
		} else {
			ok = false;
		}
	}
	return ok;
}

// The names a declarator list declares: the last identifier of each comma-separated declarator.
void declaratorNames(StringView list, Vector<String> &out) {
	size_t depth = 0;
	String last;
	auto flush = [&]() {
		if (!last.empty()) {
			out.emplace_back(sprt::move(last));
			last.clear();
		}
	};
	size_t i = 0;
	while (i < list.size()) {
		auto c = list[i];
		if (c == '(' || c == '[') {
			++depth;
		} else if (c == ')' || c == ']') {
			--depth;
		} else if (c == ',' && depth == 0) {
			flush();
		}
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
			size_t start = i;
			while (i < list.size()
					&& ((list[i] >= 'a' && list[i] <= 'z') || (list[i] >= 'A' && list[i] <= 'Z')
							|| list[i] == '_' || (list[i] >= '0' && list[i] <= '9'))) {
				++i;
			}
			if (depth == 0 || last.empty()) {
				last = list.sub(start, i - start).str<memory::StandardInterface>();
			}
			continue;
		}
		++i;
	}
	flush();
}

String upperName(StringView name) {
	String out;
	for (auto c : name) {
		if (c >= 'a' && c <= 'z') {
			out.push_back(char(c - 'a' + 'A'));
		} else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			out.push_back(c);
		} else {
			out.push_back('_');
		}
	}
	return out;
}

// A macro of a list of targets: `K_NAME_LINUX`, `K_ID`, with the list's rule.
Name targetMacro(const Item &list, StringView part, StringView target) {
	Name out;
	out.rule = list.name.rule;
	out.name = target.empty() ? mem_std::toString(list.name.name, "_", part)
							  : mem_std::toString(list.name.name, "_", part, "_", upperName(target));
	return out;
}

// The directory of a target: `dir=`, or its name with the list's suffix.
String targetDir(const Item &list, const Item &target) {
	return target.member.empty() ? mem_std::toString(target.name.name, list.suffix) : target.member;
}

// Every target of a list, groups flattened, in the order they are tested.
void listTargets(const Item &list, Vector<const Item *> &out) {
	for (auto &it : list.children) {
		if (it.kind == ItemKind::TargetGroup) {
			for (auto &t : it.children) { out.emplace_back(&t); }
		} else {
			out.emplace_back(&it);
		}
	}
}

// The names `also=` gives a target.
Vector<String> targetAliases(const Item &target) {
	Vector<String> out;
	StringView r(target.also);
	while (!r.empty()) {
		r.skipChars<StringView::Chars<' ', '\t', ','>>();
		auto name = r.readUntil<StringView::Chars<' ', '\t', ','>>();
		if (!name.empty()) {
			out.emplace_back(name.str<memory::StandardInterface>());
		}
	}
	return out;
}

bool Loader::collectSymbols(Header &header, const Item &item) {
	auto add = [&](const Name &name, kdl::Location loc) {
		if (name.name.empty()) {
			return;
		}
		for (auto &sym : header.symbols) {
			if (sym.name == name.name && sym.rule == name.rule) {
				return;
			}
		}
		header.symbols.emplace_back(Symbol{name.name, name.rule, &header, loc});
	};

	switch (item.kind) {
	case ItemKind::Macro:
	case ItemKind::Typedef:
	case ItemKind::Value: add(item.name, item.location); break;
	case ItemKind::Func: {
		add(item.name, item.location);
		for (auto &v : funcVariants(*_map, item)) {
			if (v.name != item.name.name) {
				add(Name{v.name, nullptr}, item.location);
			}
		}
		break;
	}
	case ItemKind::Targets: {
		Vector<const Item *> targets;
		listTargets(item, targets);
		for (auto t : targets) {
			add(targetMacro(item, "NAME", t->name.name), t->location);
			add(targetMacro(item, "ID", t->name.name), t->location);
			for (auto &a : targetAliases(*t)) {
				add(targetMacro(item, "NAME", a), t->location);
				add(targetMacro(item, "ID", a), t->location);
			}
			if (!item.flag.empty()) {
				add(Name{mem_std::toString(item.flag, upperName(t->name.name)), nullptr}, t->location);
			}
			for (auto &child : t->children) { collectSymbols(header, child); }
		}
		for (auto &g : item.children) {
			if (g.kind == ItemKind::TargetGroup && !item.flag.empty()) {
				add(Name{mem_std::toString(item.flag, upperName(g.name.name)), nullptr}, g.location);
			}
		}
		add(targetMacro(item, "NAME"), item.location);
		add(targetMacro(item, "ID"), item.location);
		break;
	}
	case ItemKind::Record:
	case ItemKind::Enum: {
		add(item.name, item.location);
		add(item.typedefName, item.location);
		Vector<String> names;
		declaratorNames(item.also, names);
		for (auto &it : names) { add(Name{it, nullptr}, item.location); }
		for (auto &child : item.children) { collectSymbols(header, child); }
		break;
	}
	default: break;
	}
	return true;
}

} // namespace

bool loadMap(StringView file, Map &map, Diagnostics &diag) {
	Loader loader(map, diag);
	return loader.load(file) && diag.empty();
}

} // namespace stappler::xlkdl
