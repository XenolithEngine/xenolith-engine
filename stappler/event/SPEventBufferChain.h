/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_EVENT_SPEVENTBUFFERCHAIN_H_
#define CORE_EVENT_SPEVENTBUFFERCHAIN_H_

#include "SPEvent.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct Buffer : mem_std::AllocBase {
	enum Flags {
		None = 0,
		Eos = 1 << 0,
	};

	Buffer *next = nullptr;
	memory::pool_t *pool = nullptr;

	uint8_t *buf = nullptr;
	size_t capacity = 0;
	size_t size = 0;
	size_t offset = 0;
	size_t absolute = 0;
	Flags flags = Flags::None;

	static Buffer *create(memory::pool_t *, size_t = 0);

	void release();

	StringView str() const;

	size_t availableForWrite() const;
	size_t availableForRead() const;

	uint8_t *writeTarget() const;
	uint8_t *readSource() const;

	size_t write(const uint8_t *, size_t);
};

struct BufferChain : public Ref {
	memory::pool_t *_pool = nullptr;
	Buffer *front = nullptr;
	Buffer *back = nullptr;
	Buffer **tail = nullptr;

	bool eos = false;

	explicit operator bool() const { return front != nullptr; }

	bool isSingle() const { return front != nullptr && front == back; }

	bool isEos() const;

	bool empty() const;

	size_t size() const;

	Buffer *getWriteTarget(memory::pool_t *p);

	bool write(memory::pool_t *, const uint8_t *, size_t, Buffer::Flags flags = Buffer::None);
	bool write(Buffer *);
	bool write(BufferChain &);
	bool readFromFd(memory::pool_t *, int);

	Status read(const Callback<int(const Buffer *, const uint8_t *, size_t)> &, bool release);
	Status writeToFd(int, size_t &);

	size_t getBytesRead() const;

	BytesView extract(memory::pool_t *, size_t initOffset, size_t blockSize) const;

	void releaseEmpty();
	void clear();
};

}

#endif /* CORE_EVENT_SPEVENTBUFFERCHAIN_H_ */
