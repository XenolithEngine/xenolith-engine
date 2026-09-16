/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

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

#include "SPZipSource.h"

namespace STAPPLER_VERSIONIZED stappler {

void ZipSource::setMemory(BytesView data) {
	_memory = data;
	_hasMemory = true;
	if (_position > _memory.size()) {
		_position = _memory.size();
	}
}

#ifdef MODULE_STAPPLER_FILESYSTEM

void ZipSource::setFile(filesystem::File &&file) {
	_file = sprt::move(file);
	_hasMemory = false;
	_position = 0;
}

#endif

bool ZipSource::valid() const {
#ifdef MODULE_STAPPLER_FILESYSTEM
	if (_file) {
		return true;
	}
#endif
	return _hasMemory;
}

uint64_t ZipSource::size() const {
#ifdef MODULE_STAPPLER_FILESYSTEM
	if (_file) {
		return _file.size();
	}
#endif
	return _memory.size();
}

size_t ZipSource::read(uint8_t *buf, size_t nbytes) {
#ifdef MODULE_STAPPLER_FILESYSTEM
	if (_file) {
		io::Producer producer(_file);
		auto ret = producer.read(buf, nbytes);
		_position = producer.tell();
		return ret;
	}
#endif
	if (!_hasMemory) {
		return 0;
	}

	CoderSource source(_memory);
	source.seek(int64_t(_position), io::Seek::Set);

	io::Producer producer(source);
	auto ret = producer.read(buf, nbytes);
	_position = producer.tell();
	return ret;
}

bool ZipSource::readAt(uint64_t offset, uint8_t *buf, size_t nbytes) {
	if (seek(int64_t(offset), io::Seek::Set) != offset) {
		return false;
	}
	return read(buf, nbytes) == nbytes;
}

uint64_t ZipSource::seek(int64_t offset, io::Seek s) {
#ifdef MODULE_STAPPLER_FILESYSTEM
	if (_file) {
		io::Producer producer(_file);
		auto ret = producer.seek(offset, s);
		if (ret == maxOf<size_t>()) {
			// the file layer reports a failed seek this way; leave the position untouched
			return maxOf<uint64_t>();
		}
		_position = ret;
		return _position;
	}
#endif
	if (!_hasMemory) {
		return maxOf<uint64_t>();
	}

	CoderSource source(_memory);
	source.seek(int64_t(_position), io::Seek::Set);

	io::Producer producer(source);
	_position = producer.seek(offset, s);
	return _position;
}

uint64_t ZipSource::tell() const { return _position; }

} // namespace STAPPLER_VERSIONIZED stappler
