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

#ifndef STAPPLER_FILESYSTEM_SPFILESYSTEMFILE_H_
#define STAPPLER_FILESYSTEM_SPFILESYSTEMFILE_H_

#include "SPFilesystemLookup.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

class SP_PUBLIC File final {
public:
	using traits_type = std::char_traits<char>;
	using streamsize = std::streamsize;
	using int_type = typename traits_type::int_type;

	static File open_tmp(StringView prefix, OpenFlags, Status * = nullptr);
	static File open(const FileInfo &info, OpenFlags, Status * = nullptr);
	static File open(const LocationInfo &info, StringView path, OpenFlags, Status * = nullptr);

	File();
	~File();

	File(File &&);
	File &operator=(File &&);

	File(const File &) = delete;
	File &operator=(const File &) = delete;

	size_t read(uint8_t *buf, size_t nbytes);
	size_t seek(int64_t offset, io::Seek s);

	size_t write(const uint8_t *buf, size_t nbytes);
	void flush();

	size_t tell() const;
	size_t size() const;

	int_type xsgetc();
	int_type xsputc(int_type c);

	streamsize xsputn(const char *s, streamsize n);
	streamsize xsgetn(char *s, streamsize n);

	void close();
	void close_remove();

	bool close_rename(const FileInfo &);

	bool is_open() const;
	explicit operator bool() const { return is_open(); }

	const char *path() const;

	template <typename Interface>
	auto readIntoMemory(size_t off = 0, size_t size = maxOf<size_t>()) ->
			typename Interface::BytesType {
		if (is_open()) {
			auto fsize = this->size();
			if (fsize <= off) {
				return typename Interface::BytesType();
			}
			if (fsize - off < size) {
				size = fsize - off;
			}
			typename Interface::BytesType ret;
			ret.resize(size);
			this->seek(off, io::Seek::Set);
			this->read(ret.data(), size);
			return ret;
		}
		return typename Interface::BytesType();
	}

protected:
	explicit File(const LocationInfo &, void *, OpenFlags);

	void set_tmp_path(const char *);

	size_t _size = 0;
	OpenFlags _flags = OpenFlags::None;
	void *_file = nullptr;
	const LocationInfo *_location = nullptr;
	const char *_filepath = nullptr;
};

} // namespace stappler::filesystem

namespace STAPPLER_VERSIONIZED stappler::io {

template <>
struct ProducerTraits<filesystem::File> {
	using type = filesystem::File;
	static size_t ReadFn(void *ptr, uint8_t *buf, size_t nbytes) {
		return ((type *)ptr)->read(buf, nbytes);
	}

	static size_t SeekFn(void *ptr, int64_t offset, Seek s) {
		return ((type *)ptr)->seek(offset, s);
	}
	static size_t TellFn(void *ptr) { return ((type *)ptr)->tell(); }
};

template <>
struct ConsumerTraits<filesystem::File> {
	using type = filesystem::File;

	static size_t WriteFn(void *ptr, const uint8_t *buf, size_t nbytes) {
		return ((type *)ptr)->write(buf, nbytes);
	}

	static void FlushFn(void *ptr) { ((type *)ptr)->flush(); }
};

} // namespace stappler::io

#endif
