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

// A type with the generic names of a charset replaced: LPCTSTR is LPCWSTR in the W variant.
String substitute(StringView text, const Charset *charset) {
	if (!charset) {
		return text.str<memory::StandardInterface>();
	}
	Vector<Token> tokens;
	if (!tokenize(text, tokens)) {
		return text.str<memory::StandardInterface>();
	}
	String out;
	for (auto &tok : tokens) {
		out.append(tok.space.data(), tok.space.size());
		if (tok.literal) {
			out.append(mem_std::toString("`", tok.text, "`"));
			continue;
		}
		StringView word = tok.text;
		if (tok.kind == TokenKind::Identifier) {
			for (auto &it : charset->types) {
				if (StringView(it.first) == word) {
					word = it.second;
				}
			}
		}
		out.append(word.data(), word.size());
	}
	return out;
}

bool isVoid(StringView type) {
	type.trimChars<StringView::WhiteSpace>();
	return type == "void" || type == "VOID";
}

// `RET NAME(PARAMS)` for a variant of a function, with the renderer's spelling of the types. A
// wrapper and an umbrella pass their parameters on, so an unnamed one gets a name there; `args`
// gets the names.
bool signature(Renderer &r, const Item &func, const FuncVariant &variant, StringView name,
		bool named, String &out, Vector<String> *args) {
	String list;
	size_t index = 0;
	for (auto &param : func.children) {
		++index;
		if (!list.empty()) {
			list.append(", ");
		}
		if (param.variadic) {
			list.append("...");
			continue;
		}
		auto type = substitute(param.text, variant.charset);
		String pname = param.name.name;
		if (pname.empty() && named) {
			pname = mem_std::toString("a", index);
		}
		String decl;
		if (pname.empty()) {
			if (!r.text(type, param.textLocation, SpanView<String>(), decl)) {
				return false;
			}
		} else if (!r.declaration(type, pname, param.textLocation, decl)) {
			return false;
		}
		if (args && !pname.empty()) {
			args->emplace_back(pname);
		}
		list.append(decl);
	}
	if (func.children.empty()) {
		list = func.empty;
	}
	auto ret = substitute(func.text, variant.charset);
	return r.declaration(ret, mem_std::toString(name, "(", list, ")"), func.textLocation, out);
}

// The name a variant is declared with: an A/W variant is the export, a single one takes its rule.
String declaredName(const Item &func, const FuncVariant &variant) {
	return variant.charset ? variant.name : func.name.render();
}

String joinWords(std::initializer_list<StringView> words) {
	String out;
	for (auto &it : words) {
		if (it.empty()) {
			continue;
		}
		if (!out.empty()) {
			out.push_back(' ');
		}
		out.append(it.data(), it.size());
	}
	return out;
}

} // namespace

// `ATTR CC RET NAME(PARAMS);` per variant, then the UNICODE choice between the variants.
bool Emitter::func(const Item &item, uint32_t depth) {
	separate(depth, Spacing::Typedef);
	String attr;
	if (auto linkage = _map->findLinkage(item.link)) {
		if (!linkage->attr.empty()
				&& !_r->text(linkage->attr, item.location, SpanView<String>(), attr)) {
			return false;
		}
	}
	String cc;
	String own;
	if ((!item.cc.empty() && !_r->text(item.cc, item.location, SpanView<String>(), cc))
			|| (!item.attr.empty() && !_r->text(item.attr, item.location, SpanView<String>(), own))) {
		return false;
	}
	for (auto &v : funcVariants(*_map, item)) {
		String decl;
		if (!signature(*_r, item, v, declaredName(item, v), false, decl, nullptr)) {
			return false;
		}
		indent(depth);
		_out->append(mem_std::toString(joinWords({attr, own, cc, decl}), ";\n"));
	}
	Vector<String> unicode;
	StringView r(item.unicode);
	while (!r.empty()) {
		r.skipChars<StringView::Chars<' ', '\t', ','>>();
		auto word = r.readUntil<StringView::Chars<' ', '\t', ','>>();
		if (!word.empty()) {
			unicode.emplace_back(word.str<memory::StandardInterface>());
		}
	}
	if (!unicode.empty()) {
		auto &name = item.name.name;
		_out->append(mem_std::toString("#ifdef UNICODE\n#define ", name, " ", name, unicode[0], "\n"));
		if (unicode.size() > 1) {
			_out->append(mem_std::toString("#else\n#define ", name, " ", name, unicode[1], "\n"));
		}
		_out->append("#endif\n");
	}
	return true;
}

// `BEGIN RET NAME(PARAMS) END` with the public names, and under `#if BODY-IF` a body that calls the
// function by its own name.
bool Emitter::umbrellaFunc(const Item &item, uint32_t depth) {
	if (!item.children.empty() && item.children.back().variadic) {
		_r->error(item.location, "a variadic function has no umbrella: umbrella=#false");
		return false;
	}
	auto &u = _map->umbrella;
	String cc;
	String own;
	if ((!item.cc.empty() && !_r->text(item.cc, item.location, SpanView<String>(), cc))
			|| (!item.attr.empty() && !_r->text(item.attr, item.location, SpanView<String>(), own))) {
		return false;
	}
	for (auto &v : funcVariants(*_map, item)) {
		separate(depth, Spacing::Block);
		String decl;
		Vector<String> args;
		_r->setPublic(true);
		auto ok = signature(*_r, item, v, v.name, true, decl, &args);
		_r->setPublic(false);
		if (!ok) {
			return false;
		}
		String call = declaredName(item, v);
		call.push_back('(');
		for (size_t i = 0; i < args.size(); ++i) {
			call.append(mem_std::toString(i ? ", " : "", args[i]));
		}
		call.push_back(')');
		_out->append(mem_std::toString(u.begin, "\n",
				joinWords({own, cc, decl, item.nothrow ? StringView(u.end) : StringView(u.endExcept)}),
				"\n"));
		if (!u.bodyIf.empty()) {
			_out->append(mem_std::toString("#if ", u.bodyIf, "\n"));
		}
		_out->append(mem_std::toString("{\n\t", isVoid(substitute(item.text, v.charset)) ? "" : "return ",
				call, ";\n}\n"));
		if (!u.bodyIf.empty()) {
			_out->append("#endif\n");
		}
	}
	return true;
}

String dllPath(const Map &map, const Dll &dll, DllPart part) {
	switch (part) {
	case DllPart::Def: return mem_std::toString(map.dllRoot.def, dll.name, ".def");
	case DllPart::Table: return mem_std::toString(map.dllRoot.code, "dll_", dll.name, ".h");
	case DllPart::Stubs: return mem_std::toString(map.dllRoot.code, "dllstub2_", dll.name, ".cc");
	}
	return String();
}

bool emitDll(const Map &map, const Dll &dll, DllPart part, String &out, Diagnostics &diag) {
	out.clear();
	auto note = mem_std::toString("Generated by xlkdl from ", dll.abi, " (header map ",
			filepath::lastComponent(map.file), "). Do not edit: regenerate it.");

	// The module definition: the functions of the DLL the .def takes, then its own exports.
	if (part == DllPart::Def) {
		out.append(mem_std::toString("; ", note, "\nLIBRARY ", dll.file, "\nEXPORTS\n"));
		for (auto &f : dll.funcs) {
			if (f.item->def) {
				for (auto &v : funcVariants(map, *f.item)) {
					out.append(mem_std::toString("\t", v.name, "\n"));
				}
			}
		}
		for (auto &e : dll.exports) {
			if (e.def) {
				out.append(mem_std::toString("\t", e.name, "\n"));
			}
		}
		return true;
	}

	if (!map.license.empty()) {
		out.append(map.license);
		if (out.back() != '\n') {
			out.push_back('\n');
		}
		out.push_back('\n');
	}
	out.append(mem_std::toString("// ", note, "\n\n"));

	// The table of the loader: the functions of the DLL, its other exports, and the ones resolved when
	// it loads.
	if (part == DllPart::Table) {
		auto path = dllPath(map, dll, part);
		auto guard = guardOf(map, path);
		auto &n = dll.name;
		out.append(mem_std::toString("#ifndef ", guard, "\n#define ", guard, "\n\n#include \"",
				map.dllRoot.tableInclude, "\"\n\nnamespace sprt {\n\n"));
		// The preloads as a list macro, for the code that may call them through a raw pointer.
		if (!dll.preloadList.empty()) {
			out.append(mem_std::toString("#define ", dll.preloadList, "(X)"));
			for (auto &p : dll.preloads) { out.append(mem_std::toString(" \\\n\tX(", p, ")")); }
			out.append("\n\n");
		}
		out.append(mem_std::toString("struct ", dll.table,
				" : DllTable {\n\t", dll.table, "() : DllTable(L\"", dll.file, "\", &__", n,
				"_begin, &__", n, "_end", dll.preloads.empty() ? "" : ", __preloads", ") { }\n\n"));
		out.append(mem_std::toString("\tDllTableRecord __", n, "_begin;\n"));
		for (auto &f : dll.funcs) {
			if (!f.item->inTable) {
				continue;
			}
			for (auto &v : funcVariants(map, *f.item)) {
				out.append(mem_std::toString("\tSPWIN_DEFINE_PROTO(", v.name, ")\n"));
			}
		}
		for (auto &e : dll.exports) {
			if (e.table) {
				out.append(mem_std::toString("\tSPWIN_DEFINE_PROTO(", e.name, ")\n"));
			}
		}
		out.append(mem_std::toString("\tDllTableRecord __", n, "_end;\n"));
		if (!dll.preloads.empty()) {
			out.append(mem_std::toString("\n\tDllTableRecord *__preloads[", dll.preloads.size() + 1,
					"] = {\n"));
			for (auto &p : dll.preloads) { out.append(mem_std::toString("\t\t&", p, ",\n")); }
			out.append("\t\tnullptr,\n\t};\n");
		}
		out.append(mem_std::toString("};\n\n} // namespace sprt\n\n#endif // ", guard, "\n"));
		return true;
	}

	// The wrappers: each generated one loads its function from the table and calls it.
	Vector<const Header *> headers;
	for (auto &f : dll.funcs) {
		if (f.item->stub != StubMode::Gen) {
			continue;
		}
		bool seen = false;
		for (auto h : headers) { seen = seen || h == f.header; }
		if (!seen) {
			headers.emplace_back(f.header);
		}
	}
	for (auto h : headers) { out.append(mem_std::toString("#include <", h->path, ">\n")); }
	out.append(mem_std::toString(headers.empty() ? "" : "\n", "#include \"",
			map.dllRoot.loaderInclude, "\"\n\nextern \"C\" {\n"));
	bool ok = true;
	for (auto h : headers) {
		Renderer r(map, *h, diag);
		for (auto &f : dll.funcs) {
			if (f.header != h || f.item->stub != StubMode::Gen) {
				continue;
			}
			auto &item = *f.item;
			String cc;
			String own;
			String cond;
			if ((!item.cc.empty() && !r.text(item.cc, item.location, SpanView<String>(), cc))
					|| (!item.attr.empty() && !r.text(item.attr, item.location, SpanView<String>(), own))
					|| (!item.condition.empty()
							&& !r.text(item.condition, item.conditionLocation, SpanView<String>(),
									cond))) {
				ok = false;
				continue;
			}
			// A wrapper stands under the condition its function is declared under.
			if (!cond.empty()) {
				out.append(mem_std::toString("\n#if ", cond));
			}
			for (auto &v : funcVariants(map, item)) {
				String decl;
				Vector<String> args;
				if (!signature(r, item, v, v.name, true, decl, &args)) {
					ok = false;
					continue;
				}
				String call = mem_std::toString("DLL_LOAD_AND_CALL(loader, ", dll.name, ", ", v.name);
				for (auto &a : args) { call.append(mem_std::toString(", ", a)); }
				call.push_back(')');
				out.append(mem_std::toString("\n", joinWords({own, cc, decl}),
						" {\n\tauto loader = sprt::DllLoader::get();\n\t",
						isVoid(substitute(item.text, v.charset)) ? "" : "return ", call, ";\n}\n"));
			}
			if (!cond.empty()) {
				out.append("#endif\n");
			}
		}
	}
	out.append("\n} // extern \"C\"\n");
	return ok && diag.empty();
}

} // namespace stappler::xlkdl
