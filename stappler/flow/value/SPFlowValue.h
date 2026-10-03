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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUE_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPVStore.h"

#include <sprt/c/__sprt_assert.h>

// The value layer: what a value is, how a record of values is laid out in an arena, and the
// registry of record types. Nothing here knows an entity store, a scene or a file format, so a
// graph can be built and run on this layer and stappler_vstore alone.
namespace STAPPLER_VERSIONIZED stappler::flow::value {

// The arena, its tracking kinds and the journal are stappler_vstore's, and this layer names them as
// its own.
using namespace ::stappler::vstore;

// The call counters above the arena: the arena counts arenaRead, arenaWrite and arenaMark in
// ::stappler::vstore::getCounters(), and the fields below are the callers above it. Both are
// compiled in by SP_VSTORE_COUNTERS, which only a benchmark sets. The inherited arena fields of this
// struct are filled by snapshotCounters() alone.
struct Counters : ::stappler::vstore::Counters {
	uint64_t getComponent = 0;
	uint64_t openPool = 0;
	uint64_t rowAddr = 0;
	uint64_t getField = 0;
	uint64_t setField = 0;

	// getField/setField again, by caller: the five fields of interp.NodeState, a node's own record
	// through the door, the scene through the door, and, in the arena store only, the interp.Run
	// record that carries the step and sweep numbers. Counted at the caller, in stappler_flow: one
	// schema can be a node's record in one run and something else in another. The pairs sum to
	// getField/setField less what frame initialisation and the debugger's watches spend.
	uint64_t setFieldState = 0;
	uint64_t getFieldRecord = 0;
	uint64_t setFieldRecord = 0;
	uint64_t getFieldScene = 0;
	uint64_t setFieldScene = 0;
	uint64_t getFieldRun = 0;
	uint64_t setFieldRun = 0;

	// The masks of interp.NodeState: one barrier over the field's own extent, read and written in
	// place (flow::updateStateBits), counted so that the work stays visible.
	uint64_t stateRmw = 0;

	// The two lookups by name a graph is supposed to have paid for at build time. Both are linear
	// scans, so what matters is that a running graph reaches them at all.
	uint64_t registryGet = 0;
	uint64_t fieldByName = 0;
	uint64_t arrayGet = 0;
	uint64_t arraySet = 0;

	// Linear searches of a pool started, not rows walked: a scene whose geometry the graph can
	// address answers zero here.
	uint64_t sceneFind = 0;
};

SP_PUBLIC Counters &getCounters();

// Both tallies in one value, and both cleared at once.
SP_PUBLIC Counters snapshotCounters();
SP_PUBLIC void resetCounters();

#if SP_VSTORE_COUNTERS
#define SP_FLOW_VALUE_COUNT(field) (++::stappler::flow::value::getCounters().field)
#else
#define SP_FLOW_VALUE_COUNT(field) ((void)0)
#endif

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUE_H_ */
