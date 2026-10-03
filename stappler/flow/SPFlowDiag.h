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

#ifndef STAPPLER_FLOW_SPFLOWDIAG_H_
#define STAPPLER_FLOW_SPFLOWDIAG_H_

#include "SPFlowValueDiag.h"
#include "SPFlowValueVar.h"

// What the graph reports, as numbers (SPFlowValueDiag.h): a code that names the rule, a detail that
// names the sentence, a locus kind with the ids and names it carries, and the sentence's arguments.
// Nothing here is text. A consumer turns an entry into its own words - a dictionary with a
// localized message, say - or switches on the numbers.
namespace STAPPLER_VERSIONIZED stappler::flow {

using DiagSeverity = value::DiagSeverity;
using value::Diag;
using value::DiagSink;

// The author-visible identity of a node. Stable, stored in the asset, and it survives resaving -
// unlike the node's runtime index, which is assigned by the build and is never serialized.
using NodeId = uint32_t;

static constexpr NodeId NullNodeId = 0;

// Validation reports rather than throws, and reports every problem it finds rather than the first:
// an author wants the list, and the tests compare the whole list against a golden one - code, node
// and pin, not merely "refused".
enum class DiagCode : uint16_t {
	// Operation registration.
	OpNameEmpty,
	OpPinInvalid, // the pin's type/element pair is not a legal field type
	OpPinDuplicate, // two pins of one direction share a name
	OpPinLimit, // more pins than MaxDataPins / MaxExecOut
	OpDefaultInvalid, // a pin default that is not a value of the pin's type
	OpLocalInvalid, // a declared local that collides with an output, or is not a legal field
	OpDuplicate, // an operation with this id is already registered
	OpPinRoleInvalid, // a scene role on a pin that cannot carry one, or one with nothing to belong to
	OpParallelInvalid, // a parallel class the rest of the signature contradicts
	OpSettingInvalid, // a setting declaration with a bad name, type, choice or default

	// Asset structure.
	AssetMalformed,
	AssetUnknownKey,
	NodeIdInvalid,
	NodeIdDuplicate,
	GraphTooLarge,

	// Resolution against the registry.
	UnknownOp,
	SignatureDrift, // the asset was written against another signature of this operation

	// Edges.
	UnknownNode,
	UnknownPin,
	PinKindMismatch, // a data edge into an exec pin, or the reverse
	PinArity, // a data input with two edges, an exec output with two
	TypeMismatch, // the C1 matrix rejects the conversion, or it is not Same/Widen
	ElementMismatch, // same tag, different element chain: Array<Int> is not Array<Float>
	SubtypeMismatch, // same tag, different enum family or referenced schema

	// Whole-graph shape.
	DataCycle, // an error; a cycle over EXECUTION edges is a legal loop
	MissingInput, // a required data input with neither an edge nor a value
	ConstantInvalid, // a node parameter that is not a value of the pin's type
	Unreachable, // an exec input with no incoming exec edge: it can never fire
	SettingUnknown, // a node setting the operation does not declare
	SettingInvalid, // a node setting that is not a value the declaration admits

	// Loop bodies. A scope is the set of nodes a loop re-runs, decided by the build so that the
	// interpreter never has to work out at run time which nodes belong to an iteration.
	ScopeConflict, // one node reached as part of two different loop bodies
	ScopeEscape, // an edge crossing out of a loop body, or into one sideways

	// Parallel blocks: what makes a result depend on the order
	// of the branches or on the moment of delivery is refused here, before anything runs.
	ParallelUnpaired, // a fan-out without its barrier, a barrier or collector without its block
	ParallelEscape, // an edge out of a parallel body other than into its barrier or a collector
	ParallelSerialOp, // a serial operation in a parallel body
	ParallelForeignWrite, // a body writes a field of an entity other than its own
	ParallelConflict, // an access of the block meets an access of a branch or of concurrent work
	ParallelDynamicScene, // a scene name on an edge in a body or in the block's concurrent region
	ParallelNested, // a parallel block inside a parallel body
	ParallelReentry, // the fan-out may receive another token while its block is in flight
	ParallelGpuType, // a GPU-enabled body carries a value outside the GPU set
	ParallelGpuOp, // a GPU-enabled body uses an operation with no shader form
	ParallelGpuShape, // a GPU-enabled body loops
	ParallelEnumRange, // an enum narrowed in a body belongs to a family with members outside int32
	ParallelBranchFailed, // a run: a branch of a parallel block failed
	ParallelPriorityDropped, // an advice: a path to a fan-out could not be lifted without changing a result
	ParallelBlockTimeout, // a run: a parallel block was broken off by its timeout

	// The graph's side of the scene contract: declared by the signatures through PinRole, filled in
	// by the node parameters, and checked against the scene's registry by the build.
	SceneUndeclared, // a node names something the asset's "scene" section does not declare
	SceneUnknownComponent, // a named component type the scene's registry does not have
	SceneUnknownField, // a named field the component does not have
	SceneFieldType, // the field's type is not the one the value pin declares
	SceneNameDynamic, // an advice: the name arrives on an edge, so nothing here can resolve it
	SceneUnused, // an advice: the section declares something no node names

	// A node's own enum family (PinRole::EnumFamily).
	EnumFamilyMissing, // the family name is empty
	EnumFamilyDynamic, // the family name arrives on an edge, so no output type can be decided
	EnumFamilyUnknown, // the scene's registry has no family of that name
	EnumFamilyAlias, // the name is an alias; a node names the family itself

	// The graph's side of the extension contract, split the same way: what the file can be judged
	// on alone is decided with no scene in reach, and what needs the live instances waits for
	// bindScene.
	ExtUndeclared, // a node names an instance id the asset's "extensions" section does not declare
	ExtUnknown, // a declaration names an extension this process has no definition for
	ExtInstanceMissing, // the definition exists and the scene carries no instance under that id
	ExtParamMismatch, // the live instance says it is not what the declaration describes
	ExtNameDynamic, // an advice: the id arrives on an edge, so nothing here can resolve it
	ExtUnused, // an advice: the section declares an instance no node names

	// The cost of eagerness: a predicate over the built graph plus a code. These two shapes are the
	// ones always worth saying.
	EagerUnused, // runs every time and nothing reads what it produced
	EagerSpeculative, // runs every time and is only ever read by nodes that may not run

	// The run, rather than the build. These are diagnostics like any other, and belong in this one
	// table: a code outside it cannot be parsed by readDiagCode and carries no severity.
	OpError, // an operation returned a failure while running
	Deadlock, // an activation closed with nodes still waiting on inputs that will never arrive
	ActivationLimit, // the run made more activations than it was allowed
	StepLimit, // the run took more steps than it was allowed

	// A generated unit, rather than a built graph: it carries the shape of a
	// graph as constants and an identity block naming the operations, schemas and frame layout it
	// was written against, checked against the live registries on load. Each way it can have
	// drifted is its own code, because each is a different file to open.
	CodegenOpDrift, // an operation the unit names is missing, or its signature hash moved
	CodegenSchemaDrift, // a local schema, or interp.NodeState, no longer hashes as the unit says
	CodegenLayoutDrift, // the frame layout the unit carries is not the one the store computes
	CodegenAssetDrift, // the asset a host holds is not the one the unit was generated from
	CodegenMalformed, // the unit's own tables do not describe a graph
	CodegenQuantumUnsupported, // this store cannot undo a unit of work, and the run asked it to

	ParallelGpuGuard, // a GPU body narrows a value the CPU holds off its unconditional prefix
	ParallelGpuLost, // a run: the device lost, went silent or answered with records that do not replay

	// The project's named entities (PinRole::EntityName), resolved when the graph is bound to the
	// project's table.
	NamedDynamic, // an advice: the name arrives on an edge, so nothing here can resolve it
	NamedUnknown, // the project's table has no entity of that name
	NamedArena, // the entity lives in global or custom, and a graph reaches only the scene
};

static constexpr uint32_t DiagCodeCount = uint32_t(DiagCode::NamedArena) + 1;

// Which sentence an entry is. One per thing the graph can say; several may share a code.
enum class DiagDetail : uint16_t {
	None,
	ActivationLimit,
	AssetUnknownKey,
	CodegenConstantsOutOfOrder,
	CodegenIdentityMisplaced,
	CodegenLayoutUncomputable,
	CodegenNameNotIdentifier,
	CodegenNodeStateMissing,
	CodegenNotBuilt,
	ConstCount,
	ConstDrift,
	ConstEnumFamily,
	ConstExpectedArray,
	ConstExpectedBytes,
	ConstExpectedDict,
	ConstExpectedLiteral,
	ConstExpectedString,
	ConstLossy,
	ConstNeedsConversion,
	ConstNotProjectable,
	ConstNotValidLiteral,
	ConstOutOfRange,
	ConstSchema,
	ConstUndeclaredFamily,
	DataCycle,
	DataInArity,
	Deadlock,
	DeadlockInput,
	EagerSpeculative,
	EagerUnused,
	EdgeDict,
	EdgeExecNoPin,
	EdgeFromPin,
	EdgeIds,
	EdgeKind,
	EdgeToPin,
	EdgeUnknownNode,
	EdgesArray,
	EnumFamilyAlias,
	EnumFamilyDynamic,
	EnumFamilyMissing,
	EnumFamilyUnknown,
	EnvelopeMalformed,
	ExecOutArity,
	ExtArray,
	ExtDeclDict,
	ExtDeclNames,
	ExtDeclTwice,
	ExtInstanceKind,
	ExtInstanceMissing,
	ExtNameDynamic,
	ExtParamsDict,
	ExtSceneMismatch,
	ExtUndeclared,
	ExtUnknown,
	ExtUnused,
	FormatMetaKindMissing,
	FormatMetaMalformed,
	FormatMetaVersionInvalid,
	FormatVersionUnsupported,
	GraphDict,
	LayoutDrift,
	LayoutFailed,
	MetaDict,
	MissingInput,
	NameString,
	NamedArena,
	NamedDynamic,
	NamedUnknown,
	NeedsMeta,
	NoDataIn,
	NoDataOut,
	NoExecIn,
	NoExecOut,
	NoNodeStateType,
	NodeDict,
	NodeIdDuplicate,
	NodeIdInvalid,
	NodeMetaDict,
	NodeNoOp,
	NodeParamsDict,
	NodeSettingsDict,
	NodeStateTypeDrift,
	NodesArray,
	NotAGraph,
	OpDefaultInvalid,
	OpDuplicate,
	OpExecDuplicate,
	OpExecLimit,
	OpExecName,
	OpLocalCollides,
	OpLocalRefused,
	OpNameEmpty,
	OpParallel,
	OpParallelDrift,
	OpPinDuplicate,
	OpPinInvalid,
	OpPinLimit,
	OpPinName,
	OpPinRole,
	OpRefused,
	OpSchemaDrift,
	OpScopeMany,
	OpScopeUnknown,
	OpSetting,
	OpSignatureDrift,
	ParallelBarrierEntry,
	ParallelBarrierNoBlock,
	ParallelBlockTimeout,
	ParallelBranchBudget,
	ParallelBranchFailed,
	ParallelCollectorNoBlock,
	ParallelConflictBranches,
	ParallelConflictConcurrent,
	ParallelConflictInput,
	ParallelConflictStructure,
	ParallelDynamicBody,
	ParallelDynamicConcurrent,
	ParallelEnumNoFamily,
	ParallelEnumRange,
	ParallelEscapeExec,
	ParallelEscapeValue,
	ParallelForeignWrite,
	ParallelGpuCycle,
	ParallelGpuGuard,
	ParallelGpuGuardWritten,
	ParallelGpuInvalid,
	ParallelGpuLoop,
	ParallelGpuLost,
	ParallelGpuOp,
	ParallelGpuSilent,
	ParallelGpuSize,
	ParallelGpuType,
	ParallelNested,
	ParallelNoBarrier,
	ParallelPriorityDropped,
	ParallelReentry,
	ParallelSerialOp,
	ParallelTwoBarriers,
	ParallelTwoBlocks,
	QuantumUnsupported,
	SceneArray,
	SceneDeclComponent,
	SceneDeclDict,
	SceneDeclTwice,
	SceneEnumFamilyHere,
	SceneFieldDict,
	SceneFieldName,
	SceneFieldTwice,
	SceneFieldTypeUnknown,
	SceneFieldsArray,
	SceneNameDynamic,
	SceneNoComponent,
	SceneNoField,
	SceneNoFieldNamed,
	SceneTypeDeclared,
	SceneTypeHere,
	SceneUndeclared,
	SceneUnusedComponent,
	SceneUnusedField,
	ScopeConflict,
	ScopeEscapeExec,
	ScopeEscapeValue,
	ScopeFrameBytesDrift,
	SettingInvalid,
	SettingUnknown,
	SignatureDrift,
	StepLimit,
	TooManyEdges,
	TooManyNodes,
	TypeNoReach,
	UnitBlockCount,
	UnitBlockPolicy,
	UnitDataEdgeNode,
	UnitExecEdgeNode,
	UnitFamilyDrift,
	UnitGpuDrift,
	UnitIndexPastEnd,
	UnitLayoutCover,
	UnitNoRootScope,
	UnitOpCount,
	UnitOpDrift,
	UnitOpenerUnknown,
	UnitParentUnknown,
	UnitScopeUnknown,
	UnitSlicePastEnd,
	UnknownOp,
	UnknownPin,
	Unreachable,
	VersionDisagrees,
};

// A word or a clause an entry uses as an argument rather than as its sentence: what was wrong with a
// pin role or a setting, which section of an asset, which table of a unit. Rendered by the consumer
// like a sentence of its own.
enum class DiagPhrase : uint16_t {
	None,
	PlaceholderScene, // <scene>
	PlaceholderSetting, // <setting>
	RoleComponentNameType, // a component name has to be a plain String pin
	RoleFieldNameType, // a field name has to be a plain String pin
	RoleFieldNameOrphan, // a field name with no component name before it
	RoleFieldNameSecond, // a second field name in one group
	RoleFieldValueOrphan, // a field value with no component name before it
	RoleFieldValueSecond, // a second field value in one group
	RoleFamilyNameType, // a family name has to be a plain String pin
	RoleFamilyNameSecond, // a second family name in one operation
	RoleTargetType, // a target has to be a plain EntityRef pin
	RoleTargetSecond, // a second target in one operation
	RoleBranchNoExec, // a branch value belongs to an operation with no exec input
	RoleBranchSecond, // a second branch value in one operation
	RoleEntityNameType, // an entity name has to be a plain String pin
	RoleExtensionNameType, // an extension name has to be a plain String pin
	RoleFamilyOutputType, // a family output has to be an Enum with no family
	RoleFamilyOutputOrphan, // a family output with no family name input
	RoleOutputValueOnly, // an output can only carry a field value
	RoleFieldValueNoField, // a field value in a group that names no field
	RefNoComponent, // a scene reference needs a component name
	RefTypeNoField, // a field type declared on a reference that names no field
	RefTargetNoPin, // a targeted reference on an operation with no target pin
	SettingNoName, // a setting needs a name
	SettingSecond, // a second setting of this name
	SettingTypeIllegal, // not a legal field type
	SettingChoicesNotChoice, // choices on a setting that is not a choice
	SettingChoiceShape, // a choice is a String with choices
	SettingChoiceSetShape, // a choice set is an Array<String> with choices
	SettingComponentNamesShape, // component names are an Array<String> with no choices
	SettingRoleUnknown, // an unknown setting role
	SettingValueType, // not a value of the setting's type
	SettingValueNotChoice, // not one of the choices
	SettingValueNotNames, // not a list of names
	SettingValueTwice, // a name listed twice
	SettingQueryEmpty, // a query names at least one component
	ParallelClassUnknown, // an unknown parallel class
	ScopeKindUnknown, // an unknown scope kind
	ParallelScopeNoScope, // a parallel scope kind on an operation that opens no scope
	ParallelBarrierShape, // a barrier has an exec input and opens no scope
	ParallelBlockIsFlow, // a parallel block is flow
	ParallelHostSerial, // a host call is serial
	ParallelExtensionSerial, // an extension is serial
	ParallelScopeFlowOnly, // only a flow operation may open a scope
	ParallelBarrierFlowOnly, // only a flow operation may be a barrier
	ParallelPureNoExec, // a pure operation has no exec pins
	ParallelPureNoLocals, // a pure operation declares no locals
	ParallelClassNoScene, // this class does not reach the scene
	ParallelSceneReadOnly, // a scene read reads the scene and does not write it
	ParallelSceneWrite, // a scene write writes the scene
	ParallelAddRemoveSerial, // adding or removing a component is serial
	ParallelWriteTarget, // a write has to address the target entity
	ParallelWriteFixed, // a write has to carry a value of a fixed-size type
	SectionGraph, // graph
	SectionScene, // scene
	SectionSceneField, // scene field
	SectionExtensions, // extensions
	SectionNode, // node
	SectionEdge, // edge
	TableDataIn, // data-in
	TableDataOut, // data-out
	TableExecIn, // exec-in
	TableExecOut, // exec-out
	TableCrossScope, // cross-scope
	TableSceneContract, // scene contract
	TableExtensionContract, // extension contract
	TableNamedContract, // named contract
	TableScope, // scope
	TableBlockWrites, // block writes
	TableBlockCollectors, // block collectors
	TableBlockQuery, // block query
	TableBlockCollector, // block collector
	TableEntry, // entry
	TableTerminal, // terminal
	TableScopeNode, // scope node
};

// What an entry is about, and where its ids and names are: `locusValue` holds the numbers in the
// order listed, `locusName` the names.
enum class DiagLocus : uint16_t {
	None, // the whole graph
	Node, // node
	Pin, // node; pin name (may be empty)
	Edge, // from, to; from pin, to pin names (either may be empty)
	Setting, // node; setting name
	Op, // operation name
	Scope, // scope
	NodeOp, // node, status; operation name
	NodeIteration, // node, iteration
	Limit, // limit
	Branch, // node, status, branch; operation name (may be empty)
	Timeout, // node, timeout, whether it is the default
};

// A sentence and its arguments, for the report calls below.
struct DiagText {
	DiagDetail detail = DiagDetail::None;
	value::DiagArg args[Diag::MaxArgs];
	uint8_t count = 0;

	DiagText() = default;
	explicit DiagText(DiagDetail d) : detail(d) { }

	DiagText &name(StringView v) { return push(value::DiagArg{value::DiagArgKind::Name, v, 0}); }
	DiagText &type(value::VarType v) {
		return push(value::DiagArg{value::DiagArgKind::Type, StringView(), int64_t(v)});
	}
	DiagText &number(int64_t v) {
		return push(value::DiagArg{value::DiagArgKind::Number, StringView(), v});
	}
	DiagText &phrase(DiagPhrase v) {
		return push(value::DiagArg{value::DiagArgKind::Phrase, StringView(), int64_t(v)});
	}
	DiagText &detailArg(DiagDetail v) {
		return push(value::DiagArg{value::DiagArgKind::Detail, StringView(), int64_t(v)});
	}
	// What the `inner` entry said, rendered as an argument.
	DiagText &innerArg() {
		return push(value::DiagArg{value::DiagArgKind::Inner, StringView(), 0});
	}

	DiagText &push(const value::DiagArg &a) {
		if (count < Diag::MaxArgs) {
			args[count++] = a;
		}
		return *this;
	}
};

// How a consumer's own container of diagnostics is filled from the kernel's numbers: a consumer
// specializes this for its container with a `Sink` that is built from a pointer to one and hands out
// a DiagSink through get(). Every kernel call that reports takes a DiagSink, and beside it a
// template that takes such a container; a container nobody specialized this for does not compile.
template <typename Out>
struct DiagWriterFor { };

template <typename Out>
concept DiagContainer = requires { typename DiagWriterFor<Out>::Sink; };

template <DiagContainer Out>
using DiagSinkFor = typename DiagWriterFor<Out>::Sink;

// Counts what it is given and passes it on. A caller that wants no projection - a validity check
// in the editor's inner loop - passes no sink and still gets the verdict.
class SP_PUBLIC DiagReport final {
public:
	explicit DiagReport(DiagSink *sink = nullptr) : _sink(sink) { }

	void add(const Diag &);

	// Counts an entry somebody else wrote, in words the graph has no number for.
	void note(DiagSeverity);

	void report(DiagSeverity, DiagCode, const DiagText &, const Diag *inner = nullptr);
	void reportNode(DiagSeverity, DiagCode, NodeId, const DiagText &);
	void reportPin(DiagSeverity, DiagCode, NodeId, StringView pin, const DiagText &,
			const Diag *inner = nullptr);
	void reportEdge(DiagSeverity, DiagCode, NodeId from, StringView fromPin, NodeId to,
			StringView toPin, const DiagText &);
	void reportSetting(DiagSeverity, DiagCode, NodeId, StringView setting, const DiagText &);

	// A locus the four shapes above do not cover.
	void reportAt(DiagSeverity, DiagCode, const DiagText &, DiagLocus,
			SpanView<int64_t> values = SpanView<int64_t>(),
			SpanView<StringView> names = SpanView<StringView>());

	uint32_t getErrorCount() const { return _errors; }
	uint32_t getWarningCount() const { return _warnings; }
	uint32_t getAdviceCount() const { return _advice; }
	bool hasErrors() const { return _errors > 0; }

	Status getStatus() const { return _errors > 0 ? Status::ErrorInvalidArguemnt : Status::Ok; }

	DiagSink *getSink() const { return _sink; }

private:
	DiagSink *_sink = nullptr;
	uint32_t _errors = 0;
	uint32_t _warnings = 0;
	uint32_t _advice = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWDIAG_H_ */
