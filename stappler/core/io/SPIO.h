/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_IO_SPIO_H_
#define STAPPLER_CORE_IO_SPIO_H_

#include "SPIOBuffer.h"
#include "SPIOCommon.h"
#include "SPIOConsumer.h"
#include "SPIOProducer.h"
#include "SPCommon.h"
#include "SPBuffer.h"

namespace STAPPLER_VERSIONIZED stappler::io {

SP_PUBLIC size_t read(const Producer &from, const Callback<void(const Buffer &)> &);
SP_PUBLIC size_t read(const Producer &from, const Buffer &, const Callback<void(const Buffer &)> &);
SP_PUBLIC size_t read(const Producer &from, const Consumer &to);
SP_PUBLIC size_t read(const Producer &from, const Consumer &to, const Callback<void(const Buffer &)> &);
SP_PUBLIC size_t read(const Producer &from, const Consumer &to, const Buffer &);
SP_PUBLIC size_t read(const Producer &from, const Consumer &to, const Buffer &, const Callback<void(const Buffer &)> &);

template <typename T>
inline size_t tread(const Producer &from, const T &f) {
	StackBuffer<(size_t)1_KiB> buf;
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			f(buf);
		}
	}
	return ret;
}

template <typename T>
inline size_t tread(const Producer &from, const Buffer &buf, const T &f) {
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			f(buf);
		}
	}
	return ret;
}

template <typename T>
inline size_t tread(const Producer &from, const Consumer &to, const T &f) {
	StackBuffer<(size_t)1_KiB> buf;
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			f(buf);
			to.write(buf);
		}
	}
	return ret;
}

template <typename T>
inline size_t tread(const Producer &from, const Consumer &to, const Buffer & buf, const T &f) {
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			f(buf);
			to.write(buf);
		}
	}
	return ret;
}

}

#endif /* STAPPLER_CORE_IO_SPIO_H_ */
