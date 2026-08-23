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

#ifndef STAPPLER_ZIP_SPZIPSOURCE_H_
#define STAPPLER_ZIP_SPZIPSOURCE_H_

#include "SPCoreCrypto.h" // CoderSource and its io::ProducerTraits
#include "SPIO.h"

#ifdef MODULE_STAPPLER_FILESYSTEM
#include "SPFilesystem.h"
#endif

namespace STAPPLER_VERSIONIZED stappler {

/* The seekable byte source an archive is read from.
 *
 * Whoever reads the archive - libzip today, the engine's own reader from stage 2 of
 * docs/design/libzip-removal-plan.adoc on - talks only to this, and cannot tell whether the bytes
 * live in a memory buffer or in an open file. Both backings are served by io::Producer over the
 * ProducerTraits that already exist for CoderSource and filesystem::File; this type only decides
 * which one is in play and keeps the read position.
 *
 * The position is kept HERE rather than inside a cached adapter on purpose: a writable in-memory
 * archive replaces its buffer wholesale when a write commits, so an adapter holding a view of the
 * old buffer would be left pointing at freed memory. setMemory() is therefore called again after
 * every such swap, and the memory adapter is built per call - it is four words on the stack, next
 * to nothing against the I/O it wraps.
 */
class SP_PUBLIC ZipSource {
public:
	// Points the source at a byte range. Safe to call again when the underlying buffer is replaced.
	void setMemory(BytesView);

#ifdef MODULE_STAPPLER_FILESYSTEM
	// Takes ownership of an open file and reads through it.
	void setFile(filesystem::File &&);
#endif

	bool valid() const;

	// total length of the source, which is what locating the central directory starts from
	uint64_t size() const;

	size_t read(uint8_t *buf, size_t nbytes);

	// Reads exactly `nbytes` at `offset`, or reports failure. A short read is never a partial
	// success here: everything that reads an archive is reading a record of a known size, and half
	// of one is not something a caller can do anything with.
	bool readAt(uint64_t offset, uint8_t *buf, size_t nbytes);

	// io::Seek::End counts back from the end, POSIX-style, on both backings
	uint64_t seek(int64_t offset, io::Seek);

	uint64_t tell() const;

private:
	BytesView _memory;
	bool _hasMemory = false;
	uint64_t _position = 0;

#ifdef MODULE_STAPPLER_FILESYSTEM
	filesystem::File _file;
#endif
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPSOURCE_H_ */
