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

namespace {

// The words of a list written `A W` or `A, W`.
Vector<String> listWords(StringView list) {
	Vector<String> out;
	while (!list.empty()) {
		list.skipChars<StringView::Chars<' ', '\t', ','>>();
		auto word = list.readUntil<StringView::Chars<' ', '\t', ','>>();
		if (!word.empty()) {
			out.emplace_back(word.str<memory::StandardInterface>());
		}
	}
	return out;
}

} // namespace

Vector<FuncVariant> funcVariants(const Map &map, const Item &func) {
	Vector<FuncVariant> out;
	for (auto &it : listWords(func.charset)) {
		out.emplace_back(FuncVariant{mem_std::toString(func.name.name, it), map.findCharset(it)});
	}
	if (out.empty()) {
		out.emplace_back(FuncVariant{func.name.name, nullptr});
	}
	return out;
}

namespace {

const StringView s_linkageProps[] = {"attr"};
const StringView s_umbrellaProps[] = {"begin", "end", "end-except", "body-if"};
const StringView s_dllRootProps[] = {"def", "code", "table-include", "loader-include"};
const StringView s_funcProps[] = {"if", "cc", "link", "dll", "def", "stub", "charset", "unicode",
	"umbrella", "noexcept", "empty", "attr", "redeclare", "table"};
const StringView s_funcDefaultProps[] = {"cc", "link", "dll", "def", "stub", "charset", "unicode",
	"umbrella", "noexcept"};
const StringView s_dllProps[] = {"file", "table", "preload-list"};
const StringView s_exportProps[] = {"def", "table"};

// The ABI map is loaded as a part of this one, from its own directory, and its headers are marked:
// their names are visible to the descriptions here, they are not written.
bool Loader::loadAbiMap(const KdlNode &node) {
	if (!checkShape(node, NodeShape{1, 1, SpanView<StringView>(), false})) {
		return false;
	}
	if (!_map->abiMap.empty()) {
		error(node.location, "an API map loads one ABI map");
		return false;
	}
	String path;
	if (!readString(node.arguments[0], path, "an ABI map")) {
		return false;
	}
	auto full = filepath::merge<memory::StandardInterface>(_map->dir, path);
	if (!filesystem::exists(FileInfo{full})) {
		error(node.arguments[0].location, mem_std::toString("no ABI map ", full));
		return false;
	}
	_map->abiMap = full;
	auto dir = _map->dir;
	auto file = _file;
	auto first = _map->headers.size();
	_map->dir = filepath::root(StringView(full)).str<memory::StandardInterface>();
	auto ok = loadPart(full, true);
	_map->dir = sprt::move(dir);
	_file = sprt::move(file);
	for (size_t i = first; i < _map->headers.size(); ++i) { _map->headers[i].imported = true; }
	return ok;
}

bool Loader::loadApiProfile(const KdlNode &child, bool &handled) {
	auto name = StringView(child.name);
	handled = true;
	if (name == "abi") {
		return true;
	} else if (name == "linkage") {
		if (!checkShape(child, NodeShape{1, 1, makeSpanView(s_linkageProps), false})) {
			return false;
		}
		Linkage linkage;
		if (!readString(child.arguments[0], linkage.name, "a linkage name")
				|| !readOptional(child, "attr", linkage.attr)) {
			return false;
		}
		if (linkage.name == "none" || _map->findLinkage(linkage.name)) {
			error(child.location, mem_std::toString("a second linkage '", linkage.name, "'"));
			return false;
		}
		_map->linkages.emplace_back(sprt::move(linkage));
		return true;
	} else if (name == "umbrella") {
		if (!checkShape(child, NodeShape{0, 0, makeSpanView(s_umbrellaProps), false})) {
			return false;
		}
		auto &u = _map->umbrella;
		if (!readOptional(child, "begin", u.begin) || !readOptional(child, "end", u.end)
				|| !readOptional(child, "end-except", u.endExcept)
				|| !readOptional(child, "body-if", u.bodyIf)) {
			return false;
		}
		if (u.endExcept.empty()) {
			u.endExcept = u.end;
		}
		return true;
	} else if (name == "charset") {
		if (!checkShape(child, NodeShape{1, 1, SpanView<StringView>(), true})) {
			return false;
		}
		Charset set;
		if (!readString(child.arguments[0], set.name, "a charset name")) {
			return false;
		}
		if (_map->findCharset(set.name)) {
			error(child.location, mem_std::toString("a second charset '", set.name, "'"));
			return false;
		}
		bool ok = true;
		for (auto &type : child.children) {
			String to;
			if (!checkShape(type, NodeShape{1, 1, SpanView<StringView>(), false})
					|| !readString(type.arguments[0], to, "a type")) {
				ok = false;
				continue;
			}
			set.types.emplace_back(String(type.name), sprt::move(to));
		}
		_map->charsets.emplace_back(sprt::move(set));
		return ok;
	} else if (name == "include-root") {
		if (!checkShape(child, NodeShape{1, 1, SpanView<StringView>(), false})) {
			return false;
		}
		return readString(child.arguments[0], _map->includeRoot, "an include root");
	} else if (name == "dll-root") {
		if (!checkShape(child, NodeShape{0, 0, makeSpanView(s_dllRootProps), false})) {
			return false;
		}
		auto &r = _map->dllRoot;
		return readOptional(child, "def", r.def)
				&& readOptional(child, "code", r.code)
				&& readOptional(child, "table-include", r.tableInclude)
				&& readOptional(child, "loader-include", r.loaderInclude);
	}
	handled = false;
	return true;
}

// The properties a function and the defaults of a description share. `stub` is #false, "gen" or
// "manual"; whether it was given decides its default.
bool Loader::readFuncProps(const KdlNode &node, Item &item, bool defaults, bool &stubGiven) {
	if (!readOptional(node, "cc", item.cc) || !readOptional(node, "link", item.link)
			|| !readOptional(node, "dll", item.dll) || !readOptional(node, "charset", item.charset)
			|| !readOptional(node, "unicode", item.unicode)
			|| (!defaults
					&& (!readOptional(node, "empty", item.empty)
							|| !readOptional(node, "attr", item.attr)))) {
		return false;
	}
	auto flag = [&](StringView prop, bool &out) {
		auto v = node.getPropertyValue(prop);
		if (!v) {
			return true;
		}
		if (!v->isBool()) {
			error(v->location, mem_std::toString(prop, "= is #true or #false"));
			return false;
		}
		out = v->boolean;
		return true;
	};
	if (!flag("def", item.def) || !flag("umbrella", item.umbrella)
			|| !flag("noexcept", item.nothrow)
			|| (!defaults && (!flag("redeclare", item.redeclare) || !flag("table", item.inTable)))) {
		return false;
	}
	if (auto stub = node.getPropertyValue("stub")) {
		if (stub->isBool() && !stub->boolean) {
			item.stub = StubMode::None;
		} else if (stub->isString() && stub->getString() == "gen") {
			item.stub = StubMode::Gen;
		} else if (stub->isString() && stub->getString() == "manual") {
			item.stub = StubMode::Manual;
		} else {
			error(stub->location, "stub= is \"gen\", \"manual\" or #false");
			return false;
		}
		stubGiven = true;
	}
	return true;
}

// `defaults dll= link= cc= …` at the top of a description: what its functions take unless they say.
bool Loader::loadFuncDefaults(const KdlNode &node) {
	if (!checkShape(node, NodeShape{0, 0, makeSpanView(s_funcDefaultProps), false})) {
		return false;
	}
	return readFuncProps(node, _funcDefaults, true, _funcStubGiven);
}

// `func NAME RETURN { PARAM TYPE; - TYPE; "..." }`
bool Loader::loadFunc(const KdlNode &node, Item &item, Context ctx) {
	if (ctx != Context::Top) {
		error(node.location, "a function stands at the top of a description");
		return false;
	}
	if (!checkShape(node, NodeShape{2, 2, makeSpanView(s_funcProps), true})) {
		return false;
	}
	item.kind = ItemKind::Func;
	item.cc = _funcDefaults.cc;
	item.link = _funcDefaults.link;
	item.dll = _funcDefaults.dll;
	item.charset = _funcDefaults.charset;
	item.unicode = _funcDefaults.unicode;
	item.def = _funcDefaults.def;
	item.umbrella = _funcDefaults.umbrella;
	item.nothrow = _funcDefaults.nothrow;
	item.stub = _funcDefaults.stub;
	item.textLocation = node.arguments[1].location;
	auto stubGiven = _funcStubGiven;
	if (!readName(node.arguments[0], item.name, "a function name", "func")
			|| !readString(node.arguments[1], item.text, "a return type")
			|| !readFuncProps(node, item, false, stubGiven) || !readCommon(node, item)) {
		return false;
	}

	bool ok = true;
	for (auto &child : node.children) {
		Item param;
		param.kind = ItemKind::Param;
		param.location = child.location;
		if (child.name == "...") {
			if (!checkShape(child, NodeShape{0, 0, SpanView<StringView>(), false})) {
				ok = false;
				continue;
			}
			param.variadic = true;
		} else {
			if (!checkShape(child, NodeShape{1, 1, SpanView<StringView>(), false})
					|| !readString(child.arguments[0], param.text, "a parameter type")) {
				ok = false;
				continue;
			}
			param.textLocation = child.arguments[0].location;
			if (child.name != "-") {
				param.name.name = child.name;
			}
		}
		if (!item.children.empty() && item.children.back().variadic) {
			error(child.location, "the ... is the last parameter");
			ok = false;
		}
		item.children.emplace_back(sprt::move(param));
	}
	auto variadic = !item.children.empty() && item.children.back().variadic;

	if (!item.link.empty() && item.link != "none" && !_map->findLinkage(item.link)) {
		error(node.location, mem_std::toString("no linkage '", item.link, "' in the profile"));
		ok = false;
	}
	auto sets = listWords(item.charset);
	for (auto &it : sets) {
		if (!_map->findCharset(it)) {
			error(node.location, mem_std::toString("no charset '", it, "' in the profile"));
			ok = false;
		}
	}
	auto unicode = listWords(item.unicode);
	if (unicode.size() > 2) {
		error(node.location, "unicode= names the variant under UNICODE, and the one without it");
		ok = false;
	}
	for (auto &it : unicode) {
		bool known = false;
		for (auto &s : sets) { known = known || s == it; }
		if (!known) {
			error(node.location, mem_std::toString("unicode= names ", it, ", which charset= does not"));
			ok = false;
		}
	}
	// A redeclaration only declares; whether a DLL's function has a wrapper its table decides, in
	// checkDlls, unless stub= says.
	if (item.redeclare) {
		if (!item.def || !item.inTable || (stubGiven && item.stub != StubMode::None)) {
			error(node.location, "a redeclaration takes no def=, table= or stub=");
			ok = false;
		}
		item.stub = StubMode::None;
		stubGiven = true;
	}
	item.stubGiven = stubGiven;
	if (!stubGiven) {
		item.stub = item.dll.empty() ? StubMode::None : StubMode::Gen;
	} else if (item.stub != StubMode::None && item.dll.empty()) {
		error(node.location, "stub= belongs to a function with dll=");
		ok = false;
	}
	if ((!item.def || !item.inTable) && item.dll.empty()) {
		error(node.location, "def= and table= belong to a function with dll=");
		ok = false;
	}
	if (variadic && item.stub == StubMode::Gen) {
		error(node.location,
				"a variadic function has no generated wrapper: stub=\"manual\" or stub=#false");
		ok = false;
	}
	if (item.stub == StubMode::Gen && item.name.rule) {
		error(node.location, "a generated wrapper is for a function without a name rule");
		ok = false;
	}
	return ok;
}

// `dll NAME file="x.dll" table=CLASS { preload NAMES…; export NAME [def=#true]… }` in a description:
// the DLL its functions name with dll=, and the exports it has besides them.
bool Loader::loadDll(const KdlNode &node, const Header &header) {
	if (!checkShape(node, NodeShape{1, 1, makeSpanView(s_dllProps), true})) {
		return false;
	}
	Dll dll;
	dll.location = node.location;
	dll.abi = header.abi;
	dll.abiFile = header.abiFile;
	if (!readString(node.arguments[0], dll.name, "a DLL name")
			|| !readOptional(node, "file", dll.file) || !readOptional(node, "table", dll.table)
			|| !readOptional(node, "preload-list", dll.preloadList)) {
		return false;
	}
	if (auto known = _map->findDll(dll.name)) {
		// A description shared by two headers is loaded for each of them.
		if (known->abiFile == dll.abiFile && known->location.line == dll.location.line) {
			return true;
		}
		error(node.location,
				mem_std::toString("DLL ", dll.name, " is already described in ", known->abi));
		return false;
	}
	// A DLL without table= has a .def only: the loader does not open it.
	if (dll.file.empty()) {
		error(node.location, "a DLL has its file: file=");
		return false;
	}
	if (dll.table.empty() && !dll.preloadList.empty()) {
		error(node.location, "preload-list= belongs to a DLL with a table");
		return false;
	}
	bool ok = true;
	mem_std::Map<String, size_t> seen;
	for (auto &child : node.children) {
		if (child.name == "preload") {
			if (!checkShape(child, NodeShape{1, 100'000, SpanView<StringView>(), false})) {
				ok = false;
				continue;
			}
			for (auto &arg : child.arguments) {
				String name;
				if (readString(arg, name, "a preloaded export")) {
					dll.preloads.emplace_back(sprt::move(name));
					dll.preloadLocations.emplace_back(arg.location);
				} else {
					ok = false;
				}
			}
		} else if (child.name == "export") {
			DllExport exp;
			if (!checkShape(child, NodeShape{1, 1, makeSpanView(s_exportProps), false})
					|| !readString(child.arguments[0], exp.name, "an export")) {
				ok = false;
				continue;
			}
			exp.location = child.location;
			if (auto def = child.getPropertyValue("def")) {
				exp.def = def->isBool() && def->boolean;
			}
			if (auto table = child.getPropertyValue("table")) {
				exp.table = !(table->isBool() && !table->boolean);
			}
			if (!seen.emplace(exp.name, dll.exports.size()).second) {
				error(child.location, mem_std::toString("a second export ", exp.name));
				ok = false;
				continue;
			}
			dll.exports.emplace_back(sprt::move(exp));
		} else {
			error(child.location, mem_std::toString("a DLL holds preload and export, not '", child.name, "'"));
			ok = false;
		}
	}
	_map->dlls.emplace_back(sprt::move(dll));
	return ok;
}

bool Loader::resolveUmbrellas() {
	bool ok = true;
	for (auto &header : _map->headers) {
		if (header.umbrellaOf.empty()) {
			continue;
		}
		auto target = _map->findHeader(header.umbrellaOf);
		if (!target || target->abi.empty()) {
			_diag->error(header.mapFile, header.location,
					mem_std::toString(header.path, " is the umbrella of ", header.umbrellaOf,
							", which is not a header of the map with a description"));
			ok = false;
			continue;
		}
		header.umbrellaTarget = target;
		const_cast<Header *>(target)->umbrellas.emplace_back(&header);
	}
	return ok;
}

// Every function with dll= names a DLL of the descriptions and is exported once: by one function,
// not also as an export of the DLL; the preloads are functions or exports of their DLL.
bool Loader::checkDlls() {
	bool ok = true;
	auto exported = [](const Dll &dll, StringView name) -> const DllExport * {
		for (auto &it : dll.exports) {
			if (StringView(it.name) == name) {
				return &it;
			}
		}
		return nullptr;
	};
	Vector<StringView> descriptions;
	for (auto &header : _map->headers) {
		if (header.imported || header.abi.empty()) {
			continue;
		}
		bool shared = false;
		for (auto &it : descriptions) { shared = shared || it == StringView(header.abiFile); }
		if (shared) {
			continue;
		}
		descriptions.emplace_back(header.abiFile);
		for (auto &item : header.items) {
			if (item.kind != ItemKind::Func || item.dll.empty() || item.redeclare) {
				continue;
			}
			auto dll = const_cast<Dll *>(_map->findDll(item.dll));
			if (!dll) {
				_diag->error(header.abiFile, item.location,
						mem_std::toString("no DLL '", item.dll, "' in the descriptions"));
				ok = false;
				continue;
			}
			for (auto &v : funcVariants(*_map, item)) {
				if (auto exp = exported(*dll, v.name)) {
					_diag->error(dll->abiFile, exp->location,
							mem_std::toString(v.name, " is a function of ", header.abi,
									", and no export of its own"));
					ok = false;
				}
				for (auto &f : dll->funcs) {
					for (auto &w : funcVariants(*_map, *f.item)) {
						if (w.name == v.name) {
							_diag->error(header.abiFile, item.location,
									mem_std::toString(v.name, " is already a function of ",
											f.header->abi));
							ok = false;
						}
					}
				}
			}
			// Without a table there is nothing a wrapper could load the function from.
			auto &func = const_cast<Item &>(item);
			if (!func.stubGiven && (dll->table.empty() || !func.inTable)) {
				func.stub = StubMode::None;
			}
			if (func.stub == StubMode::Gen && (dll->table.empty() || !func.inTable)) {
				_diag->error(header.abiFile, item.location,
						mem_std::toString(item.name.name,
								": a generated wrapper loads from the table, and there is none"));
				ok = false;
			}
			dll->funcs.emplace_back(DllFunc{&header, &item});
		}
	}
	for (auto &dll : _map->dlls) {
		for (size_t i = 0; i < dll.preloads.size(); ++i) {
			bool known = exported(dll, dll.preloads[i]) != nullptr;
			for (auto &f : dll.funcs) {
				for (auto &v : funcVariants(*_map, *f.item)) {
					known = known || v.name == dll.preloads[i];
				}
			}
			if (!known) {
				_diag->error(dll.abiFile, dll.preloadLocations[i],
						mem_std::toString("the preload ", dll.preloads[i],
								" is neither a function nor an export of ", dll.name));
				ok = false;
			}
		}
	}
	return ok;
}

} // namespace

} // namespace stappler::xlkdl
