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


// The function signatures and the linker. Linking is a rewrite of documents into one document: what
// the build is then handed is a graph asset like any other, whose `functions` section holds the
// bodies that run as calls, and whose call sites that run inline are already copies of their bodies.

#include "SPFlowFunction.h"
#include "SPFlowFunctionInline.h"

#include <sprt/cxx/algorithm>

namespace STAPPLER_VERSIONIZED stappler::flow {

bool isFunctionName(StringView name) {
	if (name.empty() || name == "entry" || name == "return") {
		return false;
	}
	for (auto c : name) {
		if (c == ':' || c == '#') {
			return false;
		}
	}
	return true;
}

static PinDesc makeFunctionPin(const FunctionPin &pin, bool input) {
	PinDesc out;
	out.name = pin.name;
	out.type = pin.type;
	out.element = pin.element;
	out.subtypeId = pin.subtype.empty() ? value::NullTypeId : value::makeTypeId(pin.subtype);
	if (input) {
		if (pin.required) {
			out.flags = PinFlags::Required;
		}
		out.def = pin.def;
	}
	return out;
}

void FunctionSignature::init(const FunctionInterface &iface, StringView name, StringView function,
		FunctionOpKind kind, uint32_t exit) {
	_def = OpDef();
	_name = name.str<memory::StandardInterface>();
	_function = function.str<memory::StandardInterface>();
	_in.clear();
	_out.clear();
	_execOut.clear();
	_settings.clear();
	_choices.clear();
	_locals.clear();

	const bool exec = iface.execIn || !iface.execOut.empty();

	// Inputs of the call as the call takes them - with their defaults and the Required flag - and
	// as the body sees them, where an entry hands them out and a substituted body's arg takes them
	// in the call's place.
	auto inputsIn = [&](bool required) {
		for (auto &p : iface.inputs) {
			auto pin = makeFunctionPin(p, true);
			if (!required) {
				pin.flags = PinFlags::None;
			}
			_in.emplace_back(sprt::move(pin));
		}
	};
	auto inputsOut = [&]() {
		for (auto &p : iface.inputs) { _out.emplace_back(makeFunctionPin(p, false)); }
	};
	auto outputsIn = [&]() {
		for (auto &p : iface.outputs) { _in.emplace_back(makeFunctionPin(p, false)); }
	};
	auto outputsOut = [&]() {
		for (auto &p : iface.outputs) { _out.emplace_back(makeFunctionPin(p, false)); }
	};
	auto execOutputs = [&]() {
		for (auto &e : iface.execOut) { _execOut.emplace_back(e); }
	};
	auto exitSetting = [&]() {
		if (iface.execOut.empty()) {
			return;
		}
		for (auto &e : iface.execOut) { _choices.emplace_back(e); }
		SettingDesc setting;
		setting.name = FunctionExitSetting;
		setting.type = VarType::String;
		setting.role = SettingRole::Choice;
		setting.choices = SpanView<StringView>(_choices.data(), _choices.size());
		_settings.emplace_back(sprt::move(setting));
	};

	switch (kind) {
	case FunctionOpKind::Call: {
		inputsIn(true);
		outputsOut();
		_def.hasExecIn = iface.execIn;
		execOutputs();
		_def.execExclusive = !iface.execOut.empty();

		_choices.emplace_back(FunctionModeDefault);
		_choices.emplace_back(getFunctionModeName(FunctionMode::Call));
		_choices.emplace_back(getFunctionModeName(FunctionMode::Inline));
		SettingDesc mode;
		mode.name = FunctionModeSetting;
		mode.type = VarType::String;
		mode.role = SettingRole::Choice;
		mode.choices = SpanView<StringView>(_choices.data(), _choices.size());
		_settings.emplace_back(sprt::move(mode));

		_locals.emplace_back(value::FieldDef{.name = StringView("phase"), .type = VarType::Int});
		_locals.emplace_back(value::FieldDef{.name = StringView("exit"), .type = VarType::Int});
		_def.invoke = &fn::call<OpContext>;
		_def.inlineName = StringView("flow::fn::call");
		_def.functionRole = FunctionRole::Call;
		break;
	}
	case FunctionOpKind::SourceEntry:
	case FunctionOpKind::Entry:
		// The machine copies the arguments into the linked entry's inputs; a source entry only
		// hands them out.
		if (kind == FunctionOpKind::Entry) {
			inputsIn(false);
		}
		inputsOut();
		if (iface.execIn) {
			_execOut.emplace_back(FunctionStartPin);
		}
		_def.invoke = &fn::entry<OpContext>;
		_def.inlineName = StringView("flow::fn::entry");
		_def.functionRole = FunctionRole::Entry;
		break;
	case FunctionOpKind::SourceReturn:
	case FunctionOpKind::Return:
		outputsIn();
		_def.hasExecIn = iface.execIn;
		if (kind == FunctionOpKind::SourceReturn) {
			exitSetting();
		}
		_def.invoke = &fn::ret<OpContext>;
		_def.inlineName = StringView("flow::fn::ret");
		_def.functionRole = FunctionRole::Return;
		_def.functionExit = exit;
		break;
	case FunctionOpKind::Arg:
		inputsIn(true);
		inputsOut();
		_def.hasExecIn = iface.execIn;
		if (iface.execIn) {
			_execOut.emplace_back(FunctionStartPin);
		}
		_def.parallel = exec ? OpParallel::Flow : OpParallel::Pure;
		_def.invoke = &fn::arg<OpContext>;
		_def.inlineName = StringView("flow::fn::arg");
		_def.functionRole = FunctionRole::Arg;
		break;
	case FunctionOpKind::Result:
		outputsIn();
		outputsOut();
		_def.hasExecIn = iface.execIn;
		execOutputs();
		_def.execExclusive = !iface.execOut.empty();
		_def.parallel = exec ? OpParallel::Flow : OpParallel::Pure;
		_def.invoke = &fn::result<OpContext>;
		_def.inlineName = StringView("flow::fn::result");
		_def.functionRole = FunctionRole::Result;
		_def.functionExit = exit;
		break;
	}

	_def.name = _name;
	_def.function = _function;
	_def.dataIn = SpanView<PinDesc>(_in.data(), _in.size());
	_def.dataOut = SpanView<PinDesc>(_out.data(), _out.size());
	_def.execOut = SpanView<StringView>(_execOut.data(), _execOut.size());
	_def.settings = SpanView<SettingDesc>(_settings.data(), _settings.size());
	_def.locals = SpanView<value::FieldDef>(_locals.data(), _locals.size());
}

uint64_t getFunctionCallHash(const FunctionInterface &iface, StringView name) {
	auto opName = mem_std::toString(GraphAsset::FunctionOpPrefix, name);
	FunctionSignature sig;
	sig.init(iface, opName, name, FunctionOpKind::Call);

	auto pool = memory::pool::create();
	uint64_t hash = 0;
	{
		DiagReport report;
		OpDesc desc;
		if (desc.build(sig.getDef(), pool, report) == Status::Ok) {
			hash = desc.getSignatureHash();
		}
	}
	memory::pool::destroy(pool);
	return hash;
}

GraphLink::~GraphLink() {
	clear();
	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool GraphLink::init(memory::pool_t *parent) {
	if (_pool) {
		return false;
	}
	_pool = memory::pool::create(parent);
	_ownsPool = true;
	return _pool != nullptr;
}

void GraphLink::clear() {
	if (_asset) {
		delete _asset;
		_asset = nullptr;
	}
	if (_ops) {
		delete _ops;
		_ops = nullptr;
	}
	_originIds.clear();
	_origins.clear();
	_callees.clear();
	_linked = false;
}

bool GraphLink::needsLink(const GraphAsset &asset) {
	if (asset.hasInterface() || !asset.getFunctions().empty()) {
		return true;
	}
	for (auto &n : asset.getNodes()) {
		if (n.op.starts_with(GraphAsset::FunctionOpPrefix)) {
			return true;
		}
	}
	return false;
}

const LinkOrigin *GraphLink::getOrigin(NodeId id) const {
	auto it = sprt::lower_bound(_originIds.begin(), _originIds.end(), id);
	if (it == _originIds.end() || *it != id) {
		return nullptr;
	}
	return &_origins[it - _originIds.begin()];
}

// One body to emit: the root body of a document or one of its functions.
struct LinkBody {
	const GraphAsset *doc = nullptr;
	const GraphFunction *fn = nullptr;
	const FunctionInterface *iface = nullptr;
	StringView key; // the linked name; empty for the root of the link
	StringView source; // the name an author calls it by

	SpanView<GraphNode> nodes() const { return fn ? SpanView<GraphNode>(fn->nodes) : doc->getNodes(); }
	SpanView<GraphEdge> edges() const { return fn ? SpanView<GraphEdge>(fn->edges) : doc->getEdges(); }
	bool same(const LinkBody &other) const { return doc == other.doc && fn == other.fn; }
};

// Where an emitted body starts and ends, for the call site a substituted body replaces.
struct LinkPorts {
	NodeId entry = NullNodeId;
	mem_std::Vector<sprt::pair<NodeId, uint32_t>> returns; // linked id, exit
};

struct Linker {
	struct Called {
		LinkBody body;
		bool keep = false;
		mem_std::Value nodes;
		mem_std::Value edges;
	};

	struct InlineSite {
		NodeId linked = NullNodeId;
		const FunctionInterface *iface = nullptr;
		LinkPorts ports;
	};

	GraphLink &out;
	const GraphAsset &root;
	const FunctionHost *host = nullptr;
	DiagReport &report;

	NodeId nextId = 1;
	uint32_t nodeCount = 0;
	mem_std::Vector<Called> called;
	mem_std::Vector<StringView> inlineStack;
	mem_std::Vector<const GraphAsset *> docs;
	mem_std::Vector<sprt::pair<const GraphAsset *, const GraphFunction *>> checked;
	mem_std::Vector<sprt::pair<NodeId, LinkOrigin>> origins;

	Linker(GraphLink &o, const GraphAsset &r, const FunctionHost *h, DiagReport &rep)
	: out(o), root(r), host(h), report(rep) { }

	StringView intern(StringView s) const { return s.pdup(out._pool); }

	void addDoc(const GraphAsset *doc) {
		for (auto d : docs) {
			if (d == doc) {
				return;
			}
		}
		docs.emplace_back(doc);
	}

	// A function by the name an author called it, from inside `doc`: the document's own first, the
	// library after it.
	bool resolve(const GraphAsset *doc, StringView name, LinkBody &body) {
		if (!isFunctionName(name)) {
			return false;
		}
		if (auto fn = doc->getFunction(name)) {
			body.doc = doc;
			body.fn = fn;
			body.iface = &fn->iface;
			body.key = intern(mem_std::toString(doc->getName(), ":", name));
			body.source = fn->name;
			return true;
		}
		if (host) {
			if (auto lib = host->findFunction(name); lib && lib->hasInterface()) {
				body.doc = lib;
				body.fn = nullptr;
				body.iface = &lib->getInterface();
				body.key = intern(name);
				body.source = body.key;
				return true;
			}
		}
		return false;
	}

	bool registerOp(const LinkBody &body, StringView name, FunctionOpKind kind, uint32_t exit,
			NodeId at) {
		if (out._ops->get(name)) {
			return true;
		}
		FunctionSignature sig;
		sig.init(*body.iface, name, body.key, kind, exit);
		if (!out._ops->createNative(sig.getDef(), report.getSink())) {
			report.reportNode(DiagSeverity::Error, DiagCode::FunctionInterfaceInvalid, at,
					DiagText(DiagDetail::FunctionInterfaceInvalid).name(body.source));
			return false;
		}
		return true;
	}

	StringView opName(const LinkBody &body, StringView suffix = StringView()) const {
		return intern(mem_std::toString(GraphAsset::FunctionOpPrefix, body.key, suffix));
	}

	// Queues a body to be emitted once as a called body, and registers its call operation.
	bool callBody(const LinkBody &body, NodeId at) {
		if (!registerOp(body, opName(body), FunctionOpKind::Call, 0, at)) {
			return false;
		}
		for (auto &it : called) {
			if (it.body.key == body.key) {
				return true;
			}
		}
		Called c;
		c.body = body;
		// The document's own functions keep the ids written in it; a library body takes fresh ones.
		c.keep = body.doc == &root && body.fn != nullptr;
		c.nodes = mem_std::Value(mem_std::Value::Type::ARRAY);
		c.edges = mem_std::Value(mem_std::Value::Type::ARRAY);
		called.emplace_back(sprt::move(c));
		return true;
	}

	// What makes a body a body, said once per body as written however many times it is emitted:
	// exactly one entry, nothing wired into it, and a return wherever there is something to return.
	void checkBoundary(const LinkBody &body, SpanView<sprt::pair<NodeId, NodeId>> ids) {
		for (auto &it : checked) {
			if (it.first == body.doc && it.second == body.fn) {
				return;
			}
		}
		checked.emplace_back(body.doc, body.fn);

		auto linkedOf = [&](NodeId id) {
			for (auto &it : ids) {
				if (it.first == id) {
					return it.second;
				}
			}
			return NullNodeId;
		};

		uint32_t entries = 0;
		uint32_t returns = 0;
		NodeId firstEntry = NullNodeId;
		for (auto &n : body.nodes()) {
			if (n.op == FunctionEntryOp) {
				if (++entries == 2) {
					report.reportNode(DiagSeverity::Error, DiagCode::FunctionBoundary,
							linkedOf(n.id), DiagText(DiagDetail::FunctionEntryTwice));
				}
				if (firstEntry == NullNodeId) {
					firstEntry = n.id;
				}
			} else if (n.op == FunctionReturnOp) {
				++returns;
			}
		}
		for (auto &e : body.edges()) {
			if (e.to == firstEntry && firstEntry != NullNodeId) {
				report.reportEdge(DiagSeverity::Error, DiagCode::FunctionBoundary, linkedOf(e.from),
						e.fromPin, linkedOf(e.to), e.toPin, DiagText(DiagDetail::FunctionEntryWired));
			}
		}

		auto &iface = *body.iface;
		const bool needsReturn = !iface.outputs.empty() || !iface.execOut.empty();
		if (entries == 0) {
			report.reportAt(DiagSeverity::Error, DiagCode::FunctionBoundary,
					DiagText(DiagDetail::FunctionEntryMissing).name(body.source), DiagLocus::Op,
					SpanView<int64_t>(), SpanView<StringView>(&body.source, 1));
		}
		if (needsReturn && returns == 0) {
			report.reportAt(DiagSeverity::Error, DiagCode::FunctionBoundary,
					DiagText(DiagDetail::FunctionReturnMissing).name(body.source), DiagLocus::Op,
					SpanView<int64_t>(), SpanView<StringView>(&body.source, 1));
		}
		if (!iface.execIn && returns > 1) {
			report.reportAt(DiagSeverity::Error, DiagCode::FunctionBoundary,
					DiagText(DiagDetail::FunctionReturnPure).name(body.source), DiagLocus::Op,
					SpanView<int64_t>(), SpanView<StringView>(&body.source, 1));
		}
	}

	static mem_std::Value settingsWithout(const mem_std::Value &settings, StringView key) {
		mem_std::Value out;
		if (!settings.isDictionary()) {
			return out;
		}
		for (auto &it : settings.asDict()) {
			if (StringView(it.first) != key) {
				out.setValue(it.second, it.first);
			}
		}
		return out;
	}

	// Emits one body. `keep`: the ids stay as written. `callSite`: the call node a substituted copy
	// stands for, with the call's parameters, which its arg takes in the call's place.
	void emitBody(const LinkBody &body, bool keep, bool substituted, NodeId callSite,
			const mem_std::Value *callParams, mem_std::Value &nodes, mem_std::Value &edges,
			LinkPorts &ports) {
		addDoc(body.doc);

		mem_std::Vector<sprt::pair<NodeId, NodeId>> ids; // source id, linked id
		ids.reserve(body.nodes().size());
		for (auto &n : body.nodes()) {
			auto linked = keep ? n.id : nextId++;
			ids.emplace_back(n.id, linked);
			LinkOrigin origin;
			origin.document = intern(body.doc->getName());
			origin.function = body.fn ? intern(body.fn->name) : StringView();
			origin.source = n.id;
			origin.callSite = callSite;
			origins.emplace_back(linked, origin);
		}
		nodeCount += uint32_t(ids.size());

		auto linkedOf = [&](NodeId id) {
			for (auto &it : ids) {
				if (it.first == id) {
					return it.second;
				}
			}
			return NullNodeId;
		};

		if (body.iface) {
			checkBoundary(body, ids);
		}

		mem_std::Vector<sprt::pair<NodeId, InlineSite>> sites; // source id of the call node

		for (auto &n : body.nodes()) {
			auto linked = linkedOf(n.id);
			mem_std::Value node(mem_std::Value::Type::DICTIONARY);
			node.setInteger(int64_t(linked), "id");
			if (n.meta.isDictionary() && n.meta.size() > 0) {
				node.setValue(n.meta, "meta");
			}

			if (n.op == FunctionEntryOp || n.op == FunctionReturnOp) {
				if (!body.iface) {
					report.reportNode(DiagSeverity::Error, DiagCode::FunctionBoundary, linked,
							DiagText(DiagDetail::FunctionBoundaryOutside).name(n.op));
					continue;
				}
				if (n.op == FunctionEntryOp) {
					auto kind = substituted ? FunctionOpKind::Arg : FunctionOpKind::Entry;
					auto name = opName(body, substituted ? StringView("#arg") : StringView("#entry"));
					if (!registerOp(body, name, kind, 0, linked)) {
						continue;
					}
					node.setString(name, "op");
					if (substituted) {
						if (callParams && callParams->isDictionary() && callParams->size() > 0) {
							node.setValue(*callParams, "params");
						}
					} else if (n.params.isDictionary() && n.params.size() > 0) {
						node.setValue(n.params, "params");
					}
					if (ports.entry == NullNodeId) {
						ports.entry = linked;
					}
				} else {
					uint32_t exit = 0;
					auto &exitValue = n.settings.getValue(FunctionExitSetting);
					if (exitValue.isString()) {
						auto &names = body.iface->execOut;
						bool found = false;
						for (uint32_t i = 0; i < uint32_t(names.size()); ++i) {
							if (names[i] == StringView(exitValue.getString())) {
								exit = i;
								found = true;
							}
						}
						if (!found) {
							report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid,
									linked, FunctionExitSetting,
									DiagText(DiagDetail::FunctionExitUnknown)
											.name(exitValue.getString()));
							continue;
						}
					} else if (!exitValue.isNull()) {
						report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid, linked,
								FunctionExitSetting,
								DiagText(DiagDetail::FunctionExitUnknown).name(StringView()));
						continue;
					}
					auto kind = substituted ? FunctionOpKind::Result : FunctionOpKind::Return;
					auto name = opName(body,
							mem_std::toString(substituted ? "#result:" : "#return:", exit));
					if (!registerOp(body, name, kind, exit, linked)) {
						continue;
					}
					node.setString(name, "op");
					if (n.params.isDictionary() && n.params.size() > 0) {
						node.setValue(n.params, "params");
					}
					auto rest = settingsWithout(n.settings, FunctionExitSetting);
					if (rest.isDictionary() && rest.size() > 0) {
						node.setValue(rest, "settings");
					}
					ports.returns.emplace_back(linked, exit);
				}
				nodes.addValue(sprt::move(node));
				continue;
			}

			if (n.op.starts_with(GraphAsset::FunctionOpPrefix)) {
				auto name = n.op.sub(GraphAsset::FunctionOpPrefix.size());
				LinkBody callee;
				if (!resolve(body.doc, name, callee)) {
					report.reportNode(DiagSeverity::Error, DiagCode::FunctionUnknown, linked,
							DiagText(DiagDetail::FunctionUnknown).name(name));
					continue;
				}

				if (n.opHash != 0 && n.opHash != getFunctionCallHash(*callee.iface, name)) {
					report.reportNode(DiagSeverity::Warning, DiagCode::SignatureDrift, linked,
							DiagText(DiagDetail::SignatureDrift).name(n.op));
				}

				auto mode = callee.iface->mode;
				auto &modeValue = n.settings.getValue(FunctionModeSetting);
				if (modeValue.isString()) {
					StringView m(modeValue.getString());
					if (m != FunctionModeDefault && !readFunctionMode(m, mode)) {
						report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid, linked,
								FunctionModeSetting, DiagText(DiagDetail::SettingInvalid)
										.name(FunctionModeSetting)
										.phrase(DiagPhrase::SettingValueNotChoice));
						continue;
					}
				} else if (!modeValue.isNull()) {
					report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid, linked,
							FunctionModeSetting,
							DiagText(DiagDetail::SettingInvalid)
									.name(FunctionModeSetting)
									.phrase(DiagPhrase::SettingValueType));
					continue;
				}

				if (mode == FunctionMode::Call) {
					if (!callBody(callee, linked)) {
						continue;
					}
					node.setString(opName(callee), "op");
					if (n.params.isDictionary() && n.params.size() > 0) {
						node.setValue(n.params, "params");
					}
					if (n.settings.isDictionary() && n.settings.size() > 0) {
						node.setValue(n.settings, "settings");
					}
					nodes.addValue(sprt::move(node));
					continue;
				}

				bool cycle = false;
				for (auto &k : inlineStack) {
					if (k == callee.key) {
						cycle = true;
					}
				}
				if (cycle) {
					report.reportNode(DiagSeverity::Error, DiagCode::FunctionInlineCycle, linked,
							DiagText(DiagDetail::FunctionInlineCycle).name(name));
					continue;
				}

				InlineSite site;
				site.linked = linked;
				site.iface = callee.iface;
				inlineStack.emplace_back(callee.key);
				emitBody(callee, false, true, linked, &n.params, nodes, edges, site.ports);
				inlineStack.pop_back();

				if (site.ports.returns.size() > 1 && !callee.iface->outputs.empty()) {
					report.reportNode(DiagSeverity::Error, DiagCode::FunctionInlineMultiReturn,
							linked, DiagText(DiagDetail::FunctionInlineMultiReturn).name(name));
					continue;
				}
				sites.emplace_back(n.id, sprt::move(site));
				continue;
			}

			node.setString(n.op, "op");
			if (n.opHash != 0) {
				node.setInteger(int64_t(n.opHash), "opHash");
			}
			if (n.params.isDictionary() && n.params.size() > 0) {
				node.setValue(n.params, "params");
			}
			if (n.settings.isDictionary() && n.settings.size() > 0) {
				node.setValue(n.settings, "settings");
			}
			nodes.addValue(sprt::move(node));
		}

		auto siteOf = [&](NodeId id) -> const InlineSite * {
			for (auto &it : sites) {
				if (it.first == id) {
					return &it.second;
				}
			}
			return nullptr;
		};

		auto emitEdge = [&](EdgeKind kind, NodeId from, StringView fromPin, NodeId to,
								StringView toPin) {
			if (from == NullNodeId || to == NullNodeId) {
				return;
			}
			mem_std::Value edge(mem_std::Value::Type::DICTIONARY);
			edge.setString(getEdgeKindName(kind), "kind");
			edge.setInteger(int64_t(from), "from");
			edge.setString(fromPin, "fromPin");
			edge.setInteger(int64_t(to), "to");
			if (kind == EdgeKind::Data) {
				edge.setString(toPin, "toPin");
			}
			edges.addValue(sprt::move(edge));
		};

		for (auto &e : body.edges()) {
			auto from = linkedOf(e.from);
			auto to = linkedOf(e.to);
			if (from == NullNodeId || to == NullNodeId) {
				// An id of another body, or of none: a body is wired inside itself only.
				report.reportEdge(DiagSeverity::Error, DiagCode::UnknownNode, from ? from : e.from,
						e.fromPin, to ? to : e.to, e.toPin, DiagText(DiagDetail::EdgeUnknownNode));
				continue;
			}

			// Every source the edge stands for: the node itself, or the returns of a substituted
			// body that leave through the exit this edge leaves the call by.
			mem_std::Vector<NodeId> sources;
			if (auto site = siteOf(e.from)) {
				if (e.kind == EdgeKind::Data) {
					if (!site->ports.returns.empty()) {
						sources.emplace_back(site->ports.returns.front().first);
					}
				} else {
					auto &names = site->iface->execOut;
					for (uint32_t k = 0; k < uint32_t(names.size()); ++k) {
						if (names[k] != e.fromPin) {
							continue;
						}
						for (auto &r : site->ports.returns) {
							if (r.second == k) {
								sources.emplace_back(r.first);
							}
						}
					}
				}
			} else {
				sources.emplace_back(from);
			}

			auto target = to;
			if (auto site = siteOf(e.to)) {
				target = site->ports.entry;
			}

			for (auto src : sources) { emitEdge(e.kind, src, e.fromPin, target, e.toPin); }
		}
	}

	void mergeDecls(mem_std::Value &out) {
		// A scene contract is checked only when every document states one: a document that does
		// not say leaves the linked graph deriving its contract, as that document alone would.
		bool allDeclare = true;
		for (auto doc : docs) {
			if (doc->getScene().empty()) {
				allDeclare = false;
			}
		}
		if (allDeclare) {
			mem_std::Vector<SceneDecl> merged;
			for (auto doc : docs) {
				for (auto &decl : doc->getScene()) {
					SceneDecl *target = nullptr;
					for (auto &m : merged) {
						if (m.component == decl.component) {
							target = &m;
						}
					}
					if (!target) {
						merged.emplace_back(decl);
						continue;
					}
					for (auto &f : decl.fields) {
						bool found = false;
						for (auto &mf : target->fields) {
							if (mf.name == f.name) {
								found = true;
								if (mf.type != f.type) {
									report.report(DiagSeverity::Error,
											DiagCode::FunctionDeclConflict,
											DiagText(DiagDetail::FunctionSceneConflict)
													.name(decl.component)
													.name(f.name));
								}
							}
						}
						if (!found) {
							target->fields.emplace_back(f);
						}
					}
				}
			}
			if (!merged.empty()) {
				auto &scene = out.newArray("scene");
				for (auto &decl : merged) {
					mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
					entry.setString(decl.component, "component");
					if (!decl.fields.empty()) {
						auto &fields = entry.newArray("fields");
						for (auto &f : decl.fields) {
							mem_std::Value field(mem_std::Value::Type::DICTIONARY);
							field.setString(f.name, "name");
							field.setString(value::getVarTypeName(f.type), "type");
							fields.addValue(sprt::move(field));
						}
					}
					scene.addValue(sprt::move(entry));
				}
			}
		}

		mem_std::Vector<const ExtensionDecl *> extensions;
		for (auto doc : docs) {
			for (auto &decl : doc->getExtensions()) {
				const ExtensionDecl *existing = nullptr;
				for (auto e : extensions) {
					if (e->id == decl.id) {
						existing = e;
					}
				}
				if (!existing) {
					extensions.emplace_back(&decl);
				} else if (existing->name != decl.name || existing->params != decl.params) {
					report.report(DiagSeverity::Error, DiagCode::FunctionDeclConflict,
							DiagText(DiagDetail::FunctionExtensionConflict).name(decl.id));
				}
			}
		}
		if (!extensions.empty()) {
			auto &list = out.newArray("extensions");
			for (auto decl : extensions) {
				mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
				entry.setString(decl->name, "name");
				entry.setString(decl->id, "id");
				if (decl->params.isDictionary() && decl->params.size() > 0) {
					entry.setValue(decl->params, "params");
				}
				list.addValue(sprt::move(entry));
			}
		}
	}

	Status run() {
		NodeId maxId = 0;
		for (auto &n : root.getNodes()) { maxId = sprt::max(maxId, n.id); }
		for (auto &f : root.getFunctions()) {
			for (auto &n : f.nodes) { maxId = sprt::max(maxId, n.id); }
		}
		nextId = maxId + 1;

		mem_std::Value linked(mem_std::Value::Type::DICTIONARY);
		mem_std::Value envelope(mem_std::Value::Type::DICTIONARY);
		envelope.setString(GraphAsset::Kind, "kind");
		envelope.setInteger(int64_t(GraphAsset::FunctionFormatVersion), "version");
		if (!root.getGenerator().empty()) {
			envelope.setString(root.getGenerator(), "generator");
		}
		linked.setValue(sprt::move(envelope), GraphAsset::MetaKey);
		if (!root.getName().empty()) {
			linked.setString(root.getName(), "name");
		}
		if (root.getMeta().isDictionary() && root.getMeta().size() > 0) {
			linked.setValue(root.getMeta(), "meta");
		}

		// A document that is a function runs as one when it is the root of a run: its entry hands
		// out its own parameters, and its returns have no caller to hand anything to.
		LinkBody program;
		program.doc = &root;
		program.iface = root.hasInterface() ? &root.getInterface() : nullptr;
		program.key = intern(root.getName());
		program.source = program.key;

		auto &nodes = linked.newArray("nodes");
		auto &edges = linked.newArray("edges");
		LinkPorts ports;
		emitBody(program, true, false, NullNodeId, nullptr, nodes, edges, ports);

		// Every function the document defines is linked whether called or not, so that one nobody
		// calls yet is still checked.
		for (auto &f : root.getFunctions()) {
			LinkBody body;
			if (resolve(&root, f.name, body)) {
				callBody(body, NullNodeId);
				if (host && host->findFunction(f.name)) {
					auto opName = mem_std::toString(GraphAsset::FunctionOpPrefix, f.name);
					StringView names[] = {StringView(opName)};
					report.reportAt(DiagSeverity::Warning, DiagCode::FunctionShadowed,
							DiagText(DiagDetail::FunctionShadowed).name(f.name), DiagLocus::Op,
							SpanView<int64_t>(), SpanView<StringView>(names, 1));
				}
			} else {
				report.reportAt(DiagSeverity::Error, DiagCode::FunctionInterfaceInvalid,
						DiagText(DiagDetail::FunctionNameInvalid).name(f.name), DiagLocus::None);
			}
		}

		for (uint32_t i = 0; i < uint32_t(called.size()); ++i) {
			// By index: emitting a body may queue more.
			auto body = called[i].body;
			auto keep = called[i].keep;
			mem_std::Value bodyNodes(mem_std::Value::Type::ARRAY);
			mem_std::Value bodyEdges(mem_std::Value::Type::ARRAY);
			LinkPorts bodyPorts;
			emitBody(body, keep, false, NullNodeId, nullptr, bodyNodes, bodyEdges, bodyPorts);
			called[i].nodes = sprt::move(bodyNodes);
			called[i].edges = sprt::move(bodyEdges);
		}

		if (nodeCount > MaxNodesPerGraph) {
			report.report(DiagSeverity::Error, DiagCode::GraphTooLarge,
					DiagText(DiagDetail::TooManyNodes).number(int64_t(MaxNodesPerGraph)));
		}

		if (!called.empty()) {
			auto &functions = linked.newArray("functions");
			for (auto &c : called) {
				mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
				entry.setString(c.body.key, "name");
				saveFunctionInterface(*c.body.iface, entry.emplace("interface"));
				entry.setValue(sprt::move(c.nodes), "nodes");
				entry.setValue(sprt::move(c.edges), "edges");
				functions.addValue(sprt::move(entry));
			}
		}

		mergeDecls(linked);

		sprt::sort(origins.begin(), origins.end(),
				[](const sprt::pair<NodeId, LinkOrigin> &a,
						const sprt::pair<NodeId, LinkOrigin> &b) { return a.first < b.first; });
		for (auto &it : origins) {
			out._originIds.emplace_back(it.first);
			out._origins.emplace_back(it.second);
		}

		for (auto doc : docs) {
			if (doc != &root) {
				out._callees.emplace_back(LinkCallee{intern(doc->getName()), doc->getContentHash()});
			}
		}
		sprt::sort(out._callees.begin(), out._callees.end(),
				[](const LinkCallee &a, const LinkCallee &b) { return a.name < b.name; });

		if (report.hasErrors()) {
			return report.getStatus();
		}

		out._asset = new GraphAsset();
		out._asset->init(out._pool);
		return out._asset->load(linked, report.getSink());
	}
};

Status GraphLink::link(const GraphAsset &asset, const OpRegistry &ops, const FunctionHost *host,
		DiagSink *sink) {
	if (!_pool) {
		return Status::ErrorInvalidArguemnt;
	}
	clear();

	DiagReport report(sink);
	_ops = new OpRegistry();
	_ops->init(&ops, _pool);

	Linker linker(*this, asset, host, report);
	auto st = linker.run();
	if (st != Status::Ok || report.hasErrors()) {
		if (_asset) {
			delete _asset;
			_asset = nullptr;
		}
		return st != Status::Ok ? st : report.getStatus();
	}
	_linked = true;
	return Status::Ok;
}

} // namespace stappler::flow
