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

// Writing a built graph out as constants.
// The shape of the text is fixed here and nowhere else: one table per section, one row per line,
// designated initializers in the order the row types declare their fields, numbers in decimal and
// hashes in sixteen hex digits. Nothing in the text depends on an address, a date or the host - the
// same graph gives the same bytes on every machine, which is what makes a unit committable.

#include "SPFlowCodegen.h"
#include "SPFlowGpu.h"

#include "SPData.h"

#include <sprt/runtime/hash.h>

namespace STAPPLER_VERSIONIZED stappler::flow::codegen {


namespace {

constexpr StringView NodeStateTypeName("interp.NodeState");

// The one literal that cannot be known while the text is being written: the hash of the text.
constexpr StringView TextHashPlaceholder(".textHash = 0x0000000000000000ull");

struct Text {
	mem_std::String out;

	void put(StringView s) { out.append(s.data(), s.size()); }
	void line(StringView s) {
		put(s);
		out.push_back('\n');
	}
	void nl() { out.push_back('\n'); }
};

mem_std::String hex64(uint64_t v) {
	static constexpr char digits[] = "0123456789abcdef";
	char buf[16];
	for (uint32_t i = 0; i < 16; ++i) {
		buf[15 - i] = digits[v & 0xf];
		v >>= 4;
	}
	return mem_std::toString("0x", StringView(buf, 16), "ull");
}

mem_std::String num(uint32_t v) { return mem_std::toString(v); }

mem_std::String index(uint32_t v) {
	return v == InvalidIndex ? mem_std::String("flow::InvalidIndex") : num(v);
}

StringView boolean(bool v) { return v ? StringView("true") : StringView("false"); }

// A StringView literal. Names here are identifiers and component names, but the escape is total
// so that a name nobody expected still compiles to the bytes it had.
mem_std::String quoted(StringView s) {
	mem_std::String out;
	out.push_back('"');
	static constexpr char digits[] = "0123456789abcdef";
	for (auto c : s) {
		auto b = uint8_t(c);
		if (c == '"' || c == '\\') {
			out.push_back('\\');
			out.push_back(c);
		} else if (b >= 0x20 && b < 0x7f) {
			out.push_back(c);
		} else {
			// Hex escapes swallow the digits after them, so each one is closed by ending the
			// literal and starting a new one; adjacent literals concatenate.
			out.append("\\x");
			out.push_back(digits[b >> 4]);
			out.push_back(digits[b & 0xf]);
			out.append("\"\"");
		}
	}
	out.push_back('"');
	return out;
}

mem_std::String view(StringView s) {
	return s.empty() ? mem_std::String("StringView()") : mem_std::toString("StringView(", quoted(s), ")");
}

mem_std::String castRule(value::CastRule rule) {
	return mem_std::toString("flow::value::CastRule(", uint32_t(rule), ") /* ",
			value::getCastRuleName(rule), " */");
}

mem_std::String scopeKind(ScopeKind kind) {
	return mem_std::toString("flow::ScopeKind(", uint32_t(kind), ") /* ", getScopeKindName(kind), " */");
}

mem_std::String opParallel(OpParallel value) {
	return mem_std::toString("flow::OpParallel(", uint32_t(value), ") /* ", getOpParallelName(value), " */");
}

mem_std::String parallelFailure(ParallelFailure value) {
	return mem_std::toString("flow::ParallelFailure(", uint32_t(value), ") /* ",
			getParallelFailureName(value), " */");
}

mem_std::String varType(VarType type) {
	return mem_std::toString("flow::value::VarType(", uint32_t(type), ") /* ",
			value::getVarTypeName(type), " */");
}

// A byte array, sixteen to a line. An empty one is one zero byte and a count of zero: a zero-length
// array is not C++.
void bytesTable(Text &t, StringView name, BytesView bytes) {
	t.line(mem_std::toString("\tstatic constexpr uint32_t ", name, "Size = ", uint32_t(bytes.size()),
			";"));
	t.line(mem_std::toString("\tstatic constexpr uint8_t ", name, "[", num(uint32_t(bytes.empty() ? 1 : bytes.size())),
			"] = {"));
	if (bytes.empty()) {
		t.line(StringView("\t\t0,"));
	}
	static constexpr char digits[] = "0123456789abcdef";
	for (size_t i = 0; i < bytes.size(); i += 16) {
		t.put(StringView("\t\t"));
		for (size_t k = i; k < bytes.size() && k < i + 16; ++k) {
			auto b = bytes[k];
			char lit[] = {'0', 'x', digits[b >> 4], digits[b & 0xf], ',', ' '};
			t.put(StringView(lit, k + 1 < bytes.size() && k + 1 < i + 16 ? 6 : 5));
		}
		t.nl();
	}
	t.line(StringView("\t};"));
}

// A table of rows. Same rule for the empty one.
void rowTable(Text &t, StringView type, StringView name, uint32_t count,
		const Callback<mem_std::String(uint32_t)> &row) {
	t.line(mem_std::toString("\tstatic constexpr uint32_t ", name, "Count = ", num(count), ";"));
	t.line(mem_std::toString("\tstatic constexpr ", type, " ", name, "[",
			num(count == 0 ? 1 : count), "] = {"));
	if (count == 0) {
		t.line(StringView("\t\t{},"));
	}
	for (uint32_t i = 0; i < count; ++i) { t.line(mem_std::toString("\t\t", row(i), ",")); }
	t.line(StringView("\t};"));
}

void indexTable(Text &t, StringView name, SpanView<uint32_t> values) {
	rowTable(t, StringView("uint32_t"), name, uint32_t(values.size()),
			[&](uint32_t i) { return num(values[i]); });
}

mem_std::String upper(StringView s) {
	mem_std::String out;
	for (auto c : s) { out.push_back(c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c); }
	return out;
}

} // namespace

bool isUnitName(StringView name) {
	if (name.empty()) {
		return false;
	}
	for (size_t i = 0; i < name.size(); ++i) {
		auto c = name[i];
		bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		bool digit = c >= '0' && c <= '9';
		if (!alpha && !(digit && i > 0)) {
			return false;
		}
	}
	return true;
}

bool isUnitNamespace(StringView ns, StringView &relative) {
	static constexpr StringView Root("stappler::");
	if (!ns.starts_with(Root) || ns.size() == Root.size()) {
		return false;
	}
	relative = StringView(ns.data() + Root.size(), ns.size() - Root.size());
	auto rest = relative;
	while (!rest.empty()) {
		size_t len = 0;
		while (len < rest.size() && rest[len] != ':') { ++len; }
		if (!isUnitName(StringView(rest.data(), len))) {
			return false;
		}
		if (len == rest.size()) {
			break;
		}
		if (len + 2 >= rest.size() || rest[len + 1] != ':') {
			return false;
		}
		rest = StringView(rest.data() + len + 2, rest.size() - len - 2);
	}
	return true;
}

Status emit(const RuntimeGraph &g, const GraphAsset &asset, const OpRegistry &ops,
		const EmitOptions &options, Emitted &out, DiagSink *sink) {
	DiagReport report(sink);
	out = Emitted();

	if (!g.isValid()) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
				DiagText(DiagDetail::CodegenNotBuilt));
		return report.getStatus();
	}
	if (!isUnitName(options.name)) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
				DiagText(DiagDetail::CodegenNameNotIdentifier).name(options.name));
		return report.getStatus();
	}
	// The unit's namespace, and the same named from inside `stappler`, which is where the
	// instantiation of the loaded graph is written.
	StringView relative;
	if (!isUnitNamespace(options.unitNamespace, relative)) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
				DiagText(DiagDetail::CodegenNameNotIdentifier).name(options.unitNamespace));
		return report.getStatus();
	}
	auto stateType = ops.getLocalTypes().get(NodeStateTypeName);
	if (!stateType) {
		report.report(DiagSeverity::Error, DiagCode::CodegenSchemaDrift,
				DiagText(DiagDetail::CodegenNodeStateMissing));
		return report.getStatus();
	}

	auto nodeCount = g.getNodeCount();
	auto scopeCount = g.getScopeCount();

	// The index arrays, concatenated in node order, and the begins the emitted rows point with.
	// The build lays them out this way already; recomputing them here is what makes the emitted
	// slices right by construction rather than by trusting the build's private layout.
	struct Slices {
		mem_std::Vector<uint32_t> dataIn, dataOut, execIn, execOut, crossScopeIn;
		mem_std::Vector<uint32_t> dataInBegin, dataOutBegin, execInBegin, execOutBegin,
				crossScopeInBegin;
	} slices;
	auto concat = [&](mem_std::Vector<uint32_t> &flat, mem_std::Vector<uint32_t> &begins,
						  const Callback<SpanView<uint32_t>(uint32_t)> &of) {
		begins.resize(nodeCount, 0);
		for (uint32_t n = 0; n < nodeCount; ++n) {
			begins[n] = uint32_t(flat.size());
			for (auto e : of(n)) { flat.emplace_back(e); }
		}
	};
	concat(slices.dataIn, slices.dataInBegin, [&](uint32_t n) { return g.getDataInEdges(n); });
	concat(slices.dataOut, slices.dataOutBegin, [&](uint32_t n) { return g.getDataOutEdges(n); });
	concat(slices.execIn, slices.execInBegin, [&](uint32_t n) { return g.getExecInEdges(n); });
	concat(slices.execOut, slices.execOutBegin, [&](uint32_t n) { return g.getExecOutEdges(n); });
	concat(slices.crossScopeIn, slices.crossScopeInBegin,
			[&](uint32_t n) { return g.getCrossScopeInEdges(n); });

	mem_std::Vector<uint32_t> scopeNodes;
	mem_std::Vector<uint32_t> scopeBegin(scopeCount, 0);
	for (uint32_t s = 0; s < scopeCount; ++s) {
		scopeBegin[s] = uint32_t(scopeNodes.size());
		for (auto n : g.getScopeNodes(s)) { scopeNodes.emplace_back(n); }
	}

	// The constant table: one entry per input of every node, null where the build stored none.
	mem_std::Value constants(mem_std::Value::Type::ARRAY);
	uint32_t constantCount = 0;
	for (uint32_t n = 0; n < nodeCount; ++n) {
		auto &rt = g.getNodeAt(n);
		auto pins = uint32_t(rt.op->getDataIn().size());
		if (rt.constantBegin != constantCount) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenMalformed, rt.id,
					DiagText(DiagDetail::CodegenConstantsOutOfOrder));
			return report.getStatus();
		}
		for (uint32_t p = 0; p < pins; ++p) {
			if (auto value = g.getConstant(n, p)) {
				constants.addValue(*value);
			} else {
				constants.addValue(mem_std::Value());
			}
		}
		constantCount += pins;
	}
	auto constantBytes = constantCount == 0
			? mem_std::Bytes()
			: data::write<mem_std::Interface>(constants, data::EncodeFormat::Cbor);

	// The settings table: one entry per setting every node's operation declares, as the build
	// resolved it, null where it is the type's zero.
	mem_std::Value settings(mem_std::Value::Type::ARRAY);
	uint32_t settingCount = 0;
	for (uint32_t n = 0; n < nodeCount; ++n) {
		auto &rt = g.getNodeAt(n);
		auto decls = uint32_t(rt.op->getSettings().size());
		if (decls == 0) {
			continue;
		}
		if (rt.settingBegin != settingCount) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenMalformed, rt.id,
					DiagText(DiagDetail::CodegenConstantsOutOfOrder));
			return report.getStatus();
		}
		for (uint32_t k = 0; k < decls; ++k) {
			if (auto value = g.getSetting(n, k)) {
				settings.addValue(*value);
			} else {
				settings.addValue(mem_std::Value());
			}
		}
		settingCount += decls;
	}
	auto settingBytes = settingCount == 0
			? mem_std::Bytes()
			: data::write<mem_std::Interface>(settings, data::EncodeFormat::Cbor);

	// The blocks, and the slices their rows point into: laid out again here in block order, for the
	// reason the node slices are.
	struct BlockSlices {
		mem_std::Vector<RuntimeBlock> rows;
		mem_std::Vector<RuntimeBlockWrite> writes;
		mem_std::Vector<uint32_t> collectors;
		mem_std::Vector<TypeId> query;
	} blocks;
	for (uint32_t b = 0; b < g.getBlockCount(); ++b) {
		auto row = g.getBlockAt(b);
		row.writeBegin = uint32_t(blocks.writes.size());
		for (auto &w : g.getBlockWrites(b)) { blocks.writes.emplace_back(w); }
		row.collectorBegin = uint32_t(blocks.collectors.size());
		for (auto c : g.getBlockCollectors(b)) { blocks.collectors.emplace_back(c); }
		row.withBegin = uint32_t(blocks.query.size());
		for (auto id : g.getBlockQuery(b, false)) { blocks.query.emplace_back(id); }
		row.withoutBegin = uint32_t(blocks.query.size());
		for (auto id : g.getBlockQuery(b, true)) { blocks.query.emplace_back(id); }
		blocks.rows.emplace_back(row);
	}

	/* The enum families a node names or narrows: a node whose EnumFamily outputs carry a family,
	and a node taking an Enum input to an Int32 output from such a producer. Only with the
	registry the graph was bound to - an unbound build knows the names and not the members. */
	struct FamilyRow {
		NodeId node;
		StringView name;
		uint64_t hash;
	};
	mem_std::Vector<FamilyRow> families;
	if (auto registry = g.getSceneRegistry()) {
		for (uint32_t n = 0; n < nodeCount; ++n) {
			auto &rt = g.getNodeAt(n);
			TypeId family = rt.family;
			auto ins = rt.op->getDataIn();
			auto outs = rt.op->getDataOut();
			if (family == value::NullTypeId && ins.size() == 1 && outs.size() == 1
					&& ins[0].type == VarType::Enum && outs[0].type == VarType::Int32) {
				for (auto e : g.getDataInEdges(n)) {
					auto &edge = g.getDataEdges()[e];
					auto &src = g.getNodeAt(edge.srcNode);
					family = nodeDataOut(*src.op, edge.srcPin, src.family).subtypeId;
				}
			}
			if (family == value::NullTypeId) {
				continue;
			}
			if (auto type = registry->getEnum(family)) {
				families.emplace_back(FamilyRow{rt.id, type->getName(), type->getHash()});
			}
		}
	}

	// The contracts, gathered per node into two flat tables with per-node begins.
	mem_std::Vector<SceneBinding> sceneRows;
	mem_std::Vector<ExtensionBinding> extRows;
	mem_std::Vector<uint32_t> sceneBegin(nodeCount, 0);
	mem_std::Vector<uint32_t> extBegin(nodeCount, 0);
	// The named entities' too, emitted only when a node names one, so a unit that names none is
	// the bytes it always was.
	mem_std::Vector<NamedBinding> namedRows;
	mem_std::Vector<uint32_t> namedBegin(nodeCount, 0);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		sceneBegin[n] = uint32_t(sceneRows.size());
		for (auto &b : g.getSceneContract(n)) { sceneRows.emplace_back(b); }
		extBegin[n] = uint32_t(extRows.size());
		for (auto &b : g.getExtensionContract(n)) { extRows.emplace_back(b); }
		namedBegin[n] = uint32_t(namedRows.size());
		for (auto &b : g.getNamedContract(n)) { namedRows.emplace_back(b); }
	}

	// The declarations, with their parameters encoded.
	auto extDecls = g.getExtensionDecls();
	mem_std::Vector<mem_std::Bytes> extParams;
	for (auto decl : extDecls) {
		extParams.emplace_back(decl->params.isNull()
						? mem_std::Bytes()
						: data::write<mem_std::Interface>(decl->params, data::EncodeFormat::Cbor));
	}

	// The identity: every distinct operation, sorted by name so the table is a set.
	mem_std::Vector<const OpDesc *> distinctOps;
	for (uint32_t n = 0; n < nodeCount; ++n) {
		auto op = g.getNodeAt(n).op;
		bool seen = false;
		for (auto it : distinctOps) { seen = seen || it == op; }
		if (!seen) {
			distinctOps.emplace_back(op);
		}
	}
	sprt::sort(distinctOps.begin(), distinctOps.end(),
			[](const OpDesc *a, const OpDesc *b) { return a->getName() < b->getName(); });

	mem_std::Vector<FrameSlot> slots;
	mem_std::Vector<uint32_t> frameBytes;
	if (computeFrameLayout(g, *stateType, slots, frameBytes) != Status::Ok) {
		report.report(DiagSeverity::Error, DiagCode::CodegenLayoutDrift,
				DiagText(DiagDetail::CodegenLayoutUncomputable));
		return report.getStatus();
	}

	Text t;
	auto name = options.name;
	auto guard = mem_std::toString("GEN_", upper(name), "_GEN_H_");
	// The asset's name for the reader; the identity carries it as it is, empty included.
	auto title = asset.getName().empty() ? StringView("<unnamed>") : asset.getName();

	t.line(mem_std::toString("// Generated from the graph \"", title,
			"\". Do not edit: regenerate it."));
	t.line(StringView("//"));
	t.line(StringView("// A graph as constants: the shape the build settled, the constant table, "
					  "both"));
	t.line(StringView("// contracts unbound, and the identity of what it was written against. Loaded "
					  "through"));
	t.line(StringView("// flow::StaticGraph<Tables>, which resolves the operations, checks the "
					  "identity and binds the scene;"));
	t.line(StringView("// run by Run<A>, the interpreter's own machine over the loaded rows, or held "
					  "by a host as Engine<A>."));
	t.line(StringView("//"));
	t.line(StringView("// The rows are half of it. The other half is in <name>.gen.cpp: one function "
					  "per node, with the"));
	t.line(StringView("// node index a compile-time constant, which is what the machine performs a "
					  "node through."));
	t.line(mem_std::toString("#ifndef ", guard));
	t.line(mem_std::toString("#define ", guard));
	t.nl();
	t.line(mem_std::toString("#include \"", options.include, "\""));
	if (!options.envInclude.empty() && options.envInclude != options.include) {
		t.line(mem_std::toString("#include \"", options.envInclude, "\""));
	}
	t.nl();
	/* How the code is cut (EmitOptions::split). Decided before the header is written, because the
	header declares one entry point per part. More parts than nodes would leave some empty and the
	ranges degenerate, so the count is silently taken down: the caller asked how finely to cut. */
	auto parts = options.split > 1 ? options.split : 1;
	if (parts > nodeCount && nodeCount > 0) {
		parts = nodeCount;
	}
	auto partBegin = [&](uint32_t k) { return uint32_t(uint64_t(k) * nodeCount / parts); };

	t.line(mem_std::toString("namespace STAPPLER_VERSIONIZED ", options.unitNamespace, "::", name, " {"));
	t.nl();
	t.line(StringView("// This unit's own way of performing a node: `step<N>` with the index a "
					  "constant, the door over"));
	t.line(StringView("// the tables below and the operation called by name where it has one. "
					  "Written in"));
	t.line(StringView("// <name>.gen.cpp, beside the bodies it calls, once per store a run"));
	t.line(StringView("// may live in - the arena one and the fast one."));
	t.line(mem_std::toString("const flow::CompiledStepsT<", options.env, "> *steps();"));
	t.nl();
	if (parts > 1) {
		t.line(mem_std::toString("// The code is written across ", num(parts),
				" sources, each performing a range of nodes: the graph is"));
		t.line(StringView("// large enough that one translation unit would be compiled alone while "
						  "the other cores waited."));
		t.line(StringView("// The first source dispatches by range and holds the table above; these "
						  "are the entry points it"));
		t.line(StringView("// calls, instantiated where they are defined."));
		for (uint32_t k = 0; k < parts; ++k) {
			t.line(StringView("template <typename Local>"));
			t.line(mem_std::toString("Status stepPart", num(k),
					"(flow::CompiledStepSiteT<Local> &site);"));
		}
		t.nl();
	}
	t.line(StringView("struct Tables {"));
	t.line(mem_std::toString("\tstatic constexpr uint32_t NodeCount = ", num(nodeCount), ";"));
	t.line(mem_std::toString("\tstatic constexpr uint32_t ScopeCount = ", num(scopeCount), ";"));
	t.nl();

	rowTable(t, StringView("StringView"), StringView("opNames"), nodeCount,
			[&](uint32_t n) { return view(g.getNodeAt(n).op->getName()); });
	t.nl();

	rowTable(t, StringView("flow::RuntimeNode"), StringView("nodes"), nodeCount, [&](uint32_t n) {
		auto &rt = g.getNodeAt(n);
		return mem_std::toString("{.id = ", num(rt.id), ", .dataInBegin = ", num(slices.dataInBegin[n]),
				", .dataInCount = ", num(rt.dataInCount), ", .dataOutBegin = ",
				num(slices.dataOutBegin[n]), ", .dataOutCount = ", num(rt.dataOutCount),
				", .execInBegin = ", num(slices.execInBegin[n]), ", .execInCount = ",
				num(rt.execInCount), ", .execOutBegin = ", num(slices.execOutBegin[n]),
				", .execOutCount = ", num(rt.execOutCount), ", .constantBegin = ",
				num(rt.constantBegin), ", .sceneBegin = ", num(sceneBegin[n]), ", .sceneCount = ",
				num(rt.sceneCount), ", .extBegin = ", num(extBegin[n]), ", .extCount = ",
				num(rt.extCount),
				rt.namedCount == 0 ? mem_std::String()
								   : mem_std::toString(", .namedBegin = ", num(namedBegin[n]),
											 ", .namedCount = ", num(rt.namedCount)),
				", .scope = ", num(rt.scope), ", .opensScope = ",
				index(rt.opensScope), ", .slotInScope = ", num(rt.slotInScope),
				", .sameScopeInputs = ", num(rt.sameScopeInputs), "u, .allInputs = ",
				num(rt.allInputs), "u, .crossScopeInBegin = ", num(slices.crossScopeInBegin[n]),
				", .crossScopeInCount = ", num(rt.crossScopeInCount), ", .isEntry = ",
				boolean(rt.isEntry), ", .isTerminal = ", boolean(rt.isTerminal),
				rt.familyName.empty() ? mem_std::String()
									  : mem_std::toString(", .family = ", hex64(rt.family),
												", .familyName = ", view(rt.familyName)),
				rt.op->getSettings().empty() ? mem_std::String()
											 : mem_std::toString(", .settingBegin = ", num(rt.settingBegin)),
				rt.weight == 0 ? mem_std::String() : mem_std::toString(", .weight = ", num(rt.weight)),
				"}");
	});
	t.nl();

	auto dataEdges = g.getDataEdges();
	rowTable(t, StringView("flow::RuntimeDataEdge"), StringView("dataEdges"),
			uint32_t(dataEdges.size()), [&](uint32_t i) {
		auto &e = dataEdges[i];
		return mem_std::toString("{.srcNode = ", num(e.srcNode), ", .srcPin = ", num(e.srcPin),
				", .dstNode = ", num(e.dstNode), ", .dstPin = ", num(e.dstPin), ", .cast = ",
				castRule(e.cast), ", .needsArena = ", boolean(e.needsArena), "}");
	});
	auto execEdges = g.getExecEdges();
	rowTable(t, StringView("flow::RuntimeExecEdge"), StringView("execEdges"),
			uint32_t(execEdges.size()), [&](uint32_t i) {
		auto &e = execEdges[i];
		return mem_std::toString("{.srcNode = ", num(e.srcNode), ", .srcPin = ", num(e.srcPin),
				", .dstNode = ", num(e.dstNode), ", .backEdge = ", boolean(e.backEdge), "}");
	});
	t.nl();

	indexTable(t, StringView("dataIn"), slices.dataIn);
	indexTable(t, StringView("dataOut"), slices.dataOut);
	indexTable(t, StringView("execIn"), slices.execIn);
	indexTable(t, StringView("execOut"), slices.execOut);
	indexTable(t, StringView("crossScopeIn"), slices.crossScopeIn);
	indexTable(t, StringView("entries"), g.getEntryNodes());
	indexTable(t, StringView("terminals"), g.getTerminalNodes());
	t.nl();

	rowTable(t, StringView("flow::RuntimeScope"), StringView("scopes"), scopeCount,
			[&](uint32_t s) {
		auto &scope = g.getScopeAt(s);
		// A parallel scope, or one inside a branch, says so; every other row reads as it always
		// did.
		const bool parallel = scope.kind != ScopeKind::Loop || scope.barrier != InvalidIndex
				|| scope.branchScope != InvalidIndex || scope.block != InvalidIndex || scope.headerBytes != 0;
		return mem_std::toString("{.opener = ", index(scope.opener), ", .execPin = ",
				index(scope.execPin), ", .parent = ", index(scope.parent), ", .depth = ",
				num(scope.depth), ", .nodeBegin = ", num(scopeBegin[s]), ", .nodeCount = ",
				num(scope.nodeCount),
				parallel ? mem_std::toString(", .kind = ", scopeKind(scope.kind), ", .barrier = ",
								   index(scope.barrier), ", .branchScope = ", index(scope.branchScope),
								   ", .block = ", index(scope.block), ", .headerBytes = ",
								   num(scope.headerBytes))
						 : mem_std::String(),
				"}");
	});
	indexTable(t, StringView("scopeNodes"), scopeNodes);
	t.nl();

	const bool hasBlocks = !blocks.rows.empty();
	if (hasBlocks) {
		t.line(StringView("	// The parallel blocks, what their branches write, which collectors read "
						  "them, what they query."));
		rowTable(t, StringView("flow::RuntimeBlock"), StringView("blocks"), uint32_t(blocks.rows.size()),
				[&](uint32_t i) {
			auto &b = blocks.rows[i];
			return mem_std::toString("{.scope = ", num(b.scope), ", .fanOut = ", num(b.fanOut),
					", .barrier = ", num(b.barrier), ", .entityPin = ", index(b.entityPin),
					", .indexPin = ", index(b.indexPin), ", .onFailure = ", parallelFailure(b.onFailure),
					", .maxSteps = ", num(b.maxSteps), ", .maxActivations = ", num(b.maxActivations),
					", .timeoutMs = ", num(b.timeoutMs), ", .writeBegin = ", num(b.writeBegin),
					", .writeCount = ", num(b.writeCount), ", .collectorBegin = ", num(b.collectorBegin),
					", .collectorCount = ", num(b.collectorCount), ", .withBegin = ", num(b.withBegin),
					", .withCount = ", num(b.withCount), ", .withoutBegin = ", num(b.withoutBegin),
					", .withoutCount = ", num(b.withoutCount), "}");
		});
		rowTable(t, StringView("flow::RuntimeBlockWrite"), StringView("blockWrites"),
				uint32_t(blocks.writes.size()), [&](uint32_t i) {
			auto &w = blocks.writes[i];
			return mem_std::toString("{.componentId = ", hex64(w.componentId), ", .component = ",
					view(w.component), ", .field = ", view(w.field), ", .type = ", varType(w.type),
					", .subtypeId = ", hex64(w.subtypeId), ", .flagOffset = ", num(w.flagOffset),
					", .valueOffset = ", num(w.valueOffset), "}");
		});
		indexTable(t, StringView("blockCollectors"), blocks.collectors);
		rowTable(t, StringView("flow::value::TypeId"), StringView("blockQuery"),
				uint32_t(blocks.query.size()), [&](uint32_t i) { return hex64(blocks.query[i]); });
		t.nl();
	}
	if (settingCount > 0) {
		t.line(StringView("	// The settings table, one CBOR array: an entry per setting every node's "
						  "operation declares."));
		bytesTable(t, StringView("settings"), BytesView(settingBytes));
		t.nl();
	}

	t.line(StringView("\t// The constant table, one CBOR array: an entry per input of every node, "
					  "null where the build"));
	t.line(StringView("\t// stored none."));
	bytesTable(t, StringView("constants"), BytesView(constantBytes));
	t.nl();

	/* The asset, when the caller asked for it. The canonical CBOR of GraphAsset::save() - the very
	bytes getContentHash() is taken over - so a unit that carries one carries the asset its
	identity names. The machine never reads it; a tool that needs a RuntimeGraph beside the
	static graph does. */
	mem_std::Bytes assetBytes;
	if (options.embedAsset) {
		mem_std::Value canonical;
		asset.save(canonical);
		assetBytes = data::write<mem_std::Interface>(canonical, data::EncodeFormat::Cbor);
	}
	t.line(StringView("\t// The asset this unit was written from, as canonical CBOR - empty unless "
					  "--embed-asset. The"));
	t.line(StringView("\t// machine never reads it; it is here for tools that want a RuntimeGraph "
					  "beside the tables."));
	bytesTable(t, StringView("asset"), BytesView(assetBytes));
	t.nl();

	rowTable(t, StringView("flow::SceneBinding"), StringView("sceneContract"),
			uint32_t(sceneRows.size()), [&](uint32_t i) {
		auto &b = sceneRows[i];
		return mem_std::toString("{.component = ", view(b.component), ", .field = ", view(b.field),
				", .componentId = ", hex64(b.componentId), ", .declared = ", varType(b.declared),
				", .optional = ", boolean(b.optional), ", .dynamic = ", boolean(b.dynamic), "}");
	});
	rowTable(t, StringView("flow::ExtensionBinding"), StringView("extensionContract"),
			uint32_t(extRows.size()), [&](uint32_t i) {
		auto &b = extRows[i];
		return mem_std::toString("{.id = ", view(b.id), ", .name = ", view(b.name), ", .dynamic = ",
				boolean(b.dynamic), "}");
	});
	if (!namedRows.empty()) {
		rowTable(t, StringView("flow::NamedBinding"), StringView("namedContract"),
				uint32_t(namedRows.size()), [&](uint32_t i) {
			auto &b = namedRows[i];
			return mem_std::toString("{.name = ", view(b.name), ", .dynamic = ", boolean(b.dynamic),
					"}");
		});
	}
	for (uint32_t d = 0; d < uint32_t(extDecls.size()); ++d) {
		bytesTable(t, mem_std::toString("extensionParams", d), BytesView(extParams[d]));
	}
	rowTable(t, StringView("flow::CompiledExtensionDecl"), StringView("extensionDecls"),
			uint32_t(extDecls.size()), [&](uint32_t d) {
		auto params = extParams[d].empty()
				? mem_std::String("BytesView()")
				: mem_std::toString("BytesView(extensionParams", d, ", size_t(extensionParams", d, "Size))");
		return mem_std::toString("{.name = ", view(extDecls[d]->name), ", .id = ",
				view(extDecls[d]->id), ", .params = ", params, "}");
	});
	t.nl();

	t.line(StringView("\t// The identity: what this unit was written against, checked on load."));
	rowTable(t, StringView("flow::CompiledOpIdentity"), StringView("ops"),
			uint32_t(distinctOps.size()), [&](uint32_t i) {
		auto op = distinctOps[i];
		auto schema = op->getLocalSchema();
		return mem_std::toString("{.name = ", view(op->getName()), ", .signatureHash = ",
				hex64(op->getSignatureHash()), ", .localSchemaHash = ",
				hex64(schema ? schema->getSchemaHash() : uint64_t(0)), ", .parallel = ",
				opParallel(op->getParallel()), "}");
	});
	if (hasBlocks) {
		rowTable(t, StringView("flow::CompiledBlockIdentity"), StringView("blockPolicies"),
				uint32_t(blocks.rows.size()), [&](uint32_t i) {
			auto &b = blocks.rows[i];
			mem_std::String glsl;
			auto gpuHash = flow::hashGpuBlock(g, i, options.gpu ? &glsl : nullptr);
			if (gpuHash != 0 && options.gpu) {
				out.shaders.emplace_back(Emitted::Shader{mem_std::toString(options.name, "_b", i, ".comp"),
					sprt::move(glsl)});
			}
			// A block that is not the GPU's keeps the row it always had, byte for byte.
			auto gpu = gpuHash != 0 ? mem_std::toString(", .gpuHash = ", hex64(gpuHash)) : mem_std::String();
			return mem_std::toString("{.fanOut = ", num(g.getNodeAt(b.fanOut).id), ", .onFailure = ",
					parallelFailure(b.onFailure), ", .timeoutMs = ", num(b.timeoutMs), ", .maxSteps = ",
					num(b.maxSteps), ", .maxActivations = ", num(b.maxActivations), ", .withHash = ",
					hex64(hashBlockQuery(g.getBlockQuery(i, false))), ", .withoutHash = ",
					hex64(hashBlockQuery(g.getBlockQuery(i, true))), gpu, "}");
		});
	}
	if (!families.empty()) {
		rowTable(t, StringView("flow::CompiledFamilyIdentity"), StringView("families"),
				uint32_t(families.size()), [&](uint32_t i) {
			return mem_std::toString("{.node = ", num(families[i].node), ", .name = ",
					view(families[i].name), ", .hash = ", hex64(families[i].hash), "}");
		});
	}
	indexTable(t, StringView("frameBytes"), frameBytes);
	rowTable(t, StringView("flow::FrameSlot"), StringView("slots"), nodeCount, [&](uint32_t n) {
		return mem_std::toString("{.stateOffset = ", num(slots[n].stateOffset), ", .recordOffset = ",
				index(slots[n].recordOffset), "}");
	});

	// Every field of every node's record. The door asks the site for a field's shape, and off this
	// table the answer is constants - the offset it reads at, and with it the type, which is what
	// makes the codec's switch fold to a single case instead of staying a switch.
	// A field's offset is a fact about the process, which is why nothing here was folded before. It
	// is safe now for one reason and it is checked rather than assumed: `computeSchemaHash` hashes
	// every field's offset, so a schema whose fields moved has a different hash, and
	// `CompiledGraph::verify` refuses the unit at load with `CodegenSchemaDrift`.
	// The scene is not here and never will be: its bindings are resolved by name at every bind
	// (SPFlowRuntime.cc), no hash of them rides in the identity block, and its door is under half
	// a per cent of the field traffic anyway.
	t.nl();
	t.line(StringView("\t// The record of each node, field by field: what the door reads and writes."));
	mem_std::Vector<mem_std::String> fieldRows;
	mem_std::Vector<uint32_t> fieldBegin;
	mem_std::Vector<uint32_t> fieldCount;
	fieldBegin.reserve(nodeCount);
	fieldCount.reserve(nodeCount);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		auto &rt = g.getNodeAt(n);
		auto schema = rt.op ? rt.op->getLocalSchema() : nullptr;
		fieldBegin.emplace_back(uint32_t(fieldRows.size()));
		uint32_t count = 0;
		if (schema) {
			for (auto &f : schema->getFields()) {
				fieldRows.emplace_back(mem_std::toString("{.offset = ", num(f.offset), ", .size = ",
						num(f.size), ", .type = flow::value::VarType(", num(uint32_t(f.type)), ") /* ",
						value::getVarTypeName(f.type), " */, .element = ", num(f.element),
						", .subtypeId = ", hex64(flow::nodeFieldSubtype(rt, count, f.subtypeId)),
						", .valid = true}"));
				++count;
			}
		}
		fieldCount.emplace_back(count);
	}
	rowTable(t, StringView("flow::FieldShape"), StringView("recordFields"),
			uint32_t(fieldRows.size()), [&](uint32_t i) { return fieldRows[i]; });
	indexTable(t, StringView("recordFieldBegin"), fieldBegin);
	indexTable(t, StringView("recordFieldCount"), fieldCount);
	t.nl();
	t.line(StringView("\tstatic constexpr flow::CompiledIdentity identity = {"));
	t.line(mem_std::toString("\t\t.name = ", view(asset.getName()), ","));
	t.line(mem_std::toString("\t\t.assetHash = ", hex64(asset.getContentHash()), ","));
	t.line(mem_std::toString("\t\t", TextHashPlaceholder, ","));
	t.line(mem_std::toString("\t\t.stateSchemaHash = ", hex64(stateType->getSchemaHash()), ","));
	t.line(StringView("\t\t.ops = SpanView<flow::CompiledOpIdentity>(ops, ops + opsCount),"));
	t.line(StringView("\t\t.frameBytes = SpanView<uint32_t>(frameBytes, frameBytes + frameBytesCount),"));
	t.line(StringView("\t\t.slots = SpanView<flow::FrameSlot>(slots, slots + slotsCount),"));
	if (hasBlocks) {
		t.line(StringView("\t\t.blocks = SpanView<flow::CompiledBlockIdentity>(blockPolicies, blockPolicies + blockPoliciesCount),"));
	}
	if (!families.empty()) {
		t.line(StringView("\t\t.families = SpanView<flow::CompiledFamilyIdentity>(families, families + familiesCount),"));
	}
	t.line(StringView("\t};"));
	t.nl();

	// (begin, end) and not (pointer, count): under -fms-compatibility a constant zero is a null
	// pointer, so a count of 0 makes (pointer, size_t) and (begin, end) equally good and the call
	// ambiguous on the Windows target. A pointer arithmetic form has one reading everywhere.
	t.line(StringView("\tstatic flow::CompiledTables tables() {"));
	t.line(StringView("\t\tflow::CompiledTables t;"));
	t.line(StringView("\t\tt.nodes = SpanView<flow::RuntimeNode>(nodes, nodes + nodesCount);"));
	t.line(StringView("\t\tt.opNames = SpanView<StringView>(opNames, opNames + opNamesCount);"));
	t.line(StringView("\t\tt.dataEdges = SpanView<flow::RuntimeDataEdge>(dataEdges, dataEdges + dataEdgesCount);"));
	t.line(StringView("\t\tt.execEdges = SpanView<flow::RuntimeExecEdge>(execEdges, execEdges + execEdgesCount);"));
	t.line(StringView("\t\tt.dataIn = SpanView<uint32_t>(dataIn, dataIn + dataInCount);"));
	t.line(StringView("\t\tt.dataOut = SpanView<uint32_t>(dataOut, dataOut + dataOutCount);"));
	t.line(StringView("\t\tt.execIn = SpanView<uint32_t>(execIn, execIn + execInCount);"));
	t.line(StringView("\t\tt.execOut = SpanView<uint32_t>(execOut, execOut + execOutCount);"));
	t.line(StringView("\t\tt.crossScopeIn = SpanView<uint32_t>(crossScopeIn, crossScopeIn + crossScopeInCount);"));
	t.line(StringView("\t\tt.entries = SpanView<uint32_t>(entries, entries + entriesCount);"));
	t.line(StringView("\t\tt.terminals = SpanView<uint32_t>(terminals, terminals + terminalsCount);"));
	t.line(StringView("\t\tt.scopes = SpanView<flow::RuntimeScope>(scopes, scopes + scopesCount);"));
	t.line(StringView("\t\tt.scopeNodes = SpanView<uint32_t>(scopeNodes, scopeNodes + scopeNodesCount);"));
	if (hasBlocks) {
		t.line(StringView("\t\tt.blocks = SpanView<flow::RuntimeBlock>(blocks, blocks + blocksCount);"));
		t.line(StringView("\t\tt.blockWrites = SpanView<flow::RuntimeBlockWrite>(blockWrites, blockWrites + blockWritesCount);"));
		t.line(StringView("\t\tt.blockCollectors = SpanView<uint32_t>(blockCollectors, blockCollectors + blockCollectorsCount);"));
		t.line(
				StringView("\t\tt.blockQuery = SpanView<flow::value::TypeId>(blockQuery, blockQuery + "
						   "blockQueryCount);"));
	}
	if (settingCount > 0) {
		t.line(StringView("\t\tt.settings = BytesView(settings, size_t(settingsSize));"));
	}
	t.line(StringView("\t\tt.constants = BytesView(constants, size_t(constantsSize));"));
	t.line(StringView("\t\tt.asset = BytesView(asset, size_t(assetSize));"));
	t.line(StringView("\t\tt.sceneContract = SpanView<flow::SceneBinding>(sceneContract, sceneContract + sceneContractCount);"));
	t.line(StringView("\t\tt.extensionContract = SpanView<flow::ExtensionBinding>(extensionContract, extensionContract + extensionContractCount);"));
	t.line(StringView("\t\tt.extensionDecls = SpanView<flow::CompiledExtensionDecl>(extensionDecls, extensionDecls + extensionDeclsCount);"));
	if (!namedRows.empty()) {
		t.line(StringView("\t\tt.namedContract = SpanView<flow::NamedBinding>(namedContract, namedContract + namedContractCount);"));
	}
	t.line(StringView("\t\tt.steps = steps();"));
	t.line(mem_std::toString("\t\tt.stepsEnv = ", options.env, "::Tag;"));
	t.line(StringView("\t\tt.identity = identity;"));
	t.line(StringView("\t\treturn t;"));
	t.line(StringView("\t}"));
	t.line(StringView("};"));
	t.nl();
	t.line(StringView("using Graph = flow::StaticGraph<Tables>;"));
	t.nl();
	t.line(StringView("// The run over this unit, per arena kind: the machine the"));
	t.line(StringView("// interpreter is, over these rows and the arena store, and the same behind "
					  "the executor interface."));
	t.line(StringView("// The rows are this unit's and so is the code that performs a node - "
					  "`steps()` above, written"));
	t.line(StringView("// beside them in <name>.gen.cpp, which is why a header without its source "
					  "does not link."));
	t.line(StringView("template <typename A, typename Trace = flow::TraceLog>"));
	t.line(mem_std::toString("using Run = flow::CompiledRunT<A, ", options.env, ", Trace>;"));
	t.nl();
	t.line(StringView("template <typename A>"));
	t.line(mem_std::toString("using Engine = flow::CompiledEngineT<A, ", options.env, ">;"));
	t.nl();
	t.line(StringView("// And the fast mode: the same rows and the same machine over a store that "
					  "keeps"));
	t.line(StringView("// the run's bookkeeping in host memory. `QuietEngine` is that with no "
					  "execution log at all -"));
	t.line(StringView("// what a shipped game holds, and the only one of the four that promises "
					  "nothing about order."));
	t.line(StringView("template <typename A, typename Trace = flow::TraceLog>"));
	t.line(mem_std::toString("using FastRun = flow::CompiledFastRunT<A, ", options.env,
			", Trace>;"));
	t.nl();
	t.line(StringView("template <typename A>"));
	t.line(mem_std::toString("using FastEngine = flow::CompiledFastEngineT<A, ", options.env,
			">;"));
	t.nl();
	t.line(StringView("template <typename A>"));
	t.line(mem_std::toString("using QuietEngine = flow::CompiledQuietEngineT<A, ", options.env,
			">;"));
	t.nl();
	t.line(mem_std::toString("} // namespace ", options.unitNamespace, "::", name));
	t.nl();
	t.line(mem_std::toString("#endif /* ", guard, " */"));

	// The placeholder has to be where it was written; the hash goes in after the sources, because
	// it covers them too.
	auto at = t.out.find(TextHashPlaceholder.data(), 0, TextHashPlaceholder.size());
	if (at == mem_std::String::npos) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
				DiagText(DiagDetail::CodegenIdentityMisplaced));
		return report.getStatus();
	}

	// The source: one instantiation of the loaded graph over the tables, so that a program linking
	// the unit has it once - and the unit's own code: one function per node, a switch over them,
	// and the table of entry points the loader hands the machine.
	// It is here rather than in the header on purpose. The bodies of the operations are the widest
	// thing a unit reaches, and a header that pulled them in would put the whole node library into
	// every file that names a unit - including the manifest of a corpus, which names all of them.
	// In one part or several (EmitOptions::split). Each part performs a range of nodes and exports
	// one entry point for it; the first part dispatches by range and holds the table. The tables
	// are not split - they are constants in the header.
	for (uint32_t part = 0; part < parts; ++part) {
		auto lo = partBegin(part);
		auto hi = part + 1 < parts ? partBegin(part + 1) : nodeCount;

		Text s;
		s.line(mem_std::toString("// Generated from the graph \"", title,
				"\". Do not edit: regenerate it."));
		if (parts > 1) {
			s.line(mem_std::toString("//"));
			s.line(mem_std::toString("// Part ", num(part + 1), " of ", num(parts),
					": the nodes ", num(lo), "..", num(hi == 0 ? 0 : hi - 1),
					". The parts compile in parallel and are linked together; the first holds the "
					"dispatcher"));
			s.line(StringView("// and the table of entry points."));
		}
		s.nl();
		s.line(mem_std::toString("#include \"", name, ".gen.h\""));
		s.nl();
		s.line(StringView("// The door's own bodies: this unit instantiates them over a site of its "
						  "own, which is what makes"));
		s.line(StringView("// a node index a constant inside the step below (StaticContext)."));
		s.line(StringView("#include \"SPFlowContext.hpp\""));
		if (!options.bodyIncludes.empty()) {
			s.nl();
			s.line(StringView("// The operations, as templates over the door (OpDef::inlineName)."));
			for (auto &inc : options.bodyIncludes) {
				s.line(mem_std::toString("#include \"", inc, "\""));
			}
		}
		s.nl();
		s.line(mem_std::toString("namespace STAPPLER_VERSIONIZED ", options.unitNamespace, "::", name,
				" {"));
		s.nl();
		s.line(StringView("// One node each, and the index is the constant: `stepN` is the `step<N>` "
						  "of the plan. Inside it"));
		s.line(StringView("// the edge feeding a pin, the constant behind it and the offsets of the "
						  "node's state and record"));
		s.line(StringView("// are decided by the compiler, and the operation is called by name where "
						  "it has an inline form."));
		s.line(StringView("// Everything else - the descriptor, the schema, the bindings, the decoded "
						  "constants - is resolved"));
		s.line(StringView("// when the unit is loaded, and read exactly as the interpreter reads it."));
		for (uint32_t n = lo; n < hi; ++n) {
			auto &rt = g.getNodeAt(n);
			auto inlineName = rt.op ? rt.op->getInlineName() : StringView();
			s.nl();
			s.line(mem_std::toString("// node ", num(n), ": ",
					rt.op ? rt.op->getName() : StringView(),
					inlineName.empty() ? StringView(" (through the registry: no inline form)")
									   : StringView()));
			s.line(StringView("template <typename Local>"));
			s.line(mem_std::toString("static Status step", num(n),
					"(flow::CompiledStepSiteT<Local> &site) {"));
			s.line(mem_std::toString("\tflow::StaticContext<Tables, ", num(n), ", Local> ctx;"));
			s.line(StringView("\tctx.bind(site);"));
			if (inlineName.empty()) {
				// The same door, through the registry's function pointer. An operation with no
				// template form is not refused and is not run differently: one indirect call
				// slower.
				s.line(StringView("\tauto invoke = site.op->getInvoke();"));
				s.line(StringView("\tauto st = invoke ? invoke(ctx) : Status::ErrorNotImplemented;"));
			} else {
				s.line(mem_std::toString("\tauto st = ", inlineName, "(ctx);"));
			}
			s.line(StringView("\tctx.finish(site);"));
			s.line(StringView("\treturn st;"));
			s.line(StringView("}"));
		}
		s.nl();

		// The switch this part answers for. Unsplit there is one, it covers everything and it is
		// the dispatcher; split, each part has its own and the first calls them by range.
		auto entry = parts > 1 ? mem_std::toString("stepPart", num(part)) : mem_std::String("step");
		if (parts > 1) {
			s.line(mem_std::toString("// The nodes this part performs. Declared in the header so "
									 "that the dispatcher can call it"));
			s.line(StringView("// from another translation unit, and instantiated below for every "
							  "store a run may live in."));
			s.line(StringView("template <typename Local>"));
			s.line(mem_std::toString("Status ", entry, "(flow::CompiledStepSiteT<Local> &site) {"));
		} else {
			s.line(StringView("// The switch the machine enters, and the only thing in the unit that "
							  "is not about one node."));
			s.line(StringView("template <typename Local>"));
			s.line(mem_std::toString("static Status ", entry,
					"(flow::CompiledStepSiteT<Local> &site) {"));
		}
		s.line(StringView("\tswitch (site.node) {"));
		for (uint32_t n = lo; n < hi; ++n) {
			s.line(mem_std::toString("\tcase ", num(n), ": return step", num(n), "<Local>(site);"));
		}
		s.line(StringView("\tdefault: break;"));
		s.line(StringView("\t}"));
		s.line(StringView("\t// A node index this part does not hold. The machine never asks for "
						  "one; a unit that answered"));
		s.line(StringView("\t// something for it would be answering for a graph it is not."));
		s.line(StringView("\treturn Status::ErrorNotImplemented;"));
		s.line(StringView("}"));

		// The six instantiations, so that the dispatcher in the first part links against them.
		auto locals = [&](const Callback<void(StringView)> &each) {
			static constexpr StringView stores[] = {
				StringView("flow::CompiledLocalT<flow::value::PlainArena, "),
				StringView("flow::CompiledLocalT<flow::value::TrackedArena, "),
				StringView("flow::CompiledLocalT<flow::value::ShadowArena, "),
				StringView("flow::CompiledFastLocalT<flow::value::PlainArena, "),
				StringView("flow::CompiledFastLocalT<flow::value::TrackedArena, "),
				StringView("flow::CompiledFastLocalT<flow::value::ShadowArena, "),
			};
			for (auto &store : stores) {
				auto local = mem_std::toString(store, options.env, ">");
				each(local);
			}
		};
		if (parts > 1) {
			s.nl();
			locals([&](StringView local) {
				s.line(mem_std::toString("template Status ", entry, "<", local,
						">(flow::CompiledStepSiteT<", local, "> &);"));
			});
		}

		if (part == 0) {
			if (parts > 1) {
				s.nl();
				s.line(StringView("// The dispatcher, by range: the parts are contiguous and in "
								  "order, so finding which one"));
				s.line(StringView("// holds a node is a comparison rather than a second switch over "
								  "every index."));
				s.line(StringView("template <typename Local>"));
				s.line(StringView("static Status step(flow::CompiledStepSiteT<Local> &site) {"));
				for (uint32_t k = 0; k + 1 < parts; ++k) {
					s.line(mem_std::toString("\tif (site.node < ", num(partBegin(k + 1)),
							"u) { return stepPart", num(k), "<Local>(site); }"));
				}
				s.line(mem_std::toString("\treturn stepPart", num(parts - 1), "<Local>(site);"));
				s.line(StringView("}"));
			}
			s.nl();
			s.line(StringView("// One entry per store a run may live in: the arena store and the "
							  "fast store, each over the three"));
			s.line(StringView("// arena kinds a scene is ever in. In a release build ShadowArena is "
							  "TrackedArena, so two slots of"));
			s.line(StringView("// each pair name one function - which is what the loader would have "
							  "handed out anyway."));
			s.line(mem_std::toString("const flow::CompiledStepsT<", options.env, "> *steps() {"));
			s.line(mem_std::toString("\tstatic constexpr flow::CompiledStepsT<", options.env,
					"> table = {"));
			s.line(mem_std::toString("\t\t.plain = &step<flow::CompiledLocalT<flow::value::PlainArena, ",
					options.env, ">>,"));
			s.line(mem_std::toString(
					"\t\t.tracked = &step<flow::CompiledLocalT<flow::value::TrackedArena, ", options.env,
					">>,"));
			s.line(mem_std::toString(
					"\t\t.shadow = &step<flow::CompiledLocalT<flow::value::ShadowArena, ", options.env,
					">>,"));
			s.line(mem_std::toString(
					"\t\t.fastPlain = &step<flow::CompiledFastLocalT<flow::value::PlainArena, ",
					options.env, ">>,"));
			s.line(mem_std::toString(
					"\t\t.fastTracked = &step<flow::CompiledFastLocalT<flow::value::TrackedArena, ",
					options.env, ">>,"));
			s.line(mem_std::toString(
					"\t\t.fastShadow = &step<flow::CompiledFastLocalT<flow::value::ShadowArena, ",
					options.env, ">>,"));
			s.line(StringView("\t};"));
			s.line(StringView("\treturn &table;"));
			s.line(StringView("}"));
		}
		s.nl();
		s.line(mem_std::toString("} // namespace ", options.unitNamespace, "::", name));
		if (part == 0) {
			s.nl();
			s.line(StringView("namespace STAPPLER_VERSIONIZED stappler {"));
			s.nl();
			s.line(mem_std::toString("template class flow::StaticGraph<", relative, "::", name,
					"::Tables>;"));
			s.nl();
			s.line(StringView("} // namespace stappler"));
		}
		out.sources.emplace_back(sprt::move(s.out));
	}

	/* The hash, over the whole unit: the header with the placeholder still in it, and then every
	source in order. A committed unit is proved fresh by regenerating and comparing this one
	number, and it has to cover the code as well as the tables - a change to how a step is
	written moves no table and would otherwise pass a freshness check that only read the header.
	The sources are not hashed into themselves - none of them carries the number - so there is no
	circularity: the header carries it, and the header is hashed with the placeholder in place. */
	auto hash = sprt::hash64(t.out.data(), t.out.size());
	for (auto &src : out.sources) {
		hash = sprt::hash64(src.data(), src.size(), hash);
	}
	t.out.replace(at, TextHashPlaceholder.size(), mem_std::toString(".textHash = ", hex64(hash)));

	out.header = sprt::move(t.out);
	out.textHash = hash;

	return Status::Ok;
}

} // namespace stappler::flow::codegen
