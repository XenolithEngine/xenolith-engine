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
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xlkdl {

namespace {

// One definition of a name in one description, with every header that is generated from it.
struct Definition {
	StringView kind;
	String name;
	const NameRule *rule = nullptr;
	String file;
	kdl::Location location;

	// The conditions of the node and of the records it stands in, outermost first.
	Vector<String> conditions;
	Vector<const Header *> headers;

	// What else is written from it: a function's .def, table and wrapper (role, path).
	Vector<sprt::pair<String, String>> files;
};

StringView kindName(const Item &item) {
	switch (item.kind) {
	case ItemKind::Macro: return "macro";
	case ItemKind::Alias: return "alias";
	case ItemKind::Undef: return "undef";
	case ItemKind::Typedef: return "typedef";
	case ItemKind::Record: return item.isUnion ? "union" : "struct";
	case ItemKind::Enum: return "enum";
	case ItemKind::Value: return "value";
	case ItemKind::Field: return "field";
	case ItemKind::Use: return "use";
	case ItemKind::Decl: return "decl";
	case ItemKind::Error: return "error";
	case ItemKind::Public: return "public";
	case ItemKind::Targets: return "targets";
	case ItemKind::TargetGroup: return "group";
	case ItemKind::Target: return "target";
	case ItemKind::Func: return "func";
	case ItemKind::Param: return "param";
	}
	return "";
}

// The spellings a name has in C: the rendered one, and for a wrap rule its expansion.
Vector<String> spellings(const Definition &def) {
	Vector<String> out;
	if (def.rule) {
		out.emplace_back(def.rule->apply(def.name));
		if (def.rule->wrap && !def.rule->spelled.empty()) {
			out.emplace_back(mem_std::toString(def.rule->spelled, def.name));
		}
	}
	return out;
}

// The headers that put a fragment in place, each once: a dispatcher may include it from two branches.
Vector<const Header *> usersOf(const Header &header) {
	Vector<const Header *> out;
	for (auto u : header.users) {
		bool seen = false;
		for (auto it : out) { seen = seen || it == u; }
		if (!seen) {
			out.emplace_back(u);
		}
	}
	return out;
}

class Index {
public:
	explicit Index(const Map &map) : _map(&map) {
		for (auto &header : map.headers) {
			if (header.external) {
				for (auto &sym : header.symbols) {
					Definition def;
					def.kind = "external";
					def.name = sym.name;
					def.rule = sym.rule;
					def.file = header.mapFile;
					def.location = sym.location;
					add(sprt::move(def), &header);
				}
				continue;
			}
			Vector<String> conditions;
			walk(header, header.items, conditions);
		}
		for (auto &dll : map.dlls) {
			Definition def;
			def.kind = "dll";
			def.name = dll.name;
			def.file = dll.abiFile;
			def.location = dll.location;
			def.files = dllFiles(dll, true, !dll.table.empty(), !dll.table.empty());
			add(sprt::move(def), nullptr);
			for (auto &e : dll.exports) {
				Definition exp;
				exp.kind = "export";
				exp.name = e.name;
				exp.file = dll.abiFile;
				exp.location = e.location;
				exp.files = dllFiles(dll, e.def, e.table && !dll.table.empty(), false);
				add(sprt::move(exp), nullptr);
			}
		}
	}

	const Vector<Definition> &definitions() const { return _defs; }

	bool matches(const Definition &def, StringView query) const {
		if (StringView(def.name) == query) {
			return true;
		}
		for (auto &it : spellings(def)) {
			if (StringView(it) == query) {
				return true;
			}
		}
		return false;
	}

	// The platform and the arch a fragment under the platform root belongs to, if any.
	void scope(const Header &header, StringView &platform, StringView &arch) const {
		platform = arch = StringView();
		StringView path(header.path);
		StringView root(_map->platformRoot);
		if (root.empty() || !path.starts_with(root) || path.size() <= root.size()
				|| path[root.size()] != '/') {
			return;
		}
		path = path.sub(root.size() + 1);
		auto first = path.readUntil<StringView::Chars<'/'>>();
		if (path.empty()) {
			return;
		}
		platform = first;
		path.skipChars<StringView::Chars<'/'>>();
		auto second = path.readUntil<StringView::Chars<'/'>>();
		if (!path.empty()) {
			arch = second;
		}
	}

	mem_std::Value toValue(const Definition &def) const {
		mem_std::Value out;
		out.setString(def.name, "name");
		out.setString(def.kind, "kind");
		if (def.rule) {
			out.setString(def.rule->name, "rule");
		}
		auto names = spellings(def);
		if (!names.empty()) {
			auto &c = out.emplace("c");
			for (auto &it : names) { c.addString(it); }
		}
		out.setString(relative(def.file), "file");
		out.setInteger(def.location.line, "line");
		out.setInteger(def.location.column, "column");
		if (!def.files.empty()) {
			auto &files = out.emplace("files");
			for (auto &it : def.files) {
				mem_std::Value v;
				v.setString(it.first, "role");
				v.setString(it.second, "path");
				files.addValue(sprt::move(v));
			}
		}
		if (!def.conditions.empty()) {
			auto &c = out.emplace("if");
			for (auto &it : def.conditions) { c.addString(it); }
		}
		auto &headers = out.emplace("headers");
		for (auto h : def.headers) {
			mem_std::Value v;
			v.setString(h->path, "path");
			StringView platform, arch;
			scope(*h, platform, arch);
			if (!platform.empty()) {
				v.setString(platform, "platform");
			}
			if (!arch.empty()) {
				v.setString(arch, "arch");
			}
			if (h->fragment && !h->users.empty()) {
				auto &users = v.emplace("users");
				for (auto u : usersOf(*h)) { users.addString(u->path); }
			}
			headers.addValue(sprt::move(v));
		}
		return out;
	}

	void print(const Definition &def, const Callback<void(StringView)> &out) const {
		out << def.name << "  " << def.kind;
		for (auto &it : spellings(def)) { out << "  " << it; }
		out << "\n  " << relative(def.file) << ":" << def.location.line << ":"
			<< def.location.column << "\n";
		for (auto &it : def.conditions) { out << "  if " << it << "\n"; }
		for (auto h : def.headers) {
			out << "  " << h->path;
			StringView platform, arch;
			scope(*h, platform, arch);
			if (!platform.empty()) {
				out << "  [" << platform;
				if (!arch.empty()) {
					out << " " << arch;
				}
				out << "]";
			}
			if (h->fragment && !h->users.empty()) {
				out << "  used by";
				for (auto u : usersOf(*h)) { out << " " << u->path; }
			}
			out << "\n";
		}
		for (auto &it : def.files) { out << "  " << it.first << " " << it.second << "\n"; }
	}

private:
	// The files a DLL writes, as far as an entry is in them.
	Vector<sprt::pair<String, String>> dllFiles(const Dll &dll, bool def, bool table, bool stubs) const {
		Vector<sprt::pair<String, String>> out;
		if (def) {
			out.emplace_back(String("def"), dllPath(*_map, dll, DllPart::Def));
		}
		if (table) {
			out.emplace_back(String("table"), dllPath(*_map, dll, DllPart::Table));
		}
		if (stubs) {
			out.emplace_back(String("wrapper"), dllPath(*_map, dll, DllPart::Stubs));
		}
		return out;
	}

	String relative(StringView file) const {
		StringView dir(_map->dir);
		if (file.starts_with(dir) && file.size() > dir.size() && file[dir.size()] == '/') {
			return file.sub(dir.size() + 1).str<memory::StandardInterface>();
		}
		return file.str<memory::StandardInterface>();
	}

	void walk(const Header &header, const Vector<Item> &items, Vector<String> &conditions) {
		for (auto &item : items) {
			if (!item.condition.empty()) {
				conditions.emplace_back(item.condition);
			}
			auto define = [&](const Name &name, StringView kind) -> Definition * {
				if (name.name.empty()) {
					return nullptr;
				}
				Definition def;
				def.kind = kind;
				def.name = name.name;
				def.rule = name.rule;
				def.file = header.abiFile;
				def.location = item.location;
				def.conditions = conditions;
				return add(sprt::move(def), &header);
			};
			switch (item.kind) {
			case ItemKind::Macro:
			case ItemKind::Alias:
			case ItemKind::Undef:
			case ItemKind::Typedef:
			case ItemKind::Value:
			case ItemKind::Field: define(item.name, kindName(item)); break;
			case ItemKind::Record:
			case ItemKind::Enum: {
				define(item.name, kindName(item));
				if (!item.typedefName.name.empty()) {
					define(item.typedefName, "typedef");
				}
				Vector<String> also;
				declaratorNames(item.also, also);
				for (auto &it : also) { define(Name{it, nullptr}, "typedef"); }
				if (!item.member.empty()) {
					define(Name{item.member, nullptr}, "field");
				}
				walk(header, item.children, conditions);
				break;
			}
			case ItemKind::Use: walk(header, item.children, conditions); break;
			case ItemKind::Func: {
				// A function with its A/W variants, the umbrellas that write it, and its DLL's files.
				auto dll = item.dll.empty() ? nullptr : _map->findDll(item.dll);
				auto files = dll && !item.redeclare
						? dllFiles(*dll, item.def, item.inTable && !dll->table.empty(),
								  item.stub == StubMode::Gen)
								 : Vector<sprt::pair<String, String>>();
				auto variants = funcVariants(*_map, item);
				Vector<Name> names;
				names.emplace_back(item.name);
				for (auto &v : variants) {
					if (v.name != item.name.name) {
						names.emplace_back(Name{v.name, nullptr});
					}
				}
				for (auto &n : names) {
					auto def = define(n, "func");
					if (!def) {
						continue;
					}
					for (auto u : header.umbrellas) {
						bool seen = false;
						for (auto h : def->headers) { seen = seen || h == u; }
						if (!seen) {
							def->headers.emplace_back(u);
						}
					}
					// The exports are the variants; a function without variants is its own.
					if (n.name != item.name.name || variants.size() == 1) {
						def->files = files;
					}
				}
				break;
			}
			case ItemKind::Targets: {
				define(Name{item.name.name, nullptr}, "targets");
				// The macros the list writes: per target, and the selected one.
				Vector<const Item *> targets;
				listTargets(item, targets);
				for (auto t : targets) {
					define(targetMacro(item, "NAME", t->name.name), "macro");
					define(targetMacro(item, "ID", t->name.name), "macro");
					for (auto &a : targetAliases(*t)) {
						define(targetMacro(item, "NAME", a), "macro");
						define(targetMacro(item, "ID", a), "macro");
					}
					if (!item.flag.empty()) {
						define(Name{mem_std::toString(item.flag, upperName(t->name.name)), nullptr}, "macro");
					}
				}
				for (auto &g : item.children) {
					if (g.kind == ItemKind::TargetGroup && !item.flag.empty()) {
						define(Name{mem_std::toString(item.flag, upperName(g.name.name)), nullptr}, "macro");
					}
				}
				define(targetMacro(item, "NAME"), "macro");
				define(targetMacro(item, "ID"), "macro");
				walk(header, item.children, conditions);
				break;
			}
			case ItemKind::TargetGroup:
			case ItemKind::Target:
				define(Name{item.name.name, nullptr}, kindName(item));
				walk(header, item.children, conditions);
				break;
			default: break;
			}
			if (!item.condition.empty()) {
				conditions.pop_back();
			}
		}
	}

	// Headers that share a description share its definitions.
	Definition *add(Definition &&def, const Header *header) {
		auto key = mem_std::toString(def.file, ":", def.location.line, ":", def.location.column, ":",
				def.kind, ":", def.name);
		auto it = _keys.find(key);
		if (it != _keys.end()) {
			auto &headers = _defs[it->second].headers;
			if (header && (headers.empty() || headers.back() != header)) {
				headers.emplace_back(header);
			}
			return &_defs[it->second];
		}
		if (header) {
			def.headers.emplace_back(header);
		}
		_keys.emplace(sprt::move(key), _defs.size());
		_defs.emplace_back(sprt::move(def));
		return &_defs.back();
	}

	const Map *_map = nullptr;
	Vector<Definition> _defs;
	mem_std::Map<String, size_t> _keys;
};

} // namespace

int findNames(const Map &map, SpanView<String> names, const Callback<void(StringView)> &out) {
	Index index(map);
	int missing = 0;
	for (auto &query : names) {
		bool found = false;
		for (auto &def : index.definitions()) {
			if (index.matches(def, query)) {
				index.print(def, out);
				found = true;
			}
		}
		if (!found) {
			out << query << ": not defined in the map\n";
			++missing;
		}
	}
	return missing;
}

void writeIndex(const Map &map, const Callback<void(StringView)> &out) {
	Index index(map);
	mem_std::Value list;
	for (auto &def : index.definitions()) { list.addValue(index.toValue(def)); }
	out << data::toString<memory::StandardInterface>(list, true) << "\n";
}

} // namespace stappler::xlkdl
