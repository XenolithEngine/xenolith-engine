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

// The body of a parallel block lowered for a GPU. A GPU branch is the same
// branch the machine runs: the lowering does not invent an execution order, it simulates the
// machine's front over the body at generation time. The values stay symbolic; the simulation forks
// only where a value decides the path (`flow.branch`), and the paths it finds are continuations -
// runs of nodes in the order the machine would pop them. States the paths reach twice share a
// continuation. What a shader then computes per branch is the values along its path; what happened
// to the node records, the front and the log is replayed on the CPU from the decisions the shader
// hands back. Values outside the GPU set never reach the shader as values: a row read of an Int or
// an Enum field is a column the CPU loads, narrowed per target type with a guard bit, and a widened
// value travels as a tag and 32 bits the CPU widens when it rebuilds the frame. Every check the CPU
// can make - a row, a guard, a static refusal - that stands on the body's unconditional prefix is
// made before dispatch (`preFailed`); the build refuses a narrowing of a CPU value anywhere else
// (`ParallelGpuGuard`).

#ifndef STAPPLER_FLOW_SPFLOWGPU_H_
#define STAPPLER_FLOW_SPFLOWGPU_H_

#include "SPFlowRuntime.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

// Where a value an input reads comes from.
enum class GpuSource : uint8_t {
	Zero, // no edge and no literal: the type's zero
	Cell, // a body node's output
	Literal, // the node's own constant
	Block, // a value produced outside the body: the same for every branch
	Index, // the fan-out's `index`
	OwnEntity, // the fan-out's `entity`, as a target
	Name, // a component, field or family name: not a value
};

// How a value is held on the GPU: in its own GLSL type, or as a tag and 32 bits.
enum class GpuRep : uint8_t {
	Direct,
	Tagged,
};

// The tags of a Tagged value.
struct GpuTag {
	static constexpr uint32_t Zero = 0;
	static constexpr uint32_t Int32 = 1; // bits: a widened Int32
	static constexpr uint32_t UInt32 = 2;
	static constexpr uint32_t Float32 = 3;
	static constexpr uint32_t Column = 4; // bits: a column index; the value is the CPU's
	static constexpr uint32_t Literal = 5; // bits: a literal index
	static constexpr uint32_t Block = 6; // bits: a block value index
	static constexpr uint32_t Foreign = 7; // bits: a foreign read index
};

struct GpuInput {
	GpuSource source = GpuSource::Zero;
	uint32_t index = InvalidIndex; // cell, literal or block value
	VarType type = VarType::Nil; // what the pin reads, after the edge's cast
	value::CastRule cast = value::CastRule::Same;
};

// A body node's output.
struct GpuCell {
	uint32_t node = InvalidIndex; // body index
	uint32_t pin = 0;
	VarType type = VarType::Nil; // the record field's type
	TypeId subtypeId = value::NullTypeId;
	GpuRep rep = GpuRep::Direct;
};

struct GpuLiteral {
	uint32_t node = InvalidIndex; // graph index
	uint32_t pin = 0;
	VarType type = VarType::Nil;
};

// A value from outside the body: an output of a node in an enclosing activation.
struct GpuBlockValue {
	uint32_t srcNode = InvalidIndex; // graph index
	uint32_t srcPin = 0;
	VarType type = VarType::Nil;
};

// A field of the branch's own entity the body reads: a column when its type is outside the GPU set,
// an In value when it is inside.
struct GpuColumn {
	uint32_t component = InvalidIndex; // index into GpuProgram::components
	StringView field;
	VarType type = VarType::Nil;
};

// A value the CPU holds - a column, a block value, a foreign read - narrowed to a 32-bit type, and
// the bit its guard sets when the value is out of the target's range.
struct GpuGuard {
	enum class Source : uint8_t {
		Column, // per branch: GpuProgram::columns
		Block, // per block: GpuProgram::blockValues
		Foreign, // per block: GpuProgram::foreigns
	};
	Source source = Source::Column;
	uint32_t index = InvalidIndex;
	VarType target = VarType::Nil;
	uint32_t bit = 0; // within its mask: columns in In::guardMask, the others in Block::guardMask
};

// A read through a target that is not the branch's entity: one row for the whole block.
struct GpuForeign {
	GpuSource entity = GpuSource::Zero; // Block or Literal
	uint32_t entityIndex = InvalidIndex;
	TypeId componentId = value::NullTypeId;
	StringView component;
	StringView field; // empty for `scene.has`
	VarType type = VarType::Nil;
};

struct GpuNode {
	uint32_t node = InvalidIndex; // graph index
	GpuForm form = GpuForm::None;
	bool fallible = false; // may refuse on the GPU
	uint32_t inputBegin = 0, inputCount = 0;
	uint32_t cellBegin = 0, cellCount = 0;

	// Scene forms.
	bool own = false; // the target is the branch's entity
	uint32_t component = InvalidIndex; // own: index into GpuProgram::components
	uint32_t write = InvalidIndex; // own: RuntimeBlockWrite index
	uint32_t column = InvalidIndex; // own read of a value outside the GPU set: GpuProgram::columns
	uint32_t in = InvalidIndex; // own read of a GPU value: GpuProgram::fields
	uint32_t foreign = InvalidIndex;
	Status staticFailure = Status::Ok; // refuses on every path (a null target, a literal out of range)

	// Narrow: the guard of a CPU source, when the input can be one.
	uint32_t guard = InvalidIndex;
};

// One run of the simulation between two decisions.
struct GpuCont {
	enum class End : uint8_t {
		Close, // the front ran out: the branch closes
		Budget, // the next pop found the budget spent: `node` is refused
		Branch, // `node` decides: next[0] if the condition holds, next[1] otherwise
		Jump, // next[0]
	};

	uint32_t runBegin = 0, runCount = 0; // into GpuProgram::runs, body indices
	End end = End::Close;
	uint32_t node = InvalidIndex; // body index
	uint32_t next[2] = {InvalidIndex, InvalidIndex};
};

// A check the loader makes before dispatch, in the order the body's prefix meets them.
struct GpuPrefixCheck {
	enum class Kind : uint8_t {
		OwnRow, // index: component
		ForeignRow, // index: foreign
		Guard, // index: guard
		Static, // `status`, every branch
		Budget, // the prefix spends the budget, every branch
	};
	Kind kind = Kind::Static;
	uint32_t index = InvalidIndex;
	uint32_t node = InvalidIndex; // body index of the node that refuses
	uint32_t steps = 0; // the branch's steps when it refuses
	Status status = Status::Ok;
};

// One member of a std430 struct.
struct GpuSlot {
	enum class Kind : uint8_t {
		Count,
		Index,
		PreFailed,
		PreNode,
		PreCode,
		PreSteps,
		PreBudget,
		RowMask,
		GuardMask,
		BlockGuardMask,
		ForeignRowMask,
		In, // index: field - an own read of a GPU value
		Column, // index: guard - a narrowed column
		BlockValue, // index: block value of a GPU type
		BlockNarrowed, // index: guard - a narrowed block value or foreign read
		ForeignValue, // index: foreign of a GPU type
		Status,
		FailNode,
		FailCode,
		Steps,
		Budget,
		DecisionCount,
		Decisions, // count words
		CopyMask, // count words
		Copy, // index: write
		CopyTag,
		Cell, // index: cell
		CellTag,
	};

	Kind kind = Kind::Count;
	uint32_t index = 0;
	mem_std::String name; // the member's GLSL name
	VarType type = VarType::Nil; // Nil: a plain `uint`
	uint32_t count = 1; // array length, for the masks
	uint32_t offset = 0, size = 0, align = 0;
};

struct GpuLayout {
	mem_std::Vector<GpuSlot> slots;
	uint32_t size = 0; // the struct's size: an array's stride
	uint32_t align = 0;
};

struct GpuProgram {
	uint32_t block = InvalidIndex;
	uint32_t maxSteps = 0;

	mem_std::Vector<uint32_t> bodyNodes; // body index -> graph index, ascending
	mem_std::Vector<GpuNode> nodes;
	mem_std::Vector<GpuInput> inputs;
	mem_std::Vector<GpuCell> cells;
	mem_std::Vector<GpuLiteral> literals;
	mem_std::Vector<GpuBlockValue> blockValues;
	mem_std::Vector<TypeId> components; // own components a row check reads
	mem_std::Vector<GpuColumn> columns;
	mem_std::Vector<GpuColumn> fields;
	mem_std::Vector<GpuGuard> guards;
	mem_std::Vector<GpuForeign> foreigns;

	mem_std::Vector<GpuCont> conts; // conts[0] starts at the branch's open
	mem_std::Vector<uint32_t> runs;
	mem_std::Vector<GpuPrefixCheck> prefix;
	uint32_t prefixRuns = 0; // how many runs of conts[0] are the prefix
	uint32_t maxDecisions = 0;

	GpuLayout blockLayout, inLayout, outLayout;

	mem_std::Vector<StringView> bodies; // the GLSL functions used, sorted, unique
	uint64_t hash = 0;

	const GpuNode *findNode(uint32_t graphIndex) const;
};

struct GpuLowerLimits {
	uint32_t maxConts = 1024;
	uint32_t maxRuns = 16384;
};

// Lowers one block. A block that fails the build's GPU checks is not lowered; `report` receives
// what makes an otherwise well-formed body unfit - too large (`ParallelGpuShape`), or a CPU value
// narrowed off the unconditional prefix (`ParallelGpuGuard`). Returns ErrorInvalidArguemnt then.
template <typename Graph>
Status lowerGpuBlock(const Graph &, uint32_t block, GpuProgram &, DiagReport *report = nullptr,
		const GpuLowerLimits & = GpuLowerLimits());

// Whether a block's `executors` names the GPU.
template <typename Graph>
bool isGpuBlock(const Graph &, uint32_t block);

// Whether a block may run on a device: its author demanded it, or its author named no executor at
// all and left the choice open. A candidate's body is lowered and its shader written like a
// demanded one - the heuristic cannot choose what was never compiled - but a candidate that does
// not pass the checks is simply not offered the GPU, where a demand is an error.
template <typename Graph>
bool isGpuCandidate(const Graph &, uint32_t block);

// The hash of the shader a GPU block lowers to, and its text when asked; zero for any other block
// and for a body that does not lower.
template <typename Graph>
uint64_t hashGpuBlock(const Graph &, uint32_t block, mem_std::String *text = nullptr);

// The GLSL compute shader for a lowered block. Deterministic: the same program writes the same text
// on every host.
template <typename Graph>
mem_std::String writeGpuGlsl(const Graph &, const GpuProgram &);

// The control side of one branch, replayed from what the shader returned.
struct GpuNodeState {
	uint32_t flags = 0; // NodeFlags: Ran, Token, Queued
	uint32_t inputs = 0;
	uint32_t produced = 0;
	bool stalled = false;
};

struct GpuPath {
	bool failed = false;
	bool budget = false;
	uint32_t failNode = InvalidIndex; // body index
	Status failStatus = Status::Ok;
	uint32_t steps = 0;
	mem_std::Vector<GpuNodeState> nodes; // per body node
	mem_std::Vector<uint32_t> log; // body indices of the units, in order
	mem_std::Vector<uint32_t> fired; // the exec pins each unit fired, beside `log` (RunStep::fired)
	mem_std::Vector<uint32_t> decisions; // 0 or 1 per Branch the path took
};

// Walks the program along `decisions`. A branch that failed stops at the unit `steps` names (the
// one that refused, which counts, or - under `budget` - the pop that found the budget spent, which
// does not). False when the decisions do not describe a path of this program.
template <typename Graph>
bool replayGpuPath(const Graph &, const GpuProgram &, SpanView<uint32_t> decisions, bool failed,
		bool budget, uint32_t failNode, Status failStatus, uint32_t steps, GpuPath &);

// What the loader reads: the scene as it stands when the block opens, and the values the body takes
// from its enclosing activation.
class SP_PUBLIC GpuLoadSource {
public:
	virtual ~GpuLoadSource() = default;
	virtual bool hasComponent(value::EntityId, TypeId component) const = 0;
	virtual Status readField(value::EntityId, TypeId component, StringView field,
			Var &out) const = 0;
	virtual Status readBlockValue(uint32_t srcNode, uint32_t srcPin, Var &out) const = 0;
};

// The bytes of one dispatch and what the CPU keeps beside them: the values a Tagged cell or copy
// only names by index.
struct GpuLoaded {
	uint32_t branches = 0;
	mem_std::Vector<uint8_t> block; // GpuProgram::blockLayout
	mem_std::Vector<uint8_t> ins; // branches x GpuProgram::inLayout.size
	mem_std::Vector<Var> blockValues; // per GpuProgram::blockValues
	mem_std::Vector<Var> foreignValues; // per GpuProgram::foreigns
	mem_std::Vector<Var> columns; // branches x GpuProgram::columns
	mem_std::Vector<uint8_t> preFailed; // per branch
};

template <typename Graph>
Status loadGpuBlock(const Graph &, const GpuProgram &, const GpuLoadSource &,
		SpanView<value::EntityId> entities, GpuLoaded &);

// One branch as the shader left it, widened and replayed: what a delivery needs to rebuild its
// frame.
struct GpuBranchResult {
	bool failed = false;
	bool budget = false;
	uint32_t failNode = InvalidIndex; // body index
	Status failStatus = Status::Ok;
	uint32_t steps = 0;
	mem_std::Vector<uint8_t> written; // per block write
	mem_std::Vector<Var> copies; // per block write, in the field's type
	mem_std::Vector<Var> cells; // per GpuProgram::cells, in the record field's type
	GpuPath path;
};

template <typename Graph>
bool decodeGpuBranch(const Graph &, const GpuProgram &, const GpuLoaded &, BytesView outs, uint32_t branch,
		GpuBranchResult &);

// A branch the shader ran, written into its frame the way the machine would have left it: the
// header, the copies, every node's state and record. What a delivery then commits is the GPU's. The
// stalls a closed branch leaves are the caller's to push (Local::pushStalled) - they belong to the
// run's report.
template <typename Graph, typename Local>
Status writeGpuBranch(const Graph &, Local &, const GpuProgram &, uint32_t activation, const GpuBranchResult &);

// A Var of a GPU type written at, or read from, a std430 member.
SP_PUBLIC void writeGpuValue(uint8_t *, VarType, const Var &);
SP_PUBLIC Var readGpuValue(const uint8_t *, VarType);

// The std430 rules: scalars 4/4, vec2 8/8, vec3 12/16, vec4 16/16, a Bool as a `uint`.
void layoutStd430(GpuLayout &);
uint32_t getGpuTypeSize(VarType);
uint32_t getGpuTypeAlign(VarType);
StringView getGpuGlslType(VarType);
bool isGpuValueType(VarType);

// How far a GPU value may lie from the CPU's, at level C.
struct GpuTolerance {
	double abs = 0.0;
	double rel = 0.0;
	bool exact = true;
};

SP_PUBLIC GpuTolerance getGpuTolerance(VarType);
SP_PUBLIC bool nearlyEqual(const Var &cpu, const Var &gpu);

// A collector that folds on the GPU: its kind and type, and the cell its branches hand it. The tree
// order on the CPU is pairs (i, i + s) for s = 1, 2, 4..., the left one kept on a tie, an absent
// side taking the other. `kind`: sum, min, max, count, any, all.
struct GpuReducer {
	uint32_t collector = InvalidIndex; // graph index
	StringView kind; // sum, min, max, count, any, all
	VarType type = VarType::Nil;
	GpuSource source = GpuSource::Zero; // Cell or Index
	uint32_t cell = InvalidIndex;
};

// The reducers the library carries, as (kind, type) pairs, and the text of each one's shader: a
// pass of the tree, `values[i] = op(values[i], values[i + stride])` over i = 0, 2*stride, ...
SP_PUBLIC SpanView<Pair<StringView, VarType>> getGpuReducers();
SP_PUBLIC mem_std::String writeGpuReduceGlsl(StringView kind, VarType type);
SP_PUBLIC mem_std::String getGpuReduceName(StringView kind, VarType type); // par_reduce_sum_i32

// The reducers of a block's GPU collectors; a collector of a type outside the set folds on the CPU.
template <typename Graph>
void collectGpuReducers(const Graph &, const GpuProgram &, mem_std::Vector<GpuReducer> &);

SP_PUBLIC bool treeFoldReference(StringView kind, VarType type, SpanView<Var> values,
		SpanView<uint8_t> present, Var &out);

// The conformance of one body: what a GLSL body computes against what its C++ body computes, on a
// device. The lowering's own evaluator runs the C++ bodies, so it cannot answer this. The shader is
// the same three bindings a block's is - Block, In[], Out[] - so a device runs it through the same
// path: In carries one record per case (the operation's inputs), Out takes its outputs and the
// status it returned. `in` and `out` come back laid out, so the caller writes and reads the bytes
// the way the loader does.
SP_PUBLIC mem_std::String writeGpuConformanceGlsl(const OpDesc &, GpuLayout &in, GpuLayout &out);

// The name of that shader: `conf_<shader name>`, so the file and the pipeline are one word apart.
SP_PUBLIC mem_std::String getGpuConformanceName(const OpDesc &);

// What the host attaches. There is no GLSL compiler in a run: a block reaches the GPU only when the
// host hands over the SPIR-V that was built from the text this lowering writes. The text's hash is
// the key, so a shader built from another version of the graph is refused rather than run.

struct GpuShaderBinary {
	StringView name; // the file's base name: a block's is ignored, a reducer's must match
	uint64_t textHash = 0;
	SpanView<uint32_t> spirv;
};

struct GpuReducerBinding {
	GpuReducer reducer;
	mem_std::String name;
	uint64_t textHash = 0;
	mem_std::Vector<uint32_t> spirv;
};

// One block ready for the device: its program (lowered once), its shader, and the reducers whose
// shaders came with it. A collector without one folds on the CPU.
struct GpuBlockShaders {
	uint32_t block = InvalidIndex;
	uint64_t textHash = 0;
	GpuProgram program;
	mem_std::Vector<uint32_t> spirv;
	mem_std::Vector<GpuReducerBinding> reducers;
};

class SP_PUBLIC GpuShaderTable {
public:
	// Lowers every GPU block of the graph and keeps the ones whose hash the host's shaders match.
	// Returns how many became ready. Attaching is for a graph no run holds: it moves what a launch
	// reads.
	template <typename Graph>
	uint32_t attach(const Graph &, SpanView<GpuShaderBinary>);

	const GpuBlockShaders *get(uint32_t block) const;
	uint32_t getBlockCount() const { return uint32_t(_blocks.size()); }
	SpanView<GpuBlockShaders> getBlocks() const { return _blocks; }

	// The hash of everything attached: the device keys its compiled pipelines by it.
	uint64_t getIdentity() const { return _identity; }

	void clear();

protected:
	mem_std::Vector<GpuBlockShaders> _blocks;
	uint64_t _identity = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWGPU_H_ */
