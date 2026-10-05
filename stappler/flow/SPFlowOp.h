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

#ifndef STAPPLER_FLOW_SPFLOWOP_H_
#define STAPPLER_FLOW_SPFLOWOP_H_

#include "SPFlow.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

// The GPU side of a block (SPFlowGpu.h): a graph carries what the host attached, and the machine's
// launch reads it. Declared here so both graph policies can name it.
struct GpuBlockShaders;
struct GpuShaderBinary;
class GpuShaderTable;

enum class PinFlags : uint32_t {
	None = 0,
	// An input that must have a value: an edge, a node parameter, or a default here. Without the
	// flag an unconnected input takes the type's zero, which is the right answer for an "offset"
	// and the wrong one for "which entity".
	Required = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(PinFlags)

// What a pin names, as opposed to what it carries. At run time every one of these is an ordinary
// pin; the role is what lets the build read the graph's side of the scene contract off the
// signature, instead of a convention that a pin called "component" means something. The roles
// group: a component name opens a group, and the field name and value pins after it belong to that
// group until the next component name. Registration checks the order, because the grouping is the
// order - a second declaration of which pin belongs to which could disagree with it.
enum class PinRole : uint8_t {
	None,

	// A String pin naming a scene component type. Given a literal, the build resolves it once and
	// the node never hashes a name or scans the registry again.
	ComponentName,

	// The same, except that a type the scene never registered is an answer rather than a mistake:
	// `scene.has` says no about it, `scene.removeComponent` has nothing to remove,
	// `await.component` is still waiting. The build says so as an advice and binds the group to
	// nothing.
	ComponentNameOptional,

	// A String pin naming a field of the group's component. Bound to a FieldDesc, so a read does
	// not walk the field list comparing strings.
	FieldName,

	// The pin - input or output - carrying the value of the group's field. Its declared type is
	// checked against the field's, which turns "scene.getInt on a Float field" from a step that
	// fails on the frame it first runs into a build error naming the node and the pin.
	FieldValue,

	// A String pin naming an instance of a scene extension - "board", not "grid". It opens a
	// group of its own, and that group has exactly one pin: what an extension offers is decided by
	// the extension, so there is nothing here for a field name or a value pin to mean. Which
	// extension "board" is comes from the asset's "extensions" section, so a typo is refused with
	// no scene in reach.
	ExtensionName,

	// An EntityRef input naming the entity the operation's scene groups address. At most one per
	// operation; it belongs to no group.
	EntityTarget,

	// On a String input: the name of an enum family, a literal of the node. On an Enum output with
	// no family of its own: the value takes that node's family. At most one such input.
	EnumFamily,

	// The input of a collector: a value produced in a parallel body, seen by an operation in the
	// block's parent scope as the values of all branches. Only on an operation with no exec input,
	// at most one per operation.
	BranchValue,

	// A String pin naming one of the project's named entities (Named-4). Resolved when the graph is
	// bound to the project's table, to the entity's constant id; the node never looks it up again.
	EntityName,
};

// Whether a scene group reads or writes its component.
enum class SceneAccess : uint8_t {
	Read,
	Write,
};

// One group of a signature: the pins that together name one thing in the scene. Indices into the
// operation's own pin lists, NullPin for a part the group does not have. Derived once at
// registration, so the build parses the roles per signature and not per node.
struct SceneGroup {
	uint32_t componentPin = NullPin;
	uint32_t fieldPin = NullPin;
	uint32_t valuePin = NullPin;
	bool valueIsOutput = false;
	bool optional = false;
	SceneAccess access = SceneAccess::Read;
};

// One group of a signature that names a scene extension instance. One pin, and that is the whole
// structure: a SceneGroup has three parts because a component, a field and a value are three things
// this layer understands, and an extension's own vocabulary is not this layer's business. Kept
// apart from SceneGroup rather than folded into it with null parts, because the two are indexed
// separately and a merged list would make each index a function of the other's length.
struct ExtensionGroup {
	uint32_t namePin = NullPin;
};

// A component or field the operation names itself, in C++, rather than through a pin. `wait.timer`
// is why this exists: both its types and both its field names are literals in the operation, no pin
// mentions them, and PinRole cannot reach them. These resolve through the same build phase into the
// same table and land in the node's slice after the pin-derived groups, in declaration order, so
// the operation reads them by an index it knows from its own signature.
struct SceneRef {
	StringView component;
	StringView field; // empty: the group is about the component alone
	VarType type = VarType::Nil; // Nil: do not check the field's type
	bool optional = false;
	SceneAccess access = SceneAccess::Read;
	bool targeted = false; // addressed through the operation's EntityTarget pin
};

SP_PUBLIC StringView getPinRoleName(PinRole);

// What a scope is: the turns of a loop, the branches of a parallel block, or the body of a function.
// A function's scope is opened by no edge: every call node of that function opens it.
enum class ScopeKind : uint8_t {
	Loop,
	Parallel,
	Function,
};

SP_PUBLIC StringView getScopeKindName(ScopeKind);

// What a node setting admits beyond its type.
enum class SettingRole : uint8_t {
	None, // a value of the declared type
	Choice, // a String, one of `choices`
	ChoiceSet, // an Array<String> of distinct members of `choices`
	ComponentNames, // an Array<String> of distinct scene component names
};

SP_PUBLIC StringView getSettingRoleName(SettingRole);

// A property of a node that is not a pin: never on an edge, written in the node's `settings`
// section and resolved once by the build. The declaration is part of the signature hash, the
// default is not.
struct SettingDesc {
	StringView name;
	VarType type = VarType::Nil;
	ElementChain element = 0;
	SettingRole role = SettingRole::None;
	SpanView<StringView> choices;
	mem_std::Value def; // empty: the first choice, an empty array or the type's zero
};

// What an operation may do inside a parallel block. Part of the signature hash unless Serial;
// registration refuses a class the rest of the signature contradicts.
enum class OpParallel : uint8_t {
	Serial, // anything; not allowed in a parallel body
	Pure, // inputs to outputs: no exec pins, no locals, no scene
	Local, // may keep locals; no scene
	Flow, // control flow; no scene
	SceneRead, // reads the scene, writes nothing
	SceneWrite, // writes fixed-size fields of the target entity only
};

static constexpr uint32_t OpParallelCount = uint32_t(OpParallel::SceneWrite) + 1;

SP_PUBLIC StringView getOpParallelName(OpParallel);
SP_PUBLIC bool readOpParallel(StringView, OpParallel &);

enum class OpFlags : uint32_t {
	None = 0,
	Pure = 1 << 0, // reads nothing outside its own inputs
	ReadsScene = 1 << 1,
	WritesScene = 1 << 2,
	HostCall = 1 << 3, // writes the deferred-effect buffer

	// This operation introduces no failure of its own: it rejects no input and reports no error
	// condition. What it buys is that a run does not have to be able to undo it individually, so
	// the interpreter may skip the version boundary in front of it (see RollbackQuantum). It does
	// not promise the step will return Ok - an operation that only adds two integers still reaches
	// the store to read its inputs and write its output, and that can fail for reasons that are
	// nobody's fault. So it is a hint: when the step fails anyway, the undo goes back to the
	// previous boundary instead of to this unit, and that is a state the store really had. Being
	// wrong here costs granularity and never correctness, and there is deliberately no tripwire
	// asserting otherwise - one would fire on a legitimate allocation failure.
	Infallible = 1 << 4,

	// The operation has a shader form and may run in a GPU-enabled parallel body. Outside the hash,
	// like every flag.
	ShaderForm = 1 << 5,

	// Its shader body may differ from the C++ one within the tolerance of level C (a transcendental
	// function, say), rather than bit for bit.
	ShaderInexact = 1 << 6,
};

SP_DEFINE_ENUM_AS_MASK(OpFlags)

// The part an operation plays in a function (SPFlowFunction.h): a call of it, its body's entry or
// return, or the entry or return of a body substituted at a call site. Set only on the operations a
// function contributes when a graph is linked; outside the hash, because the name already says it.
enum class FunctionRole : uint8_t {
	None,
	Call,
	Entry,
	Return,
	Arg,
	Result,
};

// The interpreter's side of an operation call: inputs, outputs, the node's local record, the scene.
// Declared here and defined by the interpreter, because what an operation may reach is the
// interpreter's decision, not the signature's.
class OpContext;

using OpFn = Status (*)(OpContext &);

// One pin of an operation's signature. Unlike the schema layer's FieldDef/FieldDesc pair there is
// only one struct here: a schema computes a layout from its input and so has something to hand
// back, while a pin is stored as it was given, with only its name interned.
struct PinDesc {
	StringView name;
	VarType type = VarType::Nil;
	ElementChain element = 0; // container element chain; 0 for a scalar
	TypeId subtypeId = value::NullTypeId; // enum family or referenced component type
	PinFlags flags = PinFlags::None;

	// What this pin names in the scene, if anything. Part of the signature hash when it is not
	// None: a pin that used to be an ordinary string and now names a component type belongs to
	// another operation, and an asset written against the old one should be told so.
	PinRole role = PinRole::None;

	// The value of this input when nothing is connected and the node names no parameter. Empty
	// means the type's zero. Ignored on an output.
	mem_std::Value def;
};

// The input of a registration. An aggregate, so a native operation is spelled in one initializer.
struct OpDef {
	StringView name;
	SpanView<PinDesc> dataIn;
	SpanView<PinDesc> dataOut;

	// An exec input is 0 or 1: "this node has a place to be told to run". Exec outputs are named
	// and ordered, and that order is the graph's, not the file's - which is why fan-out is a
	// `sequence` operation with several exec outputs rather than two edges from one pin.
	bool hasExecIn = false;
	SpanView<StringView> execOut;

	// Bit per exec output that opens an isolated scope: everything the graph reaches through it
	// belongs to a loop body, and gets a record of its own on every iteration. Whether an output
	// opens a scope is wiring, not policy - an author wires a body differently from a continuation
	// - so unlike OpFlags this is part of the signature hash, and a graph built against the other
	// meaning of the same pin is rejected as SignatureDrift rather than silently executed with one
	// record where it needed many.
	uint32_t scopeExecOut = 0;

	// What the scope opened by `scopeExecOut` is. In the hash when Parallel.
	ScopeKind scopeKind = ScopeKind::Loop;

	// The operation is a barrier: exec edges from a parallel body into it close the block, and it
	// sits in the body's parent scope. In the hash when set.
	bool joinsScope = false;

	// The operation fires at most one exec output per run. The build relies on it to tell a branch
	// from a sequence, and the machine refuses a step that breaks it. In the hash when set.
	bool execExclusive = false;

	// Properties of a node that are not pins. See SettingDesc.
	SpanView<SettingDesc> settings;

	// Extra fields of the node's local record, beyond the one-per-output the derivation already
	// makes. For an operation that has to remember something between the frames of a loop, or
	// across a stall, and that is nobody's output.
	SpanView<value::FieldDef> locals;

	// See OpParallel. Serial by default, so an operation nobody classified stays out of parallel
	// bodies.
	OpParallel parallel = OpParallel::Serial;

	// What the operation names in the scene on its own account. Unlike the pin roles this is not
	// part of the signature hash, for the same reason OpFlags is not: an operation that starts
	// reaching for another component did not change shape, and no asset was written against the old
	// answer.
	SpanView<SceneRef> sceneRefs;

	// Words an author will type looking for this operation without knowing its name: "if" for
	// flow.branch, "plus" for math.addFloat. The palette searches these beside the name. Not part
	// of the signature hash, for the reason OpFlags is not, and not part of describe() either -
	// what an operation is called in a search box is not its described form.
	SpanView<StringView> synonyms;

	OpFlags flags = OpFlags::None;
	OpFn invoke = nullptr;

	// The name of this operation's body as a template over the door, written the way a generated
	// unit reaches it: named from inside `stappler` (`flow::ops::inl::addInt`), since a unit lives
	// in some namespace there.
	// Empty for an operation that has no such form, and then a unit calls it through `invoke`
	// above, against the same door. Not part of the signature hash, for the reason OpFlags is not,
	// and not described either - what a generator writes into a source file is not part of what an
	// operation is.
	StringView inlineName;

	// The shader form, beside the template form and out of the hash for the same reason. A GLSL
	// function name with its text in `shaderSource`, or a structural form the lowering writes
	// itself: `@fireAll`, `@branch`, `@sceneGet`, `@sceneSet`, `@sceneHas`, `@narrow`, `@widen`,
	// `@passthrough`. Empty for an operation that has none.
	StringView shaderName;
	StringView shaderSource;

	// See FunctionRole: the linked name of the function, and for a return the index of the exec
	// output of the call it leaves through.
	FunctionRole functionRole = FunctionRole::None;
	StringView function;
	uint32_t functionExit = 0;
};

// The resolved form. Addresses are stable for the registry's lifetime: a RuntimeGraph holds raw
// `const OpDesc *`, exactly as an OpDesc holds a raw `const ComponentType *`.
class SP_PUBLIC OpDesc final {
public:
	StringView getName() const { return _name; }
	OpId getId() const { return _id; }

	// Hashes the wiring and nothing else - names, types, element chains, subtypes, the Required
	// flag, and the exec pins in order. Not OpFlags (an operation that starts writing the scene did
	// not change shape), not the defaults (an author may retune one), not the local schema (it is
	// derived and never appears in an asset). An asset stores this number per node, so a drift is
	// reported by name instead of surfacing as a pile of unknown-pin errors.
	uint64_t getSignatureHash() const { return _hash; }

	OpFlags getFlags() const { return _flags; }
	OpFn getInvoke() const { return _invoke; }
	OpParallel getParallel() const { return _parallel; }

	// Index of the EntityTarget input, or NullPin.
	uint32_t getTargetPin() const { return _targetPin; }

	// Index of the EnumFamily input, or NullPin.
	uint32_t getFamilyPin() const { return _familyPin; }

	// The template form's name, or empty. See OpDef::inlineName: this is what stappler_flow_codegen writes
	// into a unit as a direct call, and its emptiness is what makes the unit call through
	// getInvoke() instead.
	StringView getInlineName() const { return _inlineName; }

	// The shader form (OpDef::shaderName, OpDef::shaderSource), or empty.
	StringView getShaderName() const { return _shaderName; }
	StringView getShaderSource() const { return _shaderSource; }

	// OpDef::functionRole and the two beside it.
	FunctionRole getFunctionRole() const { return _functionRole; }
	StringView getFunction() const { return _function; }
	uint32_t getFunctionExit() const { return _functionExit; }

	SpanView<PinDesc> getDataIn() const { return _dataIn; }
	SpanView<PinDesc> getDataOut() const { return _dataOut; }

	// The search words, interned, in declaration order. Empty for an operation that declared
	// none - most of them, and that is fine: the name is already searchable.
	SpanView<StringView> getSynonyms() const { return _synonyms; }

	// The scene groups this signature declares: first the ones its pins name, in pin order, then
	// the ones it names itself through OpDef::sceneRefs, in declaration order. Empty for every
	// operation that names nothing in the scene. The order is the contract with the operation: a
	// node's bindings line up with this list one for one, so an operation reads its own group by an
	// index it can write as a literal.
	SpanView<SceneGroup> getSceneGroups() const { return _sceneGroups; }
	SpanView<SceneRef> getSceneRefs() const { return _sceneRefs; }

	// The extension instances this signature names, in pin order. Empty for everything that names
	// none, and the same contract as above: a node's extension bindings line up with this list one
	// for one, so an operation reads its own group by an index it writes as a literal.
	SpanView<ExtensionGroup> getExtensionGroups() const { return _extensionGroups; }

	bool hasExecIn() const { return _hasExecIn; }
	SpanView<StringView> getExecOut() const { return _execOut; }

	uint32_t getScopeExecOut() const { return _scopeExecOut; }
	ScopeKind getScopeKind() const { return _scopeKind; }
	bool joinsScope() const { return _joinsScope; }
	bool isExecExclusive() const { return _execExclusive; }

	// Index of the BranchValue input, or NullPin.
	uint32_t getBranchPin() const { return _branchPin; }

	SpanView<SettingDesc> getSettings() const { return _settings; }
	bool findSetting(StringView, uint32_t &out) const;
	bool opensScope(uint32_t execOut) const {
		return execOut < MaxExecOut && (_scopeExecOut & (uint32_t(1) << execOut)) != 0;
	}

	// Pins are named in the asset and resolved to indices by the build, so reordering or adding
	// pins in a signature leaves existing assets working.
	bool findDataIn(StringView, uint32_t &out) const;
	bool findDataOut(StringView, uint32_t &out) const;
	bool findExecOut(StringView, uint32_t &out) const;

	// The schema of a node's local record: one field per data output, then whatever the operation
	// declared. It is a function of the signature, not of the node - every node of one operation
	// shares this descriptor, because their records have the same shape and a type per node would
	// mean a pool per node in the local store. Null when the operation has neither outputs nor
	// declared locals: a zero-field component would become a pool with a stride of zero, which the
	// paged array refuses, and such a node simply has no local record.
	const value::ComponentType *getLocalSchema() const { return _localSchema; }

	void describe(mem_std::Value &) const;

	// Builds in place. Public because the registry owns the storage; use the registry's factory.
	Status build(const OpDef &, memory::pool_t *, DiagReport &);

	// Derives getLocalSchema() into `types`. Separate from build() because it is the registry that
	// owns the type registry, and because a refused registration must not leave a type behind.
	Status deriveLocalSchema(const OpDef &, value::TypeRegistry &types, DiagReport &);

private:
	Status checkParallel(const OpDef &, DiagReport &) const;
	Status takeSettings(const OpDef &, memory::pool_t *, DiagReport &);

	StringView _name;
	StringView _inlineName;
	StringView _shaderName;
	StringView _shaderSource;
	StringView _function;
	FunctionRole _functionRole = FunctionRole::None;
	uint32_t _functionExit = 0;
	OpId _id = value::NullTypeId;
	uint64_t _hash = 0;
	OpFlags _flags = OpFlags::None;
	OpFn _invoke = nullptr;
	OpParallel _parallel = OpParallel::Serial;
	uint32_t _targetPin = NullPin;
	uint32_t _familyPin = NullPin;
	uint32_t _branchPin = NullPin;
	uint32_t _scopeExecOut = 0;
	ScopeKind _scopeKind = ScopeKind::Loop;
	bool _joinsScope = false;
	bool _execExclusive = false;
	bool _hasExecIn = false;
	const value::ComponentType *_localSchema = nullptr;
	mem_std::Vector<PinDesc> _dataIn;
	mem_std::Vector<PinDesc> _dataOut;
	mem_std::Vector<StringView> _execOut;
	mem_std::Vector<StringView> _synonyms;
	mem_std::Vector<SceneGroup> _sceneGroups;
	mem_std::Vector<SceneRef> _sceneRefs;
	mem_std::Vector<ExtensionGroup> _extensionGroups;
	mem_std::Vector<SettingDesc> _settings;
	mem_std::Vector<StringView> _settingChoices; // flat; each setting's choices is a slice
};

// Owns the operation descriptors and the pool their interned names live in. Lifetime, and it has to
// be said once and obeyed everywhere: the registry outlives every RuntimeGraph built against it,
// and the scene's ComponentRegistry outlives the registry - the same rule ComponentRegistry keeps
// one level up.
class SP_PUBLIC OpRegistry final {
public:
	~OpRegistry();

	OpRegistry() = default;
	OpRegistry(const OpRegistry &) = delete;
	OpRegistry &operator=(const OpRegistry &) = delete;

	bool init(memory::pool_t *parent = nullptr);

	// A layer over `base`: a lookup this registry cannot answer is asked of the base, and nothing is
	// registered here under an id the base already has. What a linked graph's functions contribute
	// lives in such a layer, owned by the graph, while the base stays the application's. The base
	// outlives the layer.
	bool init(const OpRegistry *base, memory::pool_t *parent = nullptr);

	const OpRegistry *getBase() const { return _base; }

	const OpDesc *createNative(const OpDef &, DiagSink *diagnostic = nullptr);

	template <DiagContainer Out>
	const OpDesc *createNative(const OpDef &def, Out *diagnostic) {
		DiagSinkFor<Out> sink(diagnostic);
		return createNative(def, sink.get());
	}

	const OpDesc *get(OpId) const;
	const OpDesc *get(StringView) const;

	// This registry's own operations, not its base's.
	uint32_t getCount() const { return uint32_t(_order.size()); }
	const OpDesc *getAt(uint32_t i) const { return _order[i]; }

	memory::pool_t *getPool() const { return _pool; }

	// The derived local schemas, and only those. A separate registry from the scene's on purpose: a
	// scene's component registry describes what an author saves in a scene file, while these are
	// the interpreter's internal form. Mixing them would make a saved scene depend on which
	// operations happened to be registered in the run that saved it.
	const value::TypeRegistry &getLocalTypes() const { return _localTypes; }

	// Non-const, because the interpreter registers its own bookkeeping components here: a node's
	// record and the state of the node that owns it live in the same store, so they have to live in
	// the same registry.
	value::TypeRegistry &getLocalTypes() { return _localTypes; }

	// The local types of the registry at the bottom of the layers: where the interpreter's own
	// bookkeeping types are registered. A layer's own local types are its operations' records only.
	const value::TypeRegistry &getCoreTypes() const {
		return _base ? _base->getCoreTypes() : _localTypes;
	}

	void describe(mem_std::Value &) const;

private:
	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;
	const OpRegistry *_base = nullptr;
	mem_std::Vector<OpDesc *> _order;
	value::TypeRegistry _localTypes;
};

// Whether `value` can stand for a value of this pin's type, and what it looks like once it does.
// Shared by registration (a pin default) and validation (a node parameter), so an author gets the
// same answer wherever the literal is written. Two rules: the value is converted once, here, and
// `out` carries the result in the pin's own type - a literal 3 on a Float input is stored as 3.0,
// and nothing decides a type at run time; and the conversion is checked by value under
// CastPolicy::Lossless, unlike an edge, which is checked by type, so 3 reaches a Float input and
// 3.5 does not reach an Int one. A container's value is checked by shape and copied as it stands:
// materializing one needs an arena and the build has none, so the interpreter decodes it into the
// local store when the node first runs. Its element chain is therefore not checked here either; the
// identity that is checked is the pin's subtypeId, because an enum family and a referenced schema
// live in the value's own payload and a literal naming a different one is a mistake rather than an
// override. A literal naming none takes the pin's, by the first rule above.
SP_PUBLIC Status resolveConstant(const mem_std::Value &value, const PinDesc &, NodeId,
		DiagCode failure, DiagReport &, mem_std::Value &out);

/* Whether the two ends of a data edge meet: the conversion matrix admits the tags and the
parameters of the type agree exactly. On success `outRule` is the cell (Same or Widen) and the
caller stores it in the edge; on refusal `outFailure` is which of the three halves said no. Public
because the editor has to answer the same question with the same code: the interactive check on one
candidate wire is a call into this, not a copy of it, and a second implementation would agree on the
day it was written and on no day after. The rule itself is not here - a screen binds a
control to a component's field and asks the identical question, three halves, tag, element chain and
subtype, without linking this module, so it is `value::valueTypesMeet`, over two `ValueShape`s
rather than two pins. What is left here is the mapping onto `DiagCode`: a translation into this
module's diagnostic vocabulary, which has to stay on this side of the line, because two
vocabularies must not spell one code alike. */
// Whether `value` is a value of the setting, and its canonical form: an empty value is the
// declaration's default, then the first choice, an empty array or the type's zero. On refusal
// `reason` says why, as a phrase a message takes for an argument.
SP_PUBLIC bool resolveSetting(const SettingDesc &, const mem_std::Value &value, mem_std::Value &out,
		DiagPhrase &reason);

// The family name a node's EnumFamily input spells - its parameter, else the pin's default - or
// empty when the operation has no such input or the node names none.
SP_PUBLIC StringView readNodeFamilyName(const OpDesc &, const mem_std::Value &params);

// How an operation lowers into a GPU body. A Body calls the operation's GLSL function; every other
// form is written by the lowering itself. This is the one predicate the build's GPU checks and the
// lowering share.
enum class GpuForm : uint8_t {
	None,
	Body,
	FireAll, // fires every exec output (`flow.sequence`, `flow.event`)
	Branch, // fires one exec output by a Bool (`flow.branch`)
	SceneGet,
	SceneSet,
	SceneHas,
	Narrow, // a value outside the GPU set into one inside it
	Widen, // the other way
	Passthrough, // a literal
};

SP_PUBLIC GpuForm classifyGpuOp(const OpDesc &);
SP_PUBLIC StringView getGpuFormName(GpuForm);

// An output that takes the node's family: an EnumFamily output, or the Enum value a scene group of
// an operation with a family name reads (`scene.getEnum`).
inline bool carriesNodeFamily(const OpDesc &op, const PinDesc &out) {
	return out.role == PinRole::EnumFamily
			|| (out.role == PinRole::FieldValue && out.type == VarType::Enum && out.element == 0
					&& op.getFamilyPin() != NullPin);
}

// A data output as a node sees it: an EnumFamily output carries the node's family.
SP_PUBLIC PinDesc nodeDataOut(const OpDesc &, uint32_t pin, TypeId family);

SP_PUBLIC bool edgeTypesMeet(const PinDesc &from, const PinDesc &to, value::CastRule &outRule,
		DiagCode &outFailure);

// A pin of an operation, spelled with the host's own types. Mirrors SP_FLOW_VALUE_FIELD in shape so
// that a signature reads like a schema.
#define SP_FLOW_PIN(nameLit, varType) \
	::stappler::flow::PinDesc{.name = ::stappler::StringView(nameLit), .type = (varType)}

#define SP_FLOW_PIN_REQ(nameLit, varType) \
	::stappler::flow::PinDesc{.name = ::stappler::StringView(nameLit), .type = (varType), \
		.flags = ::stappler::flow::PinFlags::Required}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWOP_H_ */
