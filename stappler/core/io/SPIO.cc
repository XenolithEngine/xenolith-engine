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

#include "SPIO.h"
#include "SPBuffer.h"

namespace STAPPLER_VERSIONIZED stappler::io {

size_t Producer::read(const Buffer &buf, size_t nbytes) const {
	auto pbuf = buf.prepare(nbytes);
	auto size = read_ptr(ptr, pbuf, nbytes);
	buf.save(pbuf, nbytes, size);
	return size;
}

size_t Consumer::write(const Buffer &buf) const { return write_ptr(ptr, buf.data(), buf.size()); }

size_t read(const Producer &from, const Callback<void(const Buffer &)> &f) {
	StackBuffer<(size_t)1_KiB> buf;
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			if (f) {
				f(buf);
			}
		}
	}
	return ret;
}

size_t read(const Producer &from, const Buffer &buf, const Callback<void(const Buffer &)> &f) {
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			if (f) {
				f(buf);
			}
		}
	}
	return ret;
}

size_t read(const Producer &from, const Consumer &to) {
	StackBuffer<(size_t)4_KiB> buf;
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			to.write(buf);
		}
	}
	return ret;
}
size_t read(const Producer &from, const Consumer &to, const Callback<void(const Buffer &)> &f) {
	StackBuffer<(size_t)4_KiB> buf;
	return io::read(from, to, buf, f);
}
size_t read(const Producer &from, const Consumer &to, const Buffer &buf) {
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			to.write(buf);
		}
	}
	return ret;
}
size_t read(const Producer &from, const Consumer &to, const Buffer &buf,
		const Callback<void(const Buffer &)> &f) {
	size_t ret = 0;
	size_t cap = buf.capacity();
	size_t c = cap;
	while (cap == c) {
		c = from.read(buf, cap);
		if (c > 0) {
			ret += c;
			if (f) {
				f(buf);
			}
			to.write(buf);
		}
	}
	return ret;
}

} // namespace stappler::io
