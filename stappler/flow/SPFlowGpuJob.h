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

#ifndef STAPPLER_FLOW_SPFLOWGPUJOB_H_
#define STAPPLER_FLOW_SPFLOWGPUJOB_H_

#include "SPFlowGpu.h"
#include "SPFlowMachine.h"

// A block in flight on a device, the half that does not know the graph: bytes in, bytes out, and a
// way to say the block was broken off. A host's executor sees only this; the machine's half,
// which loads the buffers and takes the records back, is SPFlowGpuJob.hpp. The rule that keeps a
// stopped loop from hanging the machine: cancel() finishes the job at once, and a device answer
// that arrives afterwards finds its request abandoned and is dropped.
namespace STAPPLER_VERSIONIZED stappler::flow {

// Why a block did not come back. `Cancelled` is the machine's own doing (a rollback, a reset, a
// timeout); the other three are the executor's verdict and carry a diagnostic.
enum class GpuAbort : uint8_t {
	None,
	Cancelled,
	Lost, // the device was lost
	Silent, // the loop stopped, or nothing came back in time
	Invalid, // the records do not replay as a run of this block
};

// One collector's fold, handed to the device as a run of values with a presence bit each. The
// values are already in std430, in the element's own stride, and a `count` writes ones and zeros.
struct GpuReduceInput {
	uint32_t reducer = InvalidIndex; // index into the block's reducers
	StringView kind;
	VarType type = VarType::Nil;
	uint32_t count = 0;
	uint32_t stride = 0; // bytes per value
	mem_std::Vector<uint8_t> values;
	mem_std::Vector<uint8_t> present; // one uint per branch
};

class SP_PUBLIC GpuBlockJob : public BranchJob {
public:
	Kind getKind() const override { return Kind::Gpu; }

	const GpuBlockShaders &getShaders() const { return *_shaders; }

	// Which set of shaders this job belongs to: the device keys its pipelines and buffers by it.
	uint64_t getTableIdentity() const { return _table; }
	uint32_t getBranchCount() const { return _branches; }
	uint32_t getInStride() const { return _inStride; }
	uint32_t getOutStride() const { return _outStride; }
	BytesView getBlockBytes() const { return BytesView(_block.data(), _block.size()); }
	BytesView getInBytes() const { return BytesView(_ins.data(), _ins.size()); }

	// The device's answer, on the machine's thread: decoded per branch, and the reducers' inputs
	// built from the cells it returned. Anything that does not decode is `Invalid`.
	virtual Status acceptOuts(BytesView outs) = 0;

	SpanView<GpuReduceInput> getReduceInputs() const { return _reduceInputs; }
	bool hasResults() const { return _hasResults; }

	// A folded value; a reducer without one folds on the CPU.
	void acceptReduced(uint32_t reducer, const Var &folded) {
		if (reducer < _reduced.size()) {
			_reduced[reducer] = folded;
			_reducedSet[reducer] = 1;
		}
	}

	void complete() { finish(); }

	void abort(GpuAbort reason) {
		if (_abort == GpuAbort::None) {
			_abort = reason;
		}
		finish();
	}

	GpuAbort getAbort() const { return _abort; }

protected:
	void onCancel() override {
		if (_abort == GpuAbort::None) {
			_abort = GpuAbort::Cancelled;
		}
		finish();
	}

	const GpuBlockShaders *_shaders = nullptr;
	uint64_t _table = 0;
	uint32_t _branches = 0;
	uint32_t _inStride = 0;
	uint32_t _outStride = 0;
	bool _hasResults = false;
	GpuAbort _abort = GpuAbort::None;
	mem_std::Vector<uint8_t> _block;
	mem_std::Vector<uint8_t> _ins;
	mem_std::Vector<GpuReduceInput> _reduceInputs;
	mem_std::Vector<Var> _reduced;
	mem_std::Vector<uint8_t> _reducedSet;
};


} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWGPUJOB_H_ */
