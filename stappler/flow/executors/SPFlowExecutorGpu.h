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

#ifndef STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORGPU_H_
#define STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORGPU_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowGpuJob.h"
#include "SPFlowMachine.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

/* The `gpu` executor, and the device it talks to. A compute device is a backend question and this
module has none, so the executor is written against an interface of bytes: the lowering says what a
block's three buffers hold (SPFlowGpu.h), the job fills two of them and reads the third, and
`GpuDevice` is whatever puts a dispatch between them - a Vulkan one in a host that has Xenolith, or
a CPU evaluator a test drives, where a timeout, a silence and a lost device are exact rather than
lucky. Nothing may block the machine's thread: a device that never answers - a loop that was
stopped calls no callback back - is the normal case, so every flight carries its own deadline on
the wall clock and a job is finished the moment it is cancelled. `dispatch` and `reduce` are called
on the machine's thread and answer later, on whatever thread the device runs; `GpuRequest` is the
handover and the only shared object, and a request that is abandoned is never touched by the
machine again, whatever the device does with it. */

// One dispatch in flight. The device fills it; the executor reads it, once, on the machine's
// thread.
class SP_PUBLIC GpuRequest : public Ref {
public:
	enum class State : uint8_t {
		Pending,
		Done,
		Failed, // the device refused it: the block falls back
		Lost, // the device is gone
		Abandoned, // the executor gave it up; whatever comes back is dropped
	};

	State getState() const { return _state.load(); }
	bool isPending() const { return _state.load() == State::Pending; }

	// The device's answer. False when the request was abandoned, so a device can drop the work.
	bool complete(State state, BytesView data);

	void abandon();

	// Valid while the request lives, after `complete` with `Done`.
	BytesView getData() const;

	uint64_t getStartMs() const { return _startMs; }
	void setStartMs(uint64_t ms) { _startMs = ms; }

protected:
	sprt::atomic<State> _state = State::Pending;
	uint64_t _startMs = 0;
	mutable sprt::mutex _mutex;
	mem_std::Vector<uint8_t> _data;
};

// A pipeline the device must have before a block can run on it: a block's body or a reducer of the
// library, named by the text the lowering writes and keyed by that text's hash.
struct GpuPipelineSpec {
	enum class Kind : uint8_t {
		Block,
		Reduce,
	};

	Kind kind = Kind::Block;
	StringView name;
	uint64_t hash = 0;
	SpanView<uint32_t> spirv;
};

// One block activation: the Block buffer, the In records, and how big an Out record is. `slot` is
// the block's own index, so a device can keep its buffers per block and reuse them between frames.
struct GpuDispatchCall {
	uint64_t table = 0; // GpuShaderTable::getIdentity
	uint32_t slot = 0;
	uint64_t pipeline = 0; // the block's text hash
	BytesView block;
	BytesView ins;
	uint32_t count = 0;
	uint32_t inStride = 0;
	uint32_t outStride = 0;
};

// The folds of one block, all of them in one call: the device answers with a value and a presence
// flag per input, in the same order.
struct GpuReduceCall {
	uint64_t table = 0;
	uint32_t slot = 0;
	SpanView<GpuReduceInput> inputs;
};

class SP_PUBLIC GpuDevice : public Ref {
public:
	virtual ~GpuDevice() = default;

	virtual StringView getName() const = 0;

	// Whether a dispatch sent now can be expected to answer: a device that is there, running and
	// not lost. A stopped loop is not available, and that is how the executor tells silence from
	// work.
	virtual bool isAvailable() const = 0;
	virtual bool isLost() const = 0;

	// The pipelines of one shader table. `prepare` may take frames; until `isPrepared`, blocks run
	// on the CPU.
	virtual bool isPrepared(uint64_t table) const = 0;
	virtual void prepare(uint64_t table, SpanView<GpuPipelineSpec>) = 0;

	// Null refuses the call here and now, and the block falls back to threads or to the front.
	virtual Rc<GpuRequest> dispatch(const GpuDispatchCall &) = 0;
	virtual Rc<GpuRequest> reduce(const GpuReduceCall &) = 0;

	// Pumped from the machine's thread, between units of work.
	virtual void update() { }

	// Waits at most `interval` for something in flight to answer; false when nothing did. For a
	// host that has nothing else to do while a block is out.
	virtual bool waitCompletion(TimeInterval) { return false; }

	// Buffers are kept per block and grow by powers of two; this gives them up. Zero is every
	// table.
	virtual void releaseBuffers(uint64_t table = 0) = 0;

	// How many times a buffer was ever allocated, and how many sets are alive: a frame series of
	// one size must not move the first number.
	virtual uint32_t getBufferAllocations() const { return 0; }
	virtual uint32_t getLiveBufferSets() const { return 0; }
};

// The capacity of one buffer, the policy a device shares: grow by powers of two, never shrink on
// a frame, and count every allocation so a test can see there was none.
struct SP_PUBLIC GpuBufferSlot {
	uint64_t capacity = 0;
	uint32_t allocations = 0;

	// True when the buffer has to be made anew.
	bool reserve(uint64_t bytes);
	void release();
};

class SP_PUBLIC GpuExecutor final : public BlockExecutor {
public:
	enum class Policy : uint8_t {
		Author, // only blocks whose `executors` lists `gpu`
		Always,
	};

	struct Options {
		Policy policy = Policy::Author;
		uint64_t silenceMs = 10'000; // a flight with no answer and no news is broken off
		const BlockClock *wallClock = nullptr; // null reads the system's monotonic clock
		bool reducers = true; // fold the collectors on the device
	};

	explicit GpuExecutor(GpuDevice *);
	GpuExecutor(GpuDevice *, const Options &);
	~GpuExecutor();

	void submit(const BlockTicket &) override;
	bool poll(BlockTicket &) override;
	void tick() override;
	StringView getName() const override { return StringView("gpu"); }

	BlockLaunch chooseLaunch(const BlockTicket &, const BlockLaunchInfo &) override;
	bool launchGpu(GpuBlockJob &) override;
	void forgetGpu(GpuBlockJob &) override;

	// The table's pipelines, so the first block does not wait for a compile. Idempotent.
	void prepare(const GpuShaderTable &);
	bool isPrepared(const GpuShaderTable &) const;

	bool waitDelivery(TimeInterval);

	GpuDevice *getDevice() const { return _device; }

	// What the last choices were, for a test that asks which executor a block went to.
	uint32_t getDispatchCount() const { return _dispatches; }
	uint32_t getReduceCount() const { return _reduces; }

private:
	enum class Stage : uint8_t {
		Dispatch,
		Reduce,
	};

	struct Flight {
		GpuBlockJob *job = nullptr;
		Rc<GpuRequest> request;
		Stage stage = Stage::Dispatch;
		uint64_t start = 0;
	};

	uint64_t nowMs() const;
	bool advance(Flight &);
	void sendReduce(Flight &);

	GpuDevice *_device = nullptr;
	Options _options;
	mem_std::Vector<BlockTicket> _ready;
	mem_std::Vector<Flight> _flights;
	uint32_t _dispatches = 0;
	uint32_t _reduces = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORGPU_H_ */
