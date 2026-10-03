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

#ifndef STAPPLER_FLOW_SPFLOWENV_H_
#define STAPPLER_FLOW_SPFLOWENV_H_

#include "SPFlowDiag.h"
#include "SPFlowValueSchema.h"

// What a run is executed in, besides its graph and its own store: the scene its operations read and
// write, and where its diagnostics go. A run's environment is a policy, carried by the local store
// (`Local::EnvType`), so the machine and the door are compiled against it and a scene door costs a
// direct call. `NoEnv` is a run with no scene at all - the kernel's own, and every graph that only
// computes - and a host with a scene supplies its own environment of the same shape.
namespace STAPPLER_VERSIONIZED stappler::flow {

// A scene of nothing, in the shape the machine and the door call a scene through. Never valid, so
// every door that reaches it answers "no scene" and every path behind that answer is dead code.
template <typename A>
class NoSceneT final {
public:
	bool isValid() const { return false; }
	A *getArena() const { return nullptr; }
	const value::TypeRegistry *getRegistry() const { return nullptr; }

	value::EntityId getGlobalEntity() const { return value::EntityId(); }

	Addr getComponent(value::EntityId, TypeId) const { return NullAddr; }
	Addr getComponent(value::EntityId, const value::ComponentType &) const { return NullAddr; }
	Addr addComponent(value::EntityId, const value::ComponentType &) { return NullAddr; }
	Status removeComponent(value::EntityId, const value::ComponentType &) {
		return Status::ErrorNotSupported;
	}

	value::EntityId findByField(const value::ComponentType &, uint32_t, BytesView) const {
		return value::EntityId();
	}
	uint32_t readEachRow(TypeId, const Callback<bool(value::EntityId, Addr)> &) const { return 0; }
	uint32_t readEachRow(const value::ComponentType &,
			const Callback<bool(value::EntityId, Addr)> &) const {
		return 0;
	}
	uint32_t query(SpanView<TypeId>, const Callback<bool(value::EntityId)> &) const { return 0; }

	Status open(A &, const value::TypeRegistry &) { return Status::ErrorNotSupported; }
};

// A run's diagnostics, written into its report as numbers: the entry's own fields, under the names
// of those fields, and no text. What a host without a vocabulary of its own reads back.
SP_PUBLIC void writeDiagNumbers(mem_std::Value *, const Diag &);

// The scene as an operation outside the kernel may hold it: whatever the environment erases its
// scene to. Nothing, here.
struct NoSceneRef { };

struct NoEnv {
	// Which environment a generated unit's steps were compiled for: a unit compiled for one cannot
	// serve a run in another, whose stores are other types.
	static constexpr uint64_t Tag = 0x6772'6170'682e'6e6full; // "graph.no"

	template <typename A>
	using Scene = NoSceneT<A>;

	using SceneRef = NoSceneRef;

	template <typename A>
	static SceneRef refOf(NoSceneT<A> *) {
		return SceneRef();
	}

	static void writeRunDiag(mem_std::Value &out, const Diag &d) { writeDiagNumbers(&out, d); }
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWENV_H_ */
