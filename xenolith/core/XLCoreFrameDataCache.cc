/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLCoreFrameDataCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// The wire form of an operation list: {e: epoch, ops: [[op, kind, id, generation, body?], ...]}
static constexpr auto FrameDataEpochKey = "e";
static constexpr auto FrameDataOpsKey = "ops";

static sprt::atomic<size_t> s_frameDataLiveEntries = 0;

// --- FrameDataMirror ---

bool FrameDataMirror::init(size_t budget) {
	_budget = budget;
	_stats.budget = budget;
	return true;
}

void FrameDataMirror::beginSerialization() { ++_serial; }

bool FrameDataMirror::reference(FrameDataKind kind, uint64_t id, uint32_t generation) {
	if (!isEnabled() || id == 0) {
		return false;
	}
	auto it = _entries.find(id);
	if (it == _entries.end() || it->second.kind != kind || it->second.generation != generation) {
		return false;
	}
	touch(id, it->second);
	++_stats.references;
	return true;
}

bool FrameDataMirror::store(FrameDataKind kind, uint64_t id, uint32_t generation, BytesView body) {
	if (!isEnabled() || id == 0) {
		return false;
	}

	const auto cost = getFrameDataCost(body.size());
	if (cost > _budget) {
		++_stats.inlined;
		return false;
	}

	auto it = _entries.find(id);
	if (it != _entries.end()) {
		if (it->second.lastUse == _serial) {
			// This input already references the other generation, and the server resolves references
			// after the operations: replacing it would pull the data from under that reference.
			++_stats.inlined;
			return false;
		}
		// the new generation replaces the old one on both sides, no Drop
		erase(it);
	}

	// Evict the least recently used entries - but never one this input references
	while (_bytes + cost > _budget && !_lru.empty()) {
		auto lru = _lru.begin();
		if (lru->first == _serial) {
			break;
		}
		auto victim = _entries.find(lru->second);
		Value op;
		op.addInteger(toInt(FrameDataOp::Drop));
		op.addInteger(toInt(victim->second.kind));
		op.addInteger(int64_t(victim->first));
		op.addInteger(int64_t(victim->second.generation));
		_ops.addValue(sp::move(op));
		++_stats.drops;
		erase(victim);
	}

	if (_bytes + cost > _budget) {
		++_stats.inlined;
		return false;
	}

	Value op;
	op.addInteger(toInt(FrameDataOp::Store));
	op.addInteger(toInt(kind));
	op.addInteger(int64_t(id));
	op.addInteger(int64_t(generation));
	op.addBytes(body);
	_ops.addValue(sp::move(op));

	auto &entry = _entries.emplace(id, Entry{kind, generation, cost, 0}).first->second;
	_bytes += cost;
	touch(id, entry);
	++_stats.stores;
	return true;
}

Value FrameDataMirror::takeOps() {
	if (!isEnabled()) {
		return Value();
	}
	Value ret;
	ret.setInteger(int64_t(_epoch), FrameDataEpochKey);
	if (!_ops.empty()) {
		ret.setValue(sp::move(_ops), FrameDataOpsKey);
		_ops = Value();
	}
	return ret;
}

void FrameDataMirror::reset(uint32_t epoch) {
	_entries.clear();
	_lru.clear();
	_ops = Value();
	_bytes = 0;
	_epoch = epoch;
	++_stats.resets;
}

FrameDataMirrorStats FrameDataMirror::getStats() const {
	auto ret = _stats;
	ret.entries = _entries.size();
	ret.bytes = _bytes;
	ret.epoch = _epoch;
	return ret;
}

void FrameDataMirror::touch(uint64_t id, Entry &entry) {
	if (entry.lastUse != 0) {
		_lru.erase(Pair<uint64_t, uint64_t>(entry.lastUse, id));
	}
	entry.lastUse = _serial;
	_lru.emplace(entry.lastUse, id);
}

void FrameDataMirror::erase(Map<uint64_t, Entry>::iterator it) {
	_lru.erase(Pair<uint64_t, uint64_t>(it->second.lastUse, it->first));
	_bytes -= it->second.cost;
	_entries.erase(it);
}

// --- FrameDataCache ---

size_t FrameDataCache::getLiveEntries() { return s_frameDataLiveEntries.load(); }

FrameDataCache::~FrameDataCache() { clear(); }

bool FrameDataCache::init(size_t budget) {
	_budget = budget;
	_stats.budget = budget;
	return true;
}

Status FrameDataCache::apply(const Value &val) {
	if (val.empty()) {
		return Status::Ok;
	}
	if (!val.isDictionary() || !val.isInteger(FrameDataEpochKey)) {
		return Status::ErrorInvalidArguemnt;
	}
	if (uint32_t(val.getInteger(FrameDataEpochKey)) != _epoch) {
		++_stats.declined;
		return Status::Declined;
	}

	for (auto &op : val.getValue(FrameDataOpsKey).asArray()) {
		if (!op.isArray() || op.size() < 4) {
			return Status::ErrorInvalidArguemnt;
		}
		auto code = FrameDataOp(op.getInteger(0));
		auto kind = FrameDataKind(op.getInteger(1));
		auto id = uint64_t(op.getInteger(2));
		auto generation = uint32_t(op.getInteger(3));
		switch (code) {
		case FrameDataOp::Store: {
			if (id == 0 || (kind != FrameDataKind::VertexSet && kind != FrameDataKind::Gradient)
					|| !op.getValue(4).isBytes()) {
				return Status::ErrorInvalidArguemnt;
			}
			auto &body = op.getValue(4).getBytes();
			auto cost = getFrameDataCost(body.size());
			auto it = _entries.find(id);
			if (it != _entries.end()) {
				_bytes -= it->second.cost;
				it->second = Entry{kind, generation, cost, body, nullptr};
			} else {
				_entries.emplace(id, Entry{kind, generation, cost, body, nullptr});
				++s_frameDataLiveEntries;
			}
			_bytes += cost;
			++_stats.stores;
			_stats.storedBytes += body.size();
			break;
		}
		case FrameDataOp::Drop: {
			auto it = _entries.find(id);
			if (it == _entries.end() || it->second.generation != generation) {
				return Status::ErrorInvalidArguemnt;
			}
			_bytes -= it->second.cost;
			_entries.erase(it);
			--s_frameDataLiveEntries;
			++_stats.drops;
			break;
		}
		default: return Status::ErrorInvalidArguemnt;
		}
	}

	// Checked after the whole list: the client drops first and stores after, but judging it op by op
	// would make the order within the list part of the contract.
	if (_bytes > _budget) {
		return Status::ErrorBufferOverflow;
	}
	return Status::Ok;
}

FrameDataCache::Entry *FrameDataCache::find(FrameDataKind kind, uint64_t id, uint32_t generation) {
	auto it = _entries.find(id);
	if (it == _entries.end() || it->second.kind != kind || it->second.generation != generation) {
		++_stats.misses;
		return nullptr;
	}
	++_stats.hits;
	return &it->second;
}

uint32_t FrameDataCache::reset() {
	clear();
	++_epoch;
	++_stats.resets;
	return _epoch;
}

FrameDataCacheStats FrameDataCache::getStats() const {
	auto ret = _stats;
	ret.entries = _entries.size();
	ret.bytes = _bytes;
	ret.epoch = _epoch;
	return ret;
}

void FrameDataCache::clear() {
	s_frameDataLiveEntries -= _entries.size();
	_entries.clear();
	_bytes = 0;
}

} // namespace stappler::xenolith::core
