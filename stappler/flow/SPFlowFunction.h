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


#ifndef STAPPLER_FLOW_SPFLOWFUNCTION_H_
#define STAPPLER_FLOW_SPFLOWFUNCTION_H_

#include "SPFlowAsset.h"
#include "SPFlowOp.h"

// Functions: a body with an interface, defined inside a graph document (`functions`) or as a document
// of its own (`interface`), and called by name from a node `fn.<name>`. Linking turns a document and
// the functions it reaches into one graph the build takes as it takes any other: a call site that
// runs as a call names an operation whose body is a scope of its own, and a call site that runs
// inline is replaced by a copy of the body. Nothing below the link knows a function by its source
// name; what it sees are the operations spelled here.
namespace STAPPLER_VERSIONIZED stappler::flow {

// The boundary nodes of a body as an author writes them; their pins come from the body's interface.
static constexpr StringView FunctionEntryOp = StringView("fn.entry");
static constexpr StringView FunctionReturnOp = StringView("fn.return");

// The settings a call node and a return node carry, and the exec output an entry fires.
static constexpr StringView FunctionModeSetting = StringView("mode");
static constexpr StringView FunctionExitSetting = StringView("exit");
static constexpr StringView FunctionStartPin = StringView("start");

// The values of a call node's `mode`: the function's own default, or one of the two modes.
static constexpr StringView FunctionModeDefault = StringView("default");

// A name a function may be called by: not empty, not `entry` or `return`, and none of the characters
// a linked name is built with (`:`, `#`).
SP_PUBLIC bool isFunctionName(StringView);

// Where the functions a document does not define come from: a project's library, by name. A
// document qualifies when it carries an interface. The host outlives the link that asks it.
class SP_PUBLIC FunctionHost {
public:
	virtual ~FunctionHost() = default;

	virtual const GraphAsset *findFunction(StringView name) const = 0;
};

// The operations an interface spells. `Source` is the call and the boundary nodes as an editor shows
// them: a return names its exit as a setting. The rest are what the linker registers, under linked
// names, and a return or a result there is one operation per exit.
enum class FunctionOpKind : uint8_t {
	Call,
	SourceEntry,
	SourceReturn,
	Entry,
	Return,
	Arg,
	Result,
};

// An OpDef spelled from an interface, with the storage it points into. Kept alive until the
// registration that reads it returns.
class SP_PUBLIC FunctionSignature final {
public:
	FunctionSignature() = default;
	FunctionSignature(const FunctionSignature &) = delete;
	FunctionSignature &operator=(const FunctionSignature &) = delete;

	// `name` is the operation's name; `function` the linked name of the function it belongs to.
	void init(const FunctionInterface &, StringView name, StringView function, FunctionOpKind,
			uint32_t exit = 0);

	const OpDef &getDef() const { return _def; }

private:
	OpDef _def;
	mem_std::String _name;
	mem_std::String _function;
	mem_std::Vector<PinDesc> _in;
	mem_std::Vector<PinDesc> _out;
	mem_std::Vector<StringView> _execOut;
	mem_std::Vector<SettingDesc> _settings;
	mem_std::Vector<StringView> _choices;
	mem_std::Vector<value::FieldDef> _locals;
};

// The signature hash of `fn.<name>` over this interface: what a call node stores as its opHash, so
// that a change of the interface reaches its callers as signature drift.
SP_PUBLIC uint64_t getFunctionCallHash(const FunctionInterface &, StringView name);

// Where a node of a linked graph came from: the document and the node as written there, and for a
// node of a substituted body the linked id of the call node it stands for.
struct LinkOrigin {
	StringView document;
	StringView function; // the function it was written in; empty for the document's root body
	NodeId source = NullNodeId;
	NodeId callSite = NullNodeId;
};

// A library document a linked graph took a body from, and the hash of what it said.
struct LinkCallee {
	StringView name;
	uint64_t contentHash = 0;
};

// A document and the functions it reaches, as one graph. The linked asset keeps the document's
// root body under the ids written there, and every function it defines as a called body under the
// ids written there too, so a node of the document keeps its id; a library body and every
// substituted copy take fresh ids above them.
class SP_PUBLIC GraphLink final {
public:
	~GraphLink();

	GraphLink() = default;
	GraphLink(const GraphLink &) = delete;
	GraphLink &operator=(const GraphLink &) = delete;

	bool init(memory::pool_t *parent = nullptr);

	// Whether a document needs linking at all: it is a function, defines one, or calls one. A
	// document that does not is built as it stands.
	static bool needsLink(const GraphAsset &);

	// Replaces whatever was linked before. The operations the functions contribute are registered
	// in a layer over `ops`, which must outlive this link; the host is only asked during the call.
	Status link(const GraphAsset &, const OpRegistry &ops, const FunctionHost *, DiagSink *);

	template <DiagContainer Out>
	Status link(const GraphAsset &asset, const OpRegistry &ops, const FunctionHost *host,
			Out *diagnostic) {
		DiagSinkFor<Out> sink(diagnostic);
		return link(asset, ops, host, sink.get());
	}

	bool isLinked() const { return _linked; }

	const GraphAsset &getAsset() const { return *_asset; }
	const OpRegistry &getOps() const { return *_ops; }

	// Null for an id the link did not emit. Kept after a refused link, so the ids a report names
	// can still be traced to what an author wrote.
	const LinkOrigin *getOrigin(NodeId) const;

	SpanView<LinkCallee> getCallees() const { return _callees; }

	// Every emitted id, ascending, and its origin at the same index.
	SpanView<NodeId> getOriginIds() const { return _originIds; }
	SpanView<LinkOrigin> getOrigins() const { return _origins; }

private:
	friend struct Linker;

	void clear();

	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;
	bool _linked = false;
	GraphAsset *_asset = nullptr;
	OpRegistry *_ops = nullptr;
	mem_std::Vector<NodeId> _originIds; // ascending
	mem_std::Vector<LinkOrigin> _origins;
	mem_std::Vector<LinkCallee> _callees;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWFUNCTION_H_ */
