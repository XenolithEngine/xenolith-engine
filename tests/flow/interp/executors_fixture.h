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

#ifndef TESTS_FLOW_INTERP_EXECUTORS_FIXTURE_H_
#define TESTS_FLOW_INTERP_EXECUTORS_FIXTURE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"
#include "SPVStoreJournal.h"

// For the suite's arena kind - see tests.h. A fixture has to name one like everything else.
#include "../tests.h"
#include "../check/flow_check.h"

// The scene half of the executor sections. The kernel's own fixture (interp_fixture.h) runs in NoEnv,
// because a graph that only computes needs no scene - and a host's test is where a scene belongs
// (SPFlowEnv.h). The executor sections are host tests: their graphs fan out over entities, so the
// fixture brings its own scene, its own environment, and its own words for what a frame is.
//
// The scene is a store of the simplest shape the machine and the door call a scene through
// (SPFlowEnv.h names that shape NoSceneT): a row of records per component type, an entity a pair of
// numbers, and nothing else. No pages, no pools, no named table - the studio's entity store is the
// production one; this one exists so the engine's own tests can hold a block over entities without
// growing a vstore of their own.
namespace STAPPLER_VERSIONIZED stappler::test::execfx {

using namespace stappler::flow;

// What a corpus builder and the environment between two frames do to a scene - entities, components,
// fields - the four verbs the builders use.
struct SceneEnv {
	virtual ~SceneEnv() = default;

	virtual value::EntityId createEntity() = 0;

	// The scene's global entity.
	virtual value::EntityId getGlobalEntity() const = 0;

	// The record, or NullAddr when the type is not registered with the scene.
	virtual Addr addComponent(value::EntityId, TypeId) = 0;

	// On the entity's record of that component; ErrorNotFound when there is none.
	virtual Status setField(value::EntityId, TypeId, StringView field, const value::Var &) = 0;
	virtual Status getField(value::EntityId, TypeId, StringView field, value::Var &) const = 0;
};

// The scene, arena-resident: the header sits in the arena's user root and every table it names is
// arena data, so a second store opened over the same arena - the machine's view of this scene, over
// which a batch's branches run - sees the same entities and the same rows through its own reads.
// The production store pages its pools; this one keeps flat arrays, which is the whole of the
// difference a test can see.
template <typename A>
class TestSceneT {
public:
	// Frame {dt, frameIndex, elapsed} and Random {seed 1}: the environment's own components, on the
	// global entity from the first step (initScene). Idempotent, so a second scene over one registry
	// is the normal case and not a duplicate id.
	static Status registerCoreTypes(value::TypeRegistry &reg) {
		if (!reg.get(value::makeTypeId(StringView("Frame")))) {
			auto frame = reg.createDerived(StringView("Frame"), [](mem_std::Vector<value::FieldDef> &out) {
				out.emplace_back(value::FieldDef{.name = StringView("dt"),
					.type = value::VarType::Float});
				out.emplace_back(value::FieldDef{.name = StringView("frameIndex"),
					.type = value::VarType::Int});
				out.emplace_back(value::FieldDef{.name = StringView("elapsed"),
					.type = value::VarType::Float});
			});
			if (!frame) {
				return Status::ErrorInvalidArguemnt;
			}
		}
		if (!reg.get(value::makeTypeId(StringView("Random")))) {
			auto random = reg.createDerived(StringView("Random"), [](mem_std::Vector<value::FieldDef> &out) {
				out.emplace_back(value::FieldDef{.name = StringView("seed"),
					.type = value::VarType::Int,
					.def = mem_std::Value(int64_t(1))});
			});
			if (!random) {
				return Status::ErrorInvalidArguemnt;
			}
		}
		return Status::Ok;
	}

	// The scene's root, planted in the arena's user root. Every count is a use count, every capacity
	// the allocation's.
	struct SceneHeader {
		Addr entities = NullAddr;
		uint32_t entityCount = 0;
		uint32_t entityCap = 0;
		Addr pools = NullAddr;
		uint32_t poolCount = 0;
		uint32_t poolCap = 0;
	};

	struct EntitySlot {
		uint32_t generation = 1;
		uint32_t alive = 0;
	};

	struct PoolSlot {
		uint64_t type = 0;
		Addr rows = NullAddr;
		uint32_t count = 0;
		uint32_t cap = 0;
	};

	struct RowSlot {
		uint64_t entity = 0; // EntityId::pack()
		Addr record = NullAddr;
	};

	// A fresh scene: the header, the empty tables, and the global entity carrying Frame and Random.
	// Index 0 is the permanently dead slot - an EntityId's zero is invalid, and `scene.global` reads
	// that - so the global is index 1 by construction, created first.
	Status initScene(A &arena, const value::TypeRegistry &types) {
		_arena = &arena;
		_types = &types;
		auto entities = arena.alloc(16 * sizeof(EntitySlot));
		SceneHeader header;
		header.entities = entities;
		header.entityCap = 16;
		header.entityCount = 1; // the dead slot
		auto root = arena.alloc(sizeof(SceneHeader));
		if (entities == NullAddr || root == NullAddr || arena.setUserRoot(root) != Status::Ok) {
			return Status::ErrorInvalidArguemnt;
		}
		EntitySlot dead{0, 0};
		sprt::memcpy(arena.write(entities, sizeof(EntitySlot)), &dead, sizeof(EntitySlot));
		sprt::memcpy(arena.write(root, sizeof(SceneHeader)), &header, sizeof(SceneHeader));
		_header = root;
		auto global = createEntity();
		if (global.index != 1) {
			return Status::ErrorInvalidArguemnt;
		}
		for (auto name : {StringView("Frame"), StringView("Random")}) {
			auto type = types.get(name);
			if (!type || addComponent(global, *type) == NullAddr) {
				return Status::ErrorInvalidArguemnt;
			}
		}
		return Status::Ok;
	}

	// An existing scene over an arena that has one - the machine's view over this scene, and the
	// shape every store a run may hold shares (a run's own local store is a scene of its own).
	Status open(A &arena, const value::TypeRegistry &types) {
		auto root = arena.getUserRoot();
		if (root == NullAddr) {
			return Status::ErrorNotFound;
		}
		_arena = &arena;
		_types = &types;
		_header = root;
		return Status::Ok;
	}

	bool isValid() const { return _arena != nullptr && _header != NullAddr; }
	A *getArena() const { return _arena; }
	const value::TypeRegistry *getRegistry() const { return _types; }

	// The global entity: index 1 by construction (initScene), never destroyed by anything a test
	// does.
	static constexpr uint32_t GlobalEntityIndex = 1;

	value::EntityId getGlobalEntity() const {
		auto id = value::EntityId{GlobalEntityIndex, entityAt(GlobalEntityIndex).generation};
		return isAlive(id) ? id : value::EntityId();
	}

	value::EntityId createEntity() {
		auto header = readHeader();
		for (uint32_t i = 1; i < header.entityCount; ++i) {
			auto slot = entityAt(i);
			if (!slot.alive) {
				slot.alive = 1;
				++slot.generation;
				writeEntity(i, slot);
				return value::EntityId{i, slot.generation};
			}
		}
		if (header.entityCount == header.entityCap) {
			auto cap = header.entityCap ? header.entityCap * 2 : 16;
			auto next = _arena->realloc(header.entities, cap * sizeof(EntitySlot));
			if (next == NullAddr) {
				return value::EntityId();
			}
			header.entities = next;
			header.entityCap = cap;
			// The header first: a helper that re-reads it would otherwise write through the old
			// table's address.
			writeHeader(header);
		}
		EntitySlot slot{1, 1};
		writeEntity(header.entityCount, slot);
		header.entityCount += 1;
		writeHeader(header);
		return value::EntityId{header.entityCount - 1, 1};
	}

	bool isAlive(value::EntityId id) const {
		auto header = readHeader();
		if (id.index >= header.entityCount) {
			return false;
		}
		auto slot = entityAt(id.index);
		return slot.alive && slot.generation == id.generation;
	}

	// Idempotent, exactly like the production store: an entity that already has the component gets
	// the record it had.
	Addr addComponent(value::EntityId id, const value::ComponentType &type) {
		if (!isAlive(id)) {
			return NullAddr;
		}
		if (auto existing = getComponent(id, type); existing != NullAddr) {
			return existing;
		}
		auto record = type.createInstance(*_arena);
		if (record == NullAddr) {
			return NullAddr;
		}
		auto header = readHeader();
		bool found = false;
		for (uint32_t p = 0; p < header.poolCount; ++p) {
			auto pool = poolAt(p);
			if (pool.type == uint64_t(type.getId())) {
				found = true;
				if (pool.count == pool.cap) {
					auto cap = pool.cap ? pool.cap * 2 : 16;
					auto next = _arena->realloc(pool.rows, cap * sizeof(RowSlot));
					if (next == NullAddr) {
						return NullAddr;
					}
					pool.rows = next;
					pool.cap = cap;
				}
				RowSlot row{id.pack(), record};
				sprt::memcpy(
						_arena->write(pool.rows + pool.count * sizeof(RowSlot), sizeof(RowSlot)),
						&row, sizeof(RowSlot));
				pool.count += 1;
				writePool(p, pool);
				break;
			}
		}
		if (!found) {
			if (header.poolCount == header.poolCap) {
				auto cap = header.poolCap ? header.poolCap * 2 : 8;
				auto next = _arena->realloc(header.pools, cap * sizeof(PoolSlot));
				if (next == NullAddr) {
					return NullAddr;
				}
				header.pools = next;
				header.poolCap = cap;
			}
			auto rows = _arena->alloc(16 * sizeof(RowSlot));
			if (rows == NullAddr) {
				return NullAddr;
			}
			PoolSlot pool{uint64_t(type.getId()), rows, 1, 16};
			RowSlot row{id.pack(), record};
			sprt::memcpy(_arena->write(rows, sizeof(RowSlot)), &row, sizeof(RowSlot));
			sprt::memcpy(
					_arena->write(header.pools + header.poolCount * sizeof(PoolSlot),
							sizeof(PoolSlot)),
					&pool, sizeof(PoolSlot));
			header.poolCount += 1;
			writeHeader(header);
		}
		return record;
	}

	Status removeComponent(value::EntityId id, const value::ComponentType &type) {
		if (!isAlive(id)) {
			return Status::ErrorNotFound;
		}
		auto header = readHeader();
		for (uint32_t p = 0; p < header.poolCount; ++p) {
			auto pool = poolAt(p);
			if (pool.type != uint64_t(type.getId())) {
				continue;
			}
			for (uint32_t r = 0; r < pool.count; ++r) {
				auto row = rowAt(pool, r);
				if (row.entity == id.pack()) {
					type.freeInstance(*_arena, row.record);
					// Swap-remove: the order of the rows that remain is the order they were added.
					auto last = rowAt(pool, pool.count - 1);
					sprt::memcpy(
							_arena->write(pool.rows + r * sizeof(RowSlot), sizeof(RowSlot)), &last,
							sizeof(RowSlot));
					pool.count -= 1;
					writePool(p, pool);
					return Status::Ok;
				}
			}
		}
		return Status::ErrorNotFound;
	}

	Addr getComponent(value::EntityId id, const value::ComponentType &type) const {
		if (!isValid() || !isAlive(id)) {
			return NullAddr;
		}
		auto header = readHeader();
		for (uint32_t p = 0; p < header.poolCount; ++p) {
			auto pool = poolAt(p);
			if (pool.type != uint64_t(type.getId())) {
				continue;
			}
			for (uint32_t r = 0; r < pool.count; ++r) {
				auto row = rowAt(pool, r);
				if (row.entity == id.pack()) {
					return row.record;
				}
			}
		}
		return NullAddr;
	}

	Addr getComponent(value::EntityId id, TypeId type) const {
		auto ct = _types ? _types->get(type) : nullptr;
		return ct ? getComponent(id, *ct) : NullAddr;
	}

	bool hasComponent(value::EntityId id, const value::ComponentType &type) const {
		return getComponent(id, type) != NullAddr;
	}

	// The rows of one type, in the order they were added. A fan-out's branch order is this order,
	// which is what makes two fixtures that built the same scene agree on what branch 0 is.
	uint32_t readEachRow(const value::ComponentType &type,
			const Callback<bool(value::EntityId, Addr)> &cb) const {
		return readEachRow(type.getId(), cb);
	}

	uint32_t readEachRow(TypeId type, const Callback<bool(value::EntityId, Addr)> &cb) const {
		uint32_t count = 0;
		auto header = readHeader();
		for (uint32_t p = 0; p < header.poolCount; ++p) {
			auto pool = poolAt(p);
			if (pool.type != uint64_t(type)) {
				continue;
			}
			for (uint32_t r = 0; r < pool.count; ++r) {
				auto row = rowAt(pool, r);
				auto id = value::EntityId::unpack(row.entity);
				if (!isAlive(id)) {
					continue;
				}
				++count;
				if (!cb(id, row.record)) {
					return count;
				}
			}
		}
		return count;
	}

	// The entities that have every type of the query, in entity order - a stricter order than
	// readEachRow's, because a fan-out's branches are the answer and two runs of one graph must open
	// them in the same order however the pools happen to sit.
	uint32_t query(SpanView<TypeId> types, const Callback<bool(value::EntityId)> &cb) const {
		auto header = readHeader();
		mem_std::Vector<uint8_t> taken(header.entityCount, 1);
		for (auto type : types) {
			mem_std::Vector<uint8_t> has(header.entityCount, 0);
			for (uint32_t p = 0; p < header.poolCount; ++p) {
				auto pool = poolAt(p);
				if (pool.type != uint64_t(type)) {
					continue;
				}
				for (uint32_t r = 0; r < pool.count; ++r) {
					auto row = rowAt(pool, r);
					auto id = value::EntityId::unpack(row.entity);
					if (id.index < header.entityCount) {
						has[id.index] = 1;
					}
				}
			}
			for (uint32_t i = 0; i < header.entityCount; ++i) {
				taken[i] = taken[i] && has[i];
			}
		}
		uint32_t count = 0;
		for (uint32_t i = 0; i < header.entityCount; ++i) {
			if (!taken[i]) {
				continue;
			}
			auto id = value::EntityId{i, entityAt(i).generation};
			if (!isAlive(id)) {
				continue;
			}
			++count;
			if (!cb(id)) {
				return count;
			}
		}
		return count;
	}

	// The first entity, in entity order, whose record holds these bytes at this offset.
	value::EntityId findByField(const value::ComponentType &type, uint32_t fieldOffset,
			BytesView value) const {
		if (value.empty() || fieldOffset + value.size() > type.getSize()) {
			return value::EntityId();
		}
		auto header = readHeader();
		for (uint32_t p = 0; p < header.poolCount; ++p) {
			auto pool = poolAt(p);
			if (pool.type != uint64_t(type.getId())) {
				continue;
			}
			for (uint32_t r = 0; r < pool.count; ++r) {
				auto row = rowAt(pool, r);
				auto id = value::EntityId::unpack(row.entity);
				if (!isAlive(id)) {
					continue;
				}
				if (sprt::memcmp(_arena->read(row.record + fieldOffset, uint32_t(value.size())),
							value.data(), value.size())
						== 0) {
					return id;
				}
			}
		}
		return value::EntityId();
	}

	// The scene as values, for the oracle: every type in registry order, every row in row order,
	// every field through the type's own encoder - so two runs are compared under the same tolerance
	// the records are.
	void saveScene(mem_std::Value &out) const {
		auto &types = out.newArray("types");
		for (uint32_t t = 0; t < (_types ? _types->getCount() : 0); ++t) {
			auto ct = _types->getAt(t);
			auto &typeOut = types.addDict();
			typeOut.setString(ct->getName(), "name");
			auto &rows = typeOut.newArray("rows");
			readEachRow(*ct, [&](value::EntityId id, Addr record) {
				auto &entry = rows.addDict();
				entry.setInteger(id.index, "index");
				entry.setInteger(id.generation, "generation");
				ct->encodeInstance(*_arena, record, entry.newDict(StringView("fields")));
				return true;
			});
		}
	}

protected:
	SceneHeader readHeader() const {
		SceneHeader header;
		sprt::memcpy(&header, _arena->read(_header, sizeof(SceneHeader)), sizeof(SceneHeader));
		return header;
	}

	void writeHeader(const SceneHeader &header) {
		sprt::memcpy(_arena->write(_header, sizeof(SceneHeader)), &header, sizeof(SceneHeader));
	}

	EntitySlot entityAt(uint32_t i) const {
		auto header = readHeader();
		EntitySlot slot;
		sprt::memcpy(&slot, _arena->read(header.entities + i * sizeof(EntitySlot), sizeof(EntitySlot)),
				sizeof(EntitySlot));
		return slot;
	}

	void writeEntity(uint32_t i, const EntitySlot &slot) {
		auto header = readHeader();
		sprt::memcpy(
				_arena->write(header.entities + i * sizeof(EntitySlot), sizeof(EntitySlot)), &slot,
				sizeof(EntitySlot));
	}

	PoolSlot poolAt(uint32_t p) const {
		auto header = readHeader();
		PoolSlot pool;
		sprt::memcpy(&pool, _arena->read(header.pools + p * sizeof(PoolSlot), sizeof(PoolSlot)),
				sizeof(PoolSlot));
		return pool;
	}

	void writePool(uint32_t p, const PoolSlot &pool) {
		auto header = readHeader();
		sprt::memcpy(_arena->write(header.pools + p * sizeof(PoolSlot), sizeof(PoolSlot)), &pool,
				sizeof(PoolSlot));
	}

	RowSlot rowAt(const PoolSlot &pool, uint32_t r) const {
		RowSlot row;
		sprt::memcpy(&row, _arena->read(pool.rows + r * sizeof(RowSlot), sizeof(RowSlot)),
				sizeof(RowSlot));
		return row;
	}

	A *_arena = nullptr;
	const value::TypeRegistry *_types = nullptr;
	Addr _header = NullAddr;
};

// The environment the executor sections run in: the scene above, and the kernel's own words for a
// diagnostic (writeDiagNumbers, the vocabulary a host without one of its own reads back).
struct TestEnv {
	static constexpr uint64_t Tag = 0x7465'7374'2e73'636eull; // "test.scn"

	template <typename A>
	using Scene = TestSceneT<A>;

	// Kind-erased and unused here: nothing in the kernel's own operations reaches a scene through
	// the reference - the door holds the typed scene - so the shape is the env contract's and no
	// more.
	struct SceneRef { };

	template <typename A>
	static SceneRef refOf(TestSceneT<A> *) {
		return SceneRef();
	}

	static void writeRunDiag(mem_std::Value &out, const Diag &d) { writeDiagNumbers(&out, d); }
};

// A clock a caller moves by hand, for a block's timeout: the tests', and a debugger's that stops
// while the run is held.
class ManualClock final : public BlockClock {
public:
	uint64_t nowMs() const override { return _now; }
	void advance(uint64_t ms) { _now += ms; }

private:
	uint64_t _now = 0;
};

// The fixture of the executor sections: the kernel's own fixture with a scene beside it, built the
// way a host builds one - the scene's own registry (nothing derived from a node signature belongs
// there), the scene's own arena (a run's store dies with the run; a scene does not), and the graph
// bound to the scene when there is one.
template <typename A>
struct FixtureT : SceneEnv {
	using ArenaType = A;
	using SceneType = TestSceneT<A>;
	using InterpType = InterpreterT<A, TestEnv>;
	using ConfigType = RunConfigT<A, TestEnv>;

	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	A arena;

	// Built only by prepareScene(). The registry outlives the operations and holds a different set;
	// the arena outlives every run over the scene.
	value::TypeRegistry sceneTypes;
	A sceneArena;
	SceneType scene;

	// Built only by prepareJournal(). Declared before the interpreter so that it outlives the run
	// whose arenas it observes.
	vstore::Journal journal;

	InterpType interp;
	RunReport report;

	bool (*extraOps)(OpRegistry &) = nullptr;

	bool registerOps() {
		if (!ops.init() || flow::ops::registerCoreOps(ops) != Status::Ok) {
			return false;
		}
		if (InterpType::registerCoreTypes(ops.getLocalTypes()) != Status::Ok) {
			return false;
		}

		PinDesc value[] = {PinDesc{.name = StringView("value"), .type = flow::value::VarType::Float}};
		{
			OpDef def;
			def.name = StringView("test.fail");
			def.dataIn = SpanView<PinDesc>(value, 1);
			def.invoke = &opFail;
			if (!ops.createNative(def)) {
				return false;
			}
		}
		{
			const StringView then[] = {StringView("then")};
			OpDef def;
			def.name = StringView("test.split");
			def.dataIn = SpanView<PinDesc>(value, 1);
			def.dataOut = SpanView<PinDesc>(value, 1);
			def.hasExecIn = true;
			def.execOut = SpanView<StringView>(then, 1);
			def.invoke = &opSplit;
			if (!ops.createNative(def)) {
				return false;
			}
		}
		return extraOps ? extraOps(ops) : true;
	}

	// The scene, with the caller's types beside the core ones. Must come before prepare().
	bool prepareScene(bool (*types)(value::TypeRegistry &) = nullptr) {
		if (!sceneTypes.init() || !sceneArena.init()) {
			return false;
		}
		if (SceneType::registerCoreTypes(sceneTypes) != Status::Ok) {
			return false;
		}
		if (types && !types(sceneTypes)) {
			return false;
		}
		return scene.initScene(sceneArena, sceneTypes) == Status::Ok;
	}

	// One journal over both stores: a version is a consistent slice of the run and the scene
	// together, and one rollback moves both.
	bool prepareJournal() {
		if constexpr (A::IsTracked) {
			if (!arena.isInitialized() || !journal.init()) {
				return false;
			}
			journal.attach(&arena);
			if (scene.isValid()) {
				journal.attach(&sceneArena);
			}
			return true;
		} else {
			return false;
		}
	}

	ConfigType sceneConfig() {
		ConfigType config;
		config.scene = scene.isValid() ? &scene : nullptr;
		return config;
	}

	ConfigType journalConfig() {
		auto config = sceneConfig();
		config.journal = &journal;
		return config;
	}

	// The environment's half of a frame - dt and the frame index onto the global entity, before the
	// run - so that a replayed frame sees the same dt the first run did.
	Status frameEnv(double dt, uint32_t index, const ConfigType &config) {
		auto type = sceneTypes.get(value::makeTypeId(StringView("Frame")));
		auto record = type ? scene.getComponent(scene.getGlobalEntity(), *type) : NullAddr;
		if (record == NullAddr) {
			return Status::ErrorNotFound;
		}
		auto st = type->setField(sceneArena, record, StringView("dt"),
				flow::value::makeFloat(dt));
		if (st != Status::Ok) {
			return st;
		}
		st = type->setField(sceneArena, record, StringView("frameIndex"),
				flow::value::makeInt(int64_t(index)));
		if (st != Status::Ok) {
			return st;
		}
		if (config.journal) {
			config.journal->commit();
			config.journal->setKeyframe();
		}
		return Status::Ok;
	}

	// One frame: the environment's half, then the run.
	Status frame(double dt, uint32_t index, const ConfigType &config = ConfigType()) {
		auto st = frameEnv(dt, index, config);
		if (st != Status::Ok) {
			return st;
		}
		return run(config);
	}

	// The same frame, stepped: the environment's half and then begin(), so a caller can hold the
	// run at a block in flight.
	Status frameBegin(double dt, uint32_t index, const ConfigType &config) {
		auto st = frameEnv(dt, index, config);
		if (st != Status::Ok) {
			return st;
		}
		return interp.begin(graph, ops, arena, config, report);
	}

	// An EntityRef the way a node parameter spells one.
	static mem_std::String entityRef(value::EntityId id) {
		return mem_std::toString("{\"index\": ", id.index, ", \"generation\": ", id.generation, "}");
	}

	bool prepare(StringView json) {
		if (!registerOps()) {
			return false;
		}

		asset.init();
		mem_std::Value diag;
		if (asset.load(data::read<mem_std::Interface>(json), &diag) != Status::Ok) {
			sprt::cout << "       asset: " << data::toString<mem_std::Interface>(diag, true) << "\n";
			return false;
		}

		graph.init();
		mem_std::Value buildReport;
		auto st = scene.isValid() ? graph.build(asset, ops, sceneTypes, &buildReport)
								  : graph.build(asset, ops, &buildReport);
		if (st != Status::Ok) {
			sprt::cout << "       build: " << data::toString<mem_std::Interface>(buildReport, true)
					   << "\n";
			return false;
		}
		return arena.init();
	}

	Status run(const ConfigType &config = ConfigType()) {
		return interp.run(graph, ops, arena, config, report);
	}

	// ---- SceneEnv, over the fixture's scene -----------------------------------------------------

	value::EntityId createEntity() override { return scene.createEntity(); }
	value::EntityId getGlobalEntity() const override { return scene.getGlobalEntity(); }

	Addr addComponent(value::EntityId id, TypeId typeId) override {
		auto type = sceneTypes.get(typeId);
		return type ? scene.addComponent(id, *type) : NullAddr;
	}

	Status setField(value::EntityId id, TypeId typeId, StringView field,
			const flow::value::Var &value) override {
		auto type = sceneTypes.get(typeId);
		auto record = type ? scene.getComponent(id, *type) : NullAddr;
		if (record == NullAddr) {
			return Status::ErrorNotFound;
		}
		return type->setField(sceneArena, record, field, value);
	}

	Status getField(value::EntityId id, TypeId typeId, StringView field,
			flow::value::Var &out) const override {
		auto type = sceneTypes.get(typeId);
		auto record = type ? scene.getComponent(id, *type) : NullAddr;
		if (record == NullAddr) {
			return Status::ErrorNotFound;
		}
		return type->getField(sceneArena, record, field, out);
	}

	// The value a node left in one of its outputs - exactly what the next node's getInput reads.
	bool output(NodeId id, uint32_t pin, flow::value::Var &out) {
		return outputAt(id, pin, RootActivation, out);
	}

	bool outputAt(NodeId id, uint32_t pin, uint32_t activation, flow::value::Var &out) {
		auto index = graph.findNode(id);
		if (index == InvalidIndex) {
			return false;
		}
		auto &rt = graph.getNodeAt(index);
		auto record = interp.getLocal().getRecord(index, activation);
		if (!rt.localSchema || record == NullAddr) {
			return false;
		}
		auto fields = rt.localSchema->getFields();
		if (pin >= fields.size()) {
			return false;
		}
		return rt.localSchema->getField(*interp.getLocal().getArena(), record, fields[pin], out)
				== Status::Ok;
	}

	// Did this node run at all? The question every exec test asks about the branch not taken.
	bool ran(NodeId id) {
		auto index = graph.findNode(id);
		if (index == InvalidIndex) {
			return false;
		}
		auto state = interp.getLocal().readState(interp.getLocal().getState(index, RootActivation));
		return (state.flags & NodeFlags::Ran) != 0;
	}

private:
	static inline Status opFail(OpContext &) { return Status::ErrorInvalidArguemnt; }

	static inline Status opSplit(OpContext &ctx) {
		flow::value::Var value;
		auto st = ctx.getInput(0, value);
		if (st != Status::Ok) {
			return st;
		}
		st = ctx.setOutput(0, value);
		if (st != Status::Ok) {
			return st;
		}
		return ctx.fire(uint32_t(0));
	}
};

// The suite's kind, and the name every executor section reads.
using Fixture = FixtureT<Arena>;

} // namespace stappler::test::execfx

#endif /* TESTS_FLOW_INTERP_EXECUTORS_FIXTURE_H_ */
