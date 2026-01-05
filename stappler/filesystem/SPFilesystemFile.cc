/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include "SPFilesystemFile.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

File File::open_tmp(StringView prefix, OpenFlags, Status *) {
	/*if (prefix.empty()) {
		prefix = StringView("sa.tmp");
	}

	char buf[256] = {0};
	const char *tmp = "/tmp";
	size_t len = strlen(tmp);
	strcpy(&buf[0], tmp);
	strcpy(&buf[len], "/");
	strncpy(&buf[len + 1], prefix.data(), prefix.size());
	len += prefix.size();
	strcpy(&buf[len + 1], "XXXXXX");

	if (auto fd = ::mkstemp(buf)) {
		if (auto f = ::fdopen(fd, "wb+")) {
			auto ret = File(f, delOnClose ? Flags::DelOnClose : Flags::None);
			ret.set_tmp_path(buf);
			return ret;
		}
	}*/

	return File();
}

File File::open(const FileInfo &info, OpenFlags f, Status *st) {
	File ret;
	if (hasFlag(f, OpenFlags::Write)) {
		enumerateWritablePaths(info, [&](const LocationInfo &info, StringView path) {
			ret = open(info, path, f, st);
			return false;
		});
	} else {
		enumeratePaths(info, [&](const LocationInfo &info, StringView path) {
			ret = open(info, path, f, st);
			return false;
		});
	}
	return ret;
}

File File::open(const LocationInfo &info, StringView path, OpenFlags flags, Status *ist) {
	Status st = Status::Ok;
	auto ptr = info.interface->_open(info, path, flags, &st);
	if (ptr) {
		if (ist) {
			*ist = st;
		}
		return File(info, ptr, flags);
	}

	if (st != Status::Ok) {
		slog().error("filesystem", "Fail to open file: ", path, ": ", st);
	}

	if (ist) {
		*ist = st;
	}
	return File();
}

File::File() : _file(nullptr) { }

File::File(const LocationInfo &info, void *f, OpenFlags flags)
: _flags(flags), _file(f), _location(&info) {
	if (is_open()) {
		auto pos = seek(0, io::Seek::Current);
		auto size = seek(0, io::Seek::End);
		if (pos != maxOf<size_t>()) {
			seek(pos, io::Seek::Set);
		}
		_size = (size != maxOf<size_t>()) ? size : 0;
	}
}

File::File(File &&f)
: _size(f._size), _flags(f._flags), _file(f._file), _location(f._location), _filepath(f._filepath) {
	f._size = 0;
	f._file = nullptr;
	f._location = nullptr;
	f._filepath = nullptr;
}

File &File::operator=(File &&f) {
	_size = f._size;
	_flags = f._flags;
	_file = f._file;
	_location = f._location;
	_filepath = f._filepath;

	f._file = nullptr;
	f._location = nullptr;
	f._filepath = nullptr;
	f._size = 0;

	return *this;
}

File::~File() {
	close();
	if (_filepath) {
		delete _filepath;
		_filepath = nullptr;
	}
}

size_t File::read(uint8_t *buf, size_t nbytes) {
	if (is_open() && hasFlag(_flags, OpenFlags::Read)) {
		Status st = Status::Ok;
		size_t remains = _size - _location->interface->_tell(_file, &st);
		if (nbytes > remains) {
			nbytes = remains;
		}

		if (_location->interface->_read(_file, buf, nbytes, &st) == nbytes) {
			return nbytes;
		}
	}
	return 0;
}

size_t File::write(const uint8_t *buf, size_t nbytes) {
	if (is_open() && hasFlag(_flags, OpenFlags::Write)) {
		Status st = Status::Ok;
		auto ret = _location->interface->_write(_file, buf, nbytes, &st);
		if (ret > 0) {
			_size += ret;
		}
		return ret;
	}
	return 0;
}

void File::flush() {
	if (is_open() && hasFlag(_flags, OpenFlags::Write)) {
		Status st = Status::Ok;
		_location->interface->_flush(_file, &st);
	}
}

size_t File::seek(int64_t offset, io::Seek s) {
	if (is_open()) {
		Status st = Status::Ok;
		if (offset != 0 || s != io::Seek::Current) {
			auto pos = _location->interface->_seek(_file, offset, toInt(s), &st);
			if (st != Status::Ok) {
				return maxOf<size_t>();
			}
			return static_cast<size_t>(pos);
		}
		auto p = _location->interface->_tell(_file, &st);
		if (st == Status::Ok) {
			return static_cast<size_t>(p);
		} else {
			return maxOf<size_t>();
		}
	}
	return maxOf<size_t>();
}

size_t File::tell() const {
	Status st = Status::Ok;
	return _location->interface->_tell(_file, &st);
}

size_t File::size() const { return _size; }

typename File::int_type File::xsgetc() {
	int_type ret = traits_type::eof();
	if (is_open()) {
		uint8_t buf = 0;
		if (read(&buf, 1) == 1) {
			ret = buf;
		}
	}
	return ret;
}

typename File::int_type File::xsputc(int_type c) {
	int_type ret = traits_type::eof();

	uint8_t buf = c;
	if (write(&buf, 1) == 1) {
		ret = buf;
	}
	++_size;
	return ret;
}

typename File::streamsize File::xsputn(const char *s, streamsize n) {
	streamsize ret = -1;
	if (is_open()) {
		if (write((const uint8_t *)s, n) == n) {
			ret = n;
		}
	}
	_size += n;
	return ret;
}

typename File::streamsize File::xsgetn(char *s, streamsize n) {
	streamsize ret = -1;
	if (is_open()) {
		ret = read(reinterpret_cast<uint8_t *>(s), n);
	}
	return ret;
}

void File::close() {
	if (is_open()) {
		Status st = Status::Ok;
		if (_flags != OpenFlags::DelOnClose && _filepath) {
			_location->interface->_unlink(*_location, _filepath);
		}
		_location->interface->_close(_file, &st);
		if (_filepath) {
			delete[] _filepath;
			_filepath = nullptr;
		}
		_file = nullptr;
	}
}

void File::close_remove() {
	if (is_open()) {
		Status st = Status::Ok;
		if (_filepath) {
			_location->interface->_unlink(*_location, _filepath);
		}

		_location->interface->_close(_file, &st);

		if (_filepath) {
			delete[] _filepath;
			_filepath = nullptr;
		}
		_file = nullptr;
	}
}

bool File::close_rename(const FileInfo &info) {
	if (is_open() && _filepath) {
		bool success = false;
		enumerateWritablePaths(info, Access::None, [&](const LocationInfo &info, StringView str) {
			if (info.interface->_access(info, str, Access::Exists) == Status::Ok) {
				info.interface->_remove(info, str);
			}

			if (_location->interface == info.interface) {
				if (info.interface->_rename(*_location, _filepath, info, str) == Status::Ok) {
					success = true;
					return false;
				}
				if (info.interface->_copy) {
					success = info.interface->_copy(*_location, _filepath, info, str) == Status::Ok;
					if (success) {
						info.interface->_remove(*_location, _filepath);
					}
					return false;
				}
			}

			auto fFrom = File::open(*_location, _filepath, OpenFlags::Read);
			auto fTo = File::open(info, str,
					OpenFlags::Write | OpenFlags::Create | OpenFlags::Truncate);

			if (fFrom && fTo) {
				BufferTemplate<memory::StandartInterface> buffer(
						std::min(size_t(4_MiB), fFrom.size()));
				if (io::read(io::Producer(fFrom), io::Consumer(fTo), io::Buffer(buffer)) > 0) {
					success = true;
					return false;
				}
			}
			return false;
		});
	}
	return false;
}

bool File::is_open() const { return _file != nullptr; }

const char *File::path() const { return _filepath; }

void File::set_tmp_path(const char *buf) {
	auto len = strlen(buf);
	auto path = new char[len + 1];
	memcpy(path, buf, len + 1);
	_filepath = path;
}

} // namespace stappler::filesystem
