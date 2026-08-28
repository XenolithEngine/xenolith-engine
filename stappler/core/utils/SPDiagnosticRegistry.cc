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

#include "SPDiagnosticRegistry.h"

#include <sprt/cxx/mutex>

namespace STAPPLER_VERSIONIZED stappler::diagnostic {

/* Chunked storage, deliberately not a growing array.

A code is an index into this, and `getMessage` hands out a StringView pointing at the entry. A
Vector that reallocated would move the entries a previous reader is still holding; chunks never
move, so a view handed out at startup is as good at shutdown.

The whole registry is also intentionally small: it holds pointers to literals somebody else owns,
so a chunk is 64 pointers plus two lengths, not 64 strings. */
namespace {

constexpr uint32_t ChunkSize = 64;

struct Chunk {
	StringView entries[ChunkSize];
	Chunk *next = nullptr;
};

struct Registry {
	sprt::qmutex mutex;
	Chunk *first = nullptr;
	Chunk *last = nullptr;
	uint32_t count = 0; // number of registered messages; codes are 1-based

	// Never freed: the registry lives as long as the process, exactly like the literals in it. A
	// destructor here would run at exit while other statics may still be reporting diagnostics.
	Chunk *chunkFor(uint32_t index) const {
		auto chunk = first;
		for (uint32_t i = index / ChunkSize; i > 0 && chunk; --i) { chunk = chunk->next; }
		return chunk;
	}
};

static Registry &getRegistry() {
	// Function-local: a module registering its codes from a static initialiser must find the
	// registry constructed, whatever order the translation units run in.
	static Registry s_registry;
	return s_registry;
}

} // namespace

uint32_t registerMessage(StringView text) {
	if (text.empty()) {
		return NoMessage;
	}

	auto &registry = getRegistry();
	registry.mutex.lock();

	// The same text must give the same code, or two modules naming one situation would hand a
	// reader two numbers for it. Linear: the registry holds the messages of a program, not its data
	uint32_t index = 0;
	for (auto chunk = registry.first; chunk; chunk = chunk->next) {
		for (uint32_t i = 0; i < ChunkSize && index < registry.count; ++i, ++index) {
			if (chunk->entries[i] == text) {
				registry.mutex.unlock();
				return index + 1;
			}
		}
	}

	if ((registry.count % ChunkSize) == 0) {
		auto chunk = new Chunk();
		if (registry.last) {
			registry.last->next = chunk;
		} else {
			registry.first = chunk;
		}
		registry.last = chunk;
	}

	registry.last->entries[registry.count % ChunkSize] = text;
	++registry.count;

	auto code = registry.count; // 1-based: 0 is NoMessage
	registry.mutex.unlock();
	return code;
}

StringView getMessage(uint32_t code) {
	if (code == NoMessage) {
		return StringView();
	}

	auto &registry = getRegistry();
	registry.mutex.lock();

	StringView ret;
	const uint32_t index = code - 1;
	if (index < registry.count) {
		if (auto chunk = registry.chunkFor(index)) {
			ret = chunk->entries[index % ChunkSize];
		}
	}

	registry.mutex.unlock();
	return ret;
}

uint32_t getMessageCount() {
	auto &registry = getRegistry();
	registry.mutex.lock();
	auto ret = registry.count;
	registry.mutex.unlock();
	return ret;
}

} // namespace stappler::diagnostic
