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

// The engine adapter's bodies. A template body file: included by
// SPFlow.scu.cpp, which instantiates the adapter for the interpreter and for a unit's run over
// every arena kind, and by whatever instantiates a machine of its own.

#ifndef STAPPLER_FLOW_SPFLOWENGINE_HPP_
#define STAPPLER_FLOW_SPFLOWENGINE_HPP_

#include "SPFlowEngine.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

template <typename Machine>
void MachineEngineT<Machine>::setGraph(const GraphType *graph) {
	if (graph != _graph) {
		_machine.reset();
		_graph = graph;
	}
}

// The three that need the graph. An engine with none answers as the machine answers a graph that
// was not built: nothing ran, and the report says so.
template <typename Machine>
Status MachineEngineT<Machine>::run(const OpRegistry &ops, ArenaType &arena, const Config &config,
		RunReport &report) {
	if (!_graph) {
		report = RunReport();
		return Status::ErrorInvalidArguemnt;
	}
	return _machine.run(*_graph, ops, arena, config, report);
}

template <typename Machine>
Status MachineEngineT<Machine>::begin(const OpRegistry &ops, ArenaType &arena, const Config &config,
		RunReport &report) {
	if (!_graph) {
		report = RunReport();
		return Status::ErrorInvalidArguemnt;
	}
	return _machine.begin(*_graph, ops, arena, config, report);
}

template <typename Machine>
Status MachineEngineT<Machine>::attach(const OpRegistry &ops, ArenaType &arena,
		const Config &config, RunReport &report) {
	if (!_graph) {
		report.outcome = RunOutcome::Invalid;
		return Status::ErrorInvalidArguemnt;
	}
	return _machine.attach(*_graph, ops, arena, config, report);
}

template <typename Machine>
WatchResolution MachineEngineT<Machine>::resolveWatch(const Watch &watch, uint32_t activation,
		const SceneStore *scene, WatchTarget &out) const {
	return flow::resolveWatch<GraphType, LocalType>(watch, activation, _graph,
			&_machine.getLocal(), scene, out);
}

// The view: one forward each.

template <typename Machine>
Addr MachineEngineT<Machine>::activationFrame(uint32_t activation) const {
	return _machine.getLocal().activationFrame(activation);
}

template <typename Machine>
Addr MachineEngineT<Machine>::frameFor(uint32_t node, uint32_t activation) const {
	return _machine.getLocal().frameFor(node, activation);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::activationScope(uint32_t activation) const {
	return _machine.getLocal().activationScope(activation);
}

template <typename Machine>
bool MachineEngineT<Machine>::hasRecord(uint32_t node, uint32_t activation) const {
	return _machine.getLocal().hasRecord(node, activation);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getFrameBytes(uint32_t scope) const {
	return _machine.getLocal().getFrameBytes(scope);
}

template <typename Machine>
Addr MachineEngineT<Machine>::getRecord(uint32_t node, uint32_t activation) const {
	return _machine.getLocal().getRecord(node, activation);
}

template <typename Machine>
Addr MachineEngineT<Machine>::getState(uint32_t node, uint32_t activation) const {
	return _machine.getLocal().getState(node, activation);
}

template <typename Machine>
Addr MachineEngineT<Machine>::getRecordIn(Addr frame, uint32_t node) const {
	return _machine.getLocal().getRecordIn(frame, node);
}

template <typename Machine>
Addr MachineEngineT<Machine>::getStateIn(Addr frame, uint32_t node) const {
	return _machine.getLocal().getStateIn(frame, node);
}

template <typename Machine>
NodeStateData MachineEngineT<Machine>::readState(Addr stateAddr) const {
	return _machine.getLocal().readState(stateAddr);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getRecordCount() const {
	return _machine.getLocal().getRecordCount();
}

template <typename Machine>
void MachineEngineT<Machine>::forEachRecord(
		const Callback<bool(uint32_t, uint32_t, Addr)> &cb) const {
	_machine.getLocal().forEachRecord(cb);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getActivationCount() const {
	return _machine.getLocal().getActivationCount();
}

template <typename Machine>
ActivationData MachineEngineT<Machine>::readActivation(uint32_t activation) const {
	return _machine.getLocal().readActivation(activation);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::resolveScope(uint32_t activation, uint32_t scope) const {
	return _machine.getLocal().resolveScope(activation, scope);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getOpenCount() const {
	return _machine.getLocal().getOpenCount();
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getOpenAt(uint32_t index) const {
	return _machine.getLocal().getOpenAt(index);
}

template <typename Machine>
bool MachineEngineT<Machine>::peekReady(uint32_t &node, uint32_t &activation) const {
	return _machine.getLocal().peekReady(node, activation);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getReadyCount() const {
	return _machine.getLocal().getReadyCount();
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getStalledCount() const {
	return _machine.getLocal().getStalledCount();
}

template <typename Machine>
uint64_t MachineEngineT<Machine>::getStalledAt(uint32_t index) const {
	return _machine.getLocal().getStalledAt(index);
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getStep() const {
	return _machine.getLocal().getStep();
}

template <typename Machine>
uint32_t MachineEngineT<Machine>::getPass() const {
	return _machine.getLocal().getPass();
}

template <typename Machine>
void MachineEngineT<Machine>::describe(mem_std::Value &out) const {
	_machine.getLocal().describe(out);
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWENGINE_HPP_ */
