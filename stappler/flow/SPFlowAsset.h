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

#ifndef STAPPLER_FLOW_SPFLOWASSET_H_
#define STAPPLER_FLOW_SPFLOWASSET_H_

#include "SPFlow.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

enum class EdgeKind : uint8_t {
	// Carries a value from an output pin to an input pin. A data input takes at most one.
	Data,
	// Carries the token that makes a node run. An exec output emits at most one; an exec input
	// accepts many, and the target's exec input is not named because an operation has at most one.
	Exec,
};

SP_PUBLIC StringView getEdgeKindName(EdgeKind);
SP_PUBLIC bool readEdgeKind(StringView, EdgeKind &);

struct GraphNode {
	NodeId id = NullNodeId;
	StringView op; // the operation's NAME; resolved to a descriptor by the build

	// The operation's signature hash when this asset was written; 0 when the file did not say. A
	// mismatch is a warning naming what moved, not a refusal - the precise errors come from the
	// edges that reference pins which no longer exist.
	uint64_t opHash = 0;

	// Literals for unconnected data inputs, keyed by pin name. Checked against the pin's type by
	// validation, converted once, and never looked at again at run time.
	mem_std::Value params;

	// Values of the operation's declared settings (OpDef::settings), keyed by setting name. Checked
	// and resolved by the build like params; never on an edge.
	mem_std::Value settings;

	// The editor's: canvas position, comment, colour. Not interpreted by validation, by the build
	// or by the interpreter, and carried through a save/load byte for byte. Anything that affects
	// execution belongs in the strict schema instead.
	mem_std::Value meta;
};

/* The scene contract is not declared here: `SceneDecl` and `SceneFieldDecl` are the value layer's
(SPFlowValueHost.h), so a screen can bind its controls against the same statement without linking the
graph. What is spelled here is the `scene` section of the file, read by load() and written by
save() below, because a screen is handed the declarations and never reads that section itself. */
using value::SceneDecl;
using value::SceneFieldDecl;

/* One extension instance the graph declares it needs from the scene. The type itself lives beside
the interfaces a scene's host implements (SPFlowValueHost.h): a graph is not the only thing that
declares one, and what a consumer declares about a scene is a fact about the scene. */
using ExtensionDecl = value::ExtensionDecl;

struct GraphEdge {
	EdgeKind kind = EdgeKind::Data;
	NodeId from = NullNodeId;
	StringView fromPin;
	NodeId to = NullNodeId;
	StringView toPin; // empty for an exec edge
};

// How a call site runs its function when the node does not say: as a call with an activation of
// its own, or substituted into the caller when the graph is linked.
enum class FunctionMode : uint8_t {
	Call,
	Inline,
};

SP_PUBLIC StringView getFunctionModeName(FunctionMode);
SP_PUBLIC bool readFunctionMode(StringView, FunctionMode &);

// One data pin of a function's interface. The order of the pins is the order of the call node's
// pins, so it is kept as written.
struct FunctionPin {
	StringView name;
	VarType type = VarType::Nil;
	ElementChain element = 0;
	StringView subtype; // enum family or referenced component type, by name
	bool required = false;
	mem_std::Value def; // empty: the type's zero
};

// What a graph offers to its callers. The call node, `fn.entry` and `fn.return` are spelled from
// this when the graph is linked; nothing here is resolved against a registry.
struct FunctionInterface {
	mem_std::Vector<FunctionPin> inputs;
	mem_std::Vector<FunctionPin> outputs;
	bool execIn = false;
	mem_std::Vector<StringView> execOut;
	FunctionMode mode = FunctionMode::Call;
};

// A function defined inside a graph document: an interface and a body of its own. Its node ids
// share one numbering with the root body and every other function of the document, so an id names
// one node of the file wherever it stands.
struct GraphFunction {
	StringView name;
	FunctionInterface iface;
	mem_std::Value meta;
	mem_std::Vector<GraphNode> nodes;
	mem_std::Vector<GraphEdge> edges;

	const GraphNode *getNode(NodeId) const;
};

// A graph as it is written down: nodes, edges, literals, and the editor's own notes. Loading
// canonicalizes - nodes come back in ascending id order and edges in a fixed order derived from
// their endpoints, whatever order the file used, so a file that differs from another only in the
// order of its records loads into the same asset, builds into the same runtime graph and therefore
// executes identically. It also makes save -> load -> save byte-identical.
class SP_PUBLIC GraphAsset final {
public:
	static constexpr uint32_t FormatVersion = 1;

	// The version a file with an `interface` or `functions` section is written under: a reader that
	// knows only FormatVersion refuses it by number instead of by an unknown key.
	static constexpr uint32_t FunctionFormatVersion = 2;

	// The prefix of every operation a function contributes: a call node names `fn.<function>`.
	static constexpr StringView FunctionOpPrefix = StringView("fn.");

	// The envelope a graph file opens with: `{"__meta": {"kind": "graph", "version": 1,
	// "generator": "..."}}`. It is part of what a graph file is, and the content hash is taken over
	// it. The generator names the program that wrote the file; it is kept as read and written back,
	// and an asset whose file named none writes none.
	static constexpr StringView MetaKey = StringView("__meta");
	static constexpr StringView Kind = StringView("graph");

	~GraphAsset();

	GraphAsset() = default;
	GraphAsset(const GraphAsset &) = delete;
	GraphAsset &operator=(const GraphAsset &) = delete;

	bool init(memory::pool_t *parent = nullptr);

	// Transactional: a rejected file leaves the asset exactly as it was, empty or previously
	// loaded. Structure only - unknown keys, malformed records, duplicate ids. Whether an edge
	// names a node that exists, whether a pin is real and whether the types meet is validation's
	// question, and it needs the operation registry this class deliberately does not hold.
	Status load(const mem_std::Value &, DiagSink *diagnostic = nullptr);

	template <DiagContainer Out>
	Status load(const mem_std::Value &value, Out *diagnostic) {
		DiagSinkFor<Out> sink(diagnostic);
		return load(value, sink.get());
	}

	void save(mem_std::Value &out) const;

	StringView getName() const { return _name; }
	StringView getGenerator() const { return _generator; }

	// The program a save() names: a host that writes graph files stamps its own name here, the
	// way it stamps every other file it writes.
	void setGenerator(StringView);
	const mem_std::Value &getMeta() const { return _meta; }

	SpanView<GraphNode> getNodes() const { return _nodes; }
	SpanView<GraphEdge> getEdges() const { return _edges; }

	// What this graph declares it needs from the scene: the components and fields its nodes are
	// allowed to name. Empty means the file did not say, and then the build derives the contract
	// from the node parameters and cross-checks nothing - the same "the file did not say" that a
	// zero opHash means, one level up. When it is there, it is the declaration and the node
	// parameters are usages of it: a node naming something undeclared is refused, and refused
	// without a scene, which is what makes a typo in a component name something the editor can say
	// on a keystroke.
	SpanView<SceneDecl> getScene() const { return _scene; }

	// What this graph declares it needs from the scene's extensions, in `id` order. An extension
	// has no fallback derivation, so a node naming an id no declaration mentions is refused
	// outright rather than left for a scene to settle; empty is "this graph uses no extension", not
	// "the file did not say".
	SpanView<ExtensionDecl> getExtensions() const { return _extensions; }

	/* The file this was loaded from carried no `__meta` envelope: it predates the standard and was
	read by its own `formatVersion` instead. A fact about the file rather than an entry in the
	diagnostic report - the report is about the graph, and there is nothing wrong with this
	graph. An editor reads it to say "saving will upgrade this file", and save() upgrades it
	whether anyone asked or not. False for an asset that was never loaded. */
	bool isLegacyFormat() const { return _legacyFormat; }

	// Ascending ids, so this is a bisection rather than a scan. The root body only.
	const GraphNode *getNode(NodeId) const;

	// The graph is a function: it carries an `interface`, and its root body is the function's body.
	bool hasInterface() const { return _hasInterface; }
	const FunctionInterface &getInterface() const { return _interface; }

	// The functions defined inside this document, in name order.
	SpanView<GraphFunction> getFunctions() const { return _functions; }
	const GraphFunction *getFunction(StringView name) const;

	// A node of any body of the document; `body` receives the function it stands in, or null for
	// the root body.
	const GraphNode *findNode(NodeId, const GraphFunction **body = nullptr) const;

	// The version save() writes: FunctionFormatVersion when the document has an interface or a
	// function, FormatVersion otherwise.
	uint32_t getWriteVersion() const;

	/* The identity of what this asset says: a hash over its canonical form, which is what save()
	writes, encoded as CBOR. Two files that differ only in the order of their records, in whitespace
	or in the spelling of the envelope hash alike, and two that differ in one literal do not. `meta`
	is included: it is part of what save() writes, and a generated unit that embeds the asset
	 embeds it too. A unit records the hash of the asset it was written from, so a
	host holding both can say whether they are the same graph before it runs either. Zero for an
	asset that was never loaded. */
	uint64_t getContentHash() const;

	memory::pool_t *getPool() const { return _pool; }

private:
	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;
	bool _legacyFormat = false;
	bool _hasInterface = false;
	StringView _name;
	StringView _generator;
	mem_std::Value _meta;
	mem_std::Vector<SceneDecl> _scene;
	mem_std::Vector<ExtensionDecl> _extensions;
	mem_std::Vector<GraphNode> _nodes;
	mem_std::Vector<GraphEdge> _edges;
	FunctionInterface _interface;
	mem_std::Vector<GraphFunction> _functions;
};

// Writes an interface the way save() does; shared with the linker, which spells linked graphs.
SP_PUBLIC void saveFunctionInterface(const FunctionInterface &, mem_std::Value &out);

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWASSET_H_ */
