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

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

namespace {

enum class TokenKind : uint8_t {
	Identifier,
	Number,
	Literal,
	Punct,
};

struct Token {
	StringView space;
	StringView text;
	TokenKind kind = TokenKind::Punct;

	// `NAME`: written as it is, past any symbol of that name.
	bool literal = false;
};

bool isIdentStart(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}
bool isIdentChar(char c) { return isIdentStart(c) || (c >= '0' && c <= '9'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

// C preprocessing tokens, just enough to tell identifiers from everything else.
bool tokenize(StringView in, Vector<Token> &out) {
	size_t i = 0;
	while (i < in.size()) {
		auto spaceStart = i;
		while (i < in.size() && (in[i] == ' ' || in[i] == '\t' || in[i] == '\n' || in[i] == '\r')) {
			++i;
		}
		Token tok;
		tok.space = in.sub(spaceStart, i - spaceStart);
		if (i >= in.size()) {
			if (!tok.space.empty()) {
				out.emplace_back(tok);
			}
			break;
		}
		auto start = i;
		auto c = in[i];
		if (c == '`') {
			auto end = i + 1;
			while (end < in.size() && isIdentChar(in[end])) { ++end; }
			if (end == i + 1 || end >= in.size() || in[end] != '`') {
				return false;
			}
			tok.kind = TokenKind::Identifier;
			tok.literal = true;
			tok.text = in.sub(i + 1, end - i - 1);
			out.emplace_back(tok);
			i = end + 1;
			continue;
		} else if (isIdentStart(c)) {
			while (i < in.size() && isIdentChar(in[i])) { ++i; }
			tok.kind = TokenKind::Identifier;
		} else if (isDigit(c) || (c == '.' && i + 1 < in.size() && isDigit(in[i + 1]))) {
			++i;
			while (i < in.size()) {
				auto n = in[i];
				if ((n == '+' || n == '-')
						&& (in[i - 1] == 'e' || in[i - 1] == 'E' || in[i - 1] == 'p'
								|| in[i - 1] == 'P')) {
					++i;
				} else if (n == '\'' && i + 1 < in.size() && isIdentChar(in[i + 1])) {
					++i;
				} else if (isIdentChar(n) || n == '.') {
					++i;
				} else {
					break;
				}
			}
			tok.kind = TokenKind::Number;
		} else if (c == '"' || c == '\'') {
			++i;
			while (i < in.size() && in[i] != c) {
				if (in[i] == '\\') {
					++i;
				}
				++i;
			}
			if (i >= in.size()) {
				return false;
			}
			++i;
			tok.kind = TokenKind::Literal;
		} else if (c == '-' && i + 1 < in.size() && in[i + 1] == '>') {
			i += 2;
		} else if (c == '#' && i + 1 < in.size() && in[i + 1] == '#') {
			i += 2;
		} else {
			++i;
		}
		tok.text = in.sub(start, i - start);
		out.emplace_back(tok);
	}
	return true;
}

bool isCallingConvention(StringView str) {
	return str == "__stdcall" || str == "__cdecl" || str == "__fastcall" || str == "__vectorcall"
			|| str == "__thiscall" || str == "__clrcall";
}

class Renderer {
public:
	Renderer(const Map &map, const Header &header, Diagnostics &diag)
	: _map(&map), _header(&header), _diag(&diag) {
		collect(&header, _seen);
		for (auto &h : map.headers) {
			for (auto &sym : h.symbols) { add(_global, sym); }
		}
	}

	const Header &header() const { return *_header; }

	// A name with a rule is written without it, as the umbrella of a function spells it; it has to
	// be visible without its rule too.
	void setPublic(bool value) { _public = value; }

	// Whether a header is among those this one sees.
	bool sees(const Header *h) const {
		for (auto it : _seen) {
			if (it == h) {
				return true;
			}
		}
		return false;
	}

	bool text(StringView in, kdl::Location loc, SpanView<String> params, String &out) {
		Vector<Token> tokens;
		if (!tokenize(in, tokens)) {
			error(loc, mem_std::toString("an unclosed literal or `name` in '", in, "'"));
			return false;
		}
		return render(tokens, 0, tokens.size(), loc, params, out);
	}

	// A declaration of `name` with the type-id `type`: the name goes where an abstract declarator
	// leaves room for one (`int (*)(void)` with FARPROC is `int (*FARPROC)(void)`).
	bool declaration(StringView type, StringView name, kdl::Location loc, String &out) {
		Vector<Token> tokens;
		if (!tokenize(type, tokens)) {
			error(loc, mem_std::toString("an unclosed literal or `name` in '", type, "'"));
			return false;
		}
		while (!tokens.empty() && tokens.back().text.empty()) { tokens.pop_back(); }
		if (!balanced(tokens)) {
			error(loc, mem_std::toString("unbalanced brackets in the type '", type, "'"));
			return false;
		}
		if (!tokens.empty()) {
			tokens.front().space = StringView();
		}

		size_t i = 0;
		while (i < tokens.size() && !isDeclaratorStart(tokens[i])) {
			if (auto group = attributeGroup(tokens, i)) {
				i += group;
				continue;
			}
			i += isWrapCall(tokens, i, tokens.size()) ? 4 : 1;
		}
		size_t pos = 0;
		if (!declaratorPos(tokens, i, pos)) {
			error(loc, mem_std::toString("no place for a name in the type '", type, "'"));
			return false;
		}

		if (!render(tokens, 0, pos, loc, SpanView<String>(), out)) {
			return false;
		}
		if (!out.empty() && (isIdentChar(out.back()) || out.back() == ')')) {
			out.push_back(' ');
		}
		out.append(name.data(), name.size());
		return render(tokens, pos, tokens.size(), loc, SpanView<String>(), out);
	}

	// A macro an alias can point at: one with a name rule, visible from this header.
	const Symbol *findMacro(StringView name) const {
		auto it = _visible.find(name);
		return (it != _visible.end() && it->second->rule) ? it->second : nullptr;
	}

	// An umbrella writes the items of its target's description, and their places are there.
	void error(kdl::Location loc, StringView message) {
		auto file = _header->umbrellaTarget ? StringView(_header->umbrellaTarget->abiFile)
											: StringView(_header->abiFile);
		_diag->error(file, loc, message);
	}

private:
	// A name may be a symbol with a rule and one without: the plain name means the former.
	static void add(mem_std::Map<StringView, const Symbol *> &map, const Symbol &sym) {
		auto it = map.find(StringView(sym.name));
		if (it == map.end()) {
			map.emplace(StringView(sym.name), &sym);
		} else if (!it->second->rule && sym.rule) {
			it->second = &sym;
		}
	}

	static bool seenBefore(Vector<const Header *> &seen, const Header *h) {
		for (auto &it : seen) {
			if (it == h) {
				return true;
			}
		}
		seen.emplace_back(h);
		return false;
	}

	// What a header sees: its own names, the names of what it includes and the blocks it uses, and
	// for a fragment, whatever its includers see at the place they put it.
	void collect(const Header *h, Vector<const Header *> &seen) {
		if (seenBefore(seen, h)) {
			return;
		}
		for (auto &sym : h->symbols) {
			add(_visible, sym);
			if (!sym.rule) {
				_plain.emplace(StringView(sym.name), &sym);
			}
		}
		for (auto inc : h->includeHeaders) { collect(inc, seen); }
		for (auto inc : h->blocks) { collect(inc, seen); }
		if (h->fragment) {
			for (auto user : h->users) { collect(user, seen); }
		}
	}

	static bool balanced(const Vector<Token> &tokens) {
		Vector<char> stack;
		for (auto &tok : tokens) {
			if (tok.kind != TokenKind::Punct || tok.text.size() != 1) {
				continue;
			}
			auto c = tok.text[0];
			if (c == '(' || c == '[') {
				stack.emplace_back(c == '(' ? ')' : ']');
			} else if (c == ')' || c == ']') {
				if (stack.empty() || stack.back() != c) {
					return false;
				}
				stack.pop_back();
			}
		}
		return stack.empty();
	}

	// `__attribute__((...))` and its kin are part of the type, not a declarator: the tokens of the
	// whole group, or 0.
	static size_t attributeGroup(const Vector<Token> &tokens, size_t i) {
		auto &tok = tokens[i];
		if (tok.kind != TokenKind::Identifier
				|| !(tok.text == "__attribute__" || tok.text == "__attribute" || tok.text == "__declspec"
						|| tok.text == "alignas" || tok.text == "_Alignas")) {
			return 0;
		}
		if (i + 1 >= tokens.size() || tokens[i + 1].text != "(") {
			return 0;
		}
		size_t depth = 0;
		for (size_t j = i + 1; j < tokens.size(); ++j) {
			if (tokens[j].text == "(") {
				++depth;
			} else if (tokens[j].text == ")" && --depth == 0) {
				return j - i + 1;
			}
		}
		return 0;
	}

	static bool isDeclaratorStart(const Token &tok) {
		return tok.kind == TokenKind::Punct
				&& (tok.text == "*" || tok.text == "&" || tok.text == "(" || tok.text == "[");
	}

	bool isNestedDeclarator(const Vector<Token> &tokens, size_t i) const {
		for (size_t j = i + 1; j < tokens.size(); ++j) {
			auto &tok = tokens[j];
			if (tok.text == "*" || tok.text == "&" || tok.text == "^") {
				return true;
			}
			if (tok.kind != TokenKind::Identifier) {
				return false;
			}
			auto it = _visible.find(tok.text);
			if (!isCallingConvention(tok.text) && (it == _visible.end() || !it->second->rule)) {
				return false;
			}
		}
		return false;
	}

	bool declaratorPos(const Vector<Token> &tokens, size_t i, size_t &pos) const {
		while (i < tokens.size()
				&& (tokens[i].text == "*" || tokens[i].text == "&" || tokens[i].text == "&&"
						|| tokens[i].kind == TokenKind::Identifier)) {
			i += isWrapCall(tokens, i, tokens.size()) ? 4 : 1;
		}
		if (i < tokens.size() && tokens[i].text == "(" && isNestedDeclarator(tokens, i)) {
			if (!declaratorPos(tokens, i + 1, pos)) {
				return false;
			}
			// The rest of the group closes the parenthesis the name went into.
			size_t depth = 1;
			for (size_t j = pos; j < tokens.size(); ++j) {
				if (tokens[j].text == "(") {
					++depth;
				} else if (tokens[j].text == ")" && --depth == 0) {
					return true;
				}
			}
			return false;
		}
		pos = i;
		return true;
	}

	bool render(const Vector<Token> &tokens, size_t from, size_t to, kdl::Location loc,
			SpanView<String> params, String &out) {
		bool ok = true;
		for (size_t i = from; i < to; ++i) {
			auto &tok = tokens[i];
			out.append(tok.space.data(), tok.space.size());
			if (tok.kind != TokenKind::Identifier || tok.literal || isParam(tok.text, params)
					|| (i > 0 && (tokens[i - 1].text == "." || tokens[i - 1].text == "->"))) {
				out.append(tok.text.data(), tok.text.size());
				continue;
			}
			if (_public && (isWrapCall(tokens, i, to) || ruledVisible(tok.text))) {
				ok = publicName(tokens, i, to, loc, out) && ok;
				continue;
			}
			// A name already written with a wrap rule (`__SPRT_ID(x)`) is the call it is.
			if (isWrapCall(tokens, i, to)) {
				for (size_t k = i; k < i + 4; ++k) {
					if (k > i) {
						out.append(tokens[k].space.data(), tokens[k].space.size());
					}
					out.append(tokens[k].text.data(), tokens[k].text.size());
				}
				i += 3;
				continue;
			}
			auto it = _visible.find(tok.text);
			if (it != _visible.end()) {
				auto &sym = *it->second;
				auto text = sym.rule ? sym.rule->apply(sym.name) : sym.name;
				out.append(text);
				continue;
			}
			auto g = _global.find(tok.text);
			if (g != _global.end()) {
				error(loc,
						mem_std::toString("'", tok.text, "' comes from ", g->second->header->path,
								", which ", _header->path, " does not include"));
				ok = false;
			}
			out.append(tok.text.data(), tok.text.size());
		}
		return ok;
	}

	bool ruledVisible(StringView name) const {
		auto it = _visible.find(name);
		return it != _visible.end() && it->second->rule;
	}

	// The public spelling of a name with a rule: a wrap call is its argument, a symbol its bare name,
	// and that bare name has to be visible.
	bool publicName(const Vector<Token> &tokens, size_t &i, size_t to, kdl::Location loc,
			String &out) {
		StringView name = tokens[i].text;
		if (isWrapCall(tokens, i, to)) {
			name = tokens[i + 2].text;
			i += 3;
		}
		out.append(name.data(), name.size());
		if (_plain.find(name) == _plain.end()) {
			error(loc,
					mem_std::toString("'", name, "' has no public name ", _header->path,
							" sees: an includer has to declare it"));
			return false;
		}
		return true;
	}

	bool isWrapCall(const Vector<Token> &tokens, size_t i, size_t to) const {
		if (i + 3 >= to || tokens[i + 1].text != "(" || tokens[i + 2].kind != TokenKind::Identifier
				|| tokens[i + 3].text != ")") {
			return false;
		}
		for (auto &rule : _map->rules) {
			if (rule.wrap && StringView(rule.text) == tokens[i].text) {
				return true;
			}
		}
		return false;
	}

	static bool isParam(StringView name, SpanView<String> params) {
		for (auto &it : params) {
			if (StringView(it) == name) {
				return true;
			}
		}
		return false;
	}

	const Map *_map = nullptr;
	const Header *_header = nullptr;
	Diagnostics *_diag = nullptr;
	bool _public = false;
	Vector<const Header *> _seen;
	mem_std::Map<StringView, const Symbol *> _visible;
	mem_std::Map<StringView, const Symbol *> _plain;
	mem_std::Map<StringView, const Symbol *> _global;
};

enum class Spacing : uint8_t {
	None,
	Macro,
	Typedef,
	Block,
};

String includeLine(const Map &map, SourceKind kind, StringView path) {
	auto fill = [&](StringView tpl) {
		String out;
		auto pos = tpl.find("{}");
		if (pos == sprt::Max<size_t>) {
			out = tpl.str<memory::StandardInterface>();
		} else {
			out = mem_std::toString(tpl.sub(0, pos), path, tpl.sub(pos + 2));
		}
		return out;
	};
	switch (kind) {
	case SourceKind::Path: return mem_std::toString("#include <", path, ">\n");
	case SourceKind::Platform:
		return mem_std::toString("#include ", fill(map.platformInclude), "\n");
	case SourceKind::Arch: return mem_std::toString("#include ", fill(map.archInclude), "\n");
	}
	return String();
}

class Emitter {
public:
	Emitter(const Map &map, const EmitOptions &opts, Renderer &renderer, String &out,
			Diagnostics &diag, SpanView<const Item *> overrides = SpanView<const Item *>())
	: _map(&map), _opts(&opts), _r(&renderer), _out(&out), _diag(&diag), _overrides(overrides) { }

	// Functions are written as their umbrellas.
	void setUmbrella(bool value) { _umbrella = value; }

	bool items(const Vector<Item> &list, uint32_t depth) {
		bool ok = true;
		String current;
		bool open = false;
		_list = &list;
		bool publicNode = false;
		bool publics = false;
		for (auto &item : list) {
			publicNode = publicNode || item.kind == ItemKind::Public;
			publics = publics || item.isPublic;
		}
		for (auto &item : list) {
			String cond;
			if (!item.condition.empty()
					&& !_r->text(item.condition, item.conditionLocation, SpanView<String>(),
							cond)) {
				ok = false;
				continue;
			}
			if (open && cond != current) {
				_out->append("#endif\n");
				open = false;
				if (depth == 0) {
					_last = Spacing::Block;
				}
			}
			if (!open && !cond.empty()) {
				separate(depth, Spacing::Block);
				_out->append(mem_std::toString("#if ", continued(cond), "\n"));
				if (depth == 0) {
					_last = Spacing::None;
				}
				open = true;
				current = cond;
			}
			ok = item_(overridden(item), depth) && ok;
		}
		if (open) {
			_out->append("#endif\n");
			if (depth == 0) {
				_last = Spacing::Block;
			}
		}
		// Without a public node the public names go last, unguarded.
		if (publics && !publicNode) {
			separate(depth, Spacing::Macro);
			ok = publicNames(list) && ok;
		}
		return ok;
	}

	bool includes(const Header &header) {
		bool ok = true;
		for (auto &inc : header.includes) {
			String cond;
			if (!inc.condition.empty()
					&& !_r->text(inc.condition, inc.conditionLocation, SpanView<String>(), cond)) {
				ok = false;
				continue;
			}
			if (!cond.empty()) {
				_out->append(mem_std::toString("#if ", cond, "\n"));
			}
			_out->append(includeLine(*_map, inc.kind, inc.path));
			if (!cond.empty()) {
				_out->append("#endif\n");
			}
		}
		return ok;
	}

private:
	// An override of a block written inline replaces the macro of the same name.
	const Item &overridden(const Item &item) const {
		if (item.kind == ItemKind::Macro) {
			for (auto over : _overrides) {
				if (over->name.name == item.name.name && over->name.rule == item.name.rule) {
					return *over;
				}
			}
		}
		return item;
	}

	void indent(uint32_t depth) {
		for (uint32_t i = 0; i < depth; ++i) { _out->push_back('\t'); }
	}

	// Top-level items are separated by a blank line when their kind changes; records and guarded
	// blocks always stand apart.
	void separate(uint32_t depth, Spacing kind) {
		if (depth > 0) {
			return;
		}
		if (_last != Spacing::None && (_last != kind || kind == Spacing::Block)) {
			_out->push_back('\n');
		}
		_last = kind;
	}

	String params(const Item &item) {
		String out("(");
		for (size_t i = 0; i < item.params.size(); ++i) {
			if (i > 0) {
				out.append(", ");
			}
			out.append(item.params[i]);
		}
		out.push_back(')');
		return out;
	}

	// A body that spans lines keeps its lines, joined with continuations.
	static String continued(StringView body) {
		String out;
		for (auto c : body) {
			if (c == '\n') {
				out.append(" \\\n");
			} else {
				out.push_back(c);
			}
		}
		return out;
	}

	// `#define NAME __SPRT_NAME` for every public macro of the list, in its order.
	bool publicNames(const Vector<Item> &list) {
		for (auto &it : list) {
			if (!it.isPublic) {
				continue;
			}
			auto args = it.hasParams ? params(it) : String();
			_out->append(mem_std::toString("#define ", it.name.name, args, " ", it.name.render(),
					args, "\n"));
		}
		return true;
	}

	bool macro(const Item &item) {
		String body;
		if (!_r->text(item.text, item.textLocation, makeSpanView(item.params), body)) {
			return false;
		}
		_out->append(mem_std::toString("#define ", item.name.render(),
				item.hasParams ? params(item) : String(), body.empty() ? "" : " ", continued(body),
				"\n"));
		return true;
	}

	bool item_(const Item &item, uint32_t depth) {
		switch (item.kind) {
		case ItemKind::Macro: separate(depth, Spacing::Macro); return macro(item);
		case ItemKind::Undef:
			separate(depth, Spacing::Macro);
			_out->append(mem_std::toString("#undef ", item.name.render(), "\n"));
			return true;
		case ItemKind::Alias: {
			separate(depth, Spacing::Macro);
			auto target = _r->findMacro(item.target);
			if (!target) {
				_r->error(item.location,
						mem_std::toString("an alias needs a visible macro with a name rule, and '",
								item.target, "' is not one"));
				return false;
			}
			auto args = item.hasParams ? params(item) : String();
			_out->append(mem_std::toString("#define ", item.name.name, args, " ",
					target->rule->apply(target->name), args, "\n"));
			return true;
		}
		case ItemKind::Typedef: {
			separate(depth, Spacing::Typedef);
			String decl;
			if (!_r->declaration(item.text, item.name.render(), item.textLocation, decl)) {
				return false;
			}
			_out->append(mem_std::toString("typedef ", decl, ";\n"));
			return true;
		}
		case ItemKind::Field: {
			String name = item.name.name;
			if (!item.attr.empty()) {
				String attr;
				if (!_r->text(item.attr, item.location, SpanView<String>(), attr)) {
					return false;
				}
				name = mem_std::toString(attr, " ", name);
			}
			String decl;
			if (!_r->declaration(item.text, name, item.textLocation, decl)) {
				return false;
			}
			indent(depth);
			_out->append(decl);
			if (!item.bits.empty()) {
				_out->append(mem_std::toString(" : ", item.bits));
			}
			_out->append(";\n");
			return true;
		}
		case ItemKind::Value: {
			indent(depth);
			_out->append(item.name.render());
			if (!item.text.empty()) {
				String value;
				if (!_r->text(item.text, item.textLocation, SpanView<String>(), value)) {
					return false;
				}
				_out->append(mem_std::toString(" = ", continued(value)));
			}
			_out->append(",\n");
			return true;
		}
		case ItemKind::Decl: return decl(item, depth);
		case ItemKind::Error: {
			separate(depth, Spacing::Macro);
			// A message already in quotes is written as it is, like a raw one.
			StringView text(item.text);
			auto quoted = item.raw || (text.size() >= 2 && text.front() == '"' && text.back() == '"');
			_out->append(quoted ? mem_std::toString("#error ", text, "\n")
								: mem_std::toString("#error \"", text, "\"\n"));
			return true;
		}
		case ItemKind::Public:
			separate(depth, Spacing::Macro);
			return _list ? publicNames(*_list) : true;
		case ItemKind::Targets: return targets(item);
		case ItemKind::TargetGroup:
		case ItemKind::Target: return false;
		case ItemKind::Record:
		case ItemKind::Enum: return record(item, depth);
		case ItemKind::Use: return use(item, depth);
		case ItemKind::Func: return _umbrella ? umbrellaFunc(item, depth) : func(item, depth);
		case ItemKind::Param: return false;
		}
		return false;
	}

	// XlkdlApiEmit.cc: a declaration, and an umbrella.
	bool func(const Item &item, uint32_t depth);
	bool umbrellaFunc(const Item &item, uint32_t depth);

	void define(const Name &name, StringView value) {
		_out->append(mem_std::toString("#define ", name.render(), " ", value, "\n"));
	}

	// The names and numbers of every target, then the chain that selects one.
	bool targets(const Item &list) {
		separate(0, Spacing::Block);
		Vector<const Item *> all;
		listTargets(list, all);
		for (auto t : all) {
			define(targetMacro(list, "NAME", t->name.name), targetDir(list, *t));
			define(targetMacro(list, "ID", t->name.name), t->text);
			for (auto &a : targetAliases(*t)) {
				define(targetMacro(list, "NAME", a), targetMacro(list, "NAME", t->name.name).render());
				define(targetMacro(list, "ID", a), targetMacro(list, "ID", t->name.name).render());
			}
		}
		_out->push_back('\n');
		auto ok = chain(list, list.children, true);
		_last = Spacing::Block;
		return ok;
	}

	bool chain(const Item &list, const Vector<Item> &entries, bool outer) {
		bool ok = true;
		bool elseBranch = false;
		for (size_t i = 0; i < entries.size(); ++i) {
			auto &e = entries[i];
			if (e.condition.empty()) {
				_out->append("#else\n");
				elseBranch = true;
			} else {
				String cond;
				if (!_r->text(e.condition, e.conditionLocation, SpanView<String>(), cond)) {
					ok = false;
					continue;
				}
				_out->append(mem_std::toString(i == 0 ? "#if " : "#elif ", continued(cond), "\n"));
			}
			if (e.kind == ItemKind::TargetGroup) {
				ok = chain(list, e.children, false) && ok;
				if (!list.flag.empty()) {
					define(Name{mem_std::toString(list.flag, upperName(e.name.name)), nullptr},
							targetMacro(list, "ID").render());
				}
				continue;
			}
			define(targetMacro(list, "NAME"), targetMacro(list, "NAME", e.name.name).render());
			define(targetMacro(list, "ID"), targetMacro(list, "ID", e.name.name).render());
			if (!list.flag.empty()) {
				define(Name{mem_std::toString(list.flag, upperName(e.name.name)), nullptr},
						targetMacro(list, "ID", e.name.name).render());
			}
			for (auto &extra : e.children) { ok = macro(extra) && ok; }
		}
		if (outer && !elseBranch && !list.text.empty()) {
			_out->append(mem_std::toString("#else\n#error \"", list.text, "\"\n"));
		}
		_out->append("#endif\n");
		return ok;
	}

	bool decl(const Item &item, uint32_t depth) {
		separate(depth, Spacing::Typedef);
		String text;
		if (!_r->text(item.text, item.textLocation, SpanView<String>(), text)) {
			return false;
		}
		if (text.empty() || text.back() != ';') {
			text.push_back(';');
		}
		switch (item.externC) {
		case ExternC::None: _out->append(mem_std::toString(text, "\n")); break;
		case ExternC::Macro:
			_out->append(mem_std::toString(_map->externCBegin, "\n", text, "\n", _map->externCEnd,
					"\n"));
			break;
		case ExternC::Raw:
			_out->append(mem_std::toString("#ifdef __cplusplus\nextern \"C\" {\n#endif\n", text,
					"\n#ifdef __cplusplus\n}\n#endif\n"));
			break;
		}
		return true;
	}

	bool use(const Item &item, uint32_t depth) {
		separate(depth, Spacing::Block);
		auto inlined = item.source == SourceKind::Path && (item.inlined || _opts->inlineBlocks);
		const Header *target =
				item.source == SourceKind::Path ? _map->findHeader(item.sourcePath) : nullptr;
		if (inlined && target && target->fragment && target->guard.empty()) {
			Vector<const Item *> overrides;
			for (auto &over : item.children) {
				if (!definesMacro(*target, over)) {
					_r->error(over.location,
							mem_std::toString("the block ", target->path, " defines no macro '",
									over.name.name, "' to override"));
					return false;
				}
				overrides.emplace_back(&over);
			}
			Renderer renderer(*_map, *target, *_diag);
			Emitter nested(*_map, *_opts, renderer, *_out, *_diag, overrides);
			nested._last = Spacing::None;
			auto ok = nested.includes(*target) && nested.items(target->items, depth);
			if (depth == 0) {
				_last = Spacing::Block;
			}
			return ok;
		}

		_out->append(includeLine(*_map, item.source, item.sourcePath));
		bool ok = true;
		for (auto &over : item.children) {
			if (target && !definesMacro(*target, over)) {
				_r->error(over.location,
						mem_std::toString("the block ", target->path, " defines no macro '",
								over.name.name, "' to override"));
				ok = false;
				continue;
			}
			_out->append(mem_std::toString("#undef ", over.name.render(), "\n"));
			ok = macro(over) && ok;
		}
		if (depth == 0) {
			_last = Spacing::Block;
		}
		return ok;
	}

	static bool definesMacro(const Header &header, const Item &over) {
		for (auto &it : header.items) {
			if (it.kind == ItemKind::Macro && it.name.name == over.name.name
					&& it.name.rule == over.name.rule) {
				return true;
			}
		}
		return false;
	}

	bool record(const Item &item, uint32_t depth) {
		separate(depth, Spacing::Block);
		bool ok = true;
		String packCond;
		if (!item.packCondition.empty()
				&& !_r->text(item.packCondition, item.location, SpanView<String>(), packCond)) {
			return false;
		}
		if (!item.pack.empty()) {
			if (!packCond.empty()) {
				_out->append(mem_std::toString("#if ", packCond, "\n"));
			}
			_out->append(mem_std::toString("#pragma pack(push, ", item.pack, ")\n"));
			if (!packCond.empty()) {
				_out->append("#endif\n");
			}
		}
		if (!item.ignoreWarning.empty()) {
			_out->append(mem_std::toString(
					"#pragma clang diagnostic push\n#pragma clang diagnostic ignored \"",
					item.ignoreWarning, "\"\n"));
		}
		indent(depth);
		if (item.kind == ItemKind::Enum && !item.target.empty()) {
			String base;
			ok = _r->text(item.base, item.location, SpanView<String>(), base) && ok;
			_out->append(mem_std::toString("typedef ", item.target, "(", base, ", ",
					item.name.render(), ") {\n"));
			ok = items(item.children, depth + 1) && ok;
			indent(depth);
			_out->append("};\n");
			if (depth == 0) {
				_last = Spacing::Block;
			}
			return ok;
		}
		auto typedefd = !item.typedefName.name.empty();
		if (typedefd) {
			_out->append("typedef ");
		}
		_out->append(item.kind == ItemKind::Enum ? "enum" : (item.isUnion ? "union" : "struct"));
		if (!item.attr.empty()) {
			String attr;
			ok = _r->text(item.attr, item.location, SpanView<String>(), attr) && ok;
			_out->append(mem_std::toString(" ", attr));
		}
		if (!item.name.name.empty()) {
			_out->append(mem_std::toString(" ", item.name.render()));
		}
		if (!item.base.empty()) {
			String base;
			ok = _r->text(item.base, item.location, SpanView<String>(), base) && ok;
			_out->append(mem_std::toString(" : ", base));
		}
		if (!item.children.empty()) {
			_out->append(" {\n");
			ok = items(item.children, depth + 1) && ok;
			indent(depth);
			_out->push_back('}');
		}
		if (!item.attrAfter.empty()) {
			String attr;
			ok = _r->text(item.attrAfter, item.location, SpanView<String>(), attr) && ok;
			_out->append(mem_std::toString(" ", attr));
		}
		if (typedefd) {
			_out->append(mem_std::toString(" ", item.typedefName.render()));
			if (!item.also.empty()) {
				String also;
				ok = _r->text(item.also, item.location, SpanView<String>(), also) && ok;
				_out->append(mem_std::toString(", ", also));
			}
		} else if (!item.member.empty()) {
			_out->append(mem_std::toString(" ", item.member));
		}
		_out->append(";\n");
		if (!item.ignoreWarning.empty()) {
			_out->append("#pragma clang diagnostic pop\n");
		}
		if (!item.pack.empty()) {
			if (!packCond.empty()) {
				_out->append(mem_std::toString("#if ", packCond, "\n"));
			}
			_out->append("#pragma pack(pop)\n");
			if (!packCond.empty()) {
				_out->append("#endif\n");
			}
		}
		if (depth == 0) {
			_last = Spacing::Block;
		}
		return ok;
	}

	const Map *_map = nullptr;
	const EmitOptions *_opts = nullptr;
	Renderer *_r = nullptr;
	String *_out = nullptr;
	Diagnostics *_diag = nullptr;
	SpanView<const Item *> _overrides;
	Spacing _last = Spacing::None;
	bool _umbrella = false;

	// The list being written: a public node writes the public macros of it.
	const Vector<Item> *_list = nullptr;
};

} // namespace

bool emitHeader(const Map &map, const Header &header, const EmitOptions &opts, String &out,
		Diagnostics &diag) {
	Renderer renderer(map, header, diag);

	// An umbrella writes the functions of its target, those that have one, with its own includes.
	auto items = &header.items;
	Vector<Item> umbrellaItems;
	if (auto target = header.umbrellaTarget) {
		if (!renderer.sees(target)) {
			diag.error(header.mapFile, header.location,
					mem_std::toString("the umbrella ", header.path, " does not include ", target->path));
			return false;
		}
		for (auto &it : target->items) {
			if (it.kind == ItemKind::Func && it.umbrella) {
				umbrellaItems.emplace_back(it);
			}
		}
		items = &umbrellaItems;
	}
	bool funcs = false;
	for (auto &it : *items) { funcs = funcs || it.kind == ItemKind::Func; }
	StringView abi = header.umbrellaTarget ? StringView(header.umbrellaTarget->abi) : StringView(header.abi);

	out.clear();
	if (!map.license.empty()) {
		out.append(map.license);
		if (out.back() != '\n') {
			out.push_back('\n');
		}
		out.push_back('\n');
	}
	if (abi.empty()) {
		out.append(mem_std::toString("// Generated by xlkdl (header map ",
				filepath::lastComponent(map.file), "). Do not edit: regenerate it.\n\n"));
	} else {
		out.append(mem_std::toString("// Generated by xlkdl from ", abi, " (header map ",
				filepath::lastComponent(map.file), "). Do not edit: regenerate it.\n\n"));
	}
	auto guarded = !header.guard.empty();
	if (guarded) {
		out.append(mem_std::toString("#ifndef ", header.guard, "\n#define ", header.guard, "\n\n"));
	}

	String body;
	Emitter emitter(map, opts, renderer, body, diag);
	emitter.setUmbrella(header.umbrellaTarget != nullptr);
	auto ok = emitter.includes(header);
	if (!body.empty() && !items->empty()) {
		body.push_back('\n');
	}
	// Functions are declared with C linkage.
	if (funcs) {
		body.append(mem_std::toString(map.externCBegin, "\n\n"));
	}
	ok = emitter.items(*items, 0) && ok;
	while (!body.empty() && body.back() == '\n') { body.pop_back(); }
	if (funcs) {
		body.append(mem_std::toString("\n\n", map.externCEnd));
	}
	out.append(body);
	if (guarded) {
		out.append(mem_std::toString("\n\n#endif // ", header.guard, "\n"));
	} else {
		out.push_back('\n');
	}
	return ok && diag.empty();
}

} // namespace stappler::xlkdl
